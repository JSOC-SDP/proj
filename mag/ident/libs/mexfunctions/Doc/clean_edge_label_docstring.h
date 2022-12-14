
/*
 * This file is generated by a script, do not edit.
 *
 * This documentation string was generated from
 * a C comment block within a mex program
 * by `doc2docstring.py' on Thu Jul  7 21:45:38 2011.
 */
 
static const char docstring[] =
	" clean_edge_label: clean up labels at extreme edge of a disk\n"
	"\n"
	" [res,nclean]=clean_edge_label(img,center,delta,fill,mode);\n"
	" * Remove possibly-tainted labels at edge of disk by cleaning\n"
	" pixels in an annulus of width delta pixels.  Also, sets all\n"
	" off-disk values to the value of the (1,1) pixel of img.\n"
	" * There are two cleaning modes.  Easiest is, set values in\n"
	" the edge annulus to `fill' (NaN is OK).  But, if `mode'\n"
	" contains the string \"adaptive\", we instead propagate the value\n"
	" just inside the annulus radially outward to the limb, and the\n"
	" `fill' value is unused.\n"
	" * The number of on-disk pixels altered is in nclean.\n"
	" * Primary disk parameters (x-center, y-center, radius) are in\n"
	" `center'.  The sun is the disk defined by these three numbers.\n"
	" As a convenience, 'center' can be a \"geom\" vector, a 5-tuple\n"
	" with beta and p-angle at the end (these are not needed by\n"
	" this routine).\n"
	" * If delta < 0, img is propagated to res without further ado.\n"
	" (The off-disk clearing is not done either.)\n"
	" In this case, the center parameter need not be correct.\n"
	" * The \"mode\" string also switches between sesw (mode = 'sesw')\n"
	" or transposed (mode = 'sene') pixel ordering.  You must specify\n"
	" one or the other.\n"
	" * The normal HMI (and normal MDI) pixel ordering starts in the\n"
	" southeast corner, and the first scan line of pixels runs toward the\n"
	" southwest corner.  This is `sesw' ordering.  The transposed ordering\n"
	" is `sene'; this ordering is what we used for the JPL MDI processing.\n"
	" This is implemented via internal stride parameters.\n"
	"\n"
	"\n"
	" Inputs:\n"
	"   real img(m,n);\n"
	"   real center(3) or (5);\n"
	"   real delta;\n"
	"   real fill;\n"
	"   string mode;\n"
	"\n"
	" Outputs:\n"
	"   real res(m,n);\n"
	"   int nclean;\n"
	"\n"
	"\n"
	" implemented as a mex file\n"
	"\n"
	"";

/* End of generated file */

