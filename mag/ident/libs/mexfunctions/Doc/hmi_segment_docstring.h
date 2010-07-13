
/*
 * This file is generated by a script, do not edit.
 *
 * This documentation string was generated from
 * a C comment block within a mex program
 * by `doc2docstring.py' on Wed Jun 16 11:39:45 2010.
 */
 
static const char docstring[] =
	"hmi_segment	driver for HMI segmentation\n"
	"\n"
	" y=hmi_segment(xm,xp,iter,T,beta,alpha,ctr,rho,mode,m1,...)\n"
	" * Integrated routine for deriving HMI segmentations.  Uses\n"
	" models (m1,m2,...), plus a magnetogram-photogram pair,\n"
	" to deduce an integer labeling.  Besides images and models,\n"
	" it also requires some disk parameters and labeling smoothness\n"
	" parameters.\n"
	" * The defaults for iter and T are inherited from mrf_segment_wts.\n"
	"\n"
	" Inputs:\n"
	"   real xm(m,n)\n"
	"   real xp(m,n)\n"
	"   int iter[1] or [2];\n"
	"   real T[0] or [1] or [2] or [3] or [4];\n"
	"   real beta[1] or [K,K];\n"
	"   real alpha[K] or [0] = [];\n"
	"   real ctr(3) or ctr(4) or ctr(5);\n"
	"   real rho;\n"
	"   string mode\n"
	"   real m1(l,k1)\n"
	"   ...\n"
	"   real mR(l,kR)\n"
	"\n"
	" Outputs:\n"
	"   int y(m,n)\n"
	"   opt real post\n"
	"\n"
	" See Also:  makemrfdiscwts, mrf_segment_wts4, mixNprobNd\n"
	"\n"
	" turmon oct 2009, june 2010\n"
	"\n"
	"";

/* End of generated file */

