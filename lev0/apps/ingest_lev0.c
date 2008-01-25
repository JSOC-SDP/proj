/*-----------------------------------------------------------------------------
 * cvs/JSOC/proj/lev0/apps/ingest_lev0.c
 * NOTE: This originally came from hmi_lev0.c on hmi0
 *-----------------------------------------------------------------------------
 *
 * This is a module that runs with DRMS and continuously extracts images and HK
 * data from .tlm files that appear in the given input dir. It outputs images
 * to the DRMS dataset hmi.lev0 and hk data to appropriate hk datasets.

 * Call: ingest_lev0 vc=VC05 indir=/tmp/jim outdir=/tmp/jim/out [logfile=name]

 * DESCRIPTION:

*!!!!TBD UPDATE THIS DESCRIP!!!!!!!!!! e.g. how started !!!!!!!!!!!!!!!!!!!!!

 */

#include <jsoc_main.h>
#include <cmdparams.h>
#include <drms.h>
#include <drms_names.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h> /* for umask(2) */
#include <dirent.h>
#include <unistd.h> /* for alarm(2) among other things... */
#include <printk.h>
#include "imgdecode.h"
#include "packets.h"
/************************************
#include "imgstruct.h"
#include "hmi_compression.h"
#include "decompress.h"
#include "load_hk_config_files.h"
*************************************/

#define LEV0SERIESNAME "su_production.lev0_test"
#define TLMSERIESNAME "su_production.tlm_test"
#define H0LOGFILE "/usr/local/logs/lev0/ingest_lev0.%s.%s.log"
#define DIRDDS "/egse/ssim2soc"
#define IMAGEDIR "/tmp/jim"	/* dir to put the last IMAGEDIRCNT images */
#define IMAGEDIRCNT 20		/* # of images to save */
#define SEC1970TO2004 1072828800 /* approx #of secs from 1970 to 2004 */
#define PKTSZ 1788		/* size of VCDU pkt */
#define DEFAULTDB "jsoc"	/* the default db to connect to */
#define MAXFILES 512		/* max # of file can handle in tlmdir */
#define NUMTIMERS 8		/* number of seperate timers avail */
#define TESTAPPID 0x199		/* appid of test pattern packet */
#define TESTVALUE 0xc0b		/* first value in test pattern packet */
#define MAXERRMSGCNT 10		/* max # of err msg before skip the tlm file*/

#define NOTSPECIFIED "***NOTSPECIFIED***"
/* List of default parameter values. */
ModuleArgs_t module_args[] = { 
  {ARG_STRING, "vc", NOTSPECIFIED, "Primary virt channel to listen to"},
  {ARG_STRING, "indir", NOTSPECIFIED, "directory containing the files to ingest"},
  {ARG_STRING, "outdir", NOTSPECIFIED, "directory to move the files to after the ingest"}, 
  {ARG_STRING, "logfile", NOTSPECIFIED, "optional log file name. Will create one if not given"},
  {ARG_FLAG, "v", "0", "verbose flag"},
  {ARG_FLAG, "h", "0", "help flag"},
  {ARG_END}
};

CmdParams_t cmdparams;

/* Module name presented to DRMS. */
char *module_name = "ingest_lev0";

FILE *h0logfp;                  /* fp for h0 ouput log for this run */
IMG Image, ImageOld;
CCSDS_Packet_t *Hk;
DRMS_Record_t *rs;
DRMS_Segment_t *segment;
DRMS_Array_t *segArray;
static char datestr[32];
static struct timeval first[NUMTIMERS], second[NUMTIMERS];

unsigned int fsn = 0;
unsigned int fsn_prev = 0;
unsigned int fsn_pre_rexmit = 0;
unsigned int fid = 0;
SUM_t *sum;
SUMID_t uid = 0;
char **cptr;
uint64_t *dsixpt;
uint64_t alloc_index;
char alloc_wd[64];

long long vcdu_seq_num;
long long vcdu_seq_num_next;
long long total_missing_im_pdu;
unsigned int vcdu_24_cnt, vcdu_24_cnt_next;
int verbose;			/* set by get_cmd() */
double reqbytes;		/* # of bytes requested */
double dsize;			/* # of bytes used */
double bytes_used;		/* total #of bytes for all cataloged output */
int total_tlm_vcdu;
int total_missing_vcdu;
int dsds_tid;			/* originally the tid of dsds_svc */
				/* now the tid of pe_rpc that gets us to dsds*/
int abort_active;		/* set while doing an abort */
int sigtermflg = 0;		/* set on signal so prog will know */
int pflg = 0;
int imagedircnt = 0;            /* inc each time write an image to IMAGEDIR */
int ALRMSEC = 60;               /* seconds for alarm signal */
int dbxflg;			/* user defined while running dbx, else N/A */
int debugflg;			/* run all pvm servers in the debug mode */
				/* also do keyiterate. Don't use w/tae */ 
char database[MAX_STR];
char pdshost[MAX_STR];
char timetag[32];
char pchan[8];			/* primary channel to listen to e.g. VC02 */
char rchan[8];			/* redundant channel to listen to e.g. VC10 */
char *dbname = DEFAULTDB;	/* !!TBD pass this in as an arg */
char *username;			/* from getenv("USER") */
char *tlmdir;			/* tlm dir name passed in */
char *outdir;			/* output dir for .tlm file (can be /dev/null)*/
char *logfile;			/* optional log name passed in */
char *vc;			/* virtual channel to process, e.g. VC02 */
struct p_r_chans {
  char *pchan;
  char *rchan;
};
typedef struct p_r_chans P_R_CHANS;

P_R_CHANS p_r_chan_pairs[] = {
{"VC01", "VC09"},		/* AIA */
{"VC04", "VC12"},		/* AIA */
{"VC02", "VC10"},		/* HMI */
{"VC05", "VC13"},		/* HMI */
{"n/a", "n/a"}
};

struct namesort {		/* sorted file names in tlmdir */
  char *name;
};
typedef struct namesort NAMESORT;

/* linked list of open images */
struct openimg {
  struct openimg *next;
  time_t sec;
  unsigned int fsn;
};
typedef struct openimg OPENIMG;
OPENIMG *openimg_hdr = NULL;	/* linked list of open images */
OPENIMG *openimg_ptr;		/* current enty */

int nice_intro ()
  {
  int usage = cmdparams_get_int (&cmdparams, "h", NULL);
  if (usage)
    {
    printf ("Usage:\ningest_lev0 [-vh] "
	"vc=<virt chan> indir=</dir> outdir=</dir> [logfile=<file>]\n"
	"  -h: help - show this message then exit\n"
	"  -v: verbose\n"
	"vc= primary virt channel to listen to e.g. VC02\n"
	"indir= directory containing the files to ingest\n"
	"outdir= directory to move the files to after the ingest\n"
	"logfile= optional log file name. Will create one if not given\n");
    return(1);
    }
  return (0);
  }

/* Returns a time tag like  yyyy.mm.dd.hhmmss */
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


/* NOTE: This my be vestigial. Now we can only have one open image per process. !!CHECK */
/* Add an entry with the given values to the OPENIMG linked list */
void setopenimg(OPENIMG **list, time_t sec, unsigned int fsn)
{
  OPENIMG *newone;

  newone = (OPENIMG *)malloc(sizeof(OPENIMG));
  newone->next = *list;
  newone->sec = sec;
  newone->fsn = fsn;
  *list = newone;
}

/* remove the OPENIMG list entry with the given fsn */
void remopenimg(OPENIMG **list, unsigned int fsn)
{
  OPENIMG *walk = *list;
  OPENIMG *trail = NULL;

  while(walk) {
    if(walk->fsn != fsn) {
      trail = walk;
      walk = walk->next;
    }
    else {
      if(trail)
        trail->next = walk->next;
      else
        *list = walk->next;
      free(walk);
      walk = NULL;
    }
  }
}

/* get the OPENIMG list entry with the given fsn. return null if none. */
OPENIMG *getopenimg(OPENIMG *list, unsigned int fsn)
{
  OPENIMG *walk = list;

  while(walk) {
    if(walk->fsn != fsn)
      walk = walk->next;
    else
      return walk;
  }
  return walk;
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

/* Outputs the variable format message (re: printf) to the log file.
 */
int h0log(const char *fmt, ...)
{
  va_list args;
  char string[32768];

  va_start(args, fmt);
  vsprintf(string, fmt, args);
  if(h0logfp) {
    fprintf(h0logfp, string);
    fflush(h0logfp);
  } else {			/* couldn't open log */
    printf(string);
    fflush(stdout);
  }
  va_end(args);
  return(0);
}

/* Close out an image.
*/
void close_image(DRMS_Record_t *rs, DRMS_Segment_t *seg, DRMS_Array_t *array,
		IMG *Img, int fsn)
{
  int status;

  drms_setkey_int(rs, "TELNUM", Img->telnum);
  drms_setkey_int(rs, "APID", Img->apid);
  drms_setkey_int(rs, "CROPID", Img->cropid);
  drms_setkey_int(rs, "LUTID", Img->luid);
  drms_setkey_int(rs, "TAPCODE", Img->tap);
  drms_setkey_int(rs, "N", Img->N);
  drms_setkey_int(rs, "K", Img->K);
  drms_setkey_int(rs, "R", Img->R);
  drms_setkey_int(rs, "TOTVALS", Img->totalvals);
  drms_setkey_int(rs, "DATAVALS", Img->datavals);
  drms_setkey_int(rs, "NPACKETS", Img->npackets);
  drms_setkey_int(rs, "NERRORS", Img->nerrors);
  drms_setkey_int(rs, "EOIERROR", Img->last_pix_err);
  status = drms_segment_write(seg, array, 0);
  if (status) {
    printk("ERROR: drms_segment_write error=%d for fsn=%u\n", status, fsn);
  }
  array->data = NULL;        /* must do before free */
  drms_free_array(array);
  if((status = drms_close_record(rs, DRMS_INSERT_RECORD))) {
    printk("**ERROR: drms_close_record failed for %s fsn=%u\n",
                        LEV0SERIESNAME, fsn);
  }
}

/* Got a fatal error sometime after registering with SUMS. 
 * Degregister and close with SUMS as approriate.
 */
void abortit(int stat)
{
  printk("***Abort in progress ...\n");
  if(uid ) {			/* we've registered with SUMS */
    SUM_close(sum, printk);
    printk("*Closed with sum_svc uid=%ld\n", uid);
  }
  printk("**Exit ingest_lev0 w/ status = %d\n", stat);
  /*msg("Exit ingest_lev0 w/ status = %d\n\n", stat);*/
  if (h0logfp) fclose(h0logfp);
  exit(stat);
}

/* Called 60 secs after the last .tlm file was seen.
 * Will close any opened image. NOTE: a reprocessed image cannot
 * be open. It is always closed at the end of get_tlm().
 */
void alrm_sig(int sig)
{
  signal(SIGALRM, alrm_sig);
  if(Image.initialized) {
    close_image(rs, segment, segArray, &Image, fsn_prev);
  }
  printk("*Closed image on timeout FSN=%u\n", fsn_prev);
}

void sighandler(int sig)
{
  sigtermflg = 1;
}

void now_do_term_sig()
{
  abortit(2);
}


int compare_names(const void *a, const void *b)
{
  NAMESORT *x=(NAMESORT *)a, *y=(NAMESORT *)b;
  return(strcmp(x->name+4, y->name+4)); /* skip VC02/VC05 in compare */
}


unsigned short MDI_getshort (unsigned char *c)    /*  machine independent  */
{
  unsigned short s = 0;

  s = (unsigned short) *c++ << 8;
  s |= (unsigned short) *c;
  return s;
}

/* !!!TEMP to make */
int decode_hk_next_vcdu(unsigned short *tbuf, CCSDS_Packet_t **hk)
{
  return(0);
}


/* Process the tlm file to validate and to extract the lev0 image.
*/
int get_tlm(char *file, int rexmit, int higherver)
{
  static DRMS_RecordSet_t *rset;
  static DRMS_Record_t *rs_old, *rsc;
  static DRMS_Segment_t *segmentc;
  static DRMS_Array_t *cArray, *oldArray;
  FILE *fpin;
  IMG *Img;
  short *rdat;
  unsigned char cbuf[PKTSZ];
  char rexmit_dsname[256];
  long long gap_42_cnt;
  int status, rstatus, dstatus, fpkt_cnt, i, j, sync_bad_cnt;
  int imagecnt, appid, datval, eflg, firstflg, errmsgcnt;
  unsigned int cnt1, cnt2, cnt3, fsnx, gap_24_cnt;
  int zero_pn;
  unsigned short pksync1, pksync2;
  float ftmp;

  if(!(fpin = fopen(file, "r"))) {	/* open the tlm input */
    printk("*Can't open tlm file %s\n", file);
    return(1);
  }
  alarm(ALRMSEC);			//timeout if don't get another .tlm file
  BeginTimer(1);			/* time tlm file processing */
  printk("*Processing tlm file %s\n", file);
  fpkt_cnt = sync_bad_cnt = imagecnt = eflg = errmsgcnt = 0;
  zero_pn = gap_24_cnt = gap_42_cnt = 0;
  firstflg = 1; 
  if(rexmit || higherver) {
    fsn_pre_rexmit = fsn_prev;		/* restore this at end of rexmit tlm*/
    fsn_prev = 0;			/* cause a new image */
  }
  /*BeginTimer(2);			/* time each image */
  /* read a VCDU packet */
  while((status = fread(cbuf,sizeof(char),PKTSZ,fpin) ) == PKTSZ) {
    pksync1 = MDI_getshort(cbuf);
    pksync2 = MDI_getshort(cbuf+2);
    if((pksync1 == 0) && (pksync2 == 0)) { /* skip 0 pn code */
      if(!zero_pn) {			/* give msg for 1st one only */
        printk("*0 PN code at pkt# %d\n", fpkt_cnt);
        printk("*Subsequent ones will be ignored until non-0 again\n");
        zero_pn = 1;
      }
      fpkt_cnt++;			/* count # of pkts found */
      continue;
    }
    if((pksync1 != 0x1acf) || (pksync2 != 0xfc1d)) {
      printk("*Lost sync at VCDU pkt# %d. pksync1=%x pksync2=%x\n", 
		fpkt_cnt, pksync1, pksync2);
      fpkt_cnt++;			/* count # of pkts found */
      eflg++;
      if(sync_bad_cnt++ > 4) {
        printk("**Too many out of sync packets.\n");
        return(1);
      }
      printk("  Will attempt to press on...\n");
      zero_pn = 0;
      continue;
    }
    if(firstflg) {		/* print first good sync found */
      printk("*VCDU pkt# %d sync = %x %x\n", fpkt_cnt, pksync1, pksync2);
    }
    fpkt_cnt++;			/* count # of pkts found */
    /* get 24 bit VCDU counter */
    cnt1 = MDI_getshort(cbuf+6);
    cnt2 = MDI_getshort(cbuf+8);
    cnt2 = (cnt2 >> 8)& 0xFF;
    cnt2 = ((cnt1 << 8)& 0xFF00) + cnt2;
    cnt1 = (cnt1 >> 8)& 0xFF;
    vcdu_24_cnt = (cnt1*65536) + cnt2;
    if(vcdu_24_cnt_next != vcdu_24_cnt) {
      printk("*VCDU 24bit seq num out of sequence. exp: %u  rec: %u\n", 
	    vcdu_24_cnt_next, vcdu_24_cnt);
      if(vcdu_24_cnt_next > vcdu_24_cnt) {
        printk("*NOTE: VCDU 24 bit counter retarded\n"); /*cntr does go thru 0*/
        printk("*NOTE: gap report will be inaccurate (tbd)\n");
      }
      if(!firstflg) {		/* don't count gap across .tlm files */
        gap_24_cnt += vcdu_24_cnt - vcdu_24_cnt_next;
      }
    }
    vcdu_24_cnt_next = vcdu_24_cnt + 1;
    /* now get the 42bit IM_PDU counter */
    cnt1 = MDI_getshort(cbuf+10);
    cnt2 = MDI_getshort(cbuf+12);
    cnt3 = MDI_getshort(cbuf+14);
    cnt1 = cnt1 & 0x03ff;
    vcdu_seq_num = (cnt1*4294967296) + (cnt2*65536) + cnt3;
    /* printk("vcdu_seq_num = %lld\n", vcdu_seq_num); */
    if(vcdu_seq_num_next != vcdu_seq_num) {
      printk("*IM_PDU seq num out of sequence. exp: %lld  rec: %lld\n", 
	    vcdu_seq_num_next, vcdu_seq_num);
      if(vcdu_seq_num_next > vcdu_seq_num) {
        printk("*NOTE: IM_PDU 42 bit counter retarded\n");
        printk("*NOTE: gap report will be inaccurate\n");
      }
      if(!firstflg) {		/* don't count gap across .tlm files */
        gap_42_cnt += vcdu_seq_num - vcdu_seq_num_next;
      }
      eflg++;
    }
    firstflg = 0;
    vcdu_seq_num_next = vcdu_seq_num + 1;
    /* get the App ID. Low 11 bit of short at buf+18 */
    appid = MDI_getshort(cbuf+18);
    appid = appid & 0x07ff;
    if(appid == TESTAPPID) {	/* appid of test pattern */
      printk("*Test ApID of %0x found for IM_PDU Cntr = %lld\n", 
			TESTAPPID, vcdu_seq_num);
      for(i=0, j=TESTVALUE; i < 877; i=i+2, j++) {
        datval = MDI_getshort(cbuf+32+i);	/* next data value */
        if(datval != j) {
          printk("*Test data value=%0x, expected=%0x for IM_PDU Cntr=%lld\n", 
		datval, j, vcdu_seq_num);
          eflg++;
          break;		/* skip the rest of this packet */
        }
      }
      continue; 		/* go on to next packet */
    }

    /* Parse tlm packet headers. */
    if(appid == APID_HMI_SCIENCE_1 || appid == APID_HMI_SCIENCE_2 || 
	appid == APID_AIA_SCIENCE_1 || appid == APID_AIA_SCIENCE_2)
    {
      cnt1 = MDI_getshort(cbuf+32);
      cnt2 = MDI_getshort(cbuf+34);
      fsnx = (unsigned int)(cnt1<<16)+(unsigned int)(cnt2);
      if(rexmit || higherver) {
        if(fsnx != fsn_prev) {            /* the fsn has changed */
          if(fsn_prev != 0) {  /* close image of prev fsn if not 0 */
            close_image(rsc, segmentc, oldArray, Img, fsn_prev);
            imagecnt++;
          }
          Img = &ImageOld;
          fsn_prev = fsnx;
          sprintf(rexmit_dsname, "su_production.lev0_test[%u]", fsnx);
          printk("Open prev ds: %s\n", rexmit_dsname);
          rset = drms_open_records(drms_env, rexmit_dsname, &rstatus); 
          if(rstatus) {
            printk("Can't do drms_open_records(%s)\n", rexmit_dsname);
            return(1);		/* !!!TBD */
          }
          if(!rset || (rset->n == 0)) {
            printk("No prev ds\n");	/* start a new image */
            Img->initialized = 0;
            Img->reopened = 0;
            rsc = drms_create_record(drms_env, LEV0SERIESNAME,
  				DRMS_PERMANENT, &rstatus);
            if(rstatus) {
              printk("Can't create record for %s\n", LEV0SERIESNAME);
              continue;                     /* !!!TBD ck this */
            }
            rstatus = drms_setkey_int(rsc, "FSN", fsnx);
            segmentc = drms_segment_lookup(rsc, "file");
            rdat = Img->dat;
            oldArray = drms_array_create(DRMS_TYPE_SHORT,
                                               segmentc->info->naxis,
                                               segmentc->axis,
                                               rdat,
                                               &dstatus);
          }
          else {
            Img->initialized = 1;
            Img->reopened = 1;
            Img->fsn = fsnx;
            Img->apid = appid;
            rs_old = rset->records[0];
            rsc = drms_clone_record(rs_old, DRMS_PERMANENT, 
    				DRMS_COPY_SEGMENTS, &rstatus);
            if(rstatus) {
              printk("Can't do drms_clone_record()\n");
              return(1);		/* !!!TBD ck */
            }
            drms_close_records(rset, DRMS_FREE_RECORD);
            rstatus = drms_setkey_int(rsc, "FSN", fsnx);
            Img->telnum = drms_getkey_int(rsc, "TELNUM", &rstatus);
            Img->cropid = drms_getkey_int(rsc, "CROPID", &rstatus);
            Img->luid = drms_getkey_int(rsc, "LUTID", &rstatus);
            Img->tap = drms_getkey_int(rsc, "TAPCODE", &rstatus);
            Img->N = drms_getkey_int(rsc, "N", &rstatus);
            Img->K = drms_getkey_int(rsc, "K", &rstatus);
            Img->R = drms_getkey_int(rsc, "R", &rstatus);
            Img->totalvals = drms_getkey_int(rsc, "TOTVALS", &rstatus);
            Img->datavals = drms_getkey_int(rsc, "DATAVALS", &rstatus);
            Img->npackets = drms_getkey_int(rsc, "NPACKETS", &rstatus);
            Img->nerrors = drms_getkey_int(rsc, "NERRORS", &rstatus);
            Img->last_pix_err = drms_getkey_int(rsc, "EOIERROR", &rstatus);
            segmentc = drms_segment_lookupnum(rsc, 0);
            cArray = drms_segment_read(segmentc, DRMS_TYPE_SHORT, &rstatus);
            if(rstatus) {
              printk("Can't do drms_segment_read()\n");
              return(1);		/* !!!!TBD ck */
            }
            short *adata = (short *)cArray->data;
            memcpy(Img->dat, adata, 2*MAXPIXELS);
            rdat = Img->dat;
            oldArray = drms_array_create(DRMS_TYPE_SHORT, 
                                               segmentc->info->naxis,
                                               segmentc->axis,
                                               rdat,
                                               &dstatus);
          }
        }
      }
      else {			// continuing normal stream
        if(fsnx != fsn_prev) {            /* the fsn has changed */
          printk("*FSN has changed from %u to %u\n", fsn_prev, fsnx);
          if(fsn_prev != 0) {	/* close the image of the prev fsn if not 0 */
            close_image(rs, segment, segArray, Img, fsn_prev);
            imagecnt++;
          }
          /* start a new image */
          Img = &Image;
          Img->initialized = 0;
          Img->reopened = 0;
          fsn_prev = fsnx;
          rs = drms_create_record(drms_env, LEV0SERIESNAME, 
  		DRMS_PERMANENT, &dstatus);
          if(dstatus) {
            printk("Can't create record for %s\n", LEV0SERIESNAME);
            continue;			/* !!!TBD ck this */
          }
          dstatus = drms_setkey_int(rs, "FSN", fsnx);
          segment = drms_segment_lookup(rs, "file");
          rdat = Img->dat;
          segArray = drms_array_create(DRMS_TYPE_SHORT,
                                               segment->info->naxis,
                                               segment->axis,
                                               rdat,
                                               &dstatus);
        }
      }
      /* call with pointer to M_PDU_Header */
      /* rstatus = kehcheng_code((unsigned short *)(cbuf+16), Img);*/
      /*rstatus = imgdecode((unsigned short *)(cbuf+16), Img);*/
      rstatus = imgdecode((unsigned short *)(cbuf+10), Img);
      switch(rstatus) {
      case 0:
        /* A science data VCDU was successfully decoded */
        break;
      case IMGDECODE_DECOMPRESS_ERROR:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_DECOMPRESS_ERROR\n");
        break;
      case IMGDECODE_TOO_MANY_PIXELS:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_TOO_MANY_PIXELS\n");
        break;
      case IMGDECODE_BAD_N:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_BAD_N\n");
        break;
      case IMGDECODE_BAD_APID:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_BAD_APID\n");
        break;
      case IMGDECODE_NO_LOOKUP_TABLE:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_NO_LOOKUP_TABLE\n");
        break;
      case IMGDECODE_LOOKUP_ID_MISMATCH:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_LOOKUP_ID_MISMATCH\n");
        break;
      case IMGDECODE_BAD_LOOKUP_TABLE:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_BAD_LOOKUP_TABLE\n");
        break;
      case IMGDECODE_NO_CROP_TABLE:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_NO_CROP_TABLE\n");
        break;
      case IMGDECODE_CROP_ID_MISMATCH:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_CROP_ID_MISMATCH\n");
        break;
      case IMGDECODE_BAD_CROP_GEOMETRY:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_BAD_CROP_GEOMETRY\n");
        break;
      case IMGDECODE_BAD_CROP_TABLE:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_BAD_CROP_TABLE\n");
        break;
      case IMGDECODE_BAD_CROP_SKIP_TAKE:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_BAD_CROP_SKIP_TAKE\n");
        break;
      case IMGDECODE_BAD_OFFSET:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_BAD_OFFSET\n");
        break;
      case IMGDECODE_OUT_OF_MEMORY:
        errmsgcnt++;
        printk("*imgdecode() ret: IMGDECODE_OUT_OF_MEMORY\n");
        break;
      default:
        errmsgcnt++;
        printk("*imgdecode() ret: unknown err status = %d:\n", rstatus);
        break;
      }
      if(errmsgcnt >= MAXERRMSGCNT) {
        printk("*Too many errors. Skipping this tlm file.\n");
        return(1);	/* !!!TBD change. don't skip entire tlm file */
      }
    }
    else {			/* send the HK data to Carl */
      rstatus = decode_hk_next_vcdu((unsigned short *)(cbuf+10), &Hk);
    }
  }				/* end of rd of vcdu pkts */
  fclose(fpin);
  if(!eflg) {
    printk("*No errors in tlm file\n");
  }
  if(rexmit || higherver) {	/* close the opened record */
    close_image(rsc, segmentc, oldArray, Img, fsnx);
    imagecnt++;
    fsn_prev = fsn_pre_rexmit;	// restore orig for next normal .tlm file
  }
  ftmp = EndTimer(1);
  printk("**Processed %s\n**with %d images and %d VCDUs in %f sec\n\n",
	file, imagecnt, fpkt_cnt, ftmp);
  if(fpkt_cnt != total_tlm_vcdu) {
    printk("**WARNING: Found #vcdu=%d; expected=%d\n", fpkt_cnt, total_tlm_vcdu);
  }
  if(gap_24_cnt != total_missing_vcdu) {
    printk("**WARNING: VCDU 24bit cntr gaps=%d; expected=%d\n",
	 gap_24_cnt, total_missing_vcdu);
  }
  if(gap_42_cnt != total_missing_im_pdu) {
    printk("**WARNING: IM_PDU 42bit cntr gaps=%lld; expected=%lld\n",
	 gap_42_cnt, total_missing_im_pdu);
  }
  return(0);
}

/* This is called from the main loop to check if any .parc files are in
 * the pipeline to soc dir ($DIRPIPE2SOC).
 * The .parc file is sent to /dds/pipe2soc/aia or /dds/pipe2soc/hmi
 * every time the pipeline back end system does a tapearc. (during development
 * the .parc files are sent by the cron job /home/jim/cvs/jsoc/scripts/pipefe_rm
 * on d00.)
 * The .parc file
 * has info on storage units that were archived successfully by the backend,
 * and so can be marked as such in the data capture sum_main table.
 * The sum_main table is updated for its safe_tape info from the .parc.
 * A .parc file looks like:
 * dcs0.jsoc:/dds/pipe2soc/aia> t AIA_2007_131_11_56.parc 
 * VC01_2007_131_11_51_39_0123456789A_FFFFF_00 000000S1 666 2007-04-12 17:15:45
 * VC04_2007_131_11_52_09_0123456789A_FFFFF_00 000000S1 666 2007-04-12 17:15:45
 *
 * storage_unit_name pipeline_tape_id_archived_on tape_fn date
*/
void do_pipe2soc() {
  DIR *dfd;
  struct dirent *dp;
  FILE *fp;
  int ptape_fn, complete;
  char line[128], fname[128], cmd[128];
  char *su_name, *ptape_id, *ptape_date;

  char *frompipedir = "/tmp"; /* !!!TEMP to compile */

  /* only run this on the primary channel ingest_lev0 process */
  if(strcmp(pchan, "VC01") && strcmp(pchan, "VC02")) { return; }

    if(DS_ConnectDB_Q(dbname)) {
      printk("**Can't connect to DB %s\n", dbname);
      abortit(3);
    }

  if((dfd=opendir(frompipedir)) == NULL) {
    printk("**Can't opendir(%s) to find files\n", frompipedir);
    abortit(3);
  }
  while((dp=readdir(dfd)) != NULL) {
    if(strstr(dp->d_name, ".parc")) {
      sprintf(fname, "%s/%s", frompipedir, dp->d_name);
      if(!(fp=fopen(fname, "r"))) {
        printk("***Can't open %s\n", fname);
        continue;
      }
      printk("Found parc file: %s\n", fname);
      complete = 1;
      while(fgets(line, 128, fp)) {       /* get .parc file lines */
        if(line[0] == '#' || line[0] == '\n') continue;
        printk("%s", line);
        su_name = (char *)strtok(line, " ");
        ptape_id = (char *)strtok(NULL, " ");
        ptape_fn = atoi((char *)strtok(NULL, " "));
        ptape_date = (char *)strtok(NULL, "\n");
        if(SUMLIB_SafeTapeUpdate(su_name,ptape_id,ptape_fn,ptape_date)) {
          printk("**ERROR in SUMLIB_SafeTapeUpdate(%s...)\n", su_name);
          complete = 0;
        }
      }
      fclose(fp);
      if(complete) {
        sprintf(cmd, "/bin/rm -f %s", fname);
        printk("%s\n", cmd);
        system(cmd);
      }
    }
  }
  closedir(dfd);
  DS_DisConnectDB_Q();
}

/* This is the main loop that gets the .qac and .tlm files and 
 * puts them into ds TLMSERIESNAME and extracts the lev0 and puts it in 
 * LEV0SERIESNAME in DRMS.
 */
void do_ingest()
{
  FILE *fp;
  DRMS_Record_t *rs_tlm;
  DIR *dfd;
  NAMESORT *nameptr;
  struct dirent *dp;
  float ttmp;
  int i, j, status;
  int rexmit, higherversion;
  char path[DRMS_MAXPATHLEN];
  char name[128], line[128], tlmfile[128], tlmname[96];
  char cmd[128], xxname[128], tlmsize[80], vername[16];
  char *token, *filename;

  if((dfd=opendir(tlmdir)) == NULL) {
    printk("**Can't opendir(%s) to find files\n", tlmdir);
    abortit(3);
  }
  i = 0;
  if((nameptr = (NAMESORT *)malloc(MAXFILES * sizeof(NAMESORT))) == NULL) {
    printk("***Can't alloc memory for file name sort\n");
    abortit(3);
  }

  while((dp=readdir(dfd)) != NULL) {
    /* printk("%s\n", dp->d_name) ; continue;*/ /* !!TEMP */
    /* Only accept our files. */
    if(strstr(dp->d_name, pchan) || strstr(dp->d_name, rchan) || strstr(dp->d_name, ".dsf")) {
      nameptr[i++].name = strdup(dp->d_name);
      if(i >= MAXFILES) {
        printk("***Fatal error. Too many (%d) files in %s\n", MAXFILES, tlmdir);
        abortit(3);
      }
    }
  }
  closedir(dfd);
  qsort(nameptr, i, sizeof(NAMESORT), &compare_names);

  for(j=0; j < i; j++) {
    /*printk("####QSORT FILES: %s\n", nameptr[j].name); /* !!TEMP */
    /* NOTE: the dsf files stay in the indir for now */
    /* Currently the cron job pipefe_rm does this:
    /* `/bin/mv $dsfname /dds/socdc/hmi/dsf`
    /********************
    if(strstr(nameptr[j].name, ".dsf")) {
      sprintf(cmd, "/bin/mv %s/%s %s", tlmdir, nameptr[j].name, outdir);
      printk("*mv dsf file to %s\n", outdir);
      printk("%s\n", cmd);
      if(system(cmd)) {
        printk("***Error on: %s\n", cmd);
      }
    }
    *********************/
    if(!strstr(nameptr[j].name, ".qac")) {	/* can be .qac or .qacx */
      free(nameptr[j].name);
      continue;
    }
    BeginTimer(NUMTIMERS-1);
    rexmit = higherversion = 0;
    if(strstr(nameptr[j].name, ".qacx")) {  	/* this is a rexmit file */
      rexmit = 1;
    }
    sprintf(name, "%s/%s", tlmdir, nameptr[j].name);
    printk("\n*Found qac file:\n* %s\n", name);
    if(!(fp=fopen(name, "r"))) {
      printk("***Can't open %s\n", name);
      free(nameptr[j].name);
      continue;
    }
    /* NOTE: the qac file is already verified by the caller of ingest_lev0 */
    while(fgets(line, 256, fp)) {	/* get qac file lines */
      if(line[0] == '#' || line[0] == '\n') continue;
      if(strstr(line, "TLM_FILE_NAME=")) {
        token = (char *)strtok(line, "=");
        token = (char *)strtok(NULL, "\n");
        printk("tlm file is %s\n", token);
        sprintf(tlmfile, "%s/%s", tlmdir, token);
        sprintf(tlmname, "%s", token);
      }
      else if(strstr(line, "TLM_FILE_SIZE=")) {
        token = (char *)strtok(line, "=");
        token = (char *)strtok(NULL, "=");
        printk("*tlm file size is %s", token);
        sprintf(tlmsize, "%s", token);
        tlmsize[strlen(token)-1] = 0;
        reqbytes = (double)atol(token);
        /*reqbytes += (double)1000000;*/	/* add some overhead */
      }
      else if(strstr(line, "TOTAL_TLM_VCDU=")) {
        token = (char *)strtok(line, "=");
        token = (char *)strtok(NULL, "\n");
        total_tlm_vcdu = atoi(token);
      }
      else if(strstr(line, "TOTAL_MISSING_VCDU=")) {
        token = (char *)strtok(line, "=");
        token = (char *)strtok(NULL, "\n");
        total_missing_vcdu = atoi(token);
      }
      else if(strstr(line, "TOTAL_MISSING_IM_PDU=")) {
        token = (char *)strtok(line, "=");
        token = (char *)strtok(NULL, "\n");
        total_missing_im_pdu = atol(token);
        break;
      }
    }
    fclose(fp);
/* !!!NEW 12/28/07 drms calls to ingest qac/tlm to DB */
    rs_tlm = drms_create_record(drms_env, TLMSERIESNAME, 
				DRMS_PERMANENT, &status);
    if(status) {
      printk("***Can't create record for %s\n", TLMSERIESNAME);
      abortit(3);
    }
    filename = (char *)rindex(tlmname, '.');
    *filename = 0;			/* elim .tlm for filename */
    if((status = drms_setkey_string(rs_tlm, "filename", tlmname))) {
      printk("**ERROR: drms_setkey_string failed for 'filename'\n");
    }
    drms_record_directory(rs_tlm, path, 0);
    if(!*path) {
      printk("***ERROR: No path to segment for %s\n", TLMSERIESNAME);
      abortit(3);
    }
    sprintf(cmd, "cp -p %s %s", name, path);
    printk("*cp qac to %s\n", path);
    printk("%s\n", cmd);
    if(status = system(cmd)) {
      printk("**ERROR: %d on: %s\n", status, cmd);
    }
    sprintf(cmd, "cp -p %s %s", tlmfile, path);
    printk("*cp tlm to %s\n", path);
    printk("%s\n", cmd);
    if(system(cmd)) {
      printk("**ERROR: on: %s\n", cmd);
    }
    if((status = drms_close_record(rs_tlm, DRMS_INSERT_RECORD))) {
      printk("**ERROR: drms_close_record failed for %s\n", TLMSERIESNAME);
    }
    /*drms_commit(drms_env);*/

    sprintf(cmd, "/bin/mv %s %s", name, outdir);
    /*BeginTimer(7);*/
    printk("*mv qac file to %s\n", outdir);
    printk("%s\n", cmd);
    if(system(cmd)) {
      printk("***Error on: %s\n", cmd);
    }
    sprintf(cmd, "/bin/mv %s %s", tlmfile, outdir);
    printk("*mv tlm file to %s\n", outdir);
    printk("%s\n", cmd);
    if(system(cmd)) {
      printk("***Error on: %s\n", cmd);
    }

    /* new stuff to do lev0 below !!!!TBD check !!! */
    sprintf(xxname, "%s/%s.tlm", path, tlmname);
    filename = (char *)rindex(tlmname, '_');
    sprintf(vername, "%s", filename);	/* e.g. _00 or _01 etc. */
    if(strcmp(vername, "_00")) {	/* this is a higher vers # file */
      higherversion = 1;
      printk("Higher version tlm found: %s.tlm\n", tlmname);
    }
    if(get_tlm(xxname, rexmit, higherversion)) { /* lev0 extraction of image */
      printk("***Error in lev0 extraction for %s\n", xxname);
    }
    /*temp_drms_test_stuff(); /* !!!TEMP for testing */
    /*mk_test_ds();	      /* !!!TEMP for testing */

    ttmp = EndTimer(NUMTIMERS-1);
    printk("Rate tlm %s bytes in %f sec\n", tlmsize, ttmp);
    /*return; /* !!!TEMP for testing */
  }
  free(nameptr);
}

/* Initial setup stuff called when main is first entered.
 */
void setup()
{
  int i;
  time_t tval;
  struct tm *t_ptr;
  char string[128], cwdbuf[128], idstr[256];

  if (signal(SIGINT, SIG_IGN) != SIG_IGN)
    signal(SIGINT, sighandler);
  if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
    signal(SIGTERM, sighandler);
  signal(SIGALRM, alrm_sig);

  tval = time(NULL);
  t_ptr = localtime(&tval);
  sprintf(datestr, "%d.%02d.%02d_%02d:%02d:%02d", 
	  (t_ptr->tm_year+1900), (t_ptr->tm_mon+1),
	  t_ptr->tm_mday, t_ptr->tm_hour, t_ptr->tm_min, t_ptr->tm_sec);
  printk_set(h0log, h0log);	/* set for printk calls */
  printk("%s\n", datestr);
  getcwd(cwdbuf, 126);
  sprintf(idstr, "Cwd: %s\nCall: ", cwdbuf);
  sprintf(string, "ingest_lev0 started as pid=%d user=%s\n", getpid(), username);
  strcat(idstr, string);
  printk("*%s", idstr);
  strcpy(pchan, vc);		/* virtual channel primary */
  for(i=0; ; i++) {		/* ck for valid and get redundant chan */
    if(!strcmp(p_r_chan_pairs[i].pchan, pchan)) {
      strcpy(rchan, p_r_chan_pairs[i].rchan);
      break;
    }
    if(!strcmp(p_r_chan_pairs[i].pchan, "n/a")) {
      printk("!!ERROR: Invalid VCid (%s) specified\n", pchan);
      fprintf(stderr, "!!ERROR: Invalid VCid (%s) specified. Abort\n", pchan);
      abortit(1);
    }
  }
  umask(002);			/* allow group write */
  Image.initialized = 0;	/* init the two image structures */
  ImageOld.initialized = 0;
}

/* Module main function. */
int DoIt(void)
{
  char logname[128];
  int wflg = 1;

  if (nice_intro())
    return (0);
  if(!(username = (char *)getenv("USER"))) username = "nouser"; 
  vc = cmdparams_get_str(&cmdparams, "vc", NULL);
  tlmdir = cmdparams_get_str(&cmdparams, "indir", NULL);
  outdir = cmdparams_get_str(&cmdparams, "outdir", NULL);
  logfile = cmdparams_get_str(&cmdparams, "logfile", NULL);
  if (strcmp(vc, NOTSPECIFIED) == 0) {
    fprintf(stderr, "'vc' virt channel must be specified.  Abort\n");
    return(1);
  }
  if (strcmp(tlmdir, NOTSPECIFIED) == 0) {
    fprintf(stderr, "'indir' must be specified.  Abort\n");
    return(1);
  }
  if (strcmp(outdir, NOTSPECIFIED) == 0) {
    fprintf(stderr, "'outdir' must be specified.  Abort\n");
    return(1);
  }
  if (strcmp(logfile, NOTSPECIFIED) == 0) {
    sprintf(logname, H0LOGFILE, username, gettimetag());
  }
  else {
    sprintf(logname, "%s", logfile);
  }
  if((h0logfp=fopen(logname, "w")) == NULL)
    fprintf(stderr, "**Can't open the log file %s\n", logname);
  setup();
  while(wflg) {
    do_ingest();                /* loop to get files from the input dir */
    if(sigtermflg) { now_do_term_sig(); }
    sleep(120);			/* !!!TEMP */
    wflg = 0;			/* !!!TEMP */
  }
  return(0);
}

