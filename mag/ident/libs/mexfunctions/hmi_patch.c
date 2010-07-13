#include "mex.h"  /* must appear first */
#include <stdio.h>
#include <math.h>
#include "mexhead.h" /* my mex defines */
#include "Doc/hmi_patch_docstring.h"  /* autogenerated from this file */

/**************************************************************

%hmi_patch	driver for HMI patch finding
% 
% [bb,s,yrgn]=hmi_patch(y,mag,ctr,p0,beta,active,ker,kwt,tau)
% * Find active-region patches in a mask image y, returning them as 
% a list of bounding boxes bb, and a re-encoded mask image yrgn.
% * The parameter active tells what parts of y are considered
% to be within-active-region: (y == active) identifies the active
% stuff to be combined into patches.
% * The parameters ker, kwt, and tau control grouping, see
% smoothsphere for more.
% * Depending on later needs, some other morphological parameters
% might be added so that very tiny ARs are removed.  Currently
% this is not needed.
% 
% Inputs:
%   real y(m,n)
%   real mag(m,n)
%   real ctr(3)
%   real p0
%   real beta
%   int active
%   real ker(Nk)
%   real kwt(3)
%   real tau
% 
% Outputs:
%   int bb(nr,4)
%   real stats(nr,22)
%   real yrgn(m,n)
% 
% See Also:  smoothsphere region_bb concomponent roi_stats_mag

% turmon oct 2009, june 2010

****************************************************************/

/* standard boilerplate */
#define NARGIN_MIN	9	   /* min number of inputs */
#define NARGIN_MAX	9	   /* max number of inputs */
#define NARGOUT_MIN	2	   /* min number of output args */
#define NARGOUT_MAX	3	   /* max number of output args */

#define ARG_y      0
#define ARG_mag    1
#define ARG_ctr    2
#define ARG_p0     3
#define ARG_beta   4
#define ARG_active 5
#define ARG_ker    6
#define ARG_kwt    7
#define ARG_tau    8

#define ARG_bb     0
#define ARG_stats  1
#define ARG_yrgn   2

#define PROGNAME hmi_patch
static const char *progname = "hmi_patch";
static const char *in_specs[NARGIN_MAX] = {
  "RM",                   // y
  "RM",                   // mag
  "RV(3)",                // ctr
  "RS",                   // p0
  "RS",                   // beta
  "IS",                   // active
  "RV",                   // ker
  "RV(3)",                // kwt
  "RS(1)"                 // tau
};
static const char *in_names[NARGIN_MAX] = {
  "y",
  "mag",
  "ctr",
  "p0",
  "beta",
  "active",
  "ker",
  "kwt",
  "tau"
};
static const char *out_names[NARGOUT_MAX] = {
  "bb", 
  "stats",
  "yrgn"};

// short form of PROGNAME for #include identifiers
#define SHORTNAME Hpat

// declarations of mex-functions we will use
#include "smoothsphere.h"
#include "concomponent.h"
#include "region_bb.h"
#include "roi_stats_mag.h"

// define the argument numbers we actually use
#define MXT_ssp_NARGIN_USE  (MXT_ssp_NARGIN_MAX-1) // bws unused
#define MXT_ssp_NARGOUT_USE 1 // just the smoothed output
#define MXT_ccp_NARGIN_USE  MXT_ccp_NARGIN_MAX
#define MXT_ccp_NARGOUT_USE MXT_ccp_NARGOUT_MAX
#define MXT_rbb_NARGIN_USE  1 // coord unused
#define MXT_rbb_NARGOUT_USE MXT_rbb_NARGOUT_MAX
#define MXT_rsm_NARGIN_USE  MXT_rsm_NARGIN_MAX
#define MXT_rsm_NARGOUT_USE 1 // names unused


/************************************************************************
 *
 * Helper routines
 *
 ************************************************************************/
/*
 * patch_make_mask: make a binary mask out of y, and put into yp.
 */
static
void
patch_make_mask(double *yp, double *y, int N, double active)
{
  int i;
  
  for (i = 0; i < N; i++)
    if (y[i] == active)
      yp[i] = 1.0;
    else
      yp[i] = 0.0;
}

/*
 * patch_threshold_mask: make a binary mask by thresholding
 */
static
void
patch_threshold_mask(double *yp, double *x, int N, double tau)
{
  int i;
  
  for (i = 0; i < N; i++)
    if (x[i] > tau)
      yp[i] = 1.0;
    else
      yp[i] = 0.0;
}


/*
 * Gateway routine
 */
#ifdef StaticP  /* undefined under mex */
StaticP
#endif
void 
mexFunction(
	    int nlhs, 
	    mxArray *plhs[], 
	    int nrhs,
	    const mxArray *prhsC[])
{
  mxArray **prhs = (mxArray **) prhsC;    // squelch warnings
  // smoothsphere
  mxArray *prhs_ssp[MXT_ssp_NARGIN_USE];
  mxArray *plhs_ssp[MXT_ssp_NARGOUT_USE];
  // concomponent
  mxArray *prhs_ccp[MXT_ccp_NARGIN_USE];
  mxArray *plhs_ccp[MXT_ccp_NARGOUT_USE];
  // region_bb
  mxArray *prhs_rbb[MXT_rbb_NARGIN_USE];
  mxArray *plhs_rbb[MXT_rbb_NARGOUT_USE];
  // region_bb
  mxArray *prhs_rsm[MXT_rsm_NARGIN_USE];
  mxArray *plhs_rsm[MXT_rsm_NARGOUT_USE];
  // general declarations
  int M, N;         // size of y
  int Nr;           // number of regions
  char errstr[256];
  char *msg;

  /* Hook for introspection (function signature, docstring) */
  if (nrhs < 0) { 
    plhs[0] = mxt_PackSignature((mxt_Signature) (-nrhs), 
				NARGIN_MIN, NARGIN_MAX, 
				NARGOUT_MIN, NARGOUT_MAX, 
				in_names, in_specs, out_names, docstring);
    return;
  }
  /*
   * check args
   */
  if ((nrhs < NARGIN_MIN) || (nrhs > NARGIN_MAX))
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Expect %d <= input args <= %d",
			   progname, NARGIN_MIN, NARGIN_MAX), errstr));
  if ((nlhs < NARGOUT_MIN) || (nlhs > NARGOUT_MAX))
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Expect %d <= output args <= %d",
			   progname, NARGOUT_MIN, NARGOUT_MAX), errstr));
  mexargparse(nrhs, prhsC, in_names, in_specs, NULL, progname);

  /*
   * do the computation
   */
  M = mxGetM(prhs[ARG_y]);
  N = mxGetN(prhs[ARG_y]);

  // Find a 0/1 function indicating AR presence
  // Conceptually:
  //   x = patch_make_mask(y, active)
  // The code is internal to this function.
  prhs_ssp[MXT_ssp_ARG_x] = mxCreateDoubleMatrix(M, N, mxREAL);
  patch_make_mask(mxGetPr(prhs_ssp[MXT_ssp_ARG_x]), 
		  mxGetPr(prhs[ARG_y]), M*N, 
		  mxGetScalar(prhs[ARG_active]));

  // Smooth the 0/1 function:
  //   y = smoothsphere(x,center,p0,k,kparam,kwt,bws)
  // prhs_ssp[MXT_ssp_ARG_x] was set up above
  prhs_ssp[MXT_ssp_ARG_center] = prhs[ARG_ctr];
  prhs_ssp[MXT_ssp_ARG_p0]     = prhs[ARG_p0];
  prhs_ssp[MXT_ssp_ARG_k]      = prhs[ARG_ker];
  prhs_ssp[MXT_ssp_ARG_kparam] = mxCreateDoubleMatrix(1,2,mxREAL);
  // see sunspotgroupfind.m for these constants
  // (for the second, should use MDI_value * (4096/1024) )
  mxGetPr(prhs_ssp[MXT_ssp_ARG_kparam])[0] = 0.015;
  mxGetPr(prhs_ssp[MXT_ssp_ARG_kparam])[1] = rint((double) 50*4.0);
  prhs_ssp[MXT_ssp_ARG_kwt] = prhs[ARG_kwt]; // kwt
  // (let arg 6, bws, go to its default -- do not supply it)
  msg = main_smoothsphere(MXT_ssp_NARGOUT_USE, plhs_ssp, 
			  MXT_ssp_NARGIN_USE,  prhs_ssp);
  if (msg)
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Trouble in smoothsphere (%s)",
			   progname, msg), errstr));
  // free arrays that we allocate and no longer need
  // the 0/1 mask, ARG_x, is re-used below
  mxDestroyArray((mxArray *) prhs_ssp[MXT_ssp_ARG_kparam]);

  // Threshold the smoothed mask
  // Conceptually:
  //   x = patch_threshold_mask(y, tau)
  // The code is internal to this function.
  prhs_ccp[MXT_ccp_ARG_x] = mxCreateDoubleMatrix(M, N, mxREAL);
  patch_threshold_mask(mxGetPr(prhs_ccp[MXT_ccp_ARG_x]), 
		       mxGetPr(plhs_ssp[MXT_ssp_ARG_y]), M*N, 
		       mxGetScalar(prhs[ARG_tau]));
  // free arrays that we allocate and no longer need
  mxDestroyArray((mxArray *) plhs_ssp[MXT_ssp_ARG_y]);

  // Find connected components of thresholded mask
  //   y = concomponent(x,nbr)
  // prhs_ccp[MXT_ccp_ARG_x] was set up above
  prhs_ccp[MXT_ccp_ARG_nbr] = mxCreateDoubleScalar(8.0);
  msg = main_concomponent(MXT_ccp_NARGOUT_USE, plhs_ccp, 
			  MXT_ccp_NARGIN_USE,  prhs_ccp);
  if (msg)
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Trouble in smoothsphere (%s)",
			   progname, msg), errstr));
  // free arrays that we allocate and no longer need
  mxDestroyArray((mxArray *) prhs_ccp[MXT_ccp_ARG_x]);
  mxDestroyArray((mxArray *) prhs_ccp[MXT_ccp_ARG_nbr]);

  // Find bounding boxes around connected components
  //   bb = region_bb(x,coord)
  prhs_rbb[MXT_rbb_ARG_x] = plhs_ccp[MXT_ccp_ARG_y];
  // (second arg, coord, is optional; we omit it)
  // (without coord, it uses zero-based coordinates, as we wish)
  msg = main_region_bb(MXT_rbb_NARGOUT_USE, plhs_rbb, 
		       MXT_rbb_NARGIN_USE,  prhs_rbb);
  if (msg)
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Trouble in region_bb (%s)",
			   progname, msg), errstr));

  // filter/combine bounding boxes
  // (could also be above, if we wanted to remove tiny patches)
  // TBD

  // Find per-patch summary statistics
  //   s = roi_stats_mag(x,y,mag,center,beta)
  // second output arg unused
  prhs_rsm[MXT_rsm_ARG_x     ] = plhs_ccp[MXT_ccp_ARG_y]; // 1..Nr
  prhs_rsm[MXT_rsm_ARG_y     ] = prhs_ssp[MXT_ssp_ARG_x]; // 0/1
  prhs_rsm[MXT_rsm_ARG_mag   ] = prhs[ARG_mag];
  prhs_rsm[MXT_rsm_ARG_center] = prhs[ARG_ctr];
  prhs_rsm[MXT_rsm_ARG_beta  ] = prhs[ARG_beta];
  msg = main_roi_stats_mag(MXT_rsm_NARGOUT_USE, plhs_rsm, 
			   MXT_rsm_NARGIN_USE,  prhs_rsm);
  if (msg)
    mexErrMsgTxt((snprintf(errstr, sizeof(errstr),
			   "%s: Trouble in roi_stats_mag (%s)",
			   progname, msg), errstr));
  // free arrays that we allocate and no longer need
  mxDestroyArray((mxArray *) prhs_ssp[MXT_ssp_ARG_x]);

  // set up output args
  plhs[ARG_bb   ] = plhs_rbb[MXT_rbb_ARG_bb]; // bounding boxes
  plhs[ARG_stats] = plhs_rsm[MXT_rsm_ARG_s];  // statistics
  plhs[ARG_yrgn ] = plhs_ccp[MXT_ccp_ARG_y];  // mask with rgn #s
}



/* Hook for generic tail matter */
#ifdef MEX2C_TAIL_HOOK
#include "mex2c_tail.h"
#endif

