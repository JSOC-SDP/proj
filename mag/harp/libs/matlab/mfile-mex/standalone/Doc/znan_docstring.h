
/*
 * This file is generated by a script, do not edit.
 *
 * This documentation string was generated from
 * a C comment block within a mex program
 * by `doc2docstring.py' on Thu Jun 23 18:26:51 2011.
 */
 
static const char docstring[] =
	"znan	zero out NaN's, or set to val\n"
	"\n"
	" y=znan(x,val,pattern)\n"
	" * Sets y = x, except where pattern = NaN.  In these places, y is\n"
	" set to val.  It is equavalent to:\n"
	"    y = x;\n"
	"    y(isnan(pattern)) = val;\n"
	" * If pattern is not given, it is taken to be x itself, which has\n"
	" the effect of zeroing out the NaN's in x.\n"
	"\n"
	" Inputs:\n"
	"   real x(m,n);\n"
	"   opt real val = 0.0;\n"
	"   opt real pattern(m,n) = x;\n"
	"\n"
	" Outputs:\n"
	"   real y(m,n);\n"
	"\n"
	" See Also:  isnan\n"
	"\n"
	"";

/* End of generated file */
