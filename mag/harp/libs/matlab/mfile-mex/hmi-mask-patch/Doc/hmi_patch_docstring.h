
/*
 * This file is generated by a script, do not edit.
 *
 * This documentation string was generated from
 * a C comment block within a mex program
 * by `doc2docstring.py' on Thu Jun 23 18:32:25 2011.
 */
 
static const char docstring[] =
	"hmi_patch	driver for HMI patch finding\n"
	"\n"
	" [bb,s,yrgn,crit]=hmi_patch(y,mag,geom,active,ker,kwt,tau)\n"
	" * Find active-region patches in a mask image y, returning them as\n"
	" a list of bounding boxes bb, and a re-encoded mask image yrgn.\n"
	" Optionally returns the smoothed mask via crit.\n"
	" * If mag is given as [], the statistics s are not computed and\n"
	" the return value is empty.\n"
	" * The parameter active tells what parts of y are considered\n"
	" to be within-active-region: (y == active) identifies the active\n"
	" stuff to be combined into patches.\n"
	" * The parameters ker, kwt, and tau control grouping, see\n"
	" smoothsphere for more.\n"
	" * Depending on later needs, some other morphological parameters\n"
	" might be added so that very tiny ARs are removed.  Currently\n"
	" this is not needed.\n"
	"\n"
	" Inputs:\n"
	"   real y(m,n)\n"
	"   real mag(m,n) or (0,0)\n"
	"   real geom(5)\n"
	"   int active\n"
	"   real ker(Nk)\n"
	"   real kwt(3)\n"
	"   real tau\n"
	"\n"
	" Outputs:\n"
	"   int bb(nr,4)\n"
	"   real stats(nr,28) or (0,0)\n"
	"   real yrgn(m,n)\n"
	"   opt real crit(m,n)\n"
	"\n"
	" See Also:  smoothsphere region_bb concomponent roi_stats_mag\n"
	"\n"
	" turmon oct 2009, june 2010, sep 2010\n"
	"\n"
	"";

/* End of generated file */
