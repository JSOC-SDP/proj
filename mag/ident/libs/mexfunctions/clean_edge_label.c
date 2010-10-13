#include "mex.h"  /* must appear first */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mexhead.h"
#include "Doc/clean_edge_label_docstring.h"  /* autogenerated from this file */

/**************************************************************

% clean_edge_label: clean up labels at extreme edge of a disk
% 
% [res,nclean]=clean_edge_label(img,center,delta,mode);
% * Remove possibly-tainted labels at edge of disk by examining an
% annulus of width delta.  Also, sets all off-disk values to the
% value of the (1,1) pixel of img.
% * Two modes are supported.  If mode < 0, cleaning is done
% by propagating the value just inside the annulus outward.
% Otherwise, the values in the annulus are simply set to mode.
% * The number of on-disk pixels altered is in nclean.
% * Primary disk parameters (x-center, y-center, radius) are in center.
% The sun is the disk defined by these three numbers.  
% * If delta = 0, img is propagated to res without further ado.
% (The off-disk clearing is not done either.)
% In this case, the center parameter need not be correct.
% 
% Inputs:
%   real img(m,n);
%   real center(3);
%   real delta;
%   double mode;
% 
% Outputs:
%   real res(m,n);
%   int nclean;
% 

% implemented as a mex file

****************************************************************/

/* constants and globals used for error checking */

#define NARGIN_MAX	4	     /* number of inputs */
#define NARGIN_MIN	4	     /* number of inputs */
#define NARGOUT_MIN	1	 /* number of output args */
#define NARGOUT_MAX	2	 /* number of output args */

#define ARG_img      0  /* input image */
#define ARG_center   1  /* apparent center and radius of the sun */
#define ARG_delta    2  /* width of annulus */
#define ARG_mode     3  /* mode of operation */

#define ARG_out      0  /* output image */
#define ARG_nclean   1  /* pixel change count */

static const char *progname = "clean_edge_label";
#define PROGNAME clean_edge_label
static const char *in_specs[NARGIN_MAX] = {
  "RM",
  "RV(3)",
  "RS",
  "IS" };
static const char *in_names[NARGIN_MAX] = {
  "image",
  "center",
  "delta",
  "mode"};
static const char *out_names[NARGOUT_MAX] = {
  "res",
  "nclean"};

// defined just for brevity in a generated #include file
#define SHORTNAME cel

/* computational routine: 
 * identify and fix edge labels
 * Formerly: no non-edge labels are not propagated from image -> imagep
 * New Oct 2010: off-disk pixels are all set to image[0][0], and all
 * pixels are propagated here.
 */

static
void
propagate(int *count,               /* count of changed pixels */
	  double *image[],          /* original image */ 
	  double *imagep[],         /* updated image */
	  int maxx, int maxy,       /* image, imagep size */
	  double cenx, double ceny, /* image center */
	  double radius,            /* image radius */
	  double delta,             /* annulus width */
	  double mode               /* <0: propagate, >=0: set to mode */
	  )
{
  int x, y;         // loop vars
  double r2;        // radius^2 at (x,y)
  double dx, dy;    // offset from center to (x,y)
  int x_off, y_off; // integer indexes for fill-in value
  int ct = 0;       // local count for return value
  double scale;
  double offdisk;
  const double radius2 = radius*radius;
  const double radius_inside2 = (radius - delta)*(radius - delta);

  if (maxx * maxy > 0)
    offdisk = image[0][0];
  else
    offdisk = 0; // will not be used
  for (x = 0; x < maxx; x++)
    for (y = 0; y < maxy; y++) {
      // offsets from center
      dx = x - cenx;
      dy = y - ceny;
      // current point's radius-squared
      r2 = dx*dx + dy*dy;
      // case on radius
      if (r2 > radius2) {
	// set offdisk pixels to the special value
	imagep[x][y] = offdisk;
      } else if (r2 < radius_inside2) {
	// pass inside pixels thru
	imagep[x][y] = image[x][y];
      } else {
	// Within annulus: check mode
	if (mode >= 0) {
	  // set, unconditionally, to mode
	  imagep[x][y] = mode;
	} else {
	  // must fill in: generate new index
	  scale = sqrt(radius_inside2/r2); // < 1, pull into annulus interior
	  // offset to fill-in value
	  dx *= scale;  dy *= scale;
	  // round inward toward center
	  if (dx > 0) 
	    x_off = floor(cenx + dx);
	  else
	    x_off = ceil(cenx + dx);
	  if (dy > 0) 
	    y_off = floor(ceny + dy);
	  else
	    y_off = ceil(ceny + dy);
	  // set up new value
	  imagep[x][y] = image[x_off][y_off];
	} // end if (mode)
	// in both "mode" cases: increment count if the pixel changed
	if (image[x][y] != imagep[x][y])
	  ct++;
      }
    } // end for (x,y)
  *count = ct;
  /*
  printf("Altered %d pixels\n", *count);
  */
}

/* gateway routine */

#ifdef StaticP  /* undefined under mex */
StaticP
#endif
void
mexFunction(int nlhs, 
	    mxArray *plhs[], 
	    int nrhs,
	    const mxArray *prhs[])
{
  int m, n;
  double *center;  /* pointers to small vectors of parameters */
  char errstr[200];
  double datamin, datamax; /* hold range values in output image */
  int count; /* number of pixels changed */

  /* Hook for introspection (function signature, docstring) */
  if (nrhs < 0) { 
    plhs[0] = mxt_PackSignature((mxt_Signature) (-nrhs), 
				NARGIN_MIN, NARGIN_MAX, 
				NARGOUT_MIN, NARGOUT_MAX, 
				in_names, in_specs, out_names, docstring);
    return;
  }
  /* argument checking */
  if ((nrhs < NARGIN_MIN) || (nrhs > NARGIN_MAX))
     mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			    "%s: Expect %d <= input args <= %d",
			    progname, NARGIN_MIN, NARGIN_MAX), errstr));
  if (nlhs < NARGOUT_MIN || nlhs > NARGOUT_MAX)
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Expect %d <= output args <= %d",
			   progname, NARGOUT_MIN, NARGOUT_MAX), errstr));
  mexargparse(nrhs, prhs, in_names, in_specs, NULL, progname);

  /* center parameters */
  center  = mxGetPr(prhs[ARG_center]);

  /* get size of image */
  m = mxGetM(prhs[ARG_img]);
  n = mxGetN(prhs[ARG_img]);

  /* allocate output image */
  plhs[ARG_out] = mxCreateDoubleMatrix(m, n, mxREAL);
  if (nlhs > 1)
    plhs[ARG_nclean] = mxCreateDoubleMatrix(1, 1, mxREAL);

  getrange(prhs[ARG_img], &datamin, &datamax);   /* initialize range */
  /* setrange(plhs[ARG_out],  datamin,  datamax);   /* set range */
  setrange(plhs[ARG_out],  0.0,  254.0);   /* set range */

  /*
  printf("centr[%f,%f,%f];\n range[%f,%f]\n\n",
	     center[0], center[1], center[2], datamin, datamax);
  */

  /* fix the edges */
  if (mxGetScalar(prhs[ARG_delta]) != 0.0) {
    /* 2d indexing */
    double **img2 = mxt_make_matrix2(prhs[ARG_img], -1, -1, 0.0);
    double **out2 = mxt_make_matrix2(plhs[ARG_out], -1, -1, 0.0);
    propagate(&count,
	      img2, out2, 
	      n, m,        /* "n" corresponds to "x" */
	      center[0]-1, /* xcenter: C origin = (0,0) not (1,1) */
	      center[1]-1, /* ycenter: C origin = (0,0) not (1,1) */
	      center[2],   /* radius */
	      mxGetScalar(prhs[ARG_delta]),  /* width parameter */
	      mxGetScalar(prhs[ARG_mode])    /* modality */
	      );
    /* just frees the column pointers */
    mxFree(img2);
    mxFree(out2);
  } else {
    /* delta == 0: set out = img */
    memcpy(mxGetPr(plhs[ARG_out]), 
	   mxGetPr(prhs[ARG_img]), 
	   m * n * sizeof(double));
    count = 0;
  }
  /* plug in count if needed */
  if (nlhs > 1)
    *mxGetPr(plhs[ARG_nclean]) = (double) count;
} 



/* Hook for generic tail matter */
#ifdef MEX2C_TAIL_HOOK
#include "mex2c_tail.h"
#endif

