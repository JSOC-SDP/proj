/*
 */

#include <math.h>
#include <sys/time.h>
//#include "drms_dsdsapi.h"
//#include "soi_ephem.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "jsoc_main.h"
#include "imagefromchebyshev.c"

/* cmd-line parameters */
#define kRecSetIn       "in"
#define kSeriesOut      "out"
#define kSegIn          "segin"
#define kSegOut         "segout"
#define kNOTSPECIFIED   "not specified"

int noisemask(tobs, xDim, yDim, xcen, ycen, rsun, vrcenter, image, maskQuery)
    double *image;
    int xDim, yDim;
    float xcen, ycen, rsun;
    float vrcenter;
    TIME tobs;
    char *maskQuery;        // Sep 25
 {
    CmdParams_t *params = &cmdparams;
    int valid, status = 0;
    DRMS_Segment_t *inSeg, *inSegfinal, *outSeg;
    DRMS_RecordSet_t *inRS, *inRSfinal, *outRS;
    DRMS_Record_t *inRec, *inRecfinal, *outRec;
    DRMS_Array_t *inArray, *outArray;
//    char *inQuery="hmi_test.lookup_ChebyCoef_BNoise", *outQuery;
    char *inQuery="hmi.lookup_ChebyCoef_BNoise", *outQuery;
    char *inRecQuery, *outRecQuery;
    char *inQueryfinal, *vr_start_str, *vr_stop_str;
    char ttemp[64];
    TIME ChangeTime1, ChangeTime2, ChangeTime3;
    int MaskIndex = 0;
    int vr_start, vr_stop, order = 15;
    float vr_coef, weight;
    double *coef, *mask, xc_shift, yc_shift;
    int i, j, t, s, nrecs, ii, jj, k;
    int sunSize = (int)(2 * (rsun+1));  
    int ncoef;

    strcpy(ttemp, "2010.12.13_19:47:00_TAI");
    ChangeTime1 = sscan_time(ttemp);

    strcpy(ttemp, "2011.07.13_18:35:00_TAI");
    ChangeTime2 = sscan_time(ttemp);

    strcpy(ttemp, "2012.01.18_18:15:00_TAI");
    ChangeTime3 = sscan_time(ttemp);

    if (tobs<ChangeTime1) MaskIndex = 0;
    if (tobs>ChangeTime1 && tobs<ChangeTime2) MaskIndex = 1;
    if (tobs>ChangeTime2 && tobs<ChangeTime3) MaskIndex = 2;
    if (tobs>ChangeTime3) MaskIndex = 3;

    vr_start = (int)(vrcenter - 50.0);
    vr_stop = (int)(vrcenter + 50.0);
    inQueryfinal = (char *)malloc(100 * sizeof(char));
    vr_start_str = (char *)malloc(100 * sizeof(char));
    vr_stop_str = (char *)malloc(100 * sizeof(char));
    mask = (double *)malloc(sunSize * sunSize * sizeof(double));

    coef = (double *)malloc(order * order * sizeof(double));
        for (i = 0; i < order; i++)
        for (j = 0; j < order; j++) {
            coef[i * order + j] = 0;
        }

    inRS = drms_open_records(drms_env, inQuery, &status);
    if (status || inRS->n == 0) {printf("No input data found\n"); return (status ? status : -1);};
    inRec = inRS->records[0];

    sprintf(vr_start_str, "%d",  vr_start);
    sprintf(vr_stop_str, "%d",  vr_stop);
    sprintf(inQueryfinal, "%s[%s-%s][%d]", inRec->seriesinfo->seriesname, vr_start_str, vr_stop_str, MaskIndex);
     sprintf(maskQuery, "%s", inQueryfinal);
    printf("%s\n", inQueryfinal);
    drms_close_records(inRS, DRMS_FREE_RECORD);

    inRSfinal = drms_open_records(drms_env, inQueryfinal, &status);
    if (status || inRSfinal->n == 0) {printf("No input data found\n"); return (status ? status : -1);};
    nrecs = inRSfinal->n;
/*
    inRecfinal = inRSfinal->records[0];
    inSeg = drms_segment_lookupnum(inRecfinal, 0);
    inArray = drms_segment_read(inSeg, DRMS_TYPE_DOUBLE, &status);
    order = inArray->axis[0];
    coef = (double *)malloc(order * order * sizeof(double));

        for (i = 0; i < order; i++)
        for (j = 0; j < order; j++) {
            coef[i * order + j] = 0;
        }
*/

    if (nrecs == 1)
      {
        inRecfinal = inRSfinal->records[0];
        inSeg = drms_segment_lookupnum(inRecfinal, 0);
        inArray = drms_segment_read(inSeg, DRMS_TYPE_DOUBLE, &status);
        if (status)
           {
              drms_free_array(inArray);
              printf("No data array found\n"); return (status ? status : -1);
            }
         double *inData = (double *)inArray->data;

         for (jj = 0; jj < order; jj++)
             {
             for (ii = 0; ii < order; ii++)
                 {
                   coef[jj * order + ii] = inData[jj * order + ii];
                 }
             }
      }

     if (nrecs >= 2) 
      {
         ncoef = 0;
         for (i = 0; i < 2; i++)
         {
            inRecfinal = inRSfinal->records[i];
            inSeg = drms_segment_lookupnum(inRecfinal, 0);
            inArray = drms_segment_read(inSeg, DRMS_TYPE_DOUBLE, &status);
            if (status)
               {
                  printf(" No input data array found, status=%d\n", status);
                  drms_free_array(inArray);
                  ncoef += 1;
                  continue;
                }
             vr_coef = drms_getkey_float(inRecfinal, "VRCENT", &status);
             weight = 1.0 - fabs(vr_coef - vrcenter)/50.0;
             double *inData = (double *)inArray->data;
                 for (jj = 0; jj < order; jj++)
                    {
                    for (ii = 0; ii < order; ii++)
                        {
                          coef[jj * order + ii] += weight * inData[jj * order + ii];
                        }
                    }
           }
           if (ncoef == 1)
                {
                  for (jj = 0; jj < order; jj++)
                    {
                    for (ii = 0; ii < order; ii++)
                        {
                          coef[jj * order + ii] /= weight;
                        }
                    }
                }

           if (ncoef == 2) {printf("No input data array found\n"); return (-1);}
        }

        xc_shift = (double)xcen - (int)xcen;
        yc_shift = (double)ycen - (int)ycen;
        imagefromchebyshev(mask, sunSize, sunSize, order, coef, xc_shift, yc_shift);

// paste the mask in the 4096x4096 array

        double yDist, xDist, yDist2, xDist2, xDist2PlusyDist2, dToDiskCenter;
        int jy, yOff, iData, yOffmask, iMask, xdelta, ydelta;
        for (jy = 0; jy < yDim; jy++)
          {
              int ix = 0;
              yDist = (double)jy - ycen;
              yDist2 = yDist * yDist;
              yOff = jy * xDim;
              ydelta = 0;
              if (jy >= ycen-rsun & jy <= ycen+rsun) ydelta = jy - (ycen-rsun);
              yOffmask = ydelta * sunSize;
              for (ix = 0; ix < xDim; ix++)
                {
                   iData = yOff + ix;
                   iMask = 0; 
                   if (ix >= xcen-rsun & ix <= xcen+rsun) iMask = yOffmask + ix - (xcen-rsun); 
                   xDist = (double)ix - xcen;
                   xDist2 = xDist * xDist;
                   xDist2PlusyDist2 = xDist2 + yDist2;
                   dToDiskCenter = sqrt(xDist2PlusyDist2);
                   if (dToDiskCenter >= rsun || iMask >= sunSize * sunSize)
                     {
                         image[iData] = DRMS_MISSING_DOUBLE;
                         continue;
                     }
                   image[iData] = mask[iMask];    
                }
          }
//printf("status=%d, nrecs=%d, ncoef=%d\n", status, nrecs, ncoef);
     
     // Sep 23
     free(inQueryfinal); free(vr_start_str); free(vr_stop_str); free(mask);
     
    return 0;
}
/*
 */
