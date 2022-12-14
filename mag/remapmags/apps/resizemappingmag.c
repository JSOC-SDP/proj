/*
 * Module name:		resizemappingmag.c
 *                   copy from Xudong's code testresize.c
 *
 * Example: resizemappingmag in='su_yang.fd_Mremap_large[2010.08.12/1h]' out='su_yang.resizetest' nbin=3
 *
 * Note: resize remapped mags to 1801x1440 using Jesper's rebin code. The average
 *       was done with a gaussian function.
 */


#include <jsoc_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <sys/resource.h>
  
#include <mkl_blas.h>
#include <mkl_service.h>
#include <mkl_lapack.h>
#include <mkl_vml_functions.h>
#include <omp.h>

#include "fresize.h"
#include "fstats.h"

#define	DEG2RAD	(M_PI / 180.)
#define RAD2ARCSEC	(648000. / M_PI)

#define ARRLENGTH(ARR) (sizeof(ARR) / sizeof(ARR[0]))
#define DIE(msg) {fflush(stdout); fprintf(stderr, "%s, status=%d\n", msg, status); return(status);}
#define SHOW(msg) {printf("%s", msg); fflush(stdout);}


/* ################## Timing from T. Larson ################## */

double getwalltime(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec/1000.0;
}

double getcputime(double *utime, double *stime)
{

  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);
  *utime = ru.ru_utime.tv_sec * 1000.0 + ru.ru_utime.tv_usec / 1000.0;
  *stime = ru.ru_stime.tv_sec * 1000.0 + ru.ru_stime.tv_usec / 1000.0;
  return *utime + *stime;
}


/* ################## Wrapper for Jesper's code ################## */

void frebin(float *image_in, float *image_out, int nx_in, int ny_in, int nx_out, int ny_out, int nbin)
{
  struct fresize_struct fresizes;
//  int nxout, nyout;
  int nlead_in = nx_in, nlead_out = nx_out;
  
//  nxout = nx / nbin; nyout = ny / nbin;
  init_fresize_gaussian2(&fresizes, nbin, (3 * nbin)/2, (3 * nbin)/2, nbin);

//  fresize(&fresizes, image_in, image_out, nx, ny, nlead, nxout, nyout, nxout, (nbin / 2) * 2, (nbin / 2) * 2, DRMS_MISSING_FLOAT);

  fresize(&fresizes, image_in, image_out, nx_in, ny_in, nlead_in, nx_out, ny_out, nlead_out, 4*nbin + 1, 4*nbin + 1, DRMS_MISSING_FLOAT);
  free_fresize(&fresizes);

}

/* ################## Main Module ################## */

char *module_name = "resizemappingmag";	/* Module name */

ModuleArgs_t module_args[] =
{
    {ARG_STRING, "in", NULL, "Input magnetogram series."},
    {ARG_STRING, "out", NULL, "Output data series."},
    {ARG_INT, "nbin", "3", "nbin"},
    {ARG_INT, "VERB", "1", "Level of verbosity: 0=errors/warnings; 1=all messages"},
    {ARG_END}
};


int DoIt(void)
{
    int status = DRMS_SUCCESS;
    char *inQuery, *outQuery;
    DRMS_RecordSet_t *inRS = NULL;
    DRMS_RecordSet_t *outRS = NULL;
    int irec, nrecs;
//    DRMS_Record_t *outRec = NULL;
//    DRMS_Segment_t *outSeg;
//    DRMS_Array_t *outArray;
//    float *inData, *outData;
    float fltemp;
    int intemp;
    int counting = 0;

    int i, j, itest;
    int nbin;
    int verbflag;
    int outDims[2];
    int n0, n1, xout, yout;
    TIME t_rec, tnow, UNIX_epoch = -220924792.000; /* 1970.01.01_00:00:00_UTC */;

    double statMin, statMax, statMedn, statMean, statSig, statSkew, statKurt;
    int statNgood;

    // Time measuring
    double wt0, wt1, wt;
    double ut0, ut1, ut;
    double st0, st1, st;
    double ct0, ct1, ct;
    wt0 = getwalltime();
    ct0 = getcputime(&ut0, &st0);

    char historyofthemodule[2048]; // put history info into the data
    sprintf(historyofthemodule,"Smoothing edge-pixel bug corrected -- July 2013");

    /* Get parameters */
    inQuery = (char *)params_get_str(&cmdparams, "in");
    outQuery = (char *)params_get_str(&cmdparams, "out");
    verbflag = params_get_int(&cmdparams, "VERB");
    nbin = params_get_int(&cmdparams, "nbin");

    /* Open input */
    inRS = drms_open_records(drms_env, inQuery, &status);
    if (status || inRS->n == 0) DIE("No input data found");
    nrecs = inRS->n;

    /* Create output */
//    outRS = drms_create_records(drms_env, nrecs, outQuery, DRMS_PERMANENT, &status);
//    if (status) DIE("Output recordset not created");

    /* Do this for each record */
    for (irec = 0; irec < nrecs; irec++)
    {   
        DRMS_Record_t *inRec = NULL;
        DRMS_Array_t *inArray;
        DRMS_Segment_t *inSeg = NULL;
        DRMS_Record_t *outRec = NULL;
        DRMS_Segment_t *outSeg = NULL;
        DRMS_Array_t *outArray;
        float *inData = NULL;
        float *outData = NULL;
 
        if (verbflag) {
            wt1 = getwalltime();
            ct1 = getcputime(&ut1, &st1);
            printf("processing record %d...\n", irec);
        }

        /* Input record and data */
        inRec = inRS->records[irec];        
        t_rec = drms_getkey_time(inRec, "T_REC", &status);

        if (!(inSeg = drms_segment_lookupnum(inRec, 0))) {
          printf("cannot open file\n");
          continue;
        }

        inArray = drms_segment_read(inSeg, DRMS_TYPE_FLOAT, &status);
        if (status) continue;
        inData = inArray->data;
        n0 = inArray->axis[0]; n1 = inArray->axis[1];
// create an array larger than the original array. The extra data at edge are 
// mirrowed with the data at the original edge.
        int xNewdim = n0 + 8 * nbin, yNewdim = n1 + 8 * nbin;
        int xStart = 4 * nbin, yStart = 4 * nbin;
        long long iDataOld, yOffOld, iDataNew, yOffNew;
        int ii, jj;
        float *tmpData;

        xout = n0 / nbin; yout = n1 / nbin;
//        xout = xNewdim; yout = yNewdim;
        if (verbflag) printf("n0=%d, n1=%d, xout=%d, yout=%d\n", n0, n1, xout, yout); //fflush(stdout);
        
        /* Output array */
        outDims[0] = xout; outDims[1] = yout;		// Odd pixels
        outData = (float *) malloc(xout * yout * sizeof(float));
	outArray = drms_array_create(DRMS_TYPE_FLOAT, 2, outDims, outData, &status);

        tmpData = (float *) malloc(xNewdim * yNewdim * sizeof(float));

        for (jj = 0; jj < 4 * nbin; jj++) {
            yOffNew = jj * xNewdim;
            yOffOld = (4 * nbin - jj) * n0;
            for (ii = 0; ii < n0; ii++) {
                iDataNew = yOffNew + xStart + ii;
                iDataOld = yOffOld + ii;
                tmpData[iDataNew] = inData[iDataOld];
            }
        }
      
        for (jj = 0; jj < 4 * nbin; jj++) {
            yOffNew = (jj + n1 + 4 * nbin) * xNewdim;
            yOffOld = (n1 - 1 - jj) * n0;
            for (ii = 0; ii < n0; ii++) {
                iDataNew = yOffNew + xStart + ii;
                iDataOld = yOffOld + ii;
                tmpData[iDataNew] = inData[iDataOld];
            }
        }

        for (jj = 0; jj < n1; jj++) {
            yOffNew = (jj + 4 * nbin) * xNewdim;
            yOffOld = jj * n0;
            for (ii = 0; ii < n0; ii++) {
                iDataNew = yOffNew + xStart + ii;
                iDataOld = yOffOld + ii;
                tmpData[iDataNew] = inData[iDataOld];
            }
        }

        for (jj = 0; jj < yNewdim; jj++) {
            yOffNew = jj * xNewdim;
            for (ii = 0; ii < xStart; ii++) {
                iDataNew = yOffNew + (xStart - 1) - ii;
                iDataOld = yOffNew + xStart + ii;
                tmpData[iDataNew] = tmpData[iDataOld];
            }
        }

        for (jj = 0; jj < yNewdim; jj++) {
            yOffNew = jj * xNewdim;
            for (ii = 0; ii < xStart; ii++) {
                iDataNew = yOffNew + (xNewdim - xStart) + ii;
                iDataOld = yOffNew + (xNewdim - xStart) - 1 - ii;
                tmpData[iDataNew] = tmpData[iDataOld];
            }
        }
 
        /************************************************************/
        /************************ Rebin here ************************/
        /************************************************************/
        frebin(tmpData, outData, xNewdim, yNewdim, xout, yout, nbin);
        counting++;
printf("processing files number = %d\n", counting);

// compute statistics
        if (fstats(outDims[0]*outDims[1], outData, &statMin, &statMax, &statMedn, &statMean, &statSig,
            &statSkew, &statKurt, &statNgood)) printf("\n Statistics computation failed\n");

        /* Output record */

        outRec = drms_create_record(drms_env, outQuery, DRMS_PERMANENT, &status);
        outSeg = drms_segment_lookupnum(outRec, 0);
        status = drms_segment_write(outSeg, outArray, 0);
        if (status) DIE("Problem writing file");

        // Prime key
        drms_copykey(outRec, inRec, "T_REC");

        // other keywords
        drms_setkey_string(outRec, "HISTORY", historyofthemodule);
        tnow = (double)time(NULL);
        tnow += UNIX_epoch;
        drms_setkey_time(outRec, "DATE", tnow);
        drms_copykey(outRec, inRec, "INSTRUME");
        drms_copykey(outRec, inRec, "CAMERA");
        drms_copykey(outRec, inRec, "COMMENT");
        drms_copykey(outRec, inRec, "BLD_VERS");

        drms_copykey(outRec, inRec, "QUALITY");
        drms_copykey(outRec, inRec, "CADENCE");
        drms_setkey_double(outRec, "DATAMIN", statMin);
        drms_setkey_double(outRec, "DATAMAX", statMax);
        drms_setkey_double(outRec, "DATAMEDN", statMedn);
        drms_setkey_double(outRec, "DATAMEAN", statMean);
        drms_setkey_double(outRec, "DATARMS", statSig);
        drms_setkey_double(outRec, "DATASKEW", statSkew);
        drms_setkey_double(outRec, "DATAKURT", statKurt);
        i = outDims[0]*outDims[1];
        drms_setkey_int(outRec, "TOTVALS", i);
        drms_setkey_int(outRec, "DATAVALS", statNgood);
        i = outDims[0]*outDims[1]-statNgood;
        drms_setkey_int(outRec, "MISSVALS", i);

        fltemp = drms_getkey_float(inRec, "CRPIX1", &status);
        fltemp = (outDims[0]+1.0)/2.0;
        drms_setkey_float(outRec, "CRPIX1", fltemp);
        fltemp = drms_getkey_float(inRec, "CRPIX2", &status);        
        fltemp = (outDims[1]+1.0)/2.0;
        drms_setkey_float(outRec, "CRPIX2", fltemp);
        drms_copykey(outRec, inRec, "CRVAL1");
        drms_copykey(outRec, inRec, "CRVAL2");
        fltemp = drms_getkey_float(inRec, "CDELT1", &status);
        drms_setkey_float(outRec, "CDELT1", fltemp*nbin);
        fltemp = drms_getkey_float(inRec, "CDELT2", &status);
        drms_setkey_float(outRec, "CDELT2", fltemp*nbin);
        drms_copykey(outRec, inRec, "CROTA2");
        drms_copykey(outRec, inRec, "CRDER1");
        drms_copykey(outRec, inRec, "CRDER2");
        drms_copykey(outRec, inRec, "CSYSER1");
        drms_copykey(outRec, inRec, "CSYSER2");
        drms_copykey(outRec, inRec, "WCSNAME");
      
        drms_copykey(outRec, inRec, "DSUN_OBS");
        drms_copykey(outRec, inRec, "CRLN_OBS");
        drms_copykey(outRec, inRec, "CRLT_OBS");
        drms_copykey(outRec, inRec, "HGLN_OBS");
        drms_copykey(outRec, inRec, "CAR_ROT");
        drms_copykey(outRec, inRec, "OBS_VR");
        drms_copykey(outRec, inRec, "OBS_VW");
        drms_copykey(outRec, inRec, "OBS_VN");

        drms_copykey(outRec, inRec, "T_OBS");
        drms_copykey(outRec, inRec, "DATASIGN");
        drms_copykey(outRec, inRec, "MAPLGMAX");
        drms_copykey(outRec, inRec, "MAPLGMIN");
        intemp = drms_getkey_int(inRec, "MAPMMAX", &status);
        drms_setkey_float(outRec, "MAPMMAX", intemp/nbin);
        intemp = drms_getkey_int(inRec, "SINBDIVS", &status);
        drms_setkey_float(outRec, "SINBDIVS", intemp/nbin);

        drms_copykey(outRec, inRec, "MAPBMAX");
        drms_copykey(outRec, inRec, "MAPRMAX");
        drms_copykey(outRec, inRec, "LGSHIFT");
        drms_copykey(outRec, inRec, "INTERPO");
        drms_copykey(outRec, inRec, "MCORLEV");
        drms_copykey(outRec, inRec, "MOFFSET");
        drms_copykey(outRec, inRec, "CARSTRCH");
        drms_copykey(outRec, inRec, "DIFROT_A");
        drms_copykey(outRec, inRec, "DIFROT_B");
        drms_copykey(outRec, inRec, "DIFROT_C");
        drms_copykey(outRec, inRec, "CALVER64");

        /* Time measure */
        if (verbflag) {
            wt = getwalltime();
            ct = getcputime(&ut, &st);
            printf("record %d done, %.2f ms wall time, %.2f ms cpu time\n", 
                     irec, wt - wt1, ct - ct1);
        }

        /* Clean up */
        free(tmpData);
        drms_free_array(inArray);
        drms_free_array(outArray);
        drms_close_record(outRec, DRMS_INSERT_RECORD);
    } // end of loop
     
    drms_close_records(inRS, DRMS_FREE_RECORD);
    if (verbflag) {
        wt = getwalltime();
        ct = getcputime(&ut, &st);
        printf("total time spent: %.2f ms wall time, %.2f ms cpu time\n", 
                wt - wt0, ct - ct0);
    }

    return(DRMS_SUCCESS);

}  // End of module
