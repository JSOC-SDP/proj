/*-----------------------------------------------------------------------------
 * cvs/JSOC/proj/lev0/apps/build_lev1_mgr_fsn.c
 *-----------------------------------------------------------------------------
 *
 * This is a stand alone module that processes lev0
 * filtergrams to lev1 by running the build_lev1_fsn module. 
 *
 *build_lev1_mgr_fsn
 *	instru= hmi or aia
 *	dsin= default hmi.lev0e or aia.lev0e
 *	dsout= hmi.lev1e or aia.lev1e
 *	bfsn= first lev0 fsn to process
 *	efsn= last lev0 fsn to process
 *
 * Runs build_lev1_fsn processes to create lev1 datasets
 * Has two modes:
// * Stream Mode (one instance):
// * !!NO: no stream mode for build_lev1_mgr_fsn, but code has been left
//   in in case we want to revive this.
//
// *  This is the normal quick look mode that runs continuously and
// *  keeps up with the lev0 records.
// *	brec=0, erec=0 
// *	- start at the previous highest lev0 record processed
// *	  This is keep in the DB table lev1_highest_lev0_recnum
// *	- fork from 8 to MAXCPULEV1 build_lev1_fsn for every 
// *	  17 (NUMRECLEV1) lev0 records. 
// *	- when an build_lev1_fsn completes, fork another for next 17 rec
// *	- if no more lev0 records available, sleep and try again
// *	- if 8 CPU not enough to keep up with lev0, go to 16, etc.
 *
 * Reprocessing Mode (any number of instances):
 *  This is the definitive mode lev1 processing. (!!TBD define when definitive)
 *	bfsn=1000, efsn=2000 
 *	- qsub up to 16 (MAXQSUBLEV1) build_lev1_fsn for 17 records ea
 *	- when a job completes qsub next 17 records until efsn is reached
 *	- when all jobs are done, build_lev1_mgr_fsn will exit
 *
 */ 

#include <jsoc.h>
#include <cmdparams.h>
#include <drms.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>	//for umask(2)
#include <unistd.h>	//for alarm(2) among other things...
#include <printk.h>
#include <errno.h>
#include <sys/wait.h>
#include "lev0lev1.h"	//defines NUMRECLEV1. Used by this and build_lev1_fsn.c

//default in and out data series
#define LEV0SERIESNAMEHMI "hmi.lev0e"
#define LEV0SERIESNAMEAIA "aia.lev0e"
#define LEV1SERIESNAMEHMI "su_production.hmi_lev1e"	//temp test case
#define LEV1SERIESNAMEAIA "su_production.aia_lev1e"	//temp test case

#define LEV1LOG_BASEDIR "/usr/local/logs/lev1"
#define H1LOGFILE "/usr/local/logs/lev1/build_lev1_mgr_fsn.%s.log"
#define QSUBDIR "/scr21/production/qsub"
#define NUMTIMERS 8		//number of seperate timers avail
#define MAXCPULEV1 32		//max# of forks can do at a time for stream mode
#define DEFAULTCPULEV1 "8"	//default# of forks can do at a time 
#define MAXQSUBLEV1 64  //max# of qsub can do at a time for reprocessing mode
#define DEFAULTQSUBLEV1 "16"
#define MAXJIDSTR MAXQSUBLEV1*16
#define NOTSPECIFIED "***NOTSPECIFIED***"
#define LOGTEST 0
char args7sv[128];	//used when LOGTEST = 1


int qsubjob(unsigned int rec1, unsigned int rec2);

// List of default parameter values. 
ModuleArgs_t module_args[] = { 
  {ARG_STRING, "instru", NOTSPECIFIED, "instrument. either hmi or aia"},
  {ARG_STRING, "dsin", NOTSPECIFIED, "dataset of lev0 filtergrams"},
  {ARG_STRING, "dsout", NOTSPECIFIED, "dataset of lev1 output"},
  {ARG_STRING, "logfile", NOTSPECIFIED, "optional log file name. Will create one if not given"},
  {ARG_INTS, "bfsn", "0", "first lev0 fsn to process. 0=error, must specify"},
  {ARG_INTS, "efsn", "0", "last lev0 fsn to process. 0=error, must specify"},
  {ARG_INTS, "numrec", NUMRECLEV1S, "number of lev0 to lev1 fsn at a time"},
  {ARG_INTS, "numcpu", DEFAULTCPULEV1, "max# of forks to do at a time for stream mode"},
  {ARG_INTS, "numqsub", DEFAULTQSUBLEV1, "max# of qsub to do at a time for reprocessing mode"},
  {ARG_FLAG, "v", "0", "verbose flag"},
  {ARG_FLAG, "h", "0", "help flag"},
  {ARG_END}
};

ModuleArgs_t *gModArgs = module_args;
CmdParams_t cmdparams;
// Module name presented to DRMS. 
char *module_name = "build_lev1_mgr_fsn";

FILE *h1logfp;		// fp for h1 ouput log for this run 
FILE *qsubfp;		// fp for qsub script
static char datestr[32];
static char open_dsname[256];
static struct timeval first[NUMTIMERS], second[NUMTIMERS];

pid_t mypid;
uint64_t jid;
int verbose;
unsigned int bfsn, efsn;	//begin and end lev0 fsn to do
long long lastrecnum0_now = 0;	//last RT lev0 recnum seen
long long lastrecnum0_prev = 0;	//prvious RT lev0 recnum seen
int numrec, numcpu, numqsub;
int qcnt = 0;
//stream_mode is also quick look mode, i.e. brec=erec=0 (Obsolete)
int stream_mode = 0;		//0=qsub build_lev1_fsn, 1=fork it locally
int hmiaiaflg = 0;		//0=hmi, 1=aia
int loopcnt = 0;		//force last lev0 records to lev1
char logname[128];
char argdsin[128], argdsout[128], arglogfile[128], arginstru[80];
char argbfsn[80], argefsn[80], argnumrec[80], argnumcpu[80], argnumqsub[80];
char timetag[32];
char *username;			// from getenv("USER") 
char *logfile;			// optional log name passed in 
char *instru;			// instument. hmi or aia 
char *dsin;			// lev0 input dataset
char *dsout;			// lev1 output dataset


int nice_intro ()
{
  int usage = cmdparams_get_int (&cmdparams, "h", NULL);
  if (usage)
    {
    printf ("Runs build_lev1_fsn processes to create lev1 datasets.\n\n");
    printf ("Usage: build_lev1_mgr_fsn [-vh]\n"
	"instru=<hmi|aia> dsin=<lev0> dsout=<lev1> bfsn=<fsn#> efsn=<fsn#>"
	"\nnumcpu=<#> numqsub=<#> logfile=<file>\n"
	"  -h: help - show this message then exit\n"
	"  -v: verbose\n"
	"instru= instrument. must be 'hmi' or 'aia'\n"
	"dsin= data set name of lev0 input\n"
	"      default hmi=hmi.lev0e   aia=aia.lev0e\n"
	"dsout= data set name of lev1 output\n"
	"      default hmi=su_production.hmi_lev1e   aia=su_production.aia_lev1e\n"
	"bfsn= first lev0 fsn to process. 0=error\n"
	"efsn= last lev0 fsn to process. 0=error\n"
	"numcpu= max# of forks to do at a time for stream mode. Default %s\n"
	"numqsub= max# of qsub to do at a time for reprocessing mode. Default %s\n"
	"logfile= optional log file name. If not given uses:\n"
        "         /usr/local/logs/lev1/build_lev1_mgr_fsn.<time_stamp>.log\n",
	 DEFAULTCPULEV1, DEFAULTQSUBLEV1);
     printf ("\n * Has two modes:\n"
          "!!NOTE: Stream Mode has been depricated!!\n"
          " * Stream Mode (one instance):\n"
          " *  This is the normal quick look mode that runs continuously and\n"
          " *  keeps up with the lev0 records.\n"
          " *	brec=0, erec=0\n"
          " *	- start at the previous highest lev0 record processed\n"
          " *	  This is keep in the DB table lev1_highest_lev0_recnum\n"
          " *	- fork up to 8 (MAXCPULEV1) build_lev1_fsn for every \n"
          " *	  17 (NUMRECLEV1) lev0 records. \n"
          " *	- when an build_lev1_fsn completes, fork another for next 17 rec\n"
          " *	- if no more lev0 records available, sleep and try again\n"
          " *	- if 8 CPU not enough to keep up with lev0, go to 16, etc.\n"
          " *\n"
          " * Reprocessing Mode (any number of instances):\n"
          " *  This is the definitive mode lev1 processing.\n"
          " *	bfsn=1000, efsn=2000 \n"
          " *	- qsub up to 16 (MAXQSUBLEV1) build_lev1_fsn for 17 records ea\n"
          " *	- when a job completes qsub next 17 records until efsn is reached\n"
          " *	- when all jobs are done, build_lev1_mgr_fsn will exit\n\n");
    return(1);
    }
  verbose = cmdparams_get_int (&cmdparams, "v", NULL);
  return (0);
}

/* Return pointer to "Wed Jun 30 21:49:08 1993\n" */
char *get_datetime()
{
  struct timeval tvalr;
  struct tm *t_ptr;
  static char datestr[32];

  gettimeofday(&tvalr, NULL);
  t_ptr = localtime((const time_t *)&tvalr.tv_sec);
  sprintf(datestr, "%s", asctime(t_ptr));
  return(datestr);
}

// Setup global datestr[] like: 2008.07.14_08:29:31
char *do_datestr() {
  time_t tval;
  struct tm *t_ptr;

  tval = time(NULL);
  t_ptr = localtime(&tval);
  sprintf(datestr, "%d.%02d.%02d_%02d:%02d:%02d", 
	  (t_ptr->tm_year+1900), (t_ptr->tm_mon+1),
	  t_ptr->tm_mday, t_ptr->tm_hour, t_ptr->tm_min, t_ptr->tm_sec);
  return(datestr);
}

// Returns a time tag like  yyyy.mm.dd.hhmmss 
char *gettimetag()
{
  struct timeval tvalr;
  struct tm *t_ptr;

  gettimeofday(&tvalr, NULL);
  t_ptr = localtime((const time_t *)&tvalr);
  sprintf(timetag, "%04d.%02d.%02d.%02d%02d%02d",
        (t_ptr->tm_year+1900), (t_ptr->tm_mon+1), t_ptr->tm_mday, t_ptr->tm_hour, t_ptr->tm_min, t_ptr->tm_sec);
  return(timetag);
}


void BeginTimer(int n)
{
  gettimeofday (&first[n], NULL);
}

float EndTimer(int n)
{
  gettimeofday (&second[n], NULL);
  if (first[n].tv_usec > second[n].tv_usec) {
    second[n].tv_usec += 1000000;
    second[n].tv_sec--;
  }
  return (float) (second[n].tv_sec-first[n].tv_sec) +
    (float) (second[n].tv_usec-first[n].tv_usec)/1000000.0;
}

// Outputs the variable format message (re: printf) to the log file.
int h1log(const char *fmt, ...)
{
  va_list args;
  char string[32768];

  va_start(args, fmt);
  vsprintf(string, fmt, args);
  if(h1logfp) {
    fprintf(h1logfp, string);
    fflush(h1logfp);
  } 
  else {			// couldn't open log 
    printf(string);		// also print to stdout
    fflush(stdout);
  }
  va_end(args);
  return(0);
}

int send_mail(char *fmt, ...)
{
  va_list args;
  char string[1024], cmd[1024];

  va_start(args, fmt);
  vsprintf(string, fmt, args);
  sprintf(cmd, "echo \"%s\" | Mail -s \"build_lev1_fsn mail\" lev0_user", string);
  system(cmd);
  va_end(args);
  return(0);
}

// Got a fatal error. 
void abortit(int stat)
{
  char pcmd[128];

  printk("***Abort in progress ...\n");
  printk("**Exit build_lev1_mgr_fsn w/ status = %d\n", stat);
  sprintf(pcmd, "/bin/rm %s/build_lev1_mgr_fsn.stream.touch 2>/dev/null",
              LEV1LOG_BASEDIR);
  system(pcmd);
  if (h1logfp) fclose(h1logfp);
  exit(stat);
}

/* This is stream mode processing that will keep on processing lev0
 * records as they show up in the DB.
 * Process the lev0 to lev1 from recn0 to maxrecn0. 
 * Returns when all children processes are done. 
 * Note: The processing is done in sets of 17 (NUMRECLEV1) lev0 records, 
 * so the maxrecn0 may not be reached, but it will
 * get done with the next set when more lev0 records come in. forkstream()
 * is run again and will automatically process new lev0 records in
 * sets of 17 as they are seen in the DB.
 * Returns non-0 on error.
 * If force is set, then do any non-chunk size of lev0 records left.
*/
int forkstream(long long recn0, long long maxrecn0, int force)
{
  pid_t pid, wpid, fpid[MAXCPULEV1];
  unsigned int numofrecs, frec, lrec;
  int stat_loc, i, j, k, l, numtofork;
  char *args[9], pcmd[128];
  char args1[128], args2[128], args3[128], args4[128], args5[128], args6[128];
  char args7[128];

  numofrecs = (maxrecn0 - recn0) + 1;
  numtofork = numofrecs/numrec;     //this many to fork 
  j = numtofork;
  if(j == 0) {
   if(force) { j=numtofork=1; }
   else return(0);
  }
  lrec = recn0-1;
  if(j < numcpu) l = j;		//fork less then the max at a time
  else l = numcpu;			//fork the max at a time
  if(LOGTEST) {
    sprintf(args7sv, "logfile=%s/build_lev1_fsn.%s.log", 
		LEV1LOG_BASEDIR, gettimetag());
  }
  for(k=0; k < l; k++) { //fork this many to start 
    if(force) { frec = lrec+1; lrec = (frec + numofrecs)-1; }
    else { frec = lrec+1; lrec = (frec + numrec)-1; }
    if((pid = fork()) < 0) {
      printk("***Can't fork(). errno=%d\n", errno);
      return(1);			//!!TBD decide what to do
    }
    else if(pid == 0) {                   //this is the beloved child
      args[0] = "build_lev1_fsn";
      sprintf(args1, "dsin=%s", dsin);
      args[1] = args1;
      sprintf(args2, "dsout=%s", dsout);
      args[2] = args2;
      sprintf(args3, "bfsn=%u", frec);
      args[3] = args3;
      sprintf(args4, "efsn=%u", lrec);
      args[4] = args4;
      sprintf(args5, "instru=%s", instru);
      args[5] = args5;
      sprintf(args6, "quicklook=%d", stream_mode);
      args[6] = args6;
      //sprintf(args7, "logfile=%s/l1_%s_%d.log", QSUBDIR, gettimetag(), k);
      if(LOGTEST) {
        sprintf(args7, "%s", args7sv);
      }
      else {
        sprintf(args7, "logfile=%s/l1s_%u_%u.log", 
		LEV1LOG_BASEDIR, frec, lrec);
      }
      args[7] = args7;
      args[8] = NULL;
      printk("execvp: %s %s %s %s %s %s %s %s\n",
              args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]);
      if(execvp(args[0], args) < 0) {
        printk("***Can't execvp() build_lev1_fsn. errno=%d\n", errno);
        exit(1);
      }
    }
    --numtofork;
    printf("forked pid = %d\n", pid);
    fpid[k] = pid;
  }
  wpid = -1;                  //wait for any child
  while(1) {
    //don't block and report any status not yet reported
    pid = waitpid(wpid, &stat_loc, WNOHANG+WUNTRACED);
    //pid = waitpid(wpid, &stat_loc, WNOHANG);
    //printf("!!TEMP waitpid returned %d stat_loc=%d\n", pid, stat_loc);
    if(pid == 0) { sleep(5); continue; }  //nothing ready
    if(pid == -1) {
      if(errno == ECHILD) printf("!!No More Children\n");errno=0;
      //!!TBD assumes we catch up at some point and we know where we're at
      //now and can update the DB table. Check that this is ok.
      //now update lev1_highest_lev0_recnum table with lrec
      sprintf(pcmd, "echo \"update lev1_highest_lev0_recnum set lev0recnum=%lld, date='%s' where lev0series='%s'\" | psql -h hmidb jsoc", lrec, get_datetime(), dsin);
      system(pcmd);

      if(numtofork <= 0) break;
    }
    else {
      for(k=0; k < numcpu; k++) {        //make sure a good one replies and
        if(fpid[k] == pid) { break; }	//don't have to worry about wraparound
      }
      if(k == numcpu) continue;	//extraneous pid get on first wait
      printf("returned pid = %d stat_loc = %d\n", pid, stat_loc);
      if(numtofork == 0) continue;	//find out no more children
    }
    //fork one more
    frec = lrec+1; lrec = (frec + numrec)-1;
    if((pid = fork()) < 0) {
      printk("***Can't fork(). errno=%d\n", errno);
      return(1);			//!!TBD decide what to do
    }
    else if(pid == 0) {                   //this is the beloved child
      args[0] = "build_lev1_fsn";
      sprintf(args1, "dsin=%s", dsin);
      args[1] = args1;
      sprintf(args2, "dsout=%s", dsout);
      args[2] = args2;
      sprintf(args3, "bfsn=%u", frec);
      args[3] = args3;
      sprintf(args4, "efsn=%u", lrec);
      args[4] = args4;
      sprintf(args5, "instru=%s", instru);
      args[5] = args5;
      sprintf(args6, "quicklook=%d", stream_mode);
      args[6] = args6;
      //sprintf(args7, "logfile=%s/l1_%s_%d.log", QSUBDIR, gettimetag(), k);
      if(LOGTEST) {
        sprintf(args7, "%s", args7sv);	//use original log name
      }
      else {
        sprintf(args7, "logfile=%s/l1s_%u_%u.log", LEV1LOG_BASEDIR, frec, lrec);
      }
      args[7] = args7;
      args[8] = NULL;
      printk("execvp: %s %s %s %s %s %s %s %s\n",
              args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7]);
      if(execvp(args[0], args) < 0) {
        printk("***Can't execvp() build_lev1_fsn. errno=%d\n", errno);
        exit(1);
      }
    }
    --numtofork;
    printf("forked pid = %d\n", pid);
    fpid[k] = pid;
  }
  return(0);
}

//Start a qsub job. Set the global jid.
int qsubjob(unsigned int rec1, unsigned int rec2)
{
  FILE *fin;
  char astr[32], bstr[32], string[128], qlogname[128], qsubcmd[512];;
  char recrange[128];

  sprintf(recrange, "%u-%u", rec1, rec2);
  sprintf(open_dsname, "%s[%s]", dsin, recrange);
  printk("open_dsname = %s\n", open_dsname); //!!TEMP
  sprintf(qlogname, "%s/qsub_%s_%d.csh", QSUBDIR, username, qcnt++);
  if((qsubfp=fopen(qlogname, "w")) == NULL) {
    fprintf(stderr, "**Can't open the qsub log file %s\n", qlogname);
    return(1);		//!!TBD
  }
  fprintf(qsubfp, "#!/bin/csh\n");
  fprintf(qsubfp, "echo \"TMPDIR = $TMPDIR\"\n");
  fprintf(qsubfp, "build_lev1_fsn dsin=%s dsout=%s bfsn=%u efsn=%u instru=%s quicklook=%d logfile=%s/l1q_b%u_e%u_$JOB_ID.log\n", 
	dsin, dsout, rec1, rec2, instru, stream_mode, QSUBDIR, rec1, rec2); 
  fclose(qsubfp);
  sprintf(qsubcmd, "qsub -o %s -e %s -q j.q %s", 
		QSUBDIR, QSUBDIR, qlogname);
  //sprintf(qsubcmd, "qsub -q j.q %s", qlogname);
  printf("%s\n", qsubcmd);
  printk("%s\n", qsubcmd);
  sprintf(qsubcmd, "%s | grep \"Your job\"", qsubcmd);
  fin = popen(qsubcmd, "r");
  while(fgets(string, sizeof string, fin)) {  //get qsub return line
    sscanf(string, "%s %s %d", astr, bstr, &jid); /* get job_id */
  }
  pclose(fin);
  printf("\$JOB_ID = %u\n", jid);
  return(0);
}
 
int qsubmode(unsigned int frec, unsigned int lrec)
{
  FILE *fin;
  char qsubcmd[512], string[128];
  char astr[32], bstr[32], jidstr[MAXJIDSTR];
  uint64_t qjid[MAXQSUBLEV1], qstatjid[MAXQSUBLEV1];
  unsigned int numofrecs, rfirst, rlast;
  int numtoqsub, i, j, l, k, found, status;
  int jobdone=0;

  numofrecs = (lrec - frec) + 1;
  numtoqsub = numofrecs/numrec;     //this many to qsub
  if(numofrecs % numrec) numtoqsub++;
  j = numtoqsub;			//0 implies one to qsub
  rlast = frec-1;
  if(j < numqsub) l = j;            //qsub less then the max at a time
  else l = numqsub;                  //qsub the max at a time
  for(k=0; k < l; k++) { //qsub this many to start 
    rfirst = rlast+1; rlast = (rfirst + numrec)-1;
    if(rlast > lrec) rlast = lrec;	//don't go past end
    status = qsubjob(rfirst, rlast);	//schedule the qsub job. set jid
    --numtoqsub;
    qjid[k] = jid;
    if(k == 0) sprintf(jidstr, "%u", jid);
    else sprintf(jidstr, "%s,%u", jidstr, jid);
  }
  //printf("jidstr = %s\n", jidstr);	//!!TEMP
  printf("numtoqsub left = %d\n", numtoqsub); //!!TEMP
  //sprintf(qsubcmd, "qstat -j %s 2>/dev/null | grep \"job_number:\"", jidstr);
  sprintf(qsubcmd, "qstat -u production | grep \"qsub_prod\"");
  while(1) {
    //printf("\ncmd: %s\n", qsubcmd);	//!!TEMP
    if(!(fin = popen(qsubcmd, "r"))) {
      printf("!!!Fatal Error: can't do %s\n", qsubcmd);
      return(1);	//!!TBD check
    }
    //sleep(12);
    found = 0; k = 0;
    while(fgets(string, sizeof string, fin)) {  //get qstat return line
      //sscanf(string, "%s %u", astr, &jid);	// get job_id 
      sscanf(string, "%u", &jid);		// get job_id 
      printf("job id from qstat = %u\n", jid);
      qstatjid[k++] = jid;
      found = 1;
    }
    pclose(fin);

    //now see if any of the started jobs are done
    for(i=0; i < l; i++) {
      for(j=0; j < k; j++) {
        if(qjid[i] == qstatjid[j]) { //job still active
          break;
        }
      }
      if(j == k) {		//job done. start a new one
        if(qjid[i] != 0) {
          printf("Job done jid=%u\n", qjid[i]);
          jobdone++;
          qjid[i] = 0;
        }
        if(numtoqsub) {
          //start_new_qusb
          rfirst = rlast+1; rlast = (rfirst + numrec)-1;
          if(rlast > lrec) rlast = lrec;	//don't go past end
          status = qsubjob(rfirst, rlast);	//schedule the qsub job
          --numtoqsub;
          found = 1;			//job to be found again
          qjid[i] = jid;
          //if(k == 0) sprintf(jidstr, "%u", jid);
          //else sprintf(jidstr, "%s,%u", jidstr, jid);
        }
        //else break;		//all done
      }
    }
    for(i=0; i < l; i++) {
      if(i == 0) sprintf(jidstr, "%u", qjid[i]);
      else sprintf(jidstr, "%s,%u", jidstr, qjid[i]);
    }
    printf("\n");
    if(!found) break;
    sleep(3);
  }
  printf("All jobs done = %d. numtoqsub = %d\n", jobdone, numtoqsub);
  return(0);
}

/* Create lev1 from lev0 records in either stream mode or 
 * reprocessing mode. Return non-0 on error.
 * In stream mode force any non-chunk size of lev0 records at the
 * end to lev1.
*/
int do_ingest(int force)
{
  FILE *fin;
  int rstatus;
  long long recnum0, maxrecnum0;
  char string[128], pcmd[128];

  //!!NOTE: stream mode has been depricated
  if(stream_mode) {		//start past last lev0 rec# processed 
    sprintf(pcmd, "echo \"select lev0recnum from lev1_highest_lev0_recnum where lev0series='%s'\" | psql -h hmidb jsoc", dsin);
    fin = popen(pcmd, "r");
    while(fgets(string, sizeof string, fin)) {  //get psql return line
      if(strstr(string, "lev0recnum")) continue;
      if(strstr(string, "-----")) continue;
      //get lev0 rec#
      if((rstatus = sscanf(string, "%lld", &recnum0)) == 0) {
        printf("Abort no lev0 entry in lev1_highest_lev0_recnum\n");
        printk("Abort no lev0 entry in lev1_highest_lev0_recnum\n");
        abortit(1); //no rec#
      }
      recnum0++;				//start at next rec#
      break;
    }
    pclose(fin);
    sprintf(pcmd, "echo \"select max(recnum) from %s\" | psql -h hmidb jsoc", dsin);
    fin = popen(pcmd, "r");
    while(fgets(string, sizeof string, fin)) {  //get psql return line
      if(strstr(string, "max")) continue;
      if(strstr(string, "-----")) continue;
      if(!strcmp(string, "    \n")) { //new series w/no recnum
        printf("Abort no max lev0 recnum (new series?)\n");
        printk("Abort no max lev0 recnum (new series?)\n");
        abortit(1);
      }
      sscanf(string, "%lld", &maxrecnum0);
      //maxrecnum0 = maxrecnum0 - 25;	//allow time for commit of lev0 
      maxrecnum0 = maxrecnum0 - 40;	//allow (more) time for commit of lev0 
      lastrecnum0_prev = lastrecnum0_now;
      lastrecnum0_now = maxrecnum0;		//save to see if more come in
      break;
    }
    pclose(fin);
    printf("Stream Mode starting at lev0 recnum = %lld maxrecnum = %lld\n", 
		recnum0, maxrecnum0);
    if(recnum0 > maxrecnum0) return(0);	//nothing to do. go wait
    rstatus = forkstream(recnum0, maxrecnum0, force);
  }
  else {
    //reprocessing mode. use bfsn/efsn and qsub build_lev1_fsn programs
    rstatus = qsubmode(bfsn, efsn);
  }
  return(rstatus);
}

void sighandler(sig)
  int sig;
{
  char pcmd[128];
  if(sig == SIGTERM) {
    printf("*** %s build_lev1_mgr_fsn got SIGTERM. Exiting.\n", datestr);
    printk("*** %s build_lev1_mgr_fsn got SIGTERM. Exiting.\n", datestr);
    sprintf(pcmd, "/bin/rm %s/build_lev1_mgr_fsn.stream.touch 2>/dev/null",
                LEV1LOG_BASEDIR);
    system(pcmd);
    exit(1);
  }
  if(sig == SIGINT) {
    printf("*** %s build_lev1_mgr_fsn got SIGINT. Exiting.\n", datestr);
    printk("*** %s build_lev1_mgr_fsn got SIGINT. Exiting.\n", datestr);
    sprintf(pcmd, "/bin/rm %s/build_lev1_mgr_fsn.stream.touch 2>/dev/null",
                LEV1LOG_BASEDIR);
    system(pcmd);
    exit(1);
  }
  printk("*** %s build_lev1_mgr_fsn got an illegal signal %d, ignoring...\n",
                        datestr, sig);
  if (signal(SIGINT, SIG_IGN) != SIG_IGN)
      signal(SIGINT, sighandler);
  if (signal(SIGALRM, SIG_IGN) != SIG_IGN)
      signal(SIGALRM, sighandler);
}

// Initial setup stuff called when main is first entered.
void setup()
{
  FILE *fin;
  char string[128], cwdbuf[128], idstr[256], lfile[128];
  int tpid;

  do_datestr();
  printk_set(h1log, h1log);	// set for printk calls 
  printk("%s\n", datestr);
  getcwd(cwdbuf, 126);
  mypid = getpid();
  sprintf(idstr, "Cwd: %s\nCall: ", cwdbuf);
  sprintf(string, "build_lev1_mgr_fsn started as pid=%d user=%s\n", mypid, username);
  strcat(idstr, string);
  printk("%s", idstr);
  printf("%s", idstr);
  sprintf(arginstru, "instru=%s", instru);
  sprintf(argdsin, "dsin=%s", dsin);
  sprintf(argdsout, "dsout=%s", dsout);
  sprintf(argbfsn, "bfsn=%u", bfsn);
  sprintf(argefsn, "efsn=%u", efsn);
  sprintf(argnumrec, "numrec=%d", numrec);
  sprintf(argnumcpu, "numcpu=%d", numcpu);
  sprintf(argnumqsub, "numqsub=%d", numqsub);
  sprintf(arglogfile, "logfile=%s", logname);
  printk("%s %s %s %s %s %s %s %s\n", arginstru, argdsin, argdsout, argbfsn, argefsn, argnumrec, argnumcpu, argnumqsub);
  printf("%s %s %s %s %s %s %s %s\n", arginstru, argdsin, argdsout, argbfsn, argefsn, argnumrec, argnumcpu, argnumqsub);
  if (signal(SIGINT, SIG_IGN) != SIG_IGN)
      signal(SIGINT, sighandler);
  if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
      signal(SIGTERM, sighandler);
  if (signal(SIGALRM, SIG_IGN) != SIG_IGN)
      signal(SIGALRM, sighandler);
  sprintf(idstr,  "ps -ef | grep %s", LEV1VIEWERNAME);
  fin = popen(idstr, "r");
  while(fgets(string, sizeof string, fin)) {  //get ps line
    if(!(strstr(string, "perl"))) continue;
    sscanf(string, "%s %d", idstr, &tpid); // get user name & process id
    sprintf(lfile, "%s/build_lev1_mgr_restart_fsn_%d.touch", LEV1LOG_BASEDIR, tpid);
    sprintf(idstr, "/bin/touch %s", lfile);
    printk("%s\n", idstr);
    system(idstr);
  }
  umask(002);			// allow group write 
}

// Module main function. 
int main(int argc, char **argv)
{
  FILE *fin;
  int wflg = 1;
  char line[128], pcmd[128];

  if (cmdparams_parse(&cmdparams, argc, argv) == -1)
  {
     fprintf(stderr,"Error: Command line parsing failed. Aborting.\n");
     return 1;
  }
  if (nice_intro())
    return (0);
  if(!(username = (char *)getenv("USER"))) username = "nouser"; 
  instru = cmdparams_get_str(&cmdparams, "instru", NULL);
  if(strcmp(instru, "hmi") && strcmp(instru, "aia")) {
    printf("Error: instru= must be given as 'hmi' or 'aia'\n");
    return(0);
  }
  if(!strcmp(instru, "aia")) hmiaiaflg = 1;
  dsin = cmdparams_get_str(&cmdparams, "dsin", NULL);
  dsout = cmdparams_get_str(&cmdparams, "dsout", NULL);
  bfsn = cmdparams_get_int(&cmdparams, "bfsn", NULL);
  efsn = cmdparams_get_int(&cmdparams, "efsn", NULL);
  numrec = cmdparams_get_int(&cmdparams, "numrec", NULL);
  numcpu = cmdparams_get_int(&cmdparams, "numcpu", NULL);
  numqsub = cmdparams_get_int(&cmdparams, "numqsub", NULL);
  if(numcpu > MAXCPULEV1) {
    printf("numcpu exceeds max of %d\n", MAXCPULEV1);
    return(0);
  }
  if(numqsub > MAXQSUBLEV1) {
    printf("numqsub exceeds max of %d\n", MAXQSUBLEV1);
    return(0);
  }
  if(bfsn == 0 && efsn == 0) {
    //make sure there isn't a stream mode already running
    sprintf(pcmd, "ls %s/build_lev1_mgr_fsn.stream.touch 2>/dev/null", 
		LEV1LOG_BASEDIR);
    fin = popen(pcmd, "r");
    while(fgets(line, sizeof line, fin)) {  //get ps return line
      printf("Error: build_lev1_mgr_fsn already running.");
      printf(" If not so, do:\n");
      printf("/bin/rm %s/build_lev1_mgr_fsn.stream.touch\n", LEV1LOG_BASEDIR);
      pclose(fin);
      return(0);
    }
    pclose(fin);
    sprintf(pcmd, "touch %s/build_lev1_mgr_fsn.stream.touch 2>/dev/null",
                LEV1LOG_BASEDIR);
    system(pcmd);
    stream_mode = 1;		//aka quick look mode
  }
  logfile = cmdparams_get_str(&cmdparams, "logfile", NULL);
  if (strcmp(dsin, NOTSPECIFIED) == 0) {
    if(hmiaiaflg == 0) dsin = LEV0SERIESNAMEHMI;
    else dsin = LEV0SERIESNAMEAIA;
  }
  if (strcmp(dsout, NOTSPECIFIED) == 0) {
    if(hmiaiaflg == 0) dsout = LEV1SERIESNAMEHMI;
    else dsout = LEV1SERIESNAMEAIA;
  }
  if(hmiaiaflg) {
    if(strstr(dsin, "hmi") || strstr(dsout, "hmi")) {
      printf("Warning: You said instru=aia but have 'hmi' in ds name?\n");
      printf("Do you want to abort this [y/n]? ");
      if(gets(line) == NULL) { return(0); }
      if(strcmp(line, "n")) { return(0); }
    }
  }
  else {
    if(strstr(dsin, "aia") || strstr(dsout, "aia")) {
      printf("Warning: You said instru=hmi but have 'aia' in ds name?\n");
      printf("Do you want to abort this [y/n]? ");
      if(gets(line) == NULL) { return(0); }
      if(strcmp(line, "n")) { return(0); }
    }
  }
  if (strcmp(logfile, NOTSPECIFIED) == 0) {
    sprintf(logname, H1LOGFILE, gettimetag());
  }
  else {
    sprintf(logname, "%s", logfile);
  }
  if((h1logfp=fopen(logname, "w")) == NULL)
    fprintf(stderr, "**Can't open the log file %s\n", logname);
  setup();
  while(wflg) {
    if(do_ingest(0)) {        // loop to get files from the lev0
      printk("**ERROR: in do_ingest() for %s\n", open_dsname);
    }
    if(!stream_mode) return(0); //all done for reprocessing
    sleep(30);		//wait for more lev0 to appear
    if(lastrecnum0_now == lastrecnum0_prev) {  //no new lev0 in
      if(loopcnt++ == 15) {  //force any left over lev0 records to lev1
        printk("Timeout: force any left over lev0 to lev1\n");
        if(do_ingest(1)) {        // process the last lev0 records
          printk("**ERROR: in do_ingest() for %s\n", open_dsname);
        }
        loopcnt = 0;
      }
    }
    else loopcnt = 0;
    //wflg = 0;
  }
  sprintf(pcmd, "/bin/rm %s/build_lev1_mgr_fsn.stream.touch 2>/dev/null",
              LEV1LOG_BASEDIR);
  system(pcmd);
  return(0);
}

