/**
   @defgroup jsoc_rebin jsoc_rebin reduce/increase image size by integer multiples
   @ingroup su_util

   @brief Reduce (increase) the resolution of the input data to that of by an integer factor.

   @par Synopsis:
   @code
   jsoc_rebin  in=input_data out=output_data  scale=<scale> mode={nearest,boxcar}
   where scale is a multiple/fraction of 2.
   @endcode

   This is a general purpose module that takes a series of input 
   data and modifies its spatial resolution by a factor (multiples 
   or fractions of 2) as required and gives out a set of output 
   data. The method for avaraging (interpolation) can be specified 
   through the input "mode". The current version handles a simple 
   boxcar average. If 'scale' < 1 then the input is reduced in size.

   Modified from rebin2.

   @par Flags:
   @c
   -c  Crop before scaling.  Use rsun_obs/cdelt1 for limb radius.
   -u  Leave unchanged, do NOT rotate by 180 degrees if CROTA2 is near 180.  default is to do flip-flip method so
       image is norths up and no pixel values are changed.

   @par GEN_FLAGS:
   Ubiquitous flags present in every module.
   @ref jsoc_main

   @param in  The input data series.
   @param out The output series.

   @par Exit_Status:
   Brief description of abnormal, non-zero, exit values.

   @par Example:
   Takes a series of 1024 X 1024 MDI full disk magnetogram and
   produces images with the resolution of HMI, 4096 X 4096.

   @code
   jsoc_rebin in='mdi.fd_M_96m_lev18[2003.10.20/1d]' out='su_phil.mdi_M_4k' scale=4 mode='simple'
   @endcode

   @par Example:
   Reduces the resolution of HMI images 4096 X 4096 to that of MDI,
   1024 X 1024. Here the input is the HMI images and the output
   is the lower resolution HMI images.  Crop and rotate before rescaling.
   @code
   jsoc_rebin  -c in=hmi.M_45s[2011.10.20/1d]' out='su_phil.hmi_M_1k_45s' scale=0.25 mode='simple'
   @endcode

   @bug
   None known so far.

*/

#include "jsoc.h"
#include "jsoc_main.h"
#include "fstats.h"

char *module_name = "jsoc_rebin";

#define DIE(msg) {fflush(stdout);fprintf(stderr,"%s, status=%d\n",msg,status); return(status);}

ModuleArgs_t module_args[] =
{
     {ARG_STRING, "in", "NOT SPECIFIED",  "Input data series."},
     {ARG_STRING, "out", "NOT SPECIFIED",  "Output data series."},
     {ARG_FLAG, "c", "0", "Crop at rsun_obs."},
     {ARG_FLAG, "u", "0", "do not rotate by 180 if needed."},
     {ARG_FLOAT, "scale", "NOTSPECIFIED", "Scale factor."},
     {ARG_STRING, "mode", "simple", "conversion type."},
     {ARG_STRING, "requestid", "NA", "RequestID if called as an export processing step."},
     {ARG_END}
};

#define     Deg2Rad    (M_PI/180.0)
#define     Rad2arcsec (3600.0/Deg2Rad)
#define     arcsec2Rad (Deg2Rad/3600.0)
#define     Rad2Deg    (180.0/M_PI)

struct ObsInfo_struct
  {
  // from observation info
  TIME  t_obs;
  double rsun_obs, obs_vr, obs_vw, obs_vn;
  double crpix1, crpix2, cdelt1, cdelt2, crota2;
  double crval1, crval2;
  double cosa, sina;
  double obs_b0;
  // observed point
  int i,j;
  // parameters for observed point
  double x,y,r;
  double rho;
  double lon;
    double lat;
  double sinlat, coslat;
  double sig;
  double mu;
  double chi;
  double obs_v;
  };

typedef struct ObsInfo_struct ObsInfo_t;

void rebinArraySF(DRMS_Array_t *out, DRMS_Array_t *in);
int upNcenter(DRMS_Array_t *arr, ObsInfo_t *ObsLoc);
int crop_image(DRMS_Array_t *arr, ObsInfo_t *ObsLoc);

ObsInfo_t *GetObsInfo(DRMS_Segment_t *seg, ObsInfo_t *pObsLoc, int *rstatus);

int DoIt(void)
  {
  int status = DRMS_SUCCESS;
  DRMS_RecordSet_t *inRS, *outRS;
  int irec, nrecs;
  const char *inQuery = params_get_str(&cmdparams, "in");
  const char *outSeries = params_get_str(&cmdparams, "out");
  const char *mode = params_get_str(&cmdparams, "mode");
  const char *requestid = params_get_str(&cmdparams, "requestid");
  float fscale = params_get_float(&cmdparams, "scale");
  int crop = params_get_int(&cmdparams, "c");
  int as_is = params_get_int(&cmdparams, "u");
  int iscale;
  if (fscale < 1.0) // shrinking
    iscale = 1.0/fscale + 0.5;
  else  // enlarging
    iscale = fscale + 0.5;

  inRS = drms_open_records(drms_env, inQuery, &status);
  if (status || inRS->n == 0)
    DIE("No input data found");
  nrecs = inRS->n;
  if (nrecs == 0)
    DIE("No records found");
  drms_stage_records(inRS, 1, 1);

  outRS = drms_create_records(drms_env, nrecs, (char *)outSeries, DRMS_PERMANENT, &status);
  if (status)
    DIE("Output recordset not created");

  for (irec=0; irec<nrecs; irec++)
    {
    ObsInfo_t *ObsLoc;
    DRMS_Record_t *outRec, *inRec;
    DRMS_Segment_t *outSeg, *inSeg;
    DRMS_Array_t *inArray, *outArray;
    float *inData, *outData;
    int inx, iny, outx, outy, i, j;
    float val;
    int quality=0;
 
    inRec = inRS->records[irec];
    quality = drms_getkey_int(inRec, "QUALITY", &status);
    if (status || (!status && quality >= 0))
      {
      inSeg = drms_segment_lookupnum(inRec, 0);
      inArray = drms_segment_read(inSeg, DRMS_TYPE_FLOAT, &status);
      if (status)
        {
        printf(" No data file found but QUALITY not bad, status=%d\n", status);
        drms_free_array(inArray);
        continue;
        }
fprintf(stderr,"got rec\n");
      ObsLoc = GetObsInfo(inSeg, NULL, &status);
fprintf(stderr,"got obs info\n");
      if (!as_is) upNcenter(inArray, ObsLoc);
fprintf(stderr,"got rotated\n");
      if (crop) crop_image(inArray, ObsLoc);
fprintf(stderr,"got cropped\n");
  
      int in_nx = inArray->axis[0];
      int in_ny = inArray->axis[1];
      int in_inc = (fscale < 1 ? 1 : iscale);
      int out_inc = (fscale < 1 ? iscale : 1);
      int out_nx = in_nx * fscale + 0.5;
      int out_ny = in_ny * fscale + 0.5;
  
      int outDims[2] = {out_nx, out_ny};
      inData = (float *)inArray->data;
      outArray = drms_array_create(DRMS_TYPE_FLOAT, 2, outDims, NULL, &status);
      outData = (float *)outArray->data;
  
      if (strcmp(mode, "simple")==0) // setup for better HMI proxy
        {
        if (fscale < 1) // need to average input chunk
          {
fprintf(stderr,"starting reduce factor of %d\n", iscale);
          for (outy = 0; outy < out_ny; outy += 1)
            for (outx = 0; outx < out_nx; outx += 1)
              {
              double total = 0.0;
              int nn = 0;
              for (j = 0; j < in_inc; j += 1)
                for (i = 0; i < in_inc; i += 1)
                  {
                  val = inData[in_nx*(outy*iscale + j) + outx*iscale + i];
                  if (!drms_ismissing_float(val))
                      {
                       total = total + val; 
                       nn++;
                      }
                  }
              outData[out_nx*outy + outx] = (nn > 0 ? total/nn : DRMS_MISSING_FLOAT); 
              }
          }
        else  // need to replicate input point
          {
          for (iny = 0; iny < in_ny; iny += 1)
            for (inx = 0; inx < in_nx; inx += 1)
              {
              val = inData[in_nx*iny + inx];
              for (j = 0; j < out_inc; j += 1)
                for (i = 0; i < out_inc; i += 1)
                  outData[out_nx*(iny*iscale + j) + inx*iscale + i] = val;
              }
          }
        }
    else
      DIE("invalid conversion mode");
  
    drms_free_array(inArray);
  
    // write data file
    outRec = outRS->records[irec];
    outSeg = drms_segment_lookupnum(outRec, 0);

    // copy all keywords
    drms_copykeys(outRec, inRec, 0, kDRMS_KeyClass_Explicit);

    // Now fixup coordinate keywords
    // Only CRPIX1,2 and CDELT1,2 and CROTA2 should need repair.
    drms_setkey_double(outRec, "CDELT1", ObsLoc->cdelt1/fscale);
    drms_setkey_double(outRec, "CDELT2", ObsLoc->cdelt2/fscale);
    drms_setkey_double(outRec, "CRPIX1", (ObsLoc->crpix1-0.5) * fscale + 0.5);
    drms_setkey_double(outRec, "CRPIX2", (ObsLoc->crpix2-0.5) * fscale + 0.5);
    drms_setkey_double(outRec, "CROTA2", ObsLoc->crota2);
    char history[4096];
    sprintf(history, "Scale by %f%s%s\n",
          fscale,
          (crop ? ", Cropped at rsun_obs" : ""),
          (!as_is ? ", North is up" : "") );
    drms_setkey_string(outRec, "HISTORY", history);
    drms_setkey_time(outRec, "DATE", CURRENT_SYSTEM_TIME);
    if (strcmp(requestid, "NA") != 0)
        drms_setkey_string(outRec, "RequestID", requestid);

    // get info for array from segment
    outArray->bzero = outSeg->bzero;
    outArray->bscale = outSeg->bscale;
    outArray->parent_segment = outSeg;
  
    set_statistics(outSeg, outArray, 1);
    status = drms_segment_write(outSeg, outArray, 0);
    if (status)
      DIE("problem writing file");
    drms_free_array(outArray);
    }
   } //end of "irec" loop

  drms_close_records(inRS, DRMS_FREE_RECORD);
  drms_close_records(outRS, DRMS_INSERT_RECORD);
  return (DRMS_SUCCESS);
  } // end of DoIt

// ----------------------------------------------------------------------

/* center whith whole pixel shifts and rotate by 180 if needed */
int upNcenter(DRMS_Array_t *arr, ObsInfo_t *ObsLoc)
  {
  int nx, ny, ix, iy, i, j, xoff, yoff;
  double rot, x0, y0, mid;
  float *data;
  if (!arr || !ObsLoc)
    return(1);
  data = arr->data;
  nx = arr->axis[0];
  ny = arr->axis[1];
  x0 = ObsLoc->crpix1 - 1;
  y0 = ObsLoc->crpix2 - 1;
  mid = (nx-1.0)/2.0;
  if ((rot = fabs(ObsLoc->crota2)) > 179 && rot < 181)
    {
    // rotate image by 180 degrees by a flip flip
    float val;
    int half = nx / 2;
    int odd = nx & 1;
    if (odd) half++;
    for (iy=0; iy<half; iy++)
      {
      for (ix=0; ix<nx; ix++)
        {
        i = iy*nx + ix;
        j = (ny - 1 - iy)*nx + (nx - 1 - ix);
        val = data[i];
        data[i] = data[j];
        data[j] = val;
        }
      }
    x0 = nx - x0;
    y0 = ny - y0;
    rot = ObsLoc->crota2 - 180.0;
    if (rot < -90.0) rot += 360.0;
    ObsLoc->crota2 = rot;
    }
  xoff = round(x0 - mid);
  yoff = round(y0 - mid);
  if (abs(xoff) > 1.0)
    {
    for (iy=0; iy<ny; iy++)
      {
      float valarr[nx];
      for (ix=0; ix<nx; ix++)
        {
        int jx = ix - xoff;
        if (jx >= nx) jx -= nx;
        if (jx < 0) jx += nx;
        valarr[jx] = data[iy*nx + ix];
        }
      for (ix=0; ix<nx; ix++)
        data[iy*nx + ix] = valarr[ix];
      }
    x0 -= xoff;
    }
  if (abs(yoff) > 1.0)
    {
    for (ix=0; ix<nx; ix++)
      {
      float valarr[ny];
      for (iy=0; iy<ny; iy++)
        {
        int jy = iy - yoff;
        if (jy >= ny) jy -= ny;
        if (jy < 0) jy += ny;
        valarr[jy] = data[iy*nx + ix];
        }
      for (iy=0; iy<ny; iy++)
        data[iy*nx + ix] = valarr[iy];
      }
    y0 -= yoff;
    }
  ObsLoc->crpix1 = x0 + 1;
  ObsLoc->crpix2 = y0 + 1;
  return(0);
  }

// ----------------------------------------------------------------------

int crop_image(DRMS_Array_t *arr, ObsInfo_t *ObsLoc)
  {
  int nx, ny, ix, iy, i, j, xoff, yoff;
  double x0, y0;
  double rsun = ObsLoc->rsun_obs/ObsLoc->cdelt1;
  double scale, crop_limit;
  float *data;
  if (!arr || !ObsLoc)
    return(1);
  data = arr->data;
  nx = arr->axis[0];
  ny = arr->axis[1];
  x0 = ObsLoc->crpix1 - 1;
  y0 = ObsLoc->crpix2 - 1;
  scale = 1.0/rsun;
  crop_limit = 0.99975; // 1 - 1/4000, 1/2 HMI pixel.
  for (iy=0; iy<ny; iy++)
    for (ix=0; ix<nx; ix++)
      {
      double x, y, R2;
      float *Ip = data + iy*nx + ix;
      if (drms_ismissing_float(*Ip))
        continue;
      x = ((double)ix - x0) * scale; /* x,y in pixel coords */
      y = ((double)iy - y0) * scale;
      R2 = x*x + y*y;
      if (R2 > crop_limit)
        *Ip = DRMS_MISSING_FLOAT;
      }
  return(0);
  }

// ----------------------------------------------------------------------

#define CHECK(keyname) {if (status) {fprintf(stderr,"Keyword failure to find: %s, status=%d\n",keyname,status); *rstatus=status; return(NULL);}}

ObsInfo_t *GetObsInfo(DRMS_Segment_t *seg, ObsInfo_t *pObsLoc, int *rstatus)
  {
  TIME t_prev;
  DRMS_Record_t *rec;
  TIME t_obs;
  double dv;
  ObsInfo_t *ObsLoc;
  int status;

  if (!seg || !(rec = seg->record))
    { *rstatus = 1; return(NULL); }

  ObsLoc = (pObsLoc ? pObsLoc : (ObsInfo_t *)malloc(sizeof(ObsInfo_t)));
  if (!pObsLoc)
    memset(ObsLoc, 0, sizeof(ObsInfo_t));

  t_prev = ObsLoc->t_obs;
  t_obs = drms_getkey_time(rec, "T_OBS", &status); CHECK("T_OBS");

  if (t_obs <= 0.0)
    { *rstatus = 2; return(NULL); }

  if (t_obs != t_prev)
    {
    ObsLoc->crpix1 = drms_getkey_double(rec, "CRPIX1", &status); CHECK("CRPIX1");
    ObsLoc->crpix2 = drms_getkey_double(rec, "CRPIX2", &status); CHECK("CRPIX2");
    ObsLoc->crval1 = drms_getkey_double(rec, "CRVAL1", &status); CHECK("CRVAL1");
    ObsLoc->crval2 = drms_getkey_double(rec, "CRVAL2", &status); CHECK("CRVAL2");
    ObsLoc->cdelt1 = drms_getkey_double(rec, "CDELT1", &status); CHECK("CDELT1");
    ObsLoc->cdelt2 = drms_getkey_double(rec, "CDELT2", &status); CHECK("CDELT1");
    ObsLoc->crota2 = drms_getkey_double(rec, "CROTA2", &status); CHECK("CROTA2");
    ObsLoc->sina = sin(ObsLoc->crota2*Deg2Rad);
    ObsLoc->cosa = sqrt (1.0 - ObsLoc->sina*ObsLoc->sina);
    ObsLoc->rsun_obs = drms_getkey_double(rec, "RSUN_OBS", &status);
    if (status)
      {
      double dsun_obs = drms_getkey_double(rec, "DSUN_OBS", &status); CHECK("DSUN_OBS");
      ObsLoc->rsun_obs = asin(696000000.0/dsun_obs)/arcsec2Rad;
      }
    ObsLoc->obs_vr = drms_getkey_double(rec, "OBS_VR", &status); CHECK("OBS_VR");
    ObsLoc->obs_vw = drms_getkey_double(rec, "OBS_VW", &status); CHECK("OBS_VW");
    ObsLoc->obs_vn = drms_getkey_double(rec, "OBS_VN", &status); CHECK("OBS_VN");
    ObsLoc->obs_b0 = drms_getkey_double(rec, "CRLT_OBS", &status); CHECK("CRLT_OBS");
    ObsLoc->t_obs = t_obs;
    }
  *rstatus = 0;
  return(ObsLoc);
  }

// ----------------------------------------------------------------------

