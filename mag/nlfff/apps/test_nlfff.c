/*
 * Module name:		test_nlfff.c
 *
 * Description:		Wrapper module for Wiegelmann's NLFFF code
 *
 * Calling:		
 *
 * Original source:	C NLFFF model by Thomas Wiegelmann (weigelmann@linmpi.mg.de)
 *
 * Written by:		Xudong Sun (xudongs@stanford.edu)
 *
 * Version:
 *			v1.0		Feb 19 2010
 *			v1.1		Mar 21 2010
 *			v2.0		Mar 26 2010
 *			v2.1		Apr 22 2010
 *			v2.2		Oct 06 2010
 *			v2.3		Feb 13 2011
 *
 *
 * Issues:
 *			v1.0
 *			Tested and run for local potential
 *			parallelization working (setenv OMP_NUM_THREADS n)
 *			added preprocessing
 *			v1.1
 *			Separate green() from relax()
 *			Tested OpenMP, random rounding-off errors might be of concern
 *			v2.0
 *			Implemented multigrid
 *			Made some changes in relax.c (restore flag)
 *			Moved macros to main
 *			v2.1
 *			Updated I/O for Lambert projected patch
 *			v2.2
 *			Added comment keyword, added multi and prep as prime key
 *			Added input for test version
 *			v2.3
 *			Added T_OBS and energy = total(Bx^2+By^2+Bz^2)
 *
 * Example:
 *			test_nlfff "in=su_xudong.lambert_vec_S5[2010.08.01_12:00:00_TAI][2]" "out=su_xudong.nlfff_S5" "PREP=1" "MULTI=3" "nz=256"
 *			test_nlfff "in=su_xudong.nlfff_test_in[2006]" "out=su_xudong.nlfff_test_out" "nz=256" "test=1" "COMMENT=Metcalf2006"
 */


#include <jsoc_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#define PI	(M_PI)
#define	DTOR	(PI / 180.)

#define ARRLENGTH(ARR) (sizeof(ARR) / sizeof(ARR[0]))
#define DIE(msg) {fflush(stdout); fprintf(stderr, "%s, status=%d\n", msg, status); return(status);}
#define SHOW(msg) {printf("%s", msg); fflush(stdout);}

#define Macro
#define MCENTERGRAD(f,id) ((f[i+id]-f[i-id])/(2*h))
#define MLEFTGRAD(f,id)   ((-3*f[i]+4*f[i+id]-f[i+2*id])/(2*h))
#define MRIGHTGRAD(f,id)  ((+3*f[i]-4*f[i-id]+f[i-2*id])/(2*h))
#define GRADX(f,i) ((ix>0 && ix<nx-1) ? (MCENTERGRAD(f,nynz)) : ((ix==0) ? (MLEFTGRAD(f,nynz)) : ((ix==nx-1) ? (MRIGHTGRAD(f,nynz)) : (0.0))))
#define GRADY(f,i) ((iy>0 && iy<ny-1) ? (MCENTERGRAD(f,nz)) : ((iy==0) ? (MLEFTGRAD(f,nz)) : ((iy==ny-1) ? (MRIGHTGRAD(f,nz)) : (0.0))))
#define GRADZ(f,i) ((iz>0 && iz<nz-1) ? (MCENTERGRAD(f,1)) : ((iz==0) ? (MLEFTGRAD(f,1)) : ((iz==nz-1) ? (MRIGHTGRAD(f,1)) : (0.0))))

// Working part
// ======== OpenMP =========
#ifdef _OPENMP 
#include <omp.h>
#endif 

#include "rebin.c"
#include "preproc.c"
#include "green.c"
#include "relax.c"


/* Timing by T. P. Larson */

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
  *utime = ru.ru_utime.tv_sec * 1000.0 + ru.ru_utime.tv_usec/1000.0;
  *stime = ru.ru_stime.tv_sec * 1000.0 + ru.ru_stime.tv_usec/1000.0;
  return *utime + *stime;
}




char *module_name = "test_nlfff";	/* Module name */

ModuleArgs_t module_args[] =
{
    {ARG_STRING, "in", NULL, "Input data series."},
    {ARG_STRING, "out", NULL,  "Output data series."},
    {ARG_INT, "nz", "256", "Number of grids in z axis."},
    {ARG_INT, "nd", "16", "Number of grids at boundary."},
    {ARG_INT, "PREP", "0", "Preprocess vector magnetogram?"},
    {ARG_DOUBLE, "mu1", "1.0", "For vector magnetogram preprocessing."},
    {ARG_DOUBLE, "mu2", "1.0", "For vector magnetogram preprocessing."},
    {ARG_DOUBLE, "mu3", "0.001", "For vector magnetogram preprocessing."},
    {ARG_DOUBLE, "mu4", "0.01", "For vector magnetogram preprocessing."},
    {ARG_INT, "maxit", "10000", "Maximum itertation number."},
    {ARG_INT, "test", "0", "1 for test data with 3 layer input cube rather than 3 images."},
    {ARG_INT, "MULTI", "1", "Levels of multigrid algorithm, 0 and 1 with no multigrid."},
    {ARG_INT, "VERB", "2", "Level of verbosity: 0=errors/warnings; 1=status; 2=all"},
    {ARG_STRING, "COMMENT", " ", "Comments."}, 
    {ARG_END}
};


/* ################## Main Module ################## */

int DoIt(void)
{
    int status = DRMS_SUCCESS;

    #ifdef _OPENMP
    printf("Compiled by OpenMP\n");
    #endif

    char *inQuery, *outQuery;
    DRMS_RecordSet_t *inRS, *outRS;
    int irec, nrecs;
    DRMS_Record_t *inRec, *outRec;
    DRMS_Segment_t *inSegBx, *inSegBy, *inSegBz;
    DRMS_Segment_t *outSegBx, *outSegBy, *outSegBz;
    DRMS_Array_t *inArrayBx, *inArrayBy, *inArrayBz;
    DRMS_Array_t *outArrayBx, *outArrayBy, *outArrayBz;
    float *inDataBx, *inDataBy, *inDataBz;
    float *outDataBx, *outDataBy, *outDataBz;
    
    double *bx0, *by0, *bz0;
    double *bx0_now, *by0_now, *bz0_now;
    double *Bx, *By, *Bz, *Bx_tmp, *By_tmp, *Bz_tmp;
    double energy;

    int verbflag, prepflag;
    int outDims[3];

    int i, j, k, itest, dpt, dpt0;
    int nx, ny, nz, nxny, nynz, nxnynz;
    int nd, nd_now, maxit;
    double mu1, mu2, mu3, mu4;
    int multi, multi_now;
    int nx_now, ny_now, nz_now, nxny_now, nynz_now, nxnynz_now;
    int test;
    char *comment;

    // Time measuring
    double wt0, wt1, wt;
    double ut0, ut1, ut;
    double st0, st1, st;
    double ct0, ct1, ct;
    wt0 = getwalltime();
    ct0 = getcputime(&ut0, &st0);

    /* Get parameters */
    inQuery = (char *)params_get_str(&cmdparams, "in");
    outQuery = (char *)params_get_str(&cmdparams, "out");
    comment = (char *)params_get_str(&cmdparams, "COMMENT");
    prepflag = params_get_int(&cmdparams, "PREP");
    verbflag = params_get_int(&cmdparams, "VERB");
    nz = outDims[2] = params_get_int(&cmdparams, "nz");		// z nodes fixed for dataset
    nd = params_get_int(&cmdparams, "nd");
    maxit = params_get_int(&cmdparams, "maxit");
    mu1 = params_get_double(&cmdparams, "mu1");
    mu2 = params_get_double(&cmdparams, "mu2");
    mu3 = params_get_double(&cmdparams, "mu3");
    mu4 = params_get_double(&cmdparams, "mu4");
    test = params_get_int(&cmdparams, "test");
    multi = params_get_int(&cmdparams, "MULTI");
    if (multi < 1) multi = 1;

    /* Open input */
    inRS = drms_open_records(drms_env, inQuery, &status);
    if (status || inRS->n == 0) DIE("No input data found");
    nrecs = inRS->n;

    /* Create output */
    outRS = drms_create_records(drms_env, nrecs, outQuery, DRMS_PERMANENT, &status);
    if (status) DIE("Output recordset not created");

    /* Do this for each record */
    for (irec = 0; irec < nrecs; irec++)
    {

        if (verbflag) {
            wt1 = getwalltime();
            ct1 = getcputime(&ut1, &st1);
            printf("processing record %d...\n", irec);
        }
        
        /* Input record and data */
        inRec = inRS->records[irec];
        if (!test) {
            inSegBx = drms_segment_lookup(inRec, "Bx");
            inSegBy = drms_segment_lookup(inRec, "By");
            inSegBz = drms_segment_lookup(inRec, "Bz");
            inArrayBx = drms_segment_read(inSegBx, DRMS_TYPE_FLOAT, &status);
            if (status) DIE("No Bx data file found. \n");
            inArrayBy = drms_segment_read(inSegBy, DRMS_TYPE_FLOAT, &status);
            if (status) DIE("No By data file found. \n");
            inArrayBz = drms_segment_read(inSegBz, DRMS_TYPE_FLOAT, &status);
            if (status) DIE("No Bz data file found. \n");
            inDataBx = (float *)inArrayBx->data;
            inDataBy = (float *)inArrayBy->data;
            inDataBz = (float *)inArrayBz->data;
            nx = inArrayBx->axis[0]; ny = inArrayBx->axis[1];
            if (inArrayBy->axis[0] != nx || inArrayBz->axis[0] != nx || 
                inArrayBy->axis[1] != ny || inArrayBz->axis[1] != ny)
                DIE("Dimension error. \n");
            nxny = nx * ny; nynz = ny * nz; nxnynz = nxny * nz;
        } else {
           inSegBz = drms_segment_lookupnum(inRec, 0);
           inArrayBz = drms_segment_read(inSegBz, DRMS_TYPE_FLOAT, &status);
           if (status) DIE("No data file found. \n");
           if (inArrayBz->naxis != 3) DIE("Wrong data dimension. \n");
           nx = inArrayBz->axis[0]; ny = inArrayBz->axis[1];
           nxny = nx * ny; nynz = ny * nz; nxnynz = nxny * nz;
           inDataBx = (float *)inArrayBz->data;
           inDataBy = inDataBx + nxny;
           inDataBz = inDataBx + 2 * nxny;
        }

        /* Test if multigrid can be performed */
        nx_now = nx; ny_now = ny; nz_now = nz; nd_now = nd;
        for (multi_now = multi; multi_now > 1; multi_now--) {
            if ((nx_now % 2) || (ny_now % 2) || (nz_now % 2) || (nd_now % 2))
            {
                SHOW("Dimension error, multigrid cannot be performed.\n");
                nx_now = nx; ny_now = ny; nz_now = nz; nd_now = nd;
                multi = 1;
                break;
            }
            nx_now /= 2; ny_now /= 2; nz_now /= 2; nd_now /= 2;
        }

        /* Output data */
        outDims[0] = nx; outDims[1] = ny;

        outArrayBx = drms_array_create(DRMS_TYPE_FLOAT, 3, outDims, NULL, &status);
        outArrayBy = drms_array_create(DRMS_TYPE_FLOAT, 3, outDims, NULL, &status);
        outArrayBz = drms_array_create(DRMS_TYPE_FLOAT, 3, outDims, NULL, &status);
        outDataBx = (float *)outArrayBx->data;
        outDataBy = (float *)outArrayBy->data;
        outDataBz = (float *)outArrayBz->data;

        /* ======================== */
        /* This is the working part.*/
        /* ======================== */

        // Copy in magnetogram--------------- CHECKED AGAINST WIEGELMANN'S CODE, COLUMN FIRST
        bx0 = (double *)(calloc(nxny, sizeof(double)));
        by0 = (double *)(calloc(nxny, sizeof(double)));
        bz0 = (double *)(calloc(nxny, sizeof(double)));
        dpt = 0;
        for (i = 0; i < nx; i++)	// See green()
        for (j = 0; j < ny; j++) {
            dpt0 = j * nx + i;
            bx0[dpt] = inDataBx[dpt0];
            by0[dpt] = inDataBy[dpt0];
            bz0[dpt] = inDataBz[dpt0];
            dpt++;
        }

        /* Multigrid processing */

        for (multi_now = multi; multi_now > 0; multi_now--) {

            if (verbflag) {
                printf("multi=%d, nx_now=%d, ny_now=%d, nz_now=%d, nd_now=%d\n", 
                        multi_now, nx_now, ny_now, nz_now, nd_now);
                fflush(stdout);
            }

            /* Step 0: Grid */
            nxny_now = nx_now * ny_now; nynz_now = ny_now * nz_now;
            nxnynz_now = nxny_now * nz_now;

            /* Step 1: Get magnetogram at this level */
            bx0_now = (double *)(calloc(nxny_now, sizeof(double)));
            by0_now = (double *)(calloc(nxny_now, sizeof(double)));
            bz0_now = (double *)(calloc(nxny_now, sizeof(double)));
            SHOW("here");
printf("%d, %d, %d\n", nx_now, ny_now, multi_now); fflush(stdout);
            rebin2d(bx0, by0, bz0, bx0_now, by0_now, bz0_now, nx, ny, multi_now);

            /* Step 2: Preprocessing */
            if (prepflag) {
                preproc(bx0_now, by0_now, bz0_now, nx_now, ny_now, 
                        mu1, mu2, mu3, mu4, 0.0, maxit, verbflag);
            }
printf("preproc"); fflush(stdout);
            /* Step 3: Get intial guess */
            if (multi_now == multi) {
                Bx = (double *)(calloc(nxnynz_now, sizeof(double)));
                By = (double *)(calloc(nxnynz_now, sizeof(double)));
                Bz = (double *)(calloc(nxnynz_now, sizeof(double)));
                // Potential solver
                printf("ahyaya%d", nxnynz_now); fflush(stdout);
                green(bz0_now, Bx, By, Bz, nx_now, ny_now, nz_now, verbflag);
                printf("%d", multi_now); fflush(stdout);
            } else {
                Bx_tmp = Bx; By_tmp = By; Bz_tmp = Bz;
                Bx = (double *)(calloc(nxnynz_now, sizeof(double)));
                By = (double *)(calloc(nxnynz_now, sizeof(double)));
                Bz = (double *)(calloc(nxnynz_now, sizeof(double)));
                // Rebin to denser grid
                rebin3d(Bx_tmp, By_tmp, Bz_tmp, Bx, By, Bz, nx_now / 2, ny_now / 2, nz_now / 2);
                free(Bx_tmp); free(By_tmp); free(Bz_tmp);
            }
                printf("why"); fflush(stdout);
            /* Step 4: Substitute bottom layer of initial guess with original magnetogram */
            for (int iy = 0; iy < ny_now; iy++) {
            for (int ix = 0; ix < nx_now; ix++) {
                i = nynz_now * ix + nz_now * iy;	// iz = 0
                Bx[i] = bx0_now[ix * ny_now + iy];
                By[i] = by0_now[ix * ny_now + iy];
                Bz[i] = bz0_now[ix * ny_now + iy];
            }}
              //  printf("heyhey0"); fflush(stdout);
            free(bx0_now); free(by0_now); free(bz0_now);
                printf("heyhey"); fflush(stdout);
            /* Step 5: NLFFF relaxation */
            relax(Bx, By, Bz, nx_now, ny_now, nz_now, nd_now, maxit, verbflag);
                printf("ohlakla"); fflush(stdout);
            /* Step 0: Grid */
            nx_now *= 2; ny_now *= 2; nz_now *= 2; nd_now *= 2;

        }	// end of multi
     
        // Copy out result -------------- CHECKED AGAINST WIEGELMANN'S CODE
        energy = 0.;		// Feb 13 2011
        dpt = 0;
        for (k = 0; k < nz; k++) {
        for (j = 0; j < ny; j++) {
        for (i = 0; i < nx; i++) {
            dpt0 = i * nynz + j * nz + k;
            outDataBx[dpt] = Bx[dpt0];
            outDataBy[dpt] = By[dpt0];
            outDataBz[dpt] = Bz[dpt0];
            dpt++;
            energy += (Bx[dpt0] * Bx[dpt0] + By[dpt0] * By[dpt0] + Bz[dpt0] * Bz[dpt0]);
        }}}

        /* Output record */
        outRec = outRS->records[irec];
        outSegBx = drms_segment_lookupnum(outRec, 0);
        outSegBy = drms_segment_lookupnum(outRec, 1);
        outSegBz = drms_segment_lookupnum(outRec, 2);

        for (i = 0; i < 3; i++) {	// For variable dimensions
            outSegBx->axis[i] = outArrayBx->axis[i];
            outSegBy->axis[i] = outArrayBy->axis[i];
            outSegBz->axis[i] = outArrayBz->axis[i];
        }

        outArrayBx->parent_segment = outSegBx;
        outArrayBy->parent_segment = outSegBy;
        outArrayBz->parent_segment = outSegBz;

        /* Result writing */
        status = drms_segment_write(outSegBx, outArrayBx, 0);
        if (status) DIE("Problem writing file Bx");
        drms_free_array(outArrayBx);
        status = drms_segment_write(outSegBy, outArrayBy, 0);
        if (status) DIE("Problem writing file By");
        drms_free_array(outArrayBy);
        status = drms_segment_write(outSegBz, outArrayBz, 0);
        if (status) DIE("Problem writing file Bz");
        drms_free_array(outArrayBz);

        /* Set keywords */
        drms_setkey_string(outRec, "COMMENT", comment);
        drms_setkey_int(outRec, "MULTI", multi);
        drms_setkey_int(outRec, "PREP", prepflag);
    	drms_setkey_string(outRec, "BLD_VERS", jsoc_version);
        drms_setkey_time(outRec, "DATE", CURRENT_SYSTEM_TIME);
        drms_setkey_double(outRec, "ENERGY", energy);
        if (test) {
            drms_copykey(outRec, inRec, "NUM");
        } else {
            drms_copykey(outRec, inRec, "T_REC");
            drms_copykey(outRec, inRec, "T_OBS");
            drms_copykey(outRec, inRec, "HARPNUM");
            drms_copykey(outRec, inRec, "RUNNUM");
            drms_setkey_string(outRec, "COMMENT", comment);
        }

        /* Clean up */
        if (!test) {
            drms_free_array(inArrayBx);
            drms_free_array(inArrayBy);
            drms_free_array(inArrayBz);
        } else {
            drms_free_array(inArrayBz);
        }
        free(bx0); free(by0); free(bz0);
        free(Bx); free(By); free(Bz);

        /* Time measure */
        if (verbflag) {
            wt = getwalltime();
            ct = getcputime(&ut, &st);
            printf("record %d done, %.2f ms wall time, %.2f ms cpu time\n", 
                     irec, wt - wt1, ct - ct1);
        }

    }

    drms_close_records(inRS, DRMS_FREE_RECORD);
    drms_close_records(outRS, DRMS_INSERT_RECORD);

    if (verbflag) {
        wt = getwalltime();
        ct = getcputime(&ut, &st);
        printf("total time spent: %.2f ms wall time, %.2f ms cpu time\n", 
                wt - wt0, ct - ct0);
    }

    return(DRMS_SUCCESS);
}
