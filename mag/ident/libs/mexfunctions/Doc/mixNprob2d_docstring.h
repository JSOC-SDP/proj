
/*
 * This file is generated by a script, do not edit.
 *
 * This documentation string was generated from
 * a C comment block within a mex program
 * by `doc2docstring.py' on Wed Jul 14 12:49:31 2010.
 */
 
static const char docstring[] =
	" mixNprob2d: probabilities under normal mixture (2 dimensions)\n"
	"\n"
	" p=mixNprob2d(i1,i2,model,mode);\n"
	" * This routine has been superseded by mixNprobNd.  But we retain it\n"
	" because it can be much faster for 2d models.\n"
	"\n"
	" Inputs:\n"
	"   real i1[m,n];\n"
	"   real i2[m,n];\n"
	"   real model[6,k];\n"
	"   opt int logmode = 1;\n"
	"\n"
	" Outputs:\n"
	"   real p[m,n];\n"
	"\n"
	" See Also: mixN2mixture2d, mixNprobNd\n"
	"\n"
	" implemented as a mex file\n"
	"\n"
	" MJT 20 july 1998: tested exhaustively against mixNprob,\n"
	" the m-file for finding gaussian mixture probabilities,\n"
	" and found agreement to within floating-point accuracy\n"
	"\n"
	"";

/* End of generated file */

