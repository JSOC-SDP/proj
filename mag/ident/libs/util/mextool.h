/*
 * ARGUMENT TRANSLATION TEMPLATES
 * (mextool.c)
 *
 * Michael Turmon, 2002
 * 
 */

#ifndef _mextool_h_
#define _mextool_h_

#ifdef __cplusplus
extern "C" {
#endif
#ifdef NOT_DEFINED 
} /* fool emacs */
#endif

/* If mexFunction is passed nrhs < 0, it causes special behavior, including
 * passing back information about the function signature of the mexFunction.
 * This enum lists the possibilities for returned information.
 * See also mxt_PackSignature(), which wraps the information up.
 */
typedef enum {
    mxt_SignatureNARG = 1,
    mxt_SignatureDocstring,
    mxt_SignatureInNames,
    mxt_SignatureInSpecs,
    mxt_SignatureOutNames
} mxt_Signature;

mxArray * 
mxt_PackSignature(mxt_Signature index, 
		  int in_lo, int in_hi, int out_lo, int out_hi, 
		  const char **in_names, 
		  const char **in_specs, 
		  const char **out_names, 
		  const char *docstring);

char **
mxt_ArrayToStrings(const mxArray *a);

int
mxt_put_matrix(char *name,
	       int inx,
	       double *dat,
	       mwSize m,
	       mwSize n);
double
mxt_make_scalar(
		const mxArray *pm,
		double default_val);
double *
mxt_make_vector(
		const mxArray *pm,
		mwSignedIndex M,
		double *default_val,
		mwSize default_len);
double **
mxt_make_matrix1(
		 const mxArray *pm,
		 mwSignedIndex M,
		 mwSignedIndex N,
		 double default_val);
double **
mxt_make_matrix2(
		 const mxArray *pm,
		 mwSignedIndex M,
		 mwSignedIndex N,
		 double default_val);

double **
mxt_matrix_index(double *data, mwSize m, mwSize n);
double ***
mxt_matrix3_index(double *data, mwSize m, mwSize n, mwSize p);
double **
mxt_matrix_alloc(double *data, mwSize m, mwSize n);
double ***
mxt_matrix3_alloc(double *data, mwSize m, mwSize n, mwSize p);

int
mxt_transpose_double_mxArray(mxArray *pa);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* _mextool_h_ */

