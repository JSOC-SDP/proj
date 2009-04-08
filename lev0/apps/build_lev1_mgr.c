/*-----------------------------------------------------------------------------
 * cvs/JSOC/proj/lev0/apps/build_lev1_mgr.c
 *-----------------------------------------------------------------------------
 *
 * This is a stand alone module that processes lev0
 * filtergrams to lev1 by running the build_lev1 module. 
 *
 *build_lev1_mgr
 *	instru= hmi or aia
 *	dsin= default hmi.lev0e or aia.lev0e
 *	dsout= hmi.lev1e or aia.lev1e
 *	brec= first lev0 record# to process
 *	erec= last lev0 record# to process
 *
 * Runs build_lev1 processes to create lev1 datasets
 * Has two modes:
 * Stream Mode (one instance)
 *	brec=0, erec=0 
 *	- start at the previous highest lev0 record processed
 *	  This is keep in the DB table lev1_highest_lev0_recnum
 *	- fork up to 8 (MAXCPULEV1) build_lev1 for every 
 *	  12 (MAXRECLEV1) lev0 records. 
 *	- when an build_lev1 completes, fork another for next 12 rec
 *	- if no more lev0 records available, sleep and try again
 *	- if 8 CPU not enough to keep up with lev0, go to 16, etc.
 *
 * Reprocessing Mode (any number of instances)
 *	brec=1000, erec=2000 
 *	- qsub up to 16 (MAXQSUBLEV1) build_lev1 for 12 records ea
 *	- when a job completes qsub next 12 records until erec is reached
 *	- when all jobs are done, build_lev1_mgr will exit
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


//default in and out data series
#define LEV0SERIESNAMEHMI "hmi.lev0e"
#define LEV0SERIESNAMEAIA "aia.lev0e"
#define LEV1SERIESNAMEHMI "su_production.hmi_lev1e"	//temp test case
#define LEV1SERIESNAMEAIA "su_production.aia_lev1e"	//temp test case

#define LEV1LOG_BASEDIR "/usr/local/logs/lev1"
#define H1LOGFILE "/usr/local/logs/lev1/build_lev1_mgr.%s.log"
#define QSUBDIR "/scr21/production/qsub"
#define NUMTIMERS 8		//number of seperate timers avail
#define MAXRECLEV1 12	//number of lev0 to lev1 images to do at a time
#define MAXCPULEV1 8	//max# of forks to do at a time for stream mode
#define MAXQSUBLEV1 16	//max# of qsub to do at a time for reprocessing mode
#define MAXJIDSTR MAXQSUBLEV1*16
#define NOTSPECIFIED "***NOTSPECIFIED***"

int qsubjob(long long rec1, long long rec2);

// List of default parameter values. 
ModuleArgs_t module_args[] = { 
  {ARG_STRING, "instru", NOTSPECIFIED, "instrument. either hmi or aia"},
  {ARG_STRING, "dsin", NOTSPECIFIED, "dataset of lev0 filtergrams"},
  {ARG_STRING, "dsout", NOTSPECIFIED, "dataset of lev1 output"},
  {ARG_STRING, "logfile", NOTSPECIFIED, "optional log file name. Will create one if not given"},
  {ARG_INTS, "brec", "0", "first lev0 rec# to process"},
  {ARG_INTS, "erec", "0", "last lev0 rec# to process"},
  {ARG_FLAG, "v", "0", "verbose flag"},
  {ARG_FLAG, "h", "0", "help flag"},
  {ARG_END}
};

ModuleArgs_t *gModArgs = module_args;
CmdParams_t cmdparams;
// Module name presented to DRMS. 
char *module_name = "build_lev1_mgr";

FILE *h1logfp;		// fp for h1 ouput log for this run 
FILE *qsubfp;		// fp for qsub script
static char datestr[32];
static char open_dsname[256];
static struct timeval first[NUMTIMERS], second[NUMTIMERS];

uint64_t jid;
int verbose;
long long brec, erec;		//begin and end lev0 rec# to do
int qcnt = 0;
int stream_mode = 0;		//0=qsub build_lev1, 1=fork it locally
int hmiaiaflg = 0;		//0=hmi, 1=aia
char logname[128];
char argdsin[128], argdsout[128], arglogfile[128], arginstru[80];
char argbrec[80], argerec[80];
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
    printf ("Usage: build_lev1_mgr [-vh]\n"
	"instru=<hmi|aia> dsin=<lev0> dsout=<lev1> brec=<rec#> erec=<rec#>"
	"\n[logfile=<file>]\n"
	"  -h: help - show this message then exit\n"
	"  -v: verbose\n"
	"instru= instrument. must be 'hmi' or 'aia'\n"
	"dsin= data set name of lev0 input\n"
	"      default hmi=hmi.lev0e   aia=aia.lev0e\n"
	"dsout= data set name of lev1 output\n"
	"      default hmi=su_production.hmi_lev1e   aia=su_production.aia_lev1e\n"
	"brec= first lev0 rec# to process. 0=Stream Mode if erec also 0\n"
	"erec= last lev0 rec# to process. 0=Stream Mode if brec also 0\n"
	"logfile= optional log file name. If not given uses:\n"
        "         /usr/local/logs/lev1/build_lev1_mgr.<time_stamp>.log\n");
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
  sprintf(cmd, "echo \"%s\" | Mail -s \"build_lev1 mail\" lev0_user", string);
  system(cmd);
  va_end(args);
  return(0);
}

// Got a fatal error. 
void abortit(int stat)
{
  printk("***Abort in progress ...\n");
  printk("**Exit build_lev1_mgr w/ status = %d\n", stat);
  if (h1logfp) fclose(h1logfp);
  exit(stat);
}

/* This is stream mode processing that will keep on processing lev0
 * records as they show up in the DB.
 * Process the lev0 to lev1 from recn0 to maxrecn0. 
 * Returns when all children processes are done. 
 * Note: The processing is done in sets of 12 (MAXRECLEV1) lev0 records, 
 * so the maxrecn0 may not be reached, but it will
 * get done with the next set when more lev0 records come in. forkstream()
 * is run again and will automatically process new lev0 records in
 * sets of 12 as they are seen in the DB.
 * Returns non-0 on error.
*/
int forkstream(long long recn0, long long maxrecn0)
{
  pid_t pid, wpid, fpid[MAXCPULEV1];
  long long numofrecs, frec, lrec;
  int stat_loc, i, j, k, l, numtofork;
  char *args[8], pcmd[128];
  char args1[128], args2[128], args3[128], args4[128], args5[128], args6[128];

  numofrecs = (maxrecn0 - recn0) + 1;
  numtofork = numofrecs/MAXRECLEV1;     //this many to fork 
  j = numtofork;
  if(j == 0) return(0);
  lrec = recn0-1;
  if(j < MAXCPULEV1) l = j;		//fork less then the max at a time
  else l = MAXCPULEV1;			//fork the max at a time
  for(k=0; k < l; k++) { //fork this many to start 
    frec = lrec+1; lrec = (frec + MAXRECLEV1)-1;
    if((pid = fork()) < 0) {
      printk("***Can't fork(). errno=%d\n", errno);
      return(1);			//!!TBD decide what to do
    }
    else if(pid == 0) {                   //this is the beloved child
      args[0] = "build_lev1";
      sprintf(args1, "dsin=%s", dsin);
      args[1] = args1;
      sprintf(args2, "dsout=%s", dsout);
      args[2] = args2;
      sprintf(args3, "brec=%lu", frec);
      args[3] = args3;
      sprintf(args4, "erec=%lu", lrec);
      args[4] = args4;
      sprintf(args5, "instru=%s", instru);
      args[5] = args5;
      //sprintf(args6, "logfile=%s/l1_%s_%d.log", QSUBDIR, gettimetag(), k);
      sprintf(args6, "logfile=%s/l1s_%lu_%lu.log", QSUBDIR, frec, lrec);
      args[6] = args6;
      args[7] = NULL;
      printk("execvp: %s %s %s %s %s %s %s\n",
              args[0],args[1],args[2],args[3],args[4],args[5],args[6]);
      if(execvp(args[0], args) < 0) {
        printk("***Can't execvp() build_lev1. errno=%d\n", errno);
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
      sprintf(pcmd, "echo \"update lev1_highest_lev0_recnum set lev0recnum=%lu, date='%s' where lev0series='%s'\" | psql -h hmidb jsoc", lrec, get_datetime(), dsin);
      system(pcmd);

      if(numtofork <= 0) break;
    }
    else {
      for(k=0; k < MAXCPULEV1; k++) {   //make sure a good one replies and
        if(fpid[k] == pid) { break; }	//don't have to worry about wraparound
      }
      if(k == MAXCPULEV1) continue;	//extraneous pid get on first wait
      printf("returned pid = %d stat_loc = %d\n", pid, stat_loc);
      if(numtofork == 0) continue;	//find out no more children
    }
    //fork one more
    frec = lrec+1; lrec = (frec + MAXRECLEV1)-1;
    if((pid = fork()) < 0) {
      printk("***Can't fork(). errno=%d\n", errno);
      return(1);			//!!TBD decide what to do
    }
    else if(pid == 0) {                   //this is the beloved child
      args[0] = "build_lev1";
      sprintf(args1, "dsin=%s", dsin);
      args[1] = args1;
      sprintf(args2, "dsout=%s", dsout);
      args[2] = args2;
      sprintf(args3, "brec=%lu", frec);
      args[3] = args3;
      sprintf(args4, "erec=%lu", lrec);
      args[4] = args4;
      sprintf(args5, "instru=%s", instru);
      args[5] = args5;
      //sprintf(args6, "logfile=%s/l1_%s_%d.log", QSUBDIR, gettimetag(), k);
      sprintf(args6, "logfile=%s/l1s_%lu_%lu.log", QSUBDIR, frec, lrec);
      args[6] = args6;
      args[7] = NULL;
      printk("execvp: %s %s %s %s %s %s %s\n",
              args[0],args[1],args[2],args[3],args[4],args[5],args[6]);
      if(execvp(args[0], args) < 0) {
        printk("***Can't execvp() build_lev1. errno=%d\n", errno);
        exit(1);
      }
    }
    --numtofork;
    printf("forked pid = %d\n", pid);
    fpid[k] = pid;
  }
  return(0);
}

//Start a qsub job.
int qsubjob(long long rec1, long long rec2)
{
  FILE *fin;
  char astr[32], bstr[32], string[128], qlogname[128], qsubcmd[512];;
  char recrange[128];

  sprintf(recrange, ":#%lld-#%lld", rec1, rec2);
  sprintf(open_dsname, "%s[%s]", dsin, recrange);
  printk("open_dsname = %s\n", open_dsname); //!!TEMP
  sprintf(qlogname, "%s/qsub_%s_%d.csh", QSUBDIR, username, qcnt++);
  if((qsubfp=fopen(qlogname, "w")) == NULL) {
    fprintf(stderr, "**Can't open the qsub log file %s\n", qlogname);
    return(1);		//!!TBD
  }
  fprintf(qsubfp, "#!/bin/csh\n");
  fprintf(qsubfp, "echo \"TMPDIR = $TMPDIR\"\n");
  fprintf(qsubfp, "build_lev1 dsin=%s dsout=%s brec=%d erec=%d instru=%s logfile=%s/l1q_b%d_e%d_$JOB_ID.log\n", 
		dsin, dsout, rec1, rec2, instru, QSUBDIR, rec1, rec2); 
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
  fclose(fin);
  printf("\$JOB_ID = %u\n", jid);
  return(0);
}
 
int qsubmode(long long frec, long long lrec)
{
  FILE *fin;
  char qsubcmd[512], string[128];
  char astr[32], bstr[32], jidstr[MAXJIDSTR];
  uint64_t qjid[MAXQSUBLEV1], qstatjid[MAXQSUBLEV1];
  long long numofrecs, rfirst, rlast;
  int numtoqsub, i, j, l, k, found, status;
  int jobdone=0;

  numofrecs = (lrec - frec) + 1;
  numtoqsub = numofrecs/MAXRECLEV1;     //this many to qsub
  if(numofrecs % MAXRECLEV1) numtoqsub++;
  j = numtoqsub;			//0 implies one to qsub
  rlast = frec-1;
  if(j < MAXQSUBLEV1) l = j;            //fork less then the max at a time
  else l = MAXQSUBLEV1;                  //fork the max at a time
  for(k=0; k < l; k++) { //fork this many to start 
    rfirst = rlast+1; rlast = (rfirst + MAXRECLEV1)-1;
    if(rlast > lrec) rlast = lrec;	//don't go past end
    status = qsubjob(rfirst, rlast);	//schedule the qsub job
    --numtoqsub;
    qjid[k] = jid;
    if(k == 0) sprintf(jidstr, "%u", jid);
    else sprintf(jidstr, "%s,%u", jidstr, jid);
  }
  printf("numtoqsub = %d\n", numtoqsub); //!!TEMP
  sprintf(qsubcmd, "qstat -j %s 2>/dev/null | grep \"job_number:\"", jidstr);
  sleep(5);
  while(1) {
    //sprintf(qsubcmd, "qstat -u production | grep \"qsub_prod\"");
    fin = popen(qsubcmd, "r");
    found = 0; k = 0;
    while(fgets(string, sizeof string, fin)) {  //get qstat return line
      sscanf(string, "%s %u", astr, &jid);	// get job_id 
      printf("!!TEMP job id from qstat = %u\n", jid);
      qstatjid[k++] = jid;
      found = 1;
    }
    fclose(fin);
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
          rfirst = rlast+1; rlast = (rfirst + MAXRECLEV1)-1;
          if(rlast > lrec) rlast = lrec;	//don't go past end
          status = qsubjob(rfirst, rlast);	//schedule the qsub job
          --numtoqsub;
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
    sleep(2);
  }
  printf("All jobs done = %d. numtoqsub = %d\n", jobdone, numtoqsub);
  return(0);
}

/* Create lev1 from lev0 records in either stream mode or 
 * reprocessing mode. Return non-0 on error.
*/
int do_ingest()
{
  FILE *fin;
  int rstatus;
  long long recnum0, maxrecnum0;
  char string[128], pcmd[128];

  if(stream_mode) {		//start past last lev0 rec# processed 
    sprintf(pcmd, "echo \"select lev0recnum from lev1_highest_lev0_recnum where lev0series='%s'\" | psql -h hmidb jsoc", dsin);
    fin = popen(pcmd, "r");
    while(fgets(string, sizeof string, fin)) {  //get psql return line
      if(strstr(string, "lev0recnum")) continue;
      if(strstr(string, "-----")) continue;
      sscanf(string, "%lu", &recnum0);		//get lev0 rec# 
      recnum0++;				//start at next rec#
      break;
    }
    fclose(fin);
    sprintf(pcmd, "echo \"select max(recnum) from %s\" | psql -h hmidb jsoc", dsin);
    fin = popen(pcmd, "r");
    while(fgets(string, sizeof string, fin)) {  //get psql return line
      if(strstr(string, "max")) continue;
      if(strstr(string, "-----")) continue;
      sscanf(string, "%lu", &maxrecnum0);	//get max lev0 rec# 
      break;
    }
    fclose(fin);
    printf("Stream Mode starting at lev0 recnum = %lu maxrecnum = %lu\n", 
		recnum0, maxrecnum0);
    if(recnum0 > maxrecnum0) return(0);		//nothing to do. go wait
    rstatus = forkstream(recnum0, maxrecnum0);
  }
  else {
    //reprocessing mode. use brec/erec and qsub build_lev1 programs
    rstatus = qsubmode(brec, erec);
  }
  return(rstatus);
}

void sighandler(sig)
  int sig;
{
  char pcmd[128];
  if(sig == SIGTERM) {
    printf("*** %s build_lev1_mgr got SIGTERM. Exiting.\n", datestr);
    printk("*** %s build_lev1_mgr got SIGTERM. Exiting.\n", datestr);
    sprintf(pcmd, "/bin/rm %s/build_lev1_mgr.stream.touch 2>/dev/null",
                LEV1LOG_BASEDIR);
    system(pcmd);
    exit(1);
  }
  if(sig == SIGINT) {
    printf("*** %s build_lev1_mgr got SIGINT. Exiting.\n", datestr);
    printk("*** %s build_lev1_mgr got SIGINT. Exiting.\n", datestr);
    sprintf(pcmd, "/bin/rm %s/build_lev1_mgr.stream.touch 2>/dev/null",
                LEV1LOG_BASEDIR);
    system(pcmd);
    exit(1);
  }
  printk("*** %s build_lev1_mgr got an illegal signal %d, ignoring...\n",
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
  sprintf(idstr, "Cwd: %s\nCall: ", cwdbuf);
  sprintf(string, "build_lev1_mgr started as pid=%d user=%s\n", getpid(), username);
  strcat(idstr, string);
  printk("%s", idstr);
  printf("%s", idstr);
  sprintf(arginstru, "instru=%s", instru);
  sprintf(argdsin, "dsin=%s", dsin);
  sprintf(argdsout, "dsout=%s", dsout);
  sprintf(argbrec, "brec=%lu", brec);
  sprintf(argerec, "erec=%lu", erec);
  sprintf(arglogfile, "logfile=%s", logname);
  printk("%s %s %s %s %s\n", arginstru, argdsin, argdsout, argbrec, argerec);
  printf("%s %s %s %s %s\n", arginstru, argdsin, argdsout, argbrec, argerec);
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
    sprintf(lfile, "%s/build_lev1_mgr_restart_%d.touch", LEV1LOG_BASEDIR, tpid);
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
  brec = cmdparams_get_int(&cmdparams, "brec", NULL);
  erec = cmdparams_get_int(&cmdparams, "erec", NULL);
  if(brec == 0 && erec == 0) {
    //make sure there isn't a stream mode already running
    sprintf(pcmd, "ls %s/build_lev1_mgr.stream.touch 2>/dev/null", 
		LEV1LOG_BASEDIR);
    fin = popen(pcmd, "r");
    while(fgets(line, sizeof line, fin)) {  //get ps return line
      printf("Error: build_lev1_mgr already running.");
      printf(" If not so, do:\n");
      printf("/bin/rm %s/build_lev1_mgr.stream.touch\n", LEV1LOG_BASEDIR);
      fclose(fin);
      return(0);
    }
    fclose(fin);
    sprintf(pcmd, "touch %s/build_lev1_mgr.stream.touch 2>/dev/null",
                LEV1LOG_BASEDIR);
    system(pcmd);
    stream_mode = 1;
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
    if(do_ingest()) {        // loop to get files from the lev0
      printk("**ERROR: in do_ingest() for %s\n", open_dsname);
    }
    if(!stream_mode) return(0); //all done for reprocessing
    sleep(10);		//wait for more lev0 to appear
    //wflg = 0;
  }
  return(0);
}

