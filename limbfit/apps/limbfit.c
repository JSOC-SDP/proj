/* 
	I.Scholl 

		limbfit(): adapted from Solar Astrometry Program (solarSDO.c)
									Marcelo Emilio - Jan 2010 


	#define CODE_NAME 		"limbfit"
	#define CODE_VERSION 	"V1.14r0" 
	#define CODE_DATE 		"Sat Oct 22 09:44:08 HST 2011" 
*/

#include "limbfit.h"

#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))

int limbfit(LIMBFIT_INPUT *input,LIMBFIT_OUTPUT *results,FILE *opf,int debug)
{
static char *log_msg_code="limbfit";
char log_msg[200];

if (debug) lf_logmsg("DEBUG", "APP", 0, 0, "", log_msg_code, opf);

/************************************************************************
					INIT VARS coming from build_lev1.c
************************************************************************/	
	float *data=input->data;
//	float MIN_VALUE=BAD_PIXEL_VALUE;
//	float MAX_VALUE=(float)fabs(BAD_PIXEL_VALUE);


int 	ret_code = 0;

int 	w		 = ANNULUS_WIDTH;
long 	S		 = MAX_SIZE_ANN_VARS;
int 	nang	 = NUM_LDF;
int 	nprf	 = NUM_RADIAL_BINS;
int 	nreg	 = NUM_AB_BINS;
float 	rsi		 = LO_LIMIT;
float 	rso		 = UP_LIMIT;
int 	r_size	 = NUM_FITPNTS;
int		grange	 = GUESS_RANGE;
float 	dx		 = INC_X;
float 	dy		 = INC_Y;
int		naxis_row= input->img_sz0;
int		naxis_col= input->img_sz1;
float	lahi	 = AHI;
//int		skip	 = SKIPGC;
//float 	flag	 = BAD_PIXEL_VALUE; 
int		iter	 = NB_ITER;
if (input->iter!=NB_ITER) iter=input->iter; 

if (input->spe==1)
{
	iter=NB_ITER2;
	lahi=AHI2;
	rsi=LO_LIMIT2;
	rso=UP_LIMIT2;
}
	
/************************************************************************/
/*                        set parameters                               */
/************************************************************************/
long ii, jj, jk, i,j; 
float cmx, cmy,r;//, guess_cx, guess_cy, guess_r;
int nitr, ncut, ifail;
int jreg, jang, jprf;

r  = (float)naxis_row/2;
float sav_max=0.;
	
/* Initial guess estimate of center position from Richard's code*/
	cmx=(float)input->ix;
	cmy=(float)input->iy;
	r=(float)input->ir;

jreg = nreg ;
jang = nang + 1;
jprf = nprf;

/************************************************************************/
/*                        Make annulus data                             */
/************************************************************************/
float d, *x, *y, *v;

x = (float *) malloc(sizeof(float) * S);
	if(!x) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (x)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}
y = (float *) malloc(sizeof(float) * S);
	if(!y)
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (y)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}

v = (float *) malloc(sizeof(float) * S);
	if(!v) 	
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (v)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}

	/* Select Points */
	jk=-1;
	for(ii = 0; ii < naxis_row; ii++)
	{
		for(jj = 0; jj < naxis_col; jj++) 
		{ 
			d=(float)sqrt((double)(ii-cmy)*(ii-cmy)+(jj-cmx)*(jj-cmx));
			if (d<=(r+w/2.))
			{
				if (d>=(r-w/2.)) 
				{
					 jk=jk+1;
					 x[jk]=(float)jj;
					 y[jk]=(float)ii;
					 v[jk]=data[ii*naxis_col+jj];
				}
			}
		}
	}
	
	if (debug)
	{
		sprintf(log_msg," jk = %ld", jk);
		lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
	}
	if (jk >= S) return ERR_SIZE_ANN_TOO_BIG;

/************************************************************************/
/*                  Call Fortran Code Subrotine limb.f                  */
/************************************************************************/
float *anls, *rprf, *lprf, *alph, *beta, *b0, *beta1, *beta2, *beta3;
int nbc=3;
anls = (float *) malloc(sizeof(float)*(nbc*jk));
	if(!anls) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (anls)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}
rprf = (float *) malloc(sizeof(float)*(nprf));
	if(!rprf) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (rprf)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}
lprf = (float *) malloc(sizeof(float)*((nang+1)*nprf));
	if(!lprf) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (lprf)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}
alph = (float *) malloc(sizeof(float)*(nreg));
	if(!alph) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (alph)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}
beta = (float *) malloc(sizeof(float)*(nreg));
	if(!beta) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (xbeta)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}
beta1 = (float *) malloc(sizeof(float)*(nreg));
	if(!beta1) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (beta1)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}
beta2 = (float *) malloc(sizeof(float)*(nreg));
	if(!beta2)
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (beta2)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}
beta3 = (float *) malloc(sizeof(float)*(nreg));
	if(!beta3) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (beta3)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}
b0 = (float *) malloc(sizeof(float)*(nreg));
	if(!b0) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (b0)", log_msg_code, opf);
		return ERR_MALLOC_FAILED;
	}


	for (ii = 0; ii < jk; ii++) 
	{
		anls[nbc*ii]=x[ii];
		anls[nbc*ii+1]=y[ii];
		anls[nbc*ii+2]=v[ii];
	}
	for (ii=0; ii <nreg; ii++) beta[ii]=0.;

	// fortran call:
	if (debug) lf_logmsg("DEBUG", "APP", 0, 0, "entering limb", log_msg_code, opf);

	int centyp=0; //=0 do center calculation, =1 skip center calculation
	//#1
	if (input->cc == 0) centyp=1;
	limb_(&anls[0],&jk, &cmx, &cmy, &r, &nitr, &ncut, &nang, &nprf, &rprf[0], &lprf[0], &nreg, &rsi, &rso, 
		&dx, &dy, &jreg, &jang, &jprf, &alph[0], &b0[0], &ifail, &beta[0], &centyp, &lahi); 
	if (debug)
	{
		sprintf(log_msg,"exiting limb: %d", ifail);
		lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
	}
//printf("ifail %d %f %f\n",ifail,cmx,cmy);

	centyp=1;
	if (iter > 1 && ifail == 0)
	{	
		if (debug) lf_logmsg("DEBUG", "APP", 0, 0, "re-entering", log_msg_code, opf);
		//#2
		limb_(&anls[0],&jk, &cmx, &cmy, &r, &nitr, &ncut, &nang, &nprf, &rprf[0], &lprf[0], &nreg, &rsi, &rso, 
			&dx, &dy, &jreg, &jang, &jprf, &alph[0], &beta1[0], &ifail, &b0[0], &centyp, &lahi); 
		if (debug)
		{
			sprintf(log_msg," exiting limb_2nd time: %d", ifail);
			lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
		}
//printf("ifail %d %f %f\n",ifail,cmx,cmy);

		if(ifail==0) for (ii=0; ii<nreg; ii++) b0[ii]=b0[ii]+beta1[ii];// <<< TO CONVERT THIS WITH POINTERS
		//#3
		if(iter > 2 && ifail ==0)
		{	
			fprintf(stdout,"in limbfit -call fortran 3\n");
			limb_(&anls[0],&jk, &cmx, &cmy, &r, &nitr, &ncut, &nang, &nprf, &rprf[0], &lprf[0], &nreg, &rsi, &rso, 
				&dx, &dy, &jreg, &jang, &jprf, &alph[0], &beta2[0], &ifail, &b0[0], &centyp, &lahi); 
			if (debug) 
			{
				sprintf(log_msg,"exiting limb_3rd time: %d", ifail);
				lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
			}
			if(ifail==0) for (ii=0; ii<nreg; ii++) b0[ii]=b0[ii]+beta2[ii];// <<< TO CONVERT THIS WITH POINTERS
			if ((iter > 3) && (ifail ==0))
			{	
				//#4
				limb_(&anls[0],&jk, &cmx, &cmy, &r, &nitr, &ncut, &nang, &nprf, &rprf[0], &lprf[0], &nreg, &rsi, &rso, 
					&dx, &dy, &jreg, &jang, &jprf, &alph[0], &beta3[0], &ifail, &b0[0], &centyp, &lahi); 
				if (debug)
				{
					sprintf(log_msg,"exiting limb_4th time: %d", ifail);
					lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
				}
				if(ifail==0) for (ii=0; ii <nreg; ii++) b0[ii]=b0[ii]+beta3[ii];// <<< TO CONVERT THIS WITH POINTERS
			}
		} //ici faire qqch si ifail >0 sauver ldf,a,b de la version d'avant...
	}

	if (debug)
	{
		sprintf(log_msg," ifail = %6d", ifail);
		lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
		sprintf(log_msg," nitr = %6d", nitr);
		lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
		sprintf(log_msg," cmx = %6.2f, cmy = %6.2f", cmx, cmy);
		lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
	}
	
	if(ifail == 0) // || ifail == 7) 
	{		
		/************************************************************************/
		/*          Compute the mean of the center of the image                 */
		/************************************************************************/
		double cmean,ctot=0.0;
		long nbp=0;
		float limx_m=cmx-250;
		float limx_p=cmx+250;
		float limy_m=cmy-250;
		float limy_p=cmy+250;
		for (i=limx_m;i<limx_p;i++)
		{
			for (j=limy_m;j<limy_p;j++)
			{
				ctot=ctot+data[i*naxis_col+j]; 
				nbp++;
			}
		}
		cmean=ctot/nbp;
		if (debug)
		{
			sprintf(log_msg," cmean = %6.4f (ctot= %6.4f , nbp=%ld)", cmean,ctot,nbp);
			lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
		}

		/************************************************************************/
		/*                  Compute the Inflection Point                      */
		/************************************************************************/
		float *LDF, *D;
		LDF = (float *) malloc(sizeof(float)*(nprf));
			if(!LDF) 
			{
				lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (LDF)", log_msg_code, opf);
				return ERR_MALLOC_FAILED;
			}

		D = (float *) malloc(sizeof(float)*(nprf));
			if(!D)	
			{
				lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (D)", log_msg_code, opf);
				return ERR_MALLOC_FAILED;
			}

		double ip, ip1, maxim; 
		double radius = {0.0};
		double A[6] = { 0., 0., 0., 0., 0., 0. };
		double erro[6] = { 0., 0., 0., 0., 0., 0. };
		int cont, c, ret_gsl;
		float h;
			
		// for saving them in FITS file
		float *save_ldf, *save_alpha_beta; 
		double *save_params;
		long nb_p_as=6, nb_p_es=6, nb_p_radius=1, nb_p_ip=1; 
		long params_nrow=nang+1, params_ncol=nb_p_as+nb_p_es+nb_p_radius+nb_p_ip;
		long ldf_nrow=nang+2, ldf_ncol=nprf;  //ii -nbr of ldfs, jj -nbr of points for each ldf
		long ab_nrow=nreg, ab_ncol=2;

	    save_ldf 	= (float *) malloc((ldf_nrow*ldf_ncol)*sizeof(float));
			if(!save_ldf) 
			{
				lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (save_ldf)", log_msg_code, opf);
				return ERR_MALLOC_FAILED;
			}
	    save_alpha_beta = (float *) malloc((ab_nrow*ab_ncol)*sizeof(float));
			if(!save_alpha_beta) 
			{
				lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (save_alpha_beta)", log_msg_code, opf);
				return ERR_MALLOC_FAILED;
			}		
	    save_params = (double *) malloc((params_nrow*params_ncol)*sizeof(double));  
			if(!save_params) 
			{
				lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (save_params)", log_msg_code, opf);
				return ERR_MALLOC_FAILED;
			}			
			
		long zero_as=0;
		long zero_es=params_nrow*nb_p_as;
		long zero_r =params_nrow*(nb_p_as+nb_p_es);
		long zero_ip=params_nrow*(nb_p_as+nb_p_es+nb_p_radius);
			
		for (cont=0; cont<=nang; cont++)
		{	 
			ret_gsl=0;
			 ip1=0.;
			 /* dx */
			 h=(float)rprf[1]-rprf[0];
			
			 /* Take the last (mean) LDF from lprf vector */
			 for(ii = 0; ii < nprf ; ii++) 
			 	LDF[ii]=lprf[(nprf*cont)+ii];

			 /* Calculate the Derivative */
			 D[0]=(-3*LDF[4]+16*LDF[3]-36*LDF[2]+48*LDF[1]-25*LDF[0])/(12*h);
			 D[1]=(LDF[4]-6*LDF[3]+18*LDF[2]-10*LDF[1]-3*LDF[0])/(12*h);
			 for (i=2; i< nprf-2; i++) 
				D[i]=(LDF[i-2]-8*LDF[i-1]+8*LDF[i+1]-LDF[i+2])/(12*h);
			 D[nprf-2]=(-LDF[nprf-5]+6*LDF[nprf-4]-18*LDF[nprf-3]+10*LDF[nprf-2]+3*LDF[nprf-1])/(12*h);
			 D[nprf-1]=(3*LDF[nprf-5]-16*LDF[nprf-4]+36*LDF[nprf-3]-48*LDF[nprf-2]+25*LDF[nprf-1])/(12*h);
			
			 /* square the result */
			 for(ii = 0; ii < nprf ; ii++) 
				 D[ii]=D[ii]*D[ii];
			
			/* smooth */
			 for(ii = 1; ii < nprf-1 ; ii++) 
				 D[ii]=(D[ii-1]+D[ii]+D[ii+1])/3;
			
			 /* find the maximum */
			 jj=0;
			 maxim=-1;
			 for(ii = 1; ii < nprf-1 ; ii++) 
			 {
			 	if (D[ii] > maxim) 
			 	{
			  		maxim=(double)D[ii];
			   		jj=ii;
			  	}
			 }
			
			/* improve the maximum estimation looking at the 2 neibors points */
			ip=rprf[jj]-h/2.*(D[jj-1]-D[jj+1])/(2*D[jj]-D[jj-1]-D[jj+1]);
			
			if (debug)
			{
				if (cont==nang)
				{
					sprintf(log_msg," Inflection Point 1: %ld %d %8.5f %8.5f", jj, nprf, rprf[jj], ip);
					lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
				}
			}
			ip1=ip;
			/* FIT A GAUSSIAN PLUS QUADRATIC FUNCTION */
			
			/* Select Region */
			int m_in, m_ex, N;
			 /* Gaussian + quadratic function */
			 /* After Launch verify if this number is OK */
			 /* Should contain the pixels around the 3*FWHM level */
			m_in=jj-r_size;
			m_ex=jj+r_size;
			
			if (m_in < 0) m_in = 0;
			if (m_ex > nprf-1) m_ex =nprf-1;
			N=m_ex-m_in+1;
			/* parameters:- initial guess */
			A[0]= maxim;
			A[1]= ip;
			A[2]= 0.5;
			A[3]= 12*maxim;
			A[4]= 0;
			A[5]= 0.;
			for (c=0;c<6;c++) erro[c]=0.;
		
			if (N >= 6) 	// degree of freedom 6 parameters, 6 values minimum
			{
				double px[N], der[N] , sigma[N];
				
				jj=-1;
				for(ii = m_in; ii <= m_ex ; ii++)
				{
					   jj++;
					   px[jj]=rprf[ii];
					   der[jj]=D[ii];
					   sigma[jj] = 1.;
			 	}
				A[4]= der[N-1]-der[0];
				//if (debug==2) fprintf(opf,"%s DEBUG_INFO in limbfit: #: %d\n",LOGMSG1,cont);
				ret_gsl=gaussfit(der, px, sigma, A, erro, N, debug,opf);
				if (ret_gsl < 0)
				{
					if (cont==nang)
					{
						sprintf(log_msg," gaussfit failed for the averaged LDF %d err:%d", cont,ret_gsl);
						lf_logmsg("ERROR", "APP", 0, ret_gsl, log_msg, log_msg_code, opf);
					}
					for (c=0;c<6;c++) 
					{	
						A[c]=0.;
						erro[c]=0.;
					}
					radius=0.;
				}
				else
				{			
				 	/* FIND THE MAXIMUM OF THE GAUSSIAN PLUS QUADRATIC FUNCTION */
				 	radius = A[1]; /* Initial Guess */
				 	radius = fin_min(A, radius, grange, debug,opf);
					if (radius < 0)
					{
						ret_gsl=(int)radius;
						if (cont==nang)
						{
							sprintf(log_msg," fin_min failed for the averaged LDF %d err:%d", cont, ret_gsl);
							lf_logmsg("ERROR", "APP", 0, ret_gsl, log_msg, log_msg_code, opf);
						}
						for (c=0;c<6;c++) 
						{	
							A[c]=0.;
							erro[c]=0.;
						}
						radius=0.;
					}
				}
			} 
			else 
			{ 
				sprintf(log_msg," Inflection point too close to annulus data border %d", cont);
				lf_logmsg("WARNING", "APP", 0, 0, log_msg, log_msg_code, opf);
				for (c=0;c<nb_p_as;c++) erro[c]=0.;
				radius=ip;
				save_params[zero_ip+cont]=0.;
			}
			// IS: NOT SURE THIS IS AT THE RIGHT PLACE!!! just above before the "else"
			// save them
			for (c=0;c<nb_p_as;c++)
			{
				save_params[zero_as+params_nrow*c+cont]=A[c];
				save_params[zero_es+params_nrow*c+cont]=erro[c];
			}
			save_params[zero_r+cont]=radius;
			save_params[zero_ip+cont]=ip1;
		} // endfor-cont
		if (debug)
		{	
			sprintf(log_msg," Inflection Point 2: %8.5f %8.5f %8.5f", A[1], erro[1], radius);
			lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
			sprintf(log_msg," -----: %8.5f %8.5f %8.5f %8.5f %8.5f %8.5f", A[0],A[1],A[2],A[3],A[4],A[5]);
			lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
			sprintf(log_msg," -----: %8.5f %8.5f %8.5f %8.5f %8.5f %8.5f", erro[0],erro[1],erro[2],erro[3],erro[4],erro[5]);
			lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
		}

	//**********************************************************************
	//						Save LDFs & AB data in a FITS file                      
	//**********************************************************************
	

		// LDF 				// lprf & rprf come from limbfit.f			
		float *p_sldf=&save_ldf[0];
		float *pl_sldf=&save_ldf[(ldf_nrow*ldf_ncol)-1];
		float *p_rprf=&rprf[0];
		float *pl_rprf=&rprf[nprf-1];
		while (p_rprf <= pl_rprf)
			*(p_sldf++)=*(p_rprf++);

		float *p_lprf=&lprf[0];
		p_sldf=&save_ldf[ldf_ncol];
		while (p_sldf <= pl_sldf)
			*(p_sldf++)=*(p_lprf++);

		// AB 
		float *p_alph=&alph[0];
		float *pl_alph=&alph[ab_nrow-1];
		float *p_beta=&b0[0];
		float *pl_beta=&b0[ab_nrow-1]; 
		float *p_save_alpha_beta=&save_alpha_beta[0];
		while (p_alph <= pl_alph) *(p_save_alpha_beta++)=*(p_alph++);
		while (p_beta <= pl_beta) *(p_save_alpha_beta++)=*(p_beta++);
		 
		// Update Returned Structure when process succeeded                    
		results->cenx=cmx;
		results->ceny=cmy;
		results->radius=radius;
		results->cmean=cmean;
		results->max_limb=sav_max;
		results->numext=3;		
		results->fits_ldfs_naxis1=ldf_ncol;	
		results->fits_ldfs_naxis2=ldf_nrow;
//		results->fits_fldfs_tfields=fulldf_ncols;
//		results->fits_fldfs_nrows=fulldf_nrows;
		results->fits_ab_tfields=ab_ncol;
		results->fits_ab_nrows=ab_nrow;
		results->fits_params_tfields=params_ncol;
		results->fits_params_nrows=params_nrow;
//		results->nb_fbins=bins1;
		results->nb_fbins=0;
		results->ann_wd=w;
		results->mxszannv=S;
		results->nb_ldf=nang;
		results->nb_rdb=nprf;
		results->nb_abb=nreg;
		results->up_limit=rsi;
		results->lo_limit=rso;
		results->inc_x=dx;
		results->inc_y=dy;
		results->nfitpnts=r_size;
		results->nb_iter=iter;
		results->ahi=lahi;
		results->error1=ifail;		
		results->error2=ret_gsl;		
		results->cc=input->cc;
		if (ifail == 0) 
			results->quality=9; 
		//? if (cont>=nang && ret_gsl<0)
		if (ret_gsl<0)
		{
			results->quality=1; 
			ret_code=ERR_LIMBFIT_FIT_FAILED;
			if (debug)
			{	
				sprintf(log_msg,"  ret_gsl<0 = %2d", ret_gsl);
				lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
	 		}     	
		}
		results->fits_ldfs_data=save_ldf; 
		results->fits_params=save_params; 
		results->fits_alpha_beta=save_alpha_beta; 
//		results->fits_fulldfs=save_full_ldf;

		free(D);
		free(LDF);
	} // end limb OK
	else 
	{
		if (debug)
		{	
			sprintf(log_msg," limb.f routine returned ifail = %2d", ifail);
			lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
 		}     	
		// Update Returned Structure when process failed                    
		results->numext=0;		
		results->error1=ifail;
		results->error2=-1;
		results->quality=0;
      	ret_code=ERR_LIMBFIT_FAILED;
		results->ann_wd=w;
		results->mxszannv=S;
		results->nb_ldf=nang;
		results->nb_rdb=nprf;
		results->nb_abb=nreg;
		results->up_limit=rsi;
		results->lo_limit=rso;
		results->inc_x=dx;
		results->inc_y=dy;
		results->nfitpnts=r_size;
		results->nb_iter=iter;
		results->ahi=lahi;
		results->max_limb=0;
		results->cmean=0;
		results->nb_fbins=0;
		results->cc=input->cc;
	} // end limb failed
	// IS: do not free those (save_ldf,save_params,save_alpha_beta) passed from or to the structure !
		free(x);
		free(y);
		free(v); 
		free(rprf);
		free(alph);
		free(beta);
		free(beta1);
		free(beta2);
		free(beta3);
		free(b0);
		free(lprf);
		if (debug != 3) free(anls);
	if (debug)
	{	
		sprintf(log_msg," >>>>end of limbfit with: %d", ret_code);
		lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
	}
	sprintf(log_msg," end: RC: %d - limb:%d - fit:%d - quality: %d", ret_code, results->error1, results->error2, results->quality);
	lf_logmsg("INFO", "APP", 0, 0, log_msg, log_msg_code, opf);

return(ret_code);

} /* end of main */


/*--------------------------------------------------------------------------*/
int gaussfit(double y[], double t[],double sigma[], double A[6], double erro[6], long N, int debug, FILE *opf)
/* Calculate a Least SqrareGaussian + Quadratic fit              */
/*      Uses the GNU Scientific Library                          */
/* Marcelo Emilio (c) v 1.0 Jan 2009                             */
/* fits A[0]exp(-z^2/2)+A[3]+A[4]*x+A[5]*x^2                     */
/* z=(t-A[1])/A[2]                                               */
/* Need to add :                                                 */
/* #include <gsl/gsl_rng.h>                                      */
/* #include <gsl/gsl_randist.h>                                  */
/* #include <gsl/gsl_vector.h>                                   */
/* #include <gsl/gsl_blas.h>                                     */
/* #include <gsl/gsl_multifit_nlin.h>                            */
/* #include "expfit.c"                                           */
/* compiles as  gcc program.c --lm -lgsl -lgslcblas              */
/* y --> f(x) - ordinate values                                  */
/* t --> x(i) - abscissa values                                  */
/* sigma --> independent gaussian erros                          */
/* N --> Number of points in the vector                          */
/* A --> Input a initial guess and output the  result            */
/* erro --> Chi-square of A                                      */

{
	static char *log_msg_code="gaussfit";
	int ret_code=0;
	char log_msg[200];
	
	const gsl_multifit_fdfsolver_type *T;
	gsl_multifit_fdfsolver *s;
	int status;
	unsigned int i, iter = 0;
	const size_t n = N;
	const size_t p = 6;
		 
	gsl_matrix *covar = gsl_matrix_alloc (p, p);
	/* double t[N], y[N], sigma[N]; */
	struct dataF d = { n, t, y, sigma};
	gsl_multifit_function_fdf f;
	
	/* Initial Guess */
	double x_init[6];
	for (i=0; i <=5; i++) 
	   x_init[i]=A[i];
	
	gsl_vector_view x = gsl_vector_view_array (x_init, p);
	const gsl_rng_type * type;
	gsl_rng * r;
		 
	gsl_rng_env_setup();
		 
	type = gsl_rng_default;
	r = gsl_rng_alloc (type);
		 
	f.f = &expb_f;
	f.df = &expb_df;
	f.fdf = &expb_fdf;
	f.n = n;
	f.p = p;
	f.params = &d;
		 
	//double d1,d2,d3,d4;

	T = gsl_multifit_fdfsolver_lmsder;
	s = gsl_multifit_fdfsolver_alloc (T, n, p);
	gsl_multifit_fdfsolver_set (s, &f, &x.vector);  

	do
	{
	
		iter++;
		status = gsl_multifit_fdfsolver_iterate (s);     
		if (!status)	
			status = gsl_multifit_test_delta (s->dx, s->x, 1e-6, 1e-2);  		/* IS: change epsrel (last arg of gsl_multifit_test_delta) after the SDO Launch */ 
		else 
		{
			ret_code=ERR_GSL_GAUSSFIT_SET_FAILED;	
			if (debug) 
			{
				sprintf(log_msg," iter: %u, status: %d (%s)", iter,status,gsl_strerror (status));
				lf_logmsg("WARNING", "APP", ret_code, status, log_msg, log_msg_code, opf);			
			}
		}
	}
	while (status == GSL_CONTINUE && iter < 500);
			
	if (status >=0)  
	{
		gsl_multifit_covar (s->J, 0.0, covar);
			 
		#define FIT(i) gsl_vector_get(s->x, i)
		#define ERR(i) sqrt(gsl_matrix_get(covar,i,i))
			 
		double chi = gsl_blas_dnrm2(s->f);
		double dof = (double)(n - p);
		double c = GSL_MAX_DBL(1, chi / sqrt(dof)); 
		  
		
		for (i=0; i <=5; i++) {
			A[i]=FIT(i);
			erro[i]=c*ERR(i);
		}  
	}
	else
	{
		ret_code=ERR_GSL_GAUSSFIT_FDFSOLVER_FAILED;			
		if (debug)
		{
			sprintf(log_msg," (gsl_multifit_fdfsolver_iterate) failed, (iter=%u) gsl_errno=%d", iter,status);
			lf_logmsg("ERROR", "APP", ret_code, status, log_msg, log_msg_code, opf);
		}
	}

	
	if (debug==2)
	{																		
		sprintf(log_msg," Nonlinear LSQ fitting status = %s (%d)", gsl_strerror (status),status);
		lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
	}
	gsl_multifit_fdfsolver_free (s);
	gsl_matrix_free (covar);
	gsl_rng_free (r);
	return(ret_code);
}


/*--------------------------------------------------------------------------*/

double fin_min(double A[6], double m, int range, int debug, FILE *opf)
/* Calculate the maximum of the quadratic + gaussian function    */
/*                 using Brent algorithm                         */
/* Marcelo Emilio (c) v 1.0 Jan 2009                             */
/* A are the parameters of the gaussian                          */
/* m is the guess where the function is maximum                  */
/* Calculate the maximum around                                  */
/*                 (-5 + m) < m > (5 - m)  ( in pixel units )    */
/* Returns the m vaule where the function is maximum             */
/* requires: #include "expmax.c" --> definition of the function  */
/*           #include <gsl/gsl_errno.h>                          */
/*           #include <gsl/gsl_math.h>                           */
/*           #include <gsl/gsl_min.h>                            */
{
	static char *log_msg_code="fin_min";
	int status;
	gsl_set_error_handler_off();
	int ret_code=0;
	char log_msg[200];
	
	 int iter = 0, max_iter = 1000;
	 const gsl_min_fminimizer_type *T;
	 gsl_min_fminimizer *s;
	 
	 gsl_function F;
	
	//double m_expected = m+0.01; //1e-8;
	//double a = m-5000, b = m+5000;
	double a = m-range, b = m+range;      // initially = 2
	struct exp_plus_quadratic_params params= { A[0], A[1], A[2], A[3], A[4], A[5] }; 
	
	  F.function = &exp_plus_quadratic_function;
	  F.params = &params;
		 
	  T = gsl_min_fminimizer_brent;
	  s = gsl_min_fminimizer_alloc (T);
	
	  status=gsl_min_fminimizer_set (s, &F, m, a, b);     
	  if (status)
	  {
			if (status == GSL_EINVAL) 
			{
				ret_code=ERR_GSL_FINMIN_NOMIN_FAILED;
				if (debug)
				{
					sprintf(log_msg," (gsl_min_fminimizer_set) doesn't find a minimum, m=%f", m);
					lf_logmsg("DEBUG", "APP", 0, status, log_msg, log_msg_code, opf);
				}
			} 
			else 
			{
				ret_code=ERR_GSL_FINMIN_SET_FAILED;
				if (debug)
				{
					sprintf(log_msg," (gsl_min_fminimizer_set) failed, gsl_errno=%d", status);
					lf_logmsg("ERROR", "APP", ret_code, status, log_msg, log_msg_code, opf);			
				}
			}
			return ret_code;
	  }
	  
	  do
	  {
	   iter++;
	   status = gsl_min_fminimizer_iterate (s);
		 
	   m = gsl_min_fminimizer_x_minimum (s);
	   a = gsl_min_fminimizer_x_lower (s);
	   b = gsl_min_fminimizer_x_upper (s);
		 
	   status = gsl_min_test_interval (a, b, 1E-4, 0.0); 
	  }
	  while (status == GSL_CONTINUE && iter < max_iter);
	  
	  if (status)		// regarder lequel plantait et verifier si on a autre chose que GSL_EINVAL comme erreur
		{				// ajouter debug pour les messages apres
			if (status == GSL_EINVAL) 
			{
				ret_code=ERR_GSL_FINMIN_PRO_FAILED;
				if (debug)
				{
					sprintf(log_msg," invalid argument, n=%8.5f", a);
					lf_logmsg("ERROR", "APP", ret_code, status, log_msg, log_msg_code, opf);			
				}
			} 
			else 
			{			
				ret_code=ERR_GSL_FINMIN_PRO_FAILED;
				if (debug)
				{
					sprintf(log_msg," failed, gsl_errno=%d", status);
					lf_logmsg("ERROR", "APP", ret_code, status, log_msg, log_msg_code, opf);			
				}
			}
			return ret_code; 
		}
	  
	  if (debug==2)
	  {
			sprintf(log_msg," a:%8.5f b:%8.5f m:%8.5f iter=%d, b-a:%f", a,b,m,iter,b-a);
			lf_logmsg("DEBUG", "APP", 0, 0, log_msg, log_msg_code, opf);
	  }


	  gsl_min_fminimizer_free (s);
		 
	  return m;

}
