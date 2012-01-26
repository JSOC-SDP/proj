#include "mex.h"  /* must appear first */
#include <math.h>
#include <stdint.h>
#include <fitsio.h>
#include "mexhead.h"
#include "Doc/loadfitsa_docstring.h"  /* autogenerated from this file */

/****************************************************************

%loadfitsa: load array from file in FITS format
% 
% [image, error, params] = loadfitsa(file, block, type)
% * File is the name of the fits file to be read, which is an
% arbitrary-dimensional fits file.  It is loaded into Matlab in
% the "natural" ordering -- the native fits ordering, Fortran
% progession of coordinates.  The standard fitsio naming shortcuts
% are accepted, including 'file.fits[1]' for the first image
% extension, useful for loading compressed images in bintables.
% * If the primary HDU (indexed 0 in the shortcut above) is empty, 
% i.e. has NAXIS = 0, then we try each following HDU until one is 
% non-empty to look for valid data.
% * Block is the blocking factor; only elements 1, block+1, ...
% are loaded (in every dimension).
% * type is the destination Matlab type, e.g. 'double' or 'int32'.
% When using the integer types, be aware that the input FITS file
% must fit within the range of the given type, or there will be
% an error.
% * The resulting array is returned; this is 0x0 if an error occurs.
% * Error is 1 if there was an error, 0 otherwise.  An error
% synopsis is printed if error = 1. 
% * To allow robust scripting, if error is requested, a matlab 
% error is not raised even if a file-related error occurs.  The
% error flag is set, and a message is printed, but mexErrMsgTxt is
% not called.  (Exception: if the arguments are malformed, as 
% opposed to a file-reading problem, a mex error *is* raised.)
% * params, encodes the following parameters: bitpix, bscale, bzero, blank.
% It can be useful to have access to the way the FITS file was encoded.
% * loadfitsa and savefitsa are inverse functions.
%
% Inputs:
%   string filename
%   opt int block = 1
%   opt string type = 'double'
%
% Outputs:
%   real image(...)
%   int error
%   real params(4)
%
% See Also: savefitsa

% implemented as a MEX file.  
% rewritten by turmon aug 2002
% touched up for 64-bit, better error reporting, arb. class out, 4/2010.

****************************************************************/
/* Updated by -mex2pymex.py-ver1- on Wed Sep 23 16:55:50 2009 */


#define NARGIN_MIN  1
#define NARGIN_MAX  3
#define NARGOUT_MIN 0
#define NARGOUT_MAX 3

#define ARG_FILENAME 0
#define ARG_BLOCK    1
#define ARG_TYPE     2

#define ARG_IMAGE    0
#define ARG_ERROR    1
#define ARG_PARAMS   2

#define PROGNAME loadfitsa
static const char *progname = "loadfitsa";
static const char *in_specs[NARGIN_MAX ] = { "SV", "IS", "SV" };
static const char *in_names[NARGIN_MAX ] = { "filename", "block" };
static const char *out_names[NARGOUT_MAX] = { "image", "error", "params" };

/* 
 * Print out cfitsio error messages, if any, and exit program 
 * Note that stdout and stderr don't exist for matlab.
 */
static
void
fits_printerror(int status)
{
  char errmsg[81]; // cfitsio caps messages at 80 chars

  mexPrintf("%s: Runtime error (%s).  Exiting.\n",
	    progname, 
	    (status == 0) ? "not a FITSIO error" : "FITSIO trace follows");
  if (status != 0) {
    fits_get_errstatus(status, errmsg);   /* get the error description */
    mexPrintf("status = %d: %s\n", status, errmsg);
    /* get first message; null if stack is empty */
    mexPrintf("Error message stack:\n");
    while (fits_read_errmsg(errmsg))
      mexPrintf("\t%s\n", errmsg);
  }
}


static
void
fio_get_type(char *typeS, int *typeF, mxClassID *typeM, void *nullval)
{
  if (strcmp(typeS, "double") == 0) {
    *(double *)nullval = mxt_getnand();
    *typeM = mxDOUBLE_CLASS;
    *typeF = TDOUBLE;
  } else if (strcmp(typeS, "single") == 0) {
    *(float *)nullval = mxt_getnanf();
    *typeM = mxSINGLE_CLASS;
    *typeF = TFLOAT;
  } else if (strcmp(typeS, "int8") == 0) {
    *(int8_t *)nullval = INT8_MIN;
    *typeM = mxINT8_CLASS;
    *typeF = TSBYTE; // signed
  } else if (strcmp(typeS, "uint8") == 0) {
    *(uint8_t *)nullval = UINT8_MAX;
    *typeM = mxUINT8_CLASS;
    *typeF = TBYTE; // unsigned
  } else if (strcmp(typeS, "int16") == 0) {
    *(int16_t *)nullval = INT16_MIN;
    *typeM = mxINT16_CLASS;
    *typeF = TSHORT;
  } else if (strcmp(typeS, "uint16") == 0) {
    *(uint16_t *)nullval = UINT16_MAX;
    *typeM = mxUINT16_CLASS;
    *typeF = TUSHORT;
  } else if (strcmp(typeS, "int32") == 0) {
    *(int32_t *)nullval = INT32_MIN;
    *typeM = mxINT32_CLASS;
    *typeF = TINT;
  } else if (strcmp(typeS, "uint32") == 0) {
    *(uint32_t *)nullval = UINT32_MAX;
    *typeM = mxUINT32_CLASS;
    *typeF = TUINT;
  } else if (strcmp(typeS, "int64") == 0) {
    *(int64_t *)nullval = INT64_MIN;
    *typeM = mxINT64_CLASS;
    *typeF = TLONGLONG;
  } else if (strcmp(typeS, "uint64") == 0) {
    *(uint64_t *)nullval = UINT64_MAX;
    *typeM = mxUINT64_CLASS;
    *typeF = TULONG; // cfitsio has no TULONGLONG (v 3.24)
  } else {
    // this is an error
    *typeM = mxUNKNOWN_CLASS;
    *typeF = 0;
  }
}

/* scan in the fits file in filename
 * if error, return NULL.  
 * Uses blocking factor in block.
 */
static
mxArray *
fio_GetFITS(char *filename, int block, char *typeS, 
	    int *bitpix_p, double *bscale, double *bzero, double *blank)
{
  mxArray *pm;          /* returned mxArray */
  fitsfile *fptr;       /* fitsio input file */
  int status = 0;       /* fitsio status: OK to start with */
  int naxisM;           /* number of axes (mex) (>=2) */
  int naxisF;           /* number of axes (fitsio) */
  mwSize *naxesM;       /* extent along each dimension (mex)  */
  long   *naxesF;       /* extent along each dimension (fitsio) */
  int nfound;           /* was null found */
  long nelements;       /* total number of elements */
  int i;                /* count dimensions */
  int bitpix;
  char nulbuf[8];       /* can hold any output type (double, int64) */
  void *nullval = nulbuf; /* cfitsio BLANK checking */
  mxClassID typeM;      /* matlab type number */
  int typeF;            /* fitsio type number */
  int has_extension;    /* file has extensions? */
  
  fits_open_file(&fptr, filename, READONLY, &status);  /* open for reading */
  if (status) {
    fits_printerror(status);
    return NULL; /* bail before the allocation */
  }
  fits_get_img_dim(fptr, &naxisF, &status);   /* input dim number */
  fits_read_key(fptr, TLOGICAL, "EXTEND", &has_extension, NULL, &status);
  if (status) {status = 0; has_extension = 0;} // no EXTEND key => no extensions
  while (status == 0 && has_extension && naxisF == 0) {
    // suspect data is in an image extension; go to the next hdu
    fits_movrel_hdu(fptr, 1, NULL, &status);
    fits_get_img_dim(fptr, &naxisF, &status);   /* input dim number */
  }
  fits_get_img_type(fptr, &bitpix, &status);  /* get bitpix keyword */
  // printf("naxis = %d, bitpix = %d\n", (int) naxisF, (int) bitpix);
  if (status) {
    fits_printerror(status);
    return NULL; /* bail before the allocation */
  }

  // set up image parameter output, if desired
  if (bitpix_p) {
    *bitpix_p = bitpix;
  }
  if (bscale) {
    fits_read_key(fptr, TDOUBLE, "BSCALE", bscale, NULL, &status);
    if (status) {
      status = 0; *bscale = 1.0; // not fatal
    }
  }
  if (bzero) {
    fits_read_key(fptr, TDOUBLE, "BZERO",  bzero, NULL, &status);
    if (status) {
      status = 0; *bzero = 0.0; // not fatal
    }
  }
  if (blank) {
    fits_read_key(fptr, TDOUBLE, "BLANK", blank, NULL, &status);
    if (status) {
      status = 0; *blank = 0.0; // not fatal
    }
  }

  // get fitsio and matlab types, and conventional null value,
  // corresponding to the desired output class (typeS)
  // (in theory, bitpix might be useful here)
  fio_get_type(typeS, &typeF, &typeM, nullval);
  if (typeM == mxUNKNOWN_CLASS) {
    mexPrintf("Did not understand desired output class\n");
    fits_printerror(0);
    return NULL; /* trouble with destination class */
  }
  /* If input is floating point: tell cfitsio not to check for special 
   * values within the array -- just pass the values straight through.  
   * Reason is: If cfitsio checking is turned on, it converts all 
   * IEEE special values, both NaN and +/- Inf, to nullval.
   * This is broken, but that's the way it is implemented.
   * See "Support for IEEE Special Values", currently sec. 4.8
   * in the reference manual (version 3.24)
   */
  if (bitpix < 0)
    nullval = NULL; // this will disable fitsio blank checking
  /* make space for the dimensions */
  naxisM = (naxisF < 2) ? 2 : naxisF; // matlab has at least 2
  naxesF = calloc(naxisF, sizeof(*naxesF));
  naxesM = calloc(naxisM, sizeof(*naxesM));
  if (naxesM == NULL || naxesF == NULL) { /* highly unlikely */
    mexPrintf("Failed to input-convert matrix (failed calloc)\n");
    fits_printerror(0);
    return NULL; /* bail before the big allocation */
  }
  /* read the NAXIS* keywords to get image size */
  fits_get_img_size(fptr, naxisF, naxesF, &status);
  if (status) {
    free(naxesM);
    free(naxesF);
    fits_printerror(status);
    return NULL; /* bail before more allocation */
  }
  // set size of matlab matrix.
  for (i = 0; i < naxisM; i++) 
    if (i < naxisF)
      naxesM[i] = 1 + (naxesF[i] - 1) / block; // size of new mtx
    else {
      // if naxisM < naxisF, will create 0x0 or Nx1
      if (i == 0)
	naxesM[i] = 0;
      else // i == 1
	naxesM[i] = (naxesM[i-1] == 0) ? 0 : 1;
    }

  /* create matrix */
  pm = mxCreateNumericArray(naxisM, naxesM, typeM, mxREAL);
  nelements = (long) mxGetNumberOfElements(pm); // used by cfitsio
  /* read in the image */
  if (block == 1) {
    /* special-case this for (possible) speed */
    fits_read_img(fptr, typeF, 1L, nelements, nullval,
		  mxGetPr(pm), &nfound, &status);
    if (status) {
      // typically, an input type mismatch
      mxDestroyArray(pm);
      free(naxesM);
      free(naxesF);
      fits_printerror(status);
      return NULL;
    }
  } else {
    /* want nontrivial blocking */
    long *fpixel = calloc(naxisF, sizeof(long));
    long *blocks = calloc(naxisF, sizeof(long));
    if (fpixel == NULL || blocks == NULL) {
      mexPrintf("Failed to block input matrix (failed calloc)\n");
      mxDestroyArray(pm);
      free(naxesM);
      free(naxesF);
      fits_printerror(0); 
      return NULL;
    }
    for (i = 0; i < naxisF; i++) {
      fpixel[i] = 1L;  /* fitsio starts at 1 */
      blocks[i] = (long) block; /* vector, all the same */
    }
    fits_read_subset(fptr, typeF, fpixel, naxesF, blocks, nullval,
		     mxGetPr(pm), &nfound, &status);
    free(fpixel);
    free(blocks);
    if (status) {
      // typically, an input type mismatch
      mxDestroyArray(pm);
      free(naxesM);
      free(naxesF);
      fits_printerror(status);
      return NULL;
    }
  }    
  /* get rid of these now */
  free(naxesM);
  free(naxesF);
  /* close the file */
  if (fits_close_file(fptr, &status)) {
    mxDestroyArray(pm);
    fits_printerror(status);
    return NULL;
  }
#ifdef FITS_TRANSPOSE_MODE
  /* NOTE: This does not work now, but it could if it was linked
   * to mex_util */
  /* optionally convert to matlab-style ordering */
  if (!transpose_double_mxArray(pm)) {
    mexPrintf("Failed to input-convert matrix (failed malloc)\n");
    fits_printerror(0);
    return NULL;
  }
#endif
  return pm;
}


#ifdef StaticP  /* undefined under mex */
StaticP
#endif
void 
mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
   char errstr[200];
   char *filename;   // input filename
   int block;        // blocking factor
   char *dest_type;  // destination type
   mxArray *result;
   int bitpix;
   double bscale, bzero, blank;

  /* Hook for introspection (function signature, docstring) */
  if (nrhs < 0) { 
    plhs[0] = mxt_PackSignature((mxt_Signature) (-nrhs), 
				NARGIN_MIN, NARGIN_MAX, 
				NARGOUT_MIN, NARGOUT_MAX, 
				in_names, in_specs, out_names, docstring);
    return;
  }
  /* check number of arguments */
  if ((nrhs < NARGIN_MIN) || (nrhs > NARGIN_MAX))
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Expect %d <= input args <= %d",
			   progname, NARGIN_MIN, NARGIN_MAX), errstr));
  if ((nlhs < NARGOUT_MIN) || (nlhs > NARGOUT_MAX))
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Expect %d <= output args <= %d",
			   progname, NARGOUT_MIN, NARGOUT_MAX), errstr));
  mexargparse(nrhs, prhs, in_names, in_specs, NULL, progname);
  start_sizechecking();
  sizeinit(prhs[ARG_FILENAME]);
  sizeisM(1);
  sizecheck_msg(progname, in_names, ARG_FILENAME);
  if (nrhs > ARG_TYPE) {
    sizeinit(prhs[ARG_TYPE]);
    sizeisM(1);
    sizecheck_msg(progname, in_names, ARG_TYPE);
  }

  /* read in the filename */
  filename = mxArrayToString(prhs[ARG_FILENAME]);
  if (!filename)
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Trouble converting filename string",
			   progname), errstr));

  /* read in blocking factor */
  if (nrhs > ARG_BLOCK) 
    block = (int) mxGetScalar(prhs[ARG_BLOCK]);
  else
    block = 1;
  if (block <= 0)
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: blocking factor must be positive",
			   progname), errstr));

  /* read in output type */
  if (nrhs > ARG_TYPE) {
    dest_type = mxArrayToString(prhs[ARG_TYPE]);
  } else {
    dest_type = "double";
  }
  if (!dest_type)
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Trouble converting type string",
			   progname), errstr));

  /* load the image */
  result = fio_GetFITS(filename, block, dest_type, &bitpix, &bscale, &bzero, &blank);
  mxFree(filename);
  if (nrhs > ARG_TYPE)
    mxFree(dest_type); // this was allocated by matlab

  /* set up result (empty matrix if error) */
  plhs[ARG_IMAGE] = result ? 
    result : mxCreateDoubleMatrix((mwSize) 0, (mwSize) 0, mxREAL);

  /* set up error indicator, if desired */
  if (nlhs > ARG_PARAMS) {
    plhs[ARG_PARAMS] = mxCreateDoubleMatrix(1, 4, mxREAL);
    mxGetPr(plhs[ARG_PARAMS])[0] = (double) bitpix;
    mxGetPr(plhs[ARG_PARAMS])[1] = bscale;
    mxGetPr(plhs[ARG_PARAMS])[2] = bzero;
    mxGetPr(plhs[ARG_PARAMS])[3] = blank;
  }

  /* set up error indicator, if desired */
  if (nlhs > ARG_ERROR)
    plhs[ARG_ERROR] = mxCreateDoubleScalar((double) (result == NULL));

  /* raise mex error, unless the error status was asked for */
  if ((result == NULL) && (nlhs <= ARG_ERROR))
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Runtime error.  Exiting.",
			   progname), errstr));
}



/* Hook for generic tail matter */
#ifdef MEX2C_TAIL_HOOK
#include "mex2c_tail.h"
#endif

