/**

synframe sdate='2010.08.12' stime='16:00:00' in='su_yang.fd_M12m_remap_los' out='su_yang.synframe_los' synoptic='su_yang.hmi_M12m_synop' drmethod='Snodgrass' magresoln=1 synresoln=1 MAXMISSVALS=0 xx1=30

*/

// -----------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "jsoc_main.h"
#include "fstats.h"

char *module_name = "hmib3dailysynframe";

#define DIE(msg) {fflush(stdout); fprintf(stderr, "%s, status = %d \n", msg, status); return(status);}
#define PARAMETER_ERROR(PNAME)
#define     PI     4.0 * atan(1.0)
#define QUAL_CHECK      (0xfffefb00)
#define LON_CORR        (0x10000000) // YL, Longitude Correction Bite

void frebinbox(float *image_in, float *image_out, int nx, int ny, int nbinx, int nbiny);

ModuleArgs_t module_args[] =
  {
    {ARG_STRING, "in", "NOTSPECIFIED", "in"},
    {ARG_STRING, "out", "NOTSPECIFIED", "out"},
    {ARG_STRING, "synoptic", "NOTSPECIFIED", "synoptic"},
    {ARG_STRING, "smallsyn", "NOTSPECIFIED", ""},
    {ARG_STRING, "drmethod", "NOCORRDIFFROT", "drmethod"},
    {ARG_FLOAT,   "bscale", "0.0", "output scaling factor"},
    {ARG_FLOAT,   "bzero", "Default", "output offset"},
    {ARG_INT, "magresoln", "NOTSPECIFIED", "magresoln"},
    {ARG_INT, "synresoln", "NOTSPECIFIED", "synresoln"},
    {ARG_INT, "xx1", "-1", "xx1"},
    {ARG_INT, "yy1", "-1", "yy1"},
    {ARG_END}
  };

  float zgrid(int jph, int ith, int cmp, int rup, int dbl, 
       float phd[], float thd[], float lad[], float cth[], 
       float sth[], float csc[], float scs[]);

int DoIt(void)
{
  DRMS_RecordSet_t *inRS, *inRSfinal, *outRD, *smallRD;
  DRMS_Record_t *inRec, *inRecfinal, *outRec, *smalloutRec; 
  DRMS_Segment_t *inSeg, *inSegfinal, *outSeg, *smalloutSeg;
  DRMS_Array_t *inArray, *inArrayfinal;
  TIME t_rec, t_rec0;
  TIME halfw = 7200.0; //half window = 2.0 hours
  char *t_window = "240m"; //hard-coded T-window; 240 -minutes
  char *inQueryfinal, *trec_str = NULL, *smallRecQuery;
  float crlt, crln;

  double clog0;
  float aa, bb, cc, dd, ee;
  int status = DRMS_SUCCESS, nrecs, irec, quality;
  int i, j, crn;
  int idcamera; //For HMI: 1 (side camera), 2 (front camera), or 3 (both cameras)
  int xdim_syn, ydim_syn, xmg, ymg;
  char sdatestime[100], sdatetmp[100], timetmp[100];
  memset(sdatetmp, 0, sizeof(sdatetmp));
  memset(timetmp, 0, sizeof(timetmp));
  memset(sdatestime, 0, sizeof(sdatestime));

  char *sdate, *stime, *inQuery, *outQuery, *synQuery;
  char *drmethod;
  int magresoln = params_get_int(&cmdparams, "magresoln");
  int synresoln = params_get_int(&cmdparams, "synresoln");
  int xx1 = params_get_int(&cmdparams, "xx1");
  int yy1 = params_get_int(&cmdparams, "yy1");
  float bscale = params_get_float(&cmdparams, "bscale");
  float bzero = params_get_float(&cmdparams, "bzero");
  int nbin = 5;
  long long calVer;

  inQuery = (char *)params_get_str(&cmdparams, "in");
  outQuery = (char *)params_get_str(&cmdparams, "out");
  synQuery = (char *)params_get_str(&cmdparams, "synoptic");
  smallRecQuery = (char *)cmdparams_get_str(&cmdparams, "smallsyn", &status);
  drmethod = (char *)params_get_str(&cmdparams, "drmethod");

  char historyofthemodule[2048]; // put history info into the data
  char *cvsinfo = strdup("$Id: hmib3dailysynframe.c,v 1.3 2020/11/17 01:45:05 yliu Exp $");
  cvsinfo = (char *)malloc(2048 * sizeof(char));
  sprintf(historyofthemodule,"o2helio.c bug corrected -- July 2013");

     aa = 13.1988; bb = 0.0; cc = 0.0;
  if (strcmp(drmethod, "Meunier") == 0)  //  Meunier
    {
     aa = 13.562; bb = -2.04; cc = -1.4875;
    }

  if (strcmp(drmethod, "Phil") == 0)     // Phil, ApJ 241:811-819, 1980
   {
    aa = 2.917; bb = -0.40; cc = -0.40;  //in mu rad/s;sidereal
    dd = 0.202006;                       // 1 deg/day=0.202006 mu rad/s
    aa = aa/dd; bb = bb/dd; cc=cc/dd;    // in deg / day
    ee = 0.930505;                       // synodic/sidereal=0.930505
    aa = aa*ee; bb = bb*ee; cc = cc*ee;  // in synodic
   }                                     // end "phil"

  if (strcmp(drmethod, "Pevtsov") == 0)
    {
     aa = 13.51; bb = -1.72;  cc = -2.31;
    }

  if (strcmp(drmethod, "Snodgrass") == 0)
   {
    aa = 2.897; bb = -0.339; cc = -0.485;//in mu rad/s,sidereal
    dd = 0.202006;                       // 1 deg/day=0.202006 mu rad/s
    aa = aa/dd;  bb=bb/dd; cc=cc/dd;     // in deg / day
    ee = 0.930505;                       // synodic/sidereal=0.930505
    aa = aa*ee; bb = bb*ee; cc = cc*ee;  //in synodic
   }

  outRD = drms_create_records(drms_env, 1, outQuery, DRMS_PERMANENT, &status);
  if (status) DIE("Output recordset not created");
  smallRD = drms_create_records(drms_env, 1, smallRecQuery, DRMS_PERMANENT, &status);
  if (status) DIE("Output record not created");

  outRec = outRD->records[0];
  smalloutRec = smallRD->records[0];

  inRS = drms_open_records(drms_env, inQuery, &status);
  if (status || inRS->n == 0)
     DIE("No input data found -- no remapped files");
  inRec = inRS->records[0];
  t_rec = drms_getkey_time(inRec, "T_REC", &status);
  t_rec0 = t_rec - halfw;

  inQueryfinal = (char *)malloc(100 * sizeof(char));
  trec_str = (char *)malloc(30 * sizeof(char));
  sprint_time(trec_str, t_rec0, "TAI", 0);
  sprintf(inQueryfinal, "%s[%s/%s]", inRec->seriesinfo->seriesname, trec_str, t_window);
  printf("%s\n", inQueryfinal);
  drms_close_records(inRS, DRMS_FREE_RECORD);
  free(trec_str);

  inRSfinal = drms_open_records(drms_env, inQueryfinal, &status);
  if (status || inRSfinal->n == 0) DIE("No input data found -- files contain no data");
  nrecs = inRSfinal->n;

// find the middle data

  int count = 0;
  int *recp;
  int nref, rec_cen;
  recp = (int *)malloc(nrecs * sizeof(int));
  for (i = 0; i < nrecs; i++)
    {
        inRecfinal = inRSfinal->records[i];
        inSeg = drms_segment_lookup(inRecfinal, "Br");
        inArray = drms_segment_read(inSeg, DRMS_TYPE_FLOAT, &status);
        if (status)
           {
              printf(" No data file found, status=%d\n", status);
              drms_free_array(inArray);
              continue;
            }

        quality = drms_getkey_int(inRecfinal, "QUALITY", &status);
        if (quality & QUAL_CHECK)
           {
             printf("SKIP: error getting keyword %s: iRec = %d, quality = %x\n",
            "QUALITY", i, quality);
            continue;
           }

        recp[count] = i;
        count += 1;
        drms_free_array(inArray);
    }

  if (count==0) DIE("No input remapped data found");
  rec_cen = (int)(count/2);
  nref = recp[rec_cen];
  printf("middle data id=%d\n", nref);
  inRecfinal = inRSfinal->records[nref];
  inSeg = drms_segment_lookup(inRecfinal, "Br");
  inArray = drms_segment_read(inSeg, DRMS_TYPE_FLOAT, &status);
  int naxis = inArray->naxis;
  xmg = inArray->axis[0]; ymg = inArray->axis[1];
  t_rec = drms_getkey_time(inRecfinal, "T_REC", &status);
  t_rec0 = drms_getkey_time(inRecfinal, "T_OBS", &status);
  calVer = drms_getkey_longlong(inRecfinal, "CALVER64", &status); // YL 2020-11-06
  crn = drms_getkey_int(inRecfinal, "CAR_ROT", &status);
  crlt = drms_getkey_float(inRecfinal, "CRLT_OBS", &status);
  crln = drms_getkey_float(inRecfinal, "CRLN_OBS", &status);
  if (((calVer>>28) & 1) == 0) crln -= 0.081894; // YL 2020-11-06 
  clog0 = drms_getkey_double(inRecfinal, "CRVAL1", &status);
  if (((calVer>>28) & 1) == 0) clog0 -= 0.081894; // YL 2020-11-06  

  idcamera = drms_getkey_int(inRecfinal, "CAMERA", &status);

  drms_copykey(outRec, inRecfinal, "DATASIGN");
  drms_copykey(outRec, inRecfinal, "DSUN_OBS");
  drms_copykey(outRec, inRecfinal, "OBS_VR");
  drms_copykey(outRec, inRecfinal, "OBS_VW");
  drms_copykey(outRec, inRecfinal, "OBS_VN");
  drms_copykey(outRec, inRecfinal, "QUALITY");
  drms_copykey(outRec, inRecfinal, "MAPMMAX");
  drms_copykey(outRec, inRecfinal, "MAPLGMAX");
  drms_copykey(outRec, inRecfinal, "MAPLGMIN");
  drms_copykey(outRec, inRecfinal, "SINBDIVS");
  drms_copykey(outRec, inRecfinal, "MAPBMAX");
  drms_copykey(outRec, inRecfinal, "MAPRMAX");
  drms_copykey(outRec, inRecfinal, "LGSHIFT");
  drms_copykey(outRec, inRecfinal, "INTERPO");
  drms_copykey(outRec, inRecfinal, "MCORLEV");
  drms_copykey(outRec, inRecfinal, "MOFFSET");
  drms_copykey(outRec, inRecfinal, "CARSTRCH");
  drms_copykey(outRec, inRecfinal, "DIFROT_A");
  drms_copykey(outRec, inRecfinal, "DIFROT_B");
  drms_copykey(outRec, inRecfinal, "DIFROT_C");
  drms_copykey(outRec, inRecfinal, "INSTRUME");
  drms_setkey_longlong(outRec, "CALVER64", calVer | LON_CORR); //YL 2020-11-06
//  drms_copykey(outRec, inRecfinal, "CALVER64");

//  smalloutRec;
int itmp;
float ftmp;
double dtmp;

  drms_copykey(smalloutRec, inRecfinal, "DATASIGN");
  drms_copykey(smalloutRec, inRecfinal, "DSUN_OBS");
  drms_copykey(smalloutRec, inRecfinal, "OBS_VR");
  drms_copykey(smalloutRec, inRecfinal, "OBS_VW");
  drms_copykey(smalloutRec, inRecfinal, "OBS_VN");
  drms_copykey(smalloutRec, inRecfinal, "QUALITY");
  drms_copykey(smalloutRec, inRecfinal, "MAPMMAX");
  drms_copykey(smalloutRec, inRecfinal, "MAPLGMAX");
  drms_copykey(smalloutRec, inRecfinal, "MAPLGMIN");
  drms_copykey(smalloutRec, inRecfinal, "SINBDIVS");
  drms_copykey(smalloutRec, inRecfinal, "MAPBMAX");
  drms_copykey(smalloutRec, inRecfinal, "MAPRMAX");
  drms_copykey(smalloutRec, inRecfinal, "LGSHIFT");
  drms_copykey(smalloutRec, inRecfinal, "INTERPO");
  drms_copykey(smalloutRec, inRecfinal, "MCORLEV");
  drms_copykey(smalloutRec, inRecfinal, "MOFFSET");
  drms_copykey(smalloutRec, inRecfinal, "CARSTRCH");
  drms_copykey(smalloutRec, inRecfinal, "DIFROT_A");
  drms_copykey(smalloutRec, inRecfinal, "DIFROT_B");
  drms_copykey(smalloutRec, inRecfinal, "DIFROT_C");
  drms_copykey(smalloutRec, inRecfinal, "INSTRUME");
  drms_setkey_longlong(smalloutRec, "CALVER64", calVer | LON_CORR); //YL 2020-11-06
//drms_copykey(smalloutRec, inRecfinal, "CALVER64");

  drms_free_array(inArray);

// average remapped mags

  float *aveBr, *aveBt, *aveBp;
  int *countNumber;
  xdim_syn = 3600; ydim_syn = 1440;
  int inDims[2] = {xmg, ymg};
  int dxsz = 2 * inDims[0];     // jph in IDL, zgrid.pro
  int ith = inDims[1];
  int ppd = xdim_syn/360;       // pixels per degree
  int xbeg = 10;
  if (xx1 == -1) xx1 = 60;              // in degrees
  if (yy1 == -1) yy1 = 0;                  // in pixels
  int hwd = xx1;       // in degree
  int ii, jj;
  int smallDims[2], xout, yout;
  double lonppixel = 360.0/xdim_syn; // degree per pixel
  TIME tobs_total = 0.0, tobs_ave;
  xx1 *= ppd;          // in pixels
  xbeg *= ppd;
  aveBr = (float *)malloc(xmg * ymg * sizeof(float));
  aveBt = (float *)malloc(xmg * ymg * sizeof(float));
  aveBp = (float *)malloc(xmg * ymg * sizeof(float));
  countNumber = (int *)malloc(xmg * ymg * sizeof(int));

  xout = xdim_syn/nbin; yout = ydim_syn/(nbin-1);
  smallDims[0] = xout; smallDims[1] = yout;

  for (jj = 0; jj < ymg; jj++){
       for (ii = 0; ii < xmg; ii++) {
            countNumber[jj * xmg + ii] = 0;
            aveBr[jj * xmg + ii] = 0.0;
            aveBt[jj * xmg + ii] = 0.0;
            aveBp[jj * xmg + ii] = 0.0;
       }
  }

  for (i = 0; i < count; i++)
    {
        inRecfinal = inRSfinal->records[recp[i]];
        inSeg = drms_segment_lookup(inRecfinal, "Br");
        inArray = drms_segment_read(inSeg, DRMS_TYPE_FLOAT, &status);
        float *inData = (float *)inArray->data;
        int crnn = drms_getkey_int(inRecfinal, "CAR_ROT", &status);
        long long  calVern = drms_getkey_longlong(inRecfinal, "CALVER64", &status); // YL 2020-11-06
        float clogn = drms_getkey_float(inRecfinal, "CRVAL1", &status);
        if (((calVern>>28) & 1) == 0) clogn -= 0.081894; // YL 2020-11-06  
        int xshift = (rint)(ppd * ((clogn - clog0) - 360.0 * (crnn - crn)));
        TIME Tobs = drms_getkey_time(inRecfinal, "T_OBS", &status);
        tobs_total += Tobs;

// -- for Br

        for (jj = 0; jj < ymg; jj++)
            {
            for (ii = xbeg; ii < xmg - xbeg; ii++)
                {
                 if (isnan(inData[jj * xmg + ii - xshift])) continue;
                 aveBr[jj * xmg + ii] += inData[jj * xmg + ii - xshift];
                 countNumber[jj * xmg + ii] +=1;
                }
            } 
        drms_free_array(inArray);

// -- for Bt

        inSeg = drms_segment_lookup(inRecfinal, "Bt");
        inArray = drms_segment_read(inSeg, DRMS_TYPE_FLOAT, &status);
        inData = (float *)inArray->data;

        for (jj = 0; jj < ymg; jj++)
            {
            for (ii = xbeg; ii < xmg - xbeg; ii++)
                {
                 if (isnan(inData[jj * xmg + ii - xshift])) continue;
                 aveBt[jj * xmg + ii] += inData[jj * xmg + ii - xshift];
                }
            }
        drms_free_array(inArray);

// -- for Bp

        inSeg = drms_segment_lookup(inRecfinal, "Bp");
        inArray = drms_segment_read(inSeg, DRMS_TYPE_FLOAT, &status);
        inData = (float *)inArray->data;

        for (jj = 0; jj < ymg; jj++)
            {
            for (ii = xbeg; ii < xmg - xbeg; ii++)
                {
                 if (isnan(inData[jj * xmg + ii - xshift])) continue;
                 aveBp[jj * xmg + ii] += inData[jj * xmg + ii - xshift];
                }
            }
        drms_free_array(inArray);

    }

   tobs_ave = tobs_total/count;

   for (jj = 0; jj < ymg; jj++){ 
       for (ii = 0; ii < xmg; ii++){
           if (countNumber[jj * xmg + ii] == 0) 
              {
                  aveBr[jj * xmg + ii] = DRMS_MISSING_FLOAT;
                  aveBt[jj * xmg + ii] = DRMS_MISSING_FLOAT;
                  aveBp[jj * xmg + ii] = DRMS_MISSING_FLOAT;
                  continue; 
              }
            aveBr[jj * xmg + ii] /= countNumber[jj * xmg + ii];
            aveBt[jj * xmg + ii] /= countNumber[jj * xmg + ii];
            aveBp[jj * xmg + ii] /= countNumber[jj * xmg + ii];
        }
     }   
  drms_close_records(inRSfinal, DRMS_FREE_RECORD);

// read synoptic charts

  float thd[ith], csc[ith], phd[dxsz];
  float lad[ith], sth[ith], cth[ith], scs[dxsz];
  double dtor = PI/180.;
  float constrate = 13.1988; // Carrington rotation rate in degree/day
  float ratelow, sinlat;
  DRMS_RecordSet_t *synRS = NULL;
  DRMS_Record_t *synRec;
  DRMS_Segment_t *synSeg;
  DRMS_Array_t *synArray; 
  DRMS_Array_t *supsynArrayBr, *supsynArrayBt, *supsynArrayBp;
  DRMS_Array_t *outArray, *smalloutArray;
  float *synData, *supsynDataBr, *supsynDataBt, *supsynDataBp; 
  float *outDataBr, *outDataBt, *outDataBp;
  float *outDataBlos, *smalloutDataBr, *smalloutDataBp, *smalloutDataBt, *smalloutDataBlos;
  int i1, j1;
  int synleftst = ppd * hwd;
  double supleftst = ppd * (clog0 - lonppixel); // the leftmost pixel of Carrington
                                                                    // chart starts lon = lonppixel. 

  zgrid(dxsz, ith, 0, 0, 0, phd, thd, lad, cth, sth, csc, scs);

  snprintf(timetmp, sizeof(timetmp), "%s[%d/3]", synQuery, crn-2);
  synQuery = timetmp;
  printf("inputname= %s\n", synQuery);
  synRS = drms_open_records(drms_env, synQuery, &status);
  if (status || synRS->n == 0) DIE("No input data found -- no synoptic charts");
                     // start combining the synoptic charts
  int nds = synRS->n;
  int supxDim = nds * xdim_syn;
  int supsynDim[2] = {supxDim, ydim_syn};
  int synDim[2] = {xdim_syn, ydim_syn};
  supsynArrayBr = drms_array_create(DRMS_TYPE_FLOAT, 2, supsynDim, NULL, &status);
  supsynDataBr = supsynArrayBr->data;
  supsynArrayBt = drms_array_create(DRMS_TYPE_FLOAT, 2, supsynDim, NULL, &status);
  supsynDataBt = supsynArrayBt->data;
  supsynArrayBp = drms_array_create(DRMS_TYPE_FLOAT, 2, supsynDim, NULL, &status);
  supsynDataBp = supsynArrayBp->data;

//  outArray = drms_array_create(DRMS_TYPE_FLOAT, 2, synDim, NULL, &status);
//  smalloutArray = drms_array_create(DRMS_TYPE_FLOAT, 2, smallDims, NULL, &status);

  if (!(outDataBlos = (float *) malloc(synDim[0]*synDim[1]*4)))
     DIE("MALLOC_FAILED");
  if (!(outDataBr = (float *) malloc(synDim[0]*synDim[1]*4)))
     DIE("MALLOC_FAILED");
  if (!(outDataBt = (float *) malloc(synDim[0]*synDim[1]*4)))
     DIE("MALLOC_FAILED");
  if (!(outDataBp = (float *) malloc(synDim[0]*synDim[1]*4)))
     DIE("MALLOC_FAILED");
  if (!(smalloutDataBlos = (float *) malloc(smallDims[0]*smallDims[1]*4)))
     DIE("MALLOC_FAILED");
  if (!(smalloutDataBr = (float *) malloc(smallDims[0]*smallDims[1]*4)))
     DIE("MALLOC_FAILED");
  if (!(smalloutDataBt = (float *) malloc(smallDims[0]*smallDims[1]*4)))
     DIE("MALLOC_FAILED");
  if (!(smalloutDataBp = (float *) malloc(smallDims[0]*smallDims[1]*4)))
     DIE("MALLOC_FAILED");


  for (i = 0; i < nds; i++)
    {
       synRec = synRS->records[i];
       synSeg = drms_segment_lookup(synRec, "Br"); 
       synArray = drms_segment_read(synSeg, DRMS_TYPE_FLOAT, &status);
       if (status) DIE("problem reading data");
       synData = synArray->data;
       int ii = (nds - 1 - i) * xdim_syn;
       for (j1 = 0; j1 < ydim_syn; j1++)
          for (i1 = 0; i1 < xdim_syn; i1++)
            {
               supsynDataBr[supxDim * j1 + ii + i1] = synData[xdim_syn * j1 + i1];
            } 
       drms_free_array(synArray);

       synSeg = drms_segment_lookup(synRec, "Bt");
       synArray = drms_segment_read(synSeg, DRMS_TYPE_FLOAT, &status);
       if (status) DIE("problem reading data");
       synData = synArray->data;
       for (j1 = 0; j1 < ydim_syn; j1++)
          for (i1 = 0; i1 < xdim_syn; i1++)
            {
               supsynDataBt[supxDim * j1 + ii + i1] = synData[xdim_syn * j1 + i1];
            }
       drms_free_array(synArray);

       synSeg = drms_segment_lookup(synRec, "Bp");
       synArray = drms_segment_read(synSeg, DRMS_TYPE_FLOAT, &status);
       if (status) DIE("problem reading data");
       synData = synArray->data;
       for (j1 = 0; j1 < ydim_syn; j1++)
          for (i1 = 0; i1 < xdim_syn; i1++)
            {
               supsynDataBp[supxDim * j1 + ii + i1] = synData[xdim_syn * j1 + i1];
            }
       drms_free_array(synArray);
                 
     }
                   // end of the combination: the super synoptic map--supsynData

                   // start to generate the right portion of synchronic map
/*
                   // the differential rotation is taken into account.
   for (j = 0; j < ydim_syn/2; j++)
     {
        sinlat = sin(lad[j]*dtor);
        ratelow = aa + bb * sinlat * sinlat + cc * pow(sinlat, 4); // in degree/day

        for (i = synleftst; i < xdim_syn; i++)
        {
             float lon = (i - synleftst) * constrate/ratelow;
             float lon = i - synleftst;
             float delta_x = lon - (int)lon;
             int lonpixel = (int)(supleftst + lon);
             float x1 = supsynData[j * supxDim + lonpixel];
             float x2 = supsynData[j * supxDim + lonpixel + 1];
             outDataBr[j*xdim_syn + i] = (1.0 - delta_x) * x1 + delta_x * x2;
             jj = ydim_syn - 1 - j;
             x1 = supsynData[jj * supxDim + lonpixel];
             x2 = supsynData[jj * supxDim + lonpixel + 1];
             outDataBr[jj*xdim_syn + i] = (1.0 - delta_x) * x1 + delta_x * x2;
        }
      }
*/

   for (j = 0; j < ydim_syn; j++)
     {
        for (i = synleftst; i < xdim_syn; i++)
        {
             float lon = i - synleftst;
             int lonpixel = (rint)(supleftst + lon);
             outDataBr[j*xdim_syn + i] = supsynDataBr[j * supxDim + lonpixel];
             outDataBt[j*xdim_syn + i] = supsynDataBt[j * supxDim + lonpixel];
             outDataBp[j*xdim_syn + i] = supsynDataBp[j * supxDim + lonpixel];
        }
      }    

    int magleft = (rint)(ppd * (90 - hwd));
    for (j = 0; j < ydim_syn; j++)
        for (i = 0; i < 2*synleftst + 1; i++) // clog0 is the center of the central pixel (901 of 1801)
                                              // this way left hwd and right hwd are even.
            {
                outDataBr[j * xdim_syn + i] = aveBr[j * inDims[0] + magleft + i];
                outDataBt[j * xdim_syn + i] = aveBt[j * inDims[0] + magleft + i];
                outDataBp[j * xdim_syn + i] = aveBp[j * inDims[0] + magleft + i];
            }

  frebinbox(outDataBr, smalloutDataBr, xdim_syn, ydim_syn, nbin, nbin-1);
  frebinbox(outDataBt, smalloutDataBt, xdim_syn, ydim_syn, nbin, nbin-1);
  frebinbox(outDataBp, smalloutDataBp, xdim_syn, ydim_syn, nbin, nbin-1);


/* calculate Blos from Br */
/*
  int icol, jrow, rowoffset;
  float sinB, sinB2, cosB, sinBbase;
  double sinBdelta;

// regular-size synoptic chart
  sinBbase = 0.5 - synDim[1]/2.0;
  sinBdelta = 2.0/synDim[1];

  for (jrow = 0; jrow < synDim[1]; jrow++) {
    rowoffset = jrow * synDim[0];
    sinB = sinBdelta * (sinBbase + jrow);
    sinB2 = sinB * sinB;
    cosB = sqrt(1.0 - sinB2);

    for (icol = 0; icol < synDim[0]; icol++) {
        if (isnan(outDataBr[rowoffset + icol]))
           {
             outDataBlos[rowoffset + icol] = DRMS_MISSING_FLOAT;
             continue;
           }
        outDataBlos[rowoffset + icol] = outDataBr[rowoffset + icol] * cosB;
    }
  }
*/

// small-size synoptic chart smallDims[0]*smallDims[1]
/*
  sinBbase = 0.5 - smallDims[1]/2.0;
  sinBdelta = 2.0/smallDims[1];

  for (jrow = 0; jrow < smallDims[1]; jrow++) {
    rowoffset = jrow * smallDims[0];
    sinB = sinBdelta * (sinBbase + jrow);
    sinB2 = sinB * sinB;
    cosB = sqrt(1.0 - sinB2);

    for (icol = 0; icol < smallDims[0]; icol++) {
        if (isnan(smalloutDataBr[rowoffset + icol]))
           {
             smalloutDataBlos[rowoffset + icol] = DRMS_MISSING_FLOAT;
             continue;
           }
        smalloutDataBlos[rowoffset + icol] = smalloutDataBr[rowoffset + icol] * cosB;
    }
  }
*/

// begin statistics
// regular-size maps

  double statMin, statMax, statMedn, statMean, statSig, statSkew, statKurt;
  int statNgood;

// Br--map

  if (fstats(xdim_syn*ydim_syn, outDataBr, &statMin, &statMax, &statMedn, &statMean, &statSig,
      &statSkew, &statKurt, &statNgood)) printf("\n Statistics computation failed\n");

// image statistics
    i = xdim_syn*ydim_syn;
    drms_setkey_int(outRec, "TOTVALS_1", i);
    drms_setkey_int(outRec, "DATAVALS_1", statNgood);
    i = xdim_syn*ydim_syn-statNgood;
    drms_setkey_int(outRec, "MISSVALS_1", i);
    drms_setkey_double(outRec, "DATAMIN_1", statMin);
    drms_setkey_double(outRec, "DATAMAX_1", statMax);
    drms_setkey_double(outRec, "DATAMEDN_1", statMedn);
    drms_setkey_double(outRec, "DATAMEAN_1", statMean);
    drms_setkey_double(outRec, "DATARMS_1", statSig);
    drms_setkey_double(outRec, "DATASKEW_1", statSkew);
    drms_setkey_double(outRec, "DATAKURT_1", statKurt);

// Bt--map

  if (fstats(xdim_syn*ydim_syn, outDataBt, &statMin, &statMax, &statMedn, &statMean, &statSig,
      &statSkew, &statKurt, &statNgood)) printf("\n Statistics computation failed\n");

// image statistics
    i = xdim_syn*ydim_syn;
    drms_setkey_int(outRec, "TOTVALS_2", i);
    drms_setkey_int(outRec, "DATAVALS_2", statNgood);
    i = xdim_syn*ydim_syn-statNgood;
    drms_setkey_int(outRec, "MISSVALS_2", i);
    drms_setkey_double(outRec, "DATAMIN_2", statMin);
    drms_setkey_double(outRec, "DATAMAX_2", statMax);
    drms_setkey_double(outRec, "DATAMEDN_2", statMedn);
    drms_setkey_double(outRec, "DATAMEAN_2", statMean);
    drms_setkey_double(outRec, "DATARMS_2", statSig);
    drms_setkey_double(outRec, "DATASKEW_2", statSkew);
    drms_setkey_double(outRec, "DATAKURT_2", statKurt);

// Bp--map

  if (fstats(xdim_syn*ydim_syn, outDataBp, &statMin, &statMax, &statMedn, &statMean, &statSig,
      &statSkew, &statKurt, &statNgood)) printf("\n Statistics computation failed\n");

// image statistics
    i = xdim_syn*ydim_syn;
    drms_setkey_int(outRec, "TOTVALS_3", i);
    drms_setkey_int(outRec, "DATAVALS_3", statNgood);
    i = xdim_syn*ydim_syn-statNgood;
    drms_setkey_int(outRec, "MISSVALS_3", i);
    drms_setkey_double(outRec, "DATAMIN_3", statMin);
    drms_setkey_double(outRec, "DATAMAX_3", statMax);
    drms_setkey_double(outRec, "DATAMEDN_3", statMedn);
    drms_setkey_double(outRec, "DATAMEAN_3", statMean);
    drms_setkey_double(outRec, "DATARMS_3", statSig);
    drms_setkey_double(outRec, "DATASKEW_3", statSkew);
    drms_setkey_double(outRec, "DATAKURT_3", statKurt);

// small-size maps

  double smallstatMin, smallstatMax, smallstatMedn, smallstatMean, smallstatSig, smallstatSkew, smallstatKurt;
  int smallstatNgood;

// -- Br map

  if (fstats(xout*yout, smalloutDataBr, &smallstatMin, &smallstatMax, &smallstatMedn, &smallstatMean, &smallstatSig,
      &smallstatSkew, &smallstatKurt, &smallstatNgood)) printf("\n Statistics computation failed\n");

// image statistics
    i = xout*yout;
    drms_setkey_int(smalloutRec, "TOTVALS_1", i);
    drms_setkey_int(smalloutRec, "DATAVALS_1", smallstatNgood);
    i = xout*yout-smallstatNgood;
    drms_setkey_int(smalloutRec, "MISSVALS_1", i);
    drms_setkey_double(smalloutRec, "DATAMIN_1", smallstatMin);
    drms_setkey_double(smalloutRec, "DATAMAX_1", smallstatMax);
    drms_setkey_double(smalloutRec, "DATAMEDN_1", smallstatMedn);
    drms_setkey_double(smalloutRec, "DATAMEAN_1", smallstatMean);
    drms_setkey_double(smalloutRec, "DATARMS_1", smallstatSig);
    drms_setkey_double(smalloutRec, "DATASKEW_1", smallstatSkew);
    drms_setkey_double(smalloutRec, "DATAKURT_1", smallstatKurt);

// -- Bt map

  if (fstats(xout*yout, smalloutDataBt, &smallstatMin, &smallstatMax, &smallstatMedn, &smallstatMean, &smallstatSig,
      &smallstatSkew, &smallstatKurt, &smallstatNgood)) printf("\n Statistics computation failed\n");

// image statistics
    i = xout*yout;
    drms_setkey_int(smalloutRec, "TOTVALS_2", i);
    drms_setkey_int(smalloutRec, "DATAVALS_2", smallstatNgood);
    i = xout*yout-smallstatNgood;
    drms_setkey_int(smalloutRec, "MISSVALS_2", i);
    drms_setkey_double(smalloutRec, "DATAMIN_2", smallstatMin);
    drms_setkey_double(smalloutRec, "DATAMAX_2", smallstatMax);
    drms_setkey_double(smalloutRec, "DATAMEDN_2", smallstatMedn);
    drms_setkey_double(smalloutRec, "DATAMEAN_2", smallstatMean);
    drms_setkey_double(smalloutRec, "DATARMS_2", smallstatSig);
    drms_setkey_double(smalloutRec, "DATASKEW_2", smallstatSkew);
    drms_setkey_double(smalloutRec, "DATAKURT_2", smallstatKurt);

// -- Bp map

  if (fstats(xout*yout, smalloutDataBp, &smallstatMin, &smallstatMax, &smallstatMedn, &smallstatMean, &smallstatSig,
      &smallstatSkew, &smallstatKurt, &smallstatNgood)) printf("\n Statistics computation failed\n");

// image statistics
    i = xout*yout;
    drms_setkey_int(smalloutRec, "TOTVALS_3", i);
    drms_setkey_int(smalloutRec, "DATAVALS_3", smallstatNgood);
    i = xout*yout-smallstatNgood;
    drms_setkey_int(smalloutRec, "MISSVALS_3", i);
    drms_setkey_double(smalloutRec, "DATAMIN_3", smallstatMin);
    drms_setkey_double(smalloutRec, "DATAMAX_3", smallstatMax);
    drms_setkey_double(smalloutRec, "DATAMEDN_3", smallstatMedn);
    drms_setkey_double(smalloutRec, "DATAMEAN_3", smallstatMean);
    drms_setkey_double(smalloutRec, "DATARMS_3", smallstatSig);
    drms_setkey_double(smalloutRec, "DATASKEW_3", smallstatSkew);
    drms_setkey_double(smalloutRec, "DATAKURT_3", smallstatKurt);

// end of statistics

//    outRec = drms_create_record(drms_env, outQuery, DRMS_PERMANENT, &status);
//    if (status) DIE("Output recordset not created");

    drms_setkey_time(outRec, "T_REC", t_rec);
    trec_str = (char *)malloc(30 * sizeof(char));
    sprint_time(trec_str, tobs_ave, "TAI", 0);
    drms_setkey_time(outRec, "T_OBS", tobs_ave);
    drms_setkey_int(outRec, "CAR_ROT", crn);
    drms_setkey_float(outRec, "CRLT_OBS", crlt);
    drms_setkey_float(outRec, "CRLN_OBS", crln);
    drms_setkey_float(outRec, "CADENCE", 24.0*60.0*60.0);
    drms_setkey_float(outRec, "CROTA2", 0.0);
    drms_setkey_string(outRec, "WCSNAME", "Carrington Heliographic");
    drms_setkey_string(outRec, "HISTORY", historyofthemodule);
    drms_setkey_string(outRec, "BLD_VERS", jsoc_version);
    status = drms_setkey_string(outRec, "CODEVER", cvsinfo);

    if (idcamera == 1) drms_setkey_string(outRec, "INSTRUME", "HMI_SIDE1");
    if (idcamera == 2) drms_setkey_string(outRec, "INSTRUME", "HMI_FRONT2");
    if (idcamera == 3) drms_setkey_string(outRec, "INSTRUME", "HMI_COMBINED");

    double loncen = xdim_syn-(xdim_syn/2 + 1.0)+1.0;
                 // 1. CRVAL is defined as 180 degree longitude.
                 // 2. Carrington longitude goes from right to the left.
                 // 3. The counting begins with 1 but *not* 0. 
                 // 4. The pixel of interest is (xdim_syn/2+1.0) if counting starts from right to left.
                 // 5. Flipping to left to right, this pixel is xdim_syn-(xdim_syn/2+1.0)+1.0.
    drms_setkey_double(outRec, "CRPIX1", loncen);
        // origin is at the center of the first pixel
    double latcen = (ydim_syn+1.0)/2;
    drms_setkey_double(outRec, "CRPIX2", latcen);
        // origin is at the center of the first pixel
    double lonstep = -360.0/xdim_syn;
    drms_setkey_double(outRec, "CDELT1", lonstep);
    double latstep = 2.0/ydim_syn;
    drms_setkey_double(outRec, "CDELT2", latstep);
//    double lonfirst = 360.0 * (crn  - 1) - clog0 + hwd;
    double lonfirst = 360.0 * (crn  - 1) - clog0 + hwd + 360.0/xdim_syn;
                         // longitude for the last pixel is counted at the center of the pixel.
    drms_setkey_double(outRec, "LON_FRST", lonfirst);
//    double lonlast = 360.0 * crn - clog0 + hwd - 360.0/xdim_syn;
    double lonlast = 360.0 * crn - clog0 + hwd;
                         // longitude for the first pixel is counted at the center of the pixel.
    drms_setkey_double(outRec, "LON_LAST", lonlast);
//    double loncenter = lonfirst + 180.0 - 0.5 * 360.0/xdim_syn;
    double loncenter = lonfirst + 180.0;
                         //    loncenter is defined as the 180 degree longitude.
                         //    loncenter needs to be comparible to CRPIX1
    drms_setkey_double(outRec, "CRVAL1", loncenter);
    drms_setkey_double(outRec, "CARRTIME", loncenter);
    drms_setkey_double(outRec, "LON_STEP", lonstep);

// synoptic map info
    float hwnwidth = drms_getkey_float(synRec, "HWNWIDTH", &status);
    drms_setkey_float(outRec, "HWNWIDTH", hwnwidth);
    float eqpoints = drms_getkey_float(synRec, "EQPOINTS", &status);
    drms_setkey_float(outRec, "EQPOINTS", eqpoints);
    float syndro_a = drms_getkey_float(synRec, "DIFROT_A", &status);
    drms_setkey_float(outRec, "OSYNDR_A", syndro_a);
    float syndro_b = drms_getkey_float(synRec, "DIFROT_B", &status);
    drms_setkey_float(outRec, "OSYNDR_B", syndro_b);
    float syndro_c = drms_getkey_float(synRec, "DIFROT_C", &status);
    drms_setkey_float(outRec, "OSYNDR_C", syndro_c);

// frame info.  
    drms_setkey_string(outRec, "FRTIMWDN", t_window); 
    drms_setkey_string(outRec, "SYNDRORA", drmethod);
    drms_setkey_int(outRec, "FRAVEPNT", count);
    drms_setkey_float(outRec, "FRWINDOW", 2.0 * hwd);
    drms_setkey_float(outRec, "SYNDRO_A", aa);
    drms_setkey_float(outRec, "SYNDRO_B", bb);
    drms_setkey_float(outRec, "SYNDRO_C", cc);
    drms_keyword_setdate(outRec);
       printf("       WRITING OUTPUT\n");

// B-vector-synoptic
    outSeg = drms_segment_lookup(outRec, "Br");
    outArray = drms_array_create(DRMS_TYPE_FLOAT, 2, synDim, outDataBr, &status);
    outArray->israw = 0;            // always compressed
    outArray->bzero = outSeg->bzero;
    outArray->bscale = outSeg->bscale;
    status = drms_segment_write(outSeg, outArray, 0);
    if (status) DIE("problem writing file");
    drms_free_array(outArray);

    outSeg = drms_segment_lookup(outRec, "Bt");
    outArray = drms_array_create(DRMS_TYPE_FLOAT, 2, synDim, outDataBt, &status);
    outArray->israw = 0;            // always compressed
    outArray->bzero = outSeg->bzero;
    outArray->bscale = outSeg->bscale;
    status = drms_segment_write(outSeg, outArray, 0);
    if (status) DIE("problem writing file");
    drms_free_array(outArray);

    outSeg = drms_segment_lookup(outRec, "Bp");
    outArray = drms_array_create(DRMS_TYPE_FLOAT, 2, synDim, outDataBp, &status);
    outArray->israw = 0;            // always compressed
    outArray->bzero = outSeg->bzero;
    outArray->bscale = outSeg->bscale;
    status = drms_segment_write(outSeg, outArray, 0);
    if (status) DIE("problem writing file");
    drms_free_array(outArray);

// writting the small size map 
    drms_setkey_time(smalloutRec, "T_REC", t_rec);
    drms_setkey_time(smalloutRec, "T_REC_step", 720.0);
    drms_setkey_time(smalloutRec, "T_OBS", tobs_ave);
    drms_setkey_int(smalloutRec, "CAR_ROT", crn);
    drms_setkey_float(smalloutRec, "CRLT_OBS", crlt);
    drms_setkey_float(smalloutRec, "CRLN_OBS", crln);
    drms_setkey_float(smalloutRec, "CADENCE", 24.0*60.0*60.0);
    drms_setkey_float(smalloutRec, "CROTA2", 0.0);
    drms_setkey_string(smalloutRec, "WCSNAME", "Carrington Heliographic");
    drms_setkey_string(smalloutRec, "HISTORY", historyofthemodule);
    drms_setkey_string(smalloutRec, "BLD_VERS", jsoc_version);
    status = drms_setkey_string(smalloutRec, "CODEVER", cvsinfo);

    loncen = xout - ((180.0-((nbin+1)/2.0-1.0)*(360.0/xdim_syn))/(360.0/xout)+1.0) + 1.0;
           // The initial pixel coordinate is ((nbin+1)/2.0-1.0)*(360.0/xdim_syn) because of the boxcar average.
           // In this case, the first pixel in small map is rebinned from pixels 1,2,3,4,5 in the large map.
           // Carrington longitude counts from right to the left.
    drms_setkey_double(smalloutRec, "CRPIX1", loncen);
    latcen = (yout+1.0)/2;
    drms_setkey_double(smalloutRec, "CRPIX2", latcen);
    lonstep = -360.0/xout;
    drms_setkey_double(smalloutRec, "CDELT1", lonstep);
    latstep = 2.0/yout;
    drms_setkey_double(smalloutRec, "CDELT2", latstep);
//    lonfirst = 360.0 * (crn  - 1) - clog0 + hwd + ((nbin+1)/2.0-1.0)*(360.0/xdim_syn);
    lonfirst = 360.0 * (crn  - 1) - clog0 + hwd + 360.0/xdim_syn + ((nbin+1)/2.0-1.0)*(360.0/xdim_syn);
    drms_setkey_double(smalloutRec, "LON_FRST", lonfirst);
//    lonlast = 360.0 * crn - clog0 + hwd - 360.0/xdim_syn - ((nbin+1)/2.0-1.0)*(360.0/xdim_syn);
    lonlast = 360.0 * crn - clog0 + hwd - ((nbin+1)/2.0-1.0)*(360.0/xdim_syn);
    drms_setkey_double(smalloutRec, "LON_LAST", lonlast);
//    loncenter = lonfirst + 180.0 - 0.5 * 360.0/xout;
    loncenter = 360.0 * (crn - 1) - clog0 + hwd + 360.0/xdim_syn + 180.0;
                         //    loncenter is defined as the 180 degree longitude.
                         //    loncenter needs to be comparible to CRPIX1
    drms_setkey_double(smalloutRec, "CRVAL1", loncenter);
    drms_setkey_double(smalloutRec, "CARRTIME", loncenter);
    drms_setkey_double(smalloutRec, "LON_STEP", lonstep);

// synoptic map info
    hwnwidth = drms_getkey_float(synRec, "HWNWIDTH", &status);
    drms_setkey_float(smalloutRec, "HWNWIDTH", hwnwidth);
    eqpoints = drms_getkey_float(synRec, "EQPOINTS", &status);
    drms_setkey_float(smalloutRec, "EQPOINTS", eqpoints);
    syndro_a = drms_getkey_float(synRec, "DIFROT_A", &status);
    drms_setkey_float(smalloutRec, "OSYNDR_A", syndro_a);
    syndro_b = drms_getkey_float(synRec, "DIFROT_B", &status);
    drms_setkey_float(smalloutRec, "OSYNDR_B", syndro_b);
    syndro_c = drms_getkey_float(synRec, "DIFROT_C", &status);
    drms_setkey_float(smalloutRec, "OSYNDR_C", syndro_c);

// frame info.  
    drms_setkey_string(smalloutRec, "FRTIMWDN", t_window);
    drms_setkey_string(smalloutRec, "SYNDRORA", drmethod);
    drms_setkey_int(smalloutRec, "FRAVEPNT", count);
    drms_setkey_float(smalloutRec, "FRWINDOW", 2.0 * hwd);
    drms_setkey_float(smalloutRec, "SYNDRO_A", aa);
    drms_setkey_float(smalloutRec, "SYNDRO_B", bb);
    drms_setkey_float(smalloutRec, "SYNDRO_C", cc);
    drms_keyword_setdate(smalloutRec);

       printf("       WRITING OUTPUT--small-size version\n");

// Br-synoptic (first segment)
    smalloutSeg = drms_segment_lookup(smalloutRec, "Br");
    smalloutArray = drms_array_create(DRMS_TYPE_FLOAT, 2, smallDims, smalloutDataBr, &status);

    outArray->israw = 0;            // always compressed
    outArray->bzero = smalloutSeg->bzero;
    outArray->bscale = smalloutSeg->bscale;

    status = drms_segment_write(smalloutSeg, smalloutArray, 0);
    if (status) DIE("problem writing file");
    drms_free_array(smalloutArray);

// Bt-synoptic
    smalloutSeg = drms_segment_lookup(smalloutRec, "Bt");
    smalloutArray = drms_array_create(DRMS_TYPE_FLOAT, 2, smallDims, smalloutDataBt, &status);

    outArray->israw = 0;            // always compressed
    outArray->bzero = smalloutSeg->bzero;
    outArray->bscale = smalloutSeg->bscale;

    status = drms_segment_write(smalloutSeg, smalloutArray, 0);
    if (status) DIE("problem writing file");
    drms_free_array(smalloutArray);

// Bt-synoptic
    smalloutSeg = drms_segment_lookup(smalloutRec, "Bp");
    smalloutArray = drms_array_create(DRMS_TYPE_FLOAT, 2, smallDims, smalloutDataBp, &status);

    outArray->israw = 0;            // always compressed
    outArray->bzero = smalloutSeg->bzero;
    outArray->bscale = smalloutSeg->bscale;

    status = drms_segment_write(smalloutSeg, smalloutArray, 0);
    if (status) DIE("problem writing file");
    drms_free_array(smalloutArray);


    free(trec_str); free(recp); free(aveBr); free(aveBt); free(aveBp);
    free(countNumber);
    drms_free_array(supsynArrayBr);
    drms_free_array(supsynArrayBt);
    drms_free_array(supsynArrayBp);

    drms_close_records(smallRD, DRMS_INSERT_RECORD);
    drms_close_records(outRD, DRMS_INSERT_RECORD);
    drms_close_records(synRS, DRMS_FREE_RECORD);
  return 0;
}        // end DoIt

/*                
 *            zgrid()
 *
 */
float zgrid(int jph, int ith, int cmp, int rup, int dbl, 
   float phd[], float thd[], float lad[], float cth[], 
   float sth[], float csc[], float scs[])
{

  double dtor = PI/180.;
  int i, j;

  float dcth = 2.0/ith;
  float dph = 360./jph;
  float cpc[jph], thr, dphr;
  float lthd[ith], lphd[jph], lscs[jph];
  float llad[ith], lsth[ith], lcth[ith];

#if 0
  if (dbl == 1)  // checking condition "dbl"
     {
      double thd[ith], phd[jph], csc[ith], cpc[jph];
      double dph = 360./jph;
      double dcth = 2.0/ith;
      double lad[ith], sth[ith], cth[ith], thr;
      double lthd[ith], lphd[jph], lscs[jph];
      double llad[ith], lsth[ith], lcth[ith];
      double dphr;
      float scs[jph];
     }  // end if "dbl" 
#endif

  for (i =0; i < ith; i++)
     {
      cth[i] = (i + 0.5) * dcth - 1.0;   // from south to north
      thr = acos(cth[i]);
      sth[i] = sin(thr);
      thd[i] = thr/dtor;
      lad[i] = 90-thd[i];
     }

  for (j = 0; j < jph; j++)
     phd[j] = (j + 0.5) * dph;          // from left to right

  if (cmp == 1)                   // checking condition "cmp"
      {
       for (i = 0; i < ith; i++)
         csc[i] = 1/sth[i];
       for (j = 0; j < jph; j++)
         {
          dphr = (phd[j] - cmp) * dtor;
          cpc[j] = cos(dphr);
          scs[j] = 1/cpc[j];
         }
      }   //end condition: cmp = 1

  if (rup == 1)   // condition "rup"
      {
       for (i = 0; i < ith; i++)
         {
          thd[i] = lthd[ith - i];
          lad[i] = llad[ith - 1];
          cth[i] = lcth[ith - i];
          sth[i] = lsth[ith - 1];
         }
       for (j = 0; j < jph; j++)
         {
          phd[j] = lphd[jph-j];
          scs[j] = lscs[jph-j];
         }
      } // end condition "rup"
  return 0;
} // end function

/* ################## Wrapper for Jesper's code ################## */

/*
void frebin(float *image_in, float *image_out, int nx, int ny, int nbin)
{
  struct fresize_struct fresizes;
  int nxout, nyout;
  int nlead = nx;

  nxout = nx / nbin; nyout = ny / nbin;
  init_fresize_gaussian(&fresizes, (nbin / 2) * 2, (nbin / 2) * 2, nbin);
  fresize(&fresizes, image_in, image_out, nx, ny, nlead, nxout, nyout, nxout, (nbin / 2) * 2, (nbin / 2) * 2, DRMS_MISSING_FLOAT);
  free_fresize(&fresizes);

}
*/

void frebinbox(float *image_in, float *image_out, int nx, int ny, int nbinx, int nbiny)
{
  int nxout, nyout;
  int ii, jj, i, j;
  nxout = nx / nbinx; nyout = ny / nbiny;
  for (j = 0; j < nyout; j++) {
    int yOff, jy;
    jy = j * nbiny;
    for (i = 0; i < nxout; i++) {
      int ix;
      ix = i * nbinx;
      float aveval = 0.0;
      int number = 0;
      for (jj = 0; jj < nbiny; jj++) {
        yOff = (jy + jj) * nx;
        for (ii = 0; ii < nbinx; ii++) {
          int iData = yOff + ix + ii;
          if (!(isnan(image_in[iData]))) {
            aveval += image_in[iData];
            number += 1;
          }
        }
      }
    image_out[j*nxout+i] = aveval/number;
    }
  }
}

/* Modified to deal with new CRLN_OBS
 * revision: 2020/11/16 Yang
 *
 *
 *
 *
 *
 *
// ************ END ********** END ******** END ********** END ***********
