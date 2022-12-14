/* 
	I.Scholl 

		High level wrapper for the limbfit code
		Can be used as a stand-alone module (includes a 'DoIt' function)
			calls: do_one_limbfit() that calls:
										limbfit() and reformat output data to be rewritten in the DB
			do_one_limbfit() can be plugged into lev0lev1 module
			
	
	#define CODE_NAME 		"limbfit_ann"
	#define CODE_VERSION 	"V1.00" 
	#define CODE_DATE 		"Mon Sep 15 15:14:19 PDT 2014" 
	
	V4.03 from previous:	 only default parameters changed
*/

#include "limbfit_ann.h"

char *module_name = "lfwrp_ann";

ModuleArgs_t module_args[] = {
  {ARG_STRING, 	"dsin", "hmi.lev1", "input data set (default=hmi.lev1)"},
  {ARG_STRING, 	"dsout", "su_scholl.limbfit_ann", "output data set"},
  {ARG_STRING, 	"logdir", "./", "logs directory"},
  {ARG_STRING, 	"tmpdir", "./", "tmp directory"},
  {ARG_STRING, 	"bdate", " ", "begin date"},
//  {ARG_STRING, 	"edate", " ", "end date (not implemented yet"},
  {ARG_INTS, 	"bfsn", "0", "first lev1 fsn# to process"},
  {ARG_INTS, 	"efsn", "0", "last  lev1 fsn# to process"},
  {ARG_INT, 	"cam", "0", "camera selection: 0: both = default, otherwise: 1 or 2"},
  {ARG_INT, 	"fid", "0", "default=all, otherwise one FID number or mask"},
//  {ARG_INT, 	"smpl", "0", "sampling factor: default=no sampling, otherwise: number/unit eg: 4: 1/4 of the program (1 seq, skip next 3 seqs). this is combined with cam and fid"},
  {ARG_INT, 	"src", "0", "0: hmi.lev1 (default), 1: hmi.lev1_nrt  (quality test is different)"},
  {ARG_STRING, 	"comment", " ", "to add a comment in a dataset"},
  {ARG_INT, 	"debug", "0", "debug level (default: 0 no debug info)"},
  {ARG_END}

}; 

void get_sdate(char *sdate)
{

  time_t t = time(NULL);
  struct tm *timeptr;

  timeptr = localtime(&t);
  sprintf(sdate, "%d.%02d.%02d_%02d:%02d:%02d", 
	  (timeptr->tm_year+1900), (timeptr->tm_mon+1),
	  timeptr->tm_mday, timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);
}


void lf_logmsg(char * type1, char * type2, int return_code, int status, char *message, char *code_name, FILE *opf)
{
	static char sdate[32];
	get_sdate(sdate);
	if ( !(strcmp(type1,"INFO")) || (status==0 &&return_code==0))
		fprintf(opf,"%s/%20s: %s %5s %5s msg: '%s' \n",LOGMSG1, code_name, sdate,type1,type2, message);
	else
		fprintf(opf,"%s/%20s: %s %5s %5s msg: '%s' return code: %d exit: %d\n",LOGMSG1, code_name, sdate,type1,type2, message, return_code,status);
}

void close_on_error(DRMS_Record_t *record_in,DRMS_Record_t *record_out,DRMS_Array_t *data_array) //,FILE *opf)
{
	drms_free_array (data_array);
	drms_close_record (record_out, DRMS_FREE_RECORD);
	drms_close_record (record_in, DRMS_FREE_RECORD);
	//fclose(opf);
}

//************************************************************************
// Get N records, decide or not to process them
//************************************************************************

int process_n_records_fsn(char * open_dsname, LIMBFIT_INPUT *lfv, LIMBFIT_OUTPUT *lfr, int *status)    
{
	static char *log_msg_code="process_n_records";
	char log_msg[200];
	sprintf(log_msg,"doing process for %s -> %s",open_dsname,lfr->dsout);
	lf_logmsg("INFO", "APP", 0,0, log_msg, log_msg_code, lfr->opf);			
//	static char errcode[20]=PROCSTAT_NOK;
	
	//************************************************************************
	//  Open DRMS connexion, get the range of data, decide what to do, 
	//			and either call the next step or skip
	//************************************************************************
	
	DRMS_RecordSet_t *drs_in,*drs_out;
	DRMS_Record_t *record_in,*record_out;
	int rstatus, ncnt,r;

    drs_in = drms_open_records(drms_env, open_dsname, &rstatus);
	if (!drs_in) {
		sprintf(log_msg,"unable to open record set %s\n",open_dsname);
		lf_logmsg("ERROR", "DRMS", ERR_DRMS_READ, rstatus, log_msg, log_msg_code, lfr->opf);			
		fprintf (stderr, log_msg);
		*status=ERR_DRMS_READ;
		return(ERR_DRMS_READ);
	}

	ncnt = drs_in->n;
	if (ncnt < 1) {
		sprintf(log_msg,"no records in selected set %s\n",open_dsname);
		lf_logmsg("WARNING", "DRMS", WAR_DRMS_NORECORD, rstatus, log_msg, log_msg_code, lfr->opf);			
		fprintf (stderr, log_msg);
		*status=WAR_DRMS_NORECORD;
		drms_close_records(drs_in, DRMS_FREE_RECORD);
		return(WAR_DRMS_NORECORD);
	}

    drs_out = drms_create_records(drms_env, ncnt, lfr->dsout, DRMS_PERMANENT,&rstatus);
	if (!drs_out) {
		sprintf(log_msg,"unable to create record set %s\n",lfr->dsout);
		lf_logmsg("ERROR", "DRMS", ERR_DRMS_WRITE, rstatus, log_msg, log_msg_code, lfr->opf);			
		fprintf (stderr, log_msg);
		*status=ERR_DRMS_WRITE;
		drms_close_records(drs_in, DRMS_FREE_RECORD);
		return(ERR_DRMS_WRITE);
	}
    
	unsigned int fsn = 0;

    for(r=0; r < ncnt; r++) 
    {
		record_in = drs_in->records[r];
		record_out = drs_out->records[r]; 

		fsn = drms_getkey_int(record_in, "FSN", &rstatus);       
		if(rstatus) {
			lf_logmsg("ERROR", "DRMS", ERR_DRMS_READ_MISSING_KW, rstatus, "drms_getkey_string(FSN)", log_msg_code, lfr->opf);			
			write_mini_output(PROCSTAT_NO_LA_DB_READ_PB,record_in,record_out,0,lfr);
			*status=ERR_DRMS_READ_MISSING_KW;   
			drms_close_records(drs_out, DRMS_INSERT_RECORD);
			drms_close_records(drs_in, DRMS_FREE_RECORD);
			return(0);   
		}
		sprintf(log_msg,"FSN:%u processing",fsn);
		lf_logmsg("INFO", "APP", 0,0, log_msg, log_msg_code, lfr->opf);			
		printf("selection: #%u\n",fsn);
		lfv->fsn=fsn;
		if (do_one_limbfit(record_in,record_out,lfv,lfr,&rstatus))
		{
			if (rstatus < 0 && rstatus > -300)
			{
				drms_close_records(drs_out, DRMS_INSERT_RECORD);
				drms_close_records(drs_in, DRMS_FREE_RECORD);
				lf_logmsg("ERROR", "APP", rstatus, 0, "to be aborted", log_msg_code, lfr->opf);			
				return(rstatus);
			}
		}
		fflush(lfr->opf);
	}
	drms_close_records(drs_out, DRMS_INSERT_RECORD);
	drms_close_records(drs_in, DRMS_FREE_RECORD);
	lf_logmsg("INFO", "APP", 0, 0, "Records saved", log_msg_code, lfr->opf);			
 	if (lfr->debug) printf("records saved\n");
	
return(0);
}

//************************************************************************
// MAIN
// 			usage ./lfwrp +all following params
//************************************************************************

int DoIt(void)
{
	// get arguments
	
	CmdParams_t *params = &cmdparams;
	
	int  debug 	= params_get_int (params, "debug");
	int  cam 	= params_get_int (params, "cam");
	int  fid 	= params_get_int (params, "fid");
	//int  smpl 	= params_get_int (params, "smpl");
	int  src 	= params_get_int (params, "src");
	char* dsin 	= params_get_str (params, "dsin");
	char* dsout = params_get_str (params, "dsout");
	char* log_dir = params_get_str (params, "logdir");
	char* tmp_dir = params_get_str (params, "tmpdir");
	char* comment = params_get_str (params, "comment");
	long long bfsn = params_get_int (params, "bfsn");
	long long efsn = params_get_int (params, "efsn");
	char* bdate = params_get_str (params, "bdate");
	//char* edate = params_get_str (params, "edate");

	static char *log_msg_code="DoIt";
	char log_msg[200];
	int result;
		
	static char open_dsname[400];
	char recrange[128];
	static char qual[15];

	if ((bfsn == 0 || efsn == 0) && strcmp(bdate," ")==0)
    {
		fprintf(stderr, "bfsn and efsn must be given for fsn mode or begin date must be specified. \n");
		return(ERR_EXIT);
    }
	if(strcmp(bdate," ")==0 && bfsn > efsn) 
    {
		fprintf(stderr, "bfsn must be <= efsn\n");
		return(ERR_EXIT);
    }
    if(cam < 0 || cam > 2) 
    {
		fprintf(stderr, "cam must be equal to 0, 1, or 2\n");
		return(ERR_EXIT);
    }
	if(src == 0) 
		sprintf(qual,"0");
	else 
		sprintf(qual,"1073741824"); //= 0x40000000 

	static char bld_vers[16];
  	sprintf(bld_vers, "%s", jsoc_version);
 	
	//---------------------------------------------------------
	//	Create a global log file for the whole session 
	//---------------------------------------------------------
	static char flogname[128];
	static char sdate[32];
	get_sdate(sdate);

	FILE *opf;
	sprintf(flogname, "%s/limbfit_%s_%lld_%lld.log",log_dir,sdate,bfsn,efsn);
	if((opf=fopen(flogname, "w")) == NULL)
	{
		fprintf(stderr, "**Can't open the log file %s\n", flogname);
		return(ERR_EXIT);
	}
	lf_logmsg("INFO", "APP", 0, 0, "Begin... ", log_msg_code, opf);

	//prepa base name
	static char tcam[20];
	static char tfid[100];
	static char tfil[20];
	static char tbase[128];
	sprintf(tfil, "[? quality = %s ", qual);
	if (cam == 1 || cam == 2) sprintf(tcam, " and camera = %d ", cam);	else sprintf(tcam, " ");
	if (fid != 0) sprintf(tfid, " and fid = %d ", fid);	else sprintf(tfid, " ");
	//endif
	sprintf(tbase, "%s%s%s ?]", tfil,tcam,tfid);	

	//------------------------------------------------------
	// initialize global structures
	//------------------------------------------------------
	static LIMBFIT_INPUT limbfit_vars ;
	static LIMBFIT_INPUT *lfv = &limbfit_vars;
	static LIMBFIT_OUTPUT limbfit_res ;
	static LIMBFIT_OUTPUT *lfr = &limbfit_res;
	
	int * mask = (int *) malloc(sizeof(int)*IMG_SIZE);
	if(!mask) 
	{
		lf_logmsg("ERROR", "APP", ERR_MALLOC_FAILED, 0,"malloc failed (mask)", log_msg_code, opf);
		return(ERR_EXIT);
	}
	lfv->mask=mask;	
	lfv->pf_mask=&mask[0];	
	lfv->pl_mask=&mask[IMG_SIZE-1];

	lfr->code_name=CODE_NAME;
	lfr->code_version=CODE_VERSION;
	lfr->code_date=CODE_DATE;
	lfr->comment=comment;
	lfr->dsin=dsin;
	lfr->bld_vers=bld_vers;
	lfr->opf=opf;
	lfr->tmp_dir=tmp_dir;
	lfr->dsout=dsout;
	lfr->debug=debug;	
	lfr->cenx=CENTX;
	lfr->ceny=CENTY;
	lfr->r_max=R_MAX;
	lfr->r_min=R_MIN;

	// calcul mask
		double iimcmy2,jjmcmx;
		float d,r,cmx,cmy,w2p,w2m;
		long tmask_one=0;
		long tmask_zero=0;
		long ii, jj;

		// 	if fixed size
		cmx=CENTX;
		cmy=CENTY;
		w2p=R_MAX;
		w2m=R_MIN;
		r=(w2p+w2m)/2;
		
		/* 	if variable size
			{
				cmx=(float)input->ix;
				cmy=(float)input->iy;
				r=(float)input->ir;
				w2p=r+40.;
				w2m=r-40.;
			}
		*/
	
	//	if (input->is_firstobs == 0) 
	//	{
	//		input->is_firstobs=1;

//		w2p <= ann d <= w2m		

//		float w2p=r+(float)w/2;
//		float w2m=r-(float)w/2;
//		if (d<=w2p && d>=w2m) :  ann=w2m<=d<=w2p
//		if (d<w2m || d>w2p)	:	 not annulus



			/* Select Points to create the mask*/
			for(ii = 0; ii < NAXIS_ROW; ii++)
			{
				iimcmy2=(ii-cmy)*(ii-cmy);
				for(jj = 0; jj < NAXIS_COL; jj++) 
				{ 
					jjmcmx=jj-cmx;
					d=(float)sqrt(iimcmy2+(jjmcmx)*(jjmcmx));
					if (d<w2m || d>w2p)
					{
						lfv->mask[ii*NAXIS_COL+jj]=0;
						tmask_zero++;
					}
					else
					{
						lfv->mask[ii*NAXIS_COL+jj]=1;
						tmask_one++;
					}
				}
			}
			sprintf(log_msg,"Create Mask: total ann points = %ld /total points zeroed = %ld / r = %f, w2m = %f, w2p = %f, ", tmask_one,tmask_zero,r,w2m,w2p);
			lf_logmsg("INFO", "APP", 0, 0, log_msg, log_msg_code, lfr->opf);
// add some print info to check the size of the INT!!!!
	//	}

	//------------------------------------------------------
	// run it!
	//------------------------------------------------------

		//------------------------------------------------------
		// process NUMRECPERTRANS at a time during one transaction
		//------------------------------------------------------
		long long numofrecs, frec, lrec;
		int numrec, numofchunks, i;    
		numofrecs = (efsn - bfsn) + 1;
		numrec = NUMRECPERTRANS;
		numofchunks = numofrecs/numrec;
		if((numofrecs % numrec) != 0) numofchunks++; //extra loop for partial chunk
		lrec = bfsn-1;

		for(i = 0; i < numofchunks; i++) 
		{
			frec = lrec+1; 
			lrec = (frec + numrec)-1;
			if(lrec > efsn) lrec=efsn;
			sprintf(recrange, "%lld-%lld", frec, lrec);
			sprintf(open_dsname, "%s[][%s]%s", dsin, recrange,tbase);	
			
			sprintf(log_msg,"open %s -> %s (logdir: %s, tmpdir: %s, cam: %d, comment: %s , debug: %d)",
									open_dsname,dsout,log_dir,tmp_dir,cam,comment,debug);
	
			lf_logmsg("INFO", "APP", 0, 0, log_msg, log_msg_code, opf);
			if(process_n_records_fsn(open_dsname, lfv, lfr, &result)) 
			{ 
				if (result < 0 && result > -400)
				{
					lf_logmsg("ERROR", "ABORT", result, 0, "", log_msg_code, opf);
					fprintf(opf,"lfwrp abort\nSee log: %s\n", flogname); 
					fprintf(opf,"lfwrp: Restart it as %s[][%lld-%lld]%s\n", dsin,frec,efsn,tbase); 
					//send_mail("build_lev1 abort\nSee log: %s\n", logname); 
					fclose(opf);
					return(ERR_EXIT);
				}
			}
			sprintf(log_msg,"close %s", open_dsname);
			lf_logmsg("INFO", "APP", 0, 0, log_msg, log_msg_code, opf);
			drms_server_end_transaction(drms_env,0,0);
			drms_server_begin_transaction(drms_env);
		}
	free(mask);
	lf_logmsg("INFO", "APP", 0, 0, "Batch End... ", log_msg_code, opf);
	fclose(opf);
	printf("Batch end\n");

return(0);
} //end doit()
    
