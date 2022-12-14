/* ingest_dsds_a.c */

#include "jsoc_main.h"
#include "drms_types.h"
#include <time.h>
#include <math.h>

char *module_name = "opendsrecs";

#define kRecSetIn	"in"
#define kRecSetOut	"out"
#define kNameList	"map"
#define kNOT_SPEC	"Not Specified"

#define DIE(msg)	return(fprintf(stderr,"%s",msg),1)
#define DIE_status(msg)	return(fprintf(stderr,"%s, status=%d",msg,status),1)

ModuleArgs_t module_args[] =
{
     {ARG_STRING, kRecSetIn, kNOT_SPEC, "Input data series."},
     {ARG_STRING, kRecSetOut, kNOT_SPEC, "Output data series."},
     {ARG_STRING, kNameList, kNOT_SPEC, "Name conversion list."},
     {ARG_FLAG, "M", "0", "SkipMissingFiles - no records if DATAFILE is blank."},
     {ARG_FLAG, "v", "0", "verbose - more diagnostics"},
     {ARG_END}
};

// #include <sys/time.h>
TIME time_now()
  {
  TIME now;
  TIME UNIX_epoch = -220924792.000; /* 1970.01.01_00:00:00_UTC */
  struct timeval tp;
  gettimeofday(&tp, NULL);
  now = (double)tp.tv_sec + (double)tp.tv_usec/1.0e6;
  return(now +  UNIX_epoch);
  }
  

/* Name check code.  This code uses an external table to drive the mapping of
dsds keywords into drms keywords.  It should contain a row for each keyword in
the target drms series.  Each row should contain 3 fields, drms_name, dsds_name, action.
The action will be a string which matches the action table.  The action table
converts the string to an integer.
Sample file lines like:
  cadence CADENCE copy
  date DATE copy
Lines begging with "#" are comments and will be ignored.
Blank lines are not allowed.
*/

typedef struct NameListLookup_struct
  {
  char *drms_name;
  char *dsds_name;
  int action;
  struct NameListLookup_struct *next;
  } NameListLookup_t;

typedef enum {
  ACT_NOP, ACT_COPY, ACT_ANGLE, ACT_CENTER, ACT_TIME, ACT_AU, ACT_NOT_FOUND
  } Action_t;

Action_t actions[] = {
  ACT_NOP, ACT_COPY, ACT_ANGLE, ACT_CENTER, ACT_TIME, ACT_AU, ACT_NOT_FOUND
  };

char *action_names[] = {
  "nop",   "copy",   "pangle",   "center",   "time",  "au",   "done"
  };

int keyNameCheck(char *name, char **fromname)
  {
  static int first_call = 1;
  static NameListLookup_t *actionlist;
  NameListLookup_t *this;
  if (first_call)
    {
    char *actionlistname;
    FILE *flist;
    char drms_name[100], dsds_name[100], action_name[100];
    char line[1024];
    // int action;
    NameListLookup_t *last;

    first_call = 0;
    actionlistname = cmdparams_get_str(&cmdparams, kNameList, NULL);
    if (strcmp(actionlistname, kNOT_SPEC) == 0)
      {
      fprintf(stderr, "Name mapping list must be specified\n");
      exit(1);
      }
    flist = fopen(actionlistname, "r");
    if (!flist)
      {
      fprintf(stderr, "Name mapping file not found\n");
      exit(1);
      }
    last = actionlist = (NameListLookup_t *)malloc(sizeof(struct NameListLookup_struct));
    last->next = NULL;
    while (fgets(line, 1024, flist))
      {
      if (line[0] == '#')
        continue;
      else if (sscanf(line,"%s %s %s\n", drms_name, dsds_name, action_name)==3)
        {
        int iname;
	this = last;
        last = this->next = (NameListLookup_t *)malloc(sizeof(struct NameListLookup_struct));
        last->next = NULL;
        this->drms_name = strdup(drms_name);
        this->dsds_name = strdup(dsds_name);
        this->action = ACT_NOT_FOUND;
        for (iname=0; actions[iname] != ACT_NOT_FOUND; iname++)
          if (strcmp(action_name, action_names[iname]) == 0)
            {
            this->action = actions[iname];
            break;
            }
        }
      else fprintf(stderr, "Name map read error on line containing %s, ignoring this line\n",line);
      }
    }
  /* lookup name and return action */
  for (this=actionlist; this->next; this = this->next)
    {
    if (strcmp(this->drms_name, name) == 0)
      {
      *fromname = this->dsds_name;
      return(this->action);
      }
    }
  *fromname = name;
  return(ACT_NOT_FOUND);
  }

int DoIt(void) 
   {
   int status = DRMS_SUCCESS;
   int SkipMissingFiles;
   int verbose;
   int nRecs, iRec;
   char *inRecQuery, *outRecQuery;
   DRMS_RecordSet_t *inRecSet, *outRecSet; 

   inRecQuery = cmdparams_get_str(&cmdparams, kRecSetIn, NULL);
   outRecQuery = cmdparams_get_str(&cmdparams, kRecSetOut, NULL);
   SkipMissingFiles = cmdparams_get_int(&cmdparams, "M", NULL) != 0;
   verbose = cmdparams_get_int(&cmdparams, "v", NULL) != 0;

   if (strcmp(inRecQuery, kNOT_SPEC) == 0 || strcmp(outRecQuery, kNOT_SPEC) == 0)
      DIE("Both the "kRecSetIn" and "kRecSetOut" dataseries must be specified.\n");

   inRecSet = drms_open_records(drms_env, inRecQuery, &status);
   if (!inRecSet)
      DIE_status("Input dataseries not found\n");
   if ((nRecs = inRecSet->n) == 0)
      DIE("No input records found\n");
   printf("%d input records found\n", nRecs);

   for (iRec=0; iRec<nRecs; iRec++)
      {
      char *DataFile;
      int Record_OK = 1;
      DRMS_Record_t *inRec, *outRec;
      DRMS_Keyword_t *outKey;
      DRMS_Segment_t *inSeg, *outSeg;
      HIterator_t *outKey_last = NULL;
//      DRMS_Link_t *outLink;

      /* create output series rec prototype */
      inRec = inRecSet->records[iRec];
      outRecSet = drms_create_records(drms_env, 1, outRecQuery, DRMS_PERMANENT, &status);
      if (!outRecSet || outRecSet->n != 1)
         DIE_status("Output dataseries not found or can't create records\n");

      outRec = outRecSet->records[0];

      /* loop through all target keywords */
      outKey_last = NULL;
      while (outKey = drms_record_nextkey(outRec, &outKey_last, 1))
	{
	char *wantKey, *keyName = outKey->info->name;
        int action = keyNameCheck(keyName, &wantKey);
        if (!drms_keyword_inclass(outKey, kDRMS_KeyClass_Explicit))
	    continue;  // skip implicit keywords.
        switch (action)
          {
	  case ACT_NOP:
		break;
	  case ACT_COPY:
		{
		DRMS_Value_t inValue = {DRMS_TYPE_STRING, NULL};
		inValue = drms_getkey_p(inRec, wantKey, &status);
		if (status == DRMS_ERROR_UNKNOWNKEYWORD)
			break;
			if (status && verbose)fprintf(stderr,"*** ACT_COPY drms_getkey_p %s status=%d\n",wantKey,status);
		drms_setkey_p(outRec, keyName, &inValue);
		if ((inValue.type == DRMS_TYPE_STRING) && inValue.value.string_val)
		  free(inValue.value.string_val);
		inValue.value.string_val = NULL;
                break;
                }
	  case ACT_ANGLE:
		{ /* on CROTA2 set CROTA2, SAT_ROT, INST_ROT */
		double pangle, sat_rot;
		pangle = drms_getkey_double(inRec, "SOLAR_P", &status);
		if (status && verbose)fprintf(stderr,"*** ACT_ANGLE drms_getkey_double SOLAR_P status=%d, pangle=%f\n",status,pangle);
		sat_rot = -pangle;
		drms_setkey_double(outRec, "CROTA2", sat_rot);
		drms_setkey_double(outRec, "SAT_ROT", sat_rot);
		drms_setkey_double(outRec, "INST_ROT", 0.0);
		break;
		}
	  case ACT_CENTER:
		{ /* on CRPIX1 set CRPIX1, CRPIX2, CRVAL1, CRVAL2 */
		double x0, y0;
		x0 = drms_getkey_double(inRec, "X0", &status);
		if (status && verbose)fprintf(stderr,"*** ACT_CENTER drms_getkey_double X0 status=%d, x0=%f\n",status,x0);
		y0 = drms_getkey_double(inRec, "Y0", &status);
		if (status && verbose)fprintf(stderr,"*** ACT_CENTER drms_getkey_double Y0 status=%d, y0=%f\n",status,y0);
		drms_setkey_double(outRec, "CRPIX1", x0+1.0);
		drms_setkey_double(outRec, "CRPIX2", y0+1.0);
		drms_setkey_double(outRec, "CRVAL1", 0.0);
		drms_setkey_double(outRec, "CRVAL2", 0.0);
		break;
		}
	  case ACT_TIME:
		{ /* on T_OBS set T_OBS, DATE-OBS, EXPTIME, CADENCE, TIME, MJD */
		char timebuf[1024];
		TIME MJD_epoch = -3727641600.000; /* 1858.11.17_00:00:00_UT  */
		TIME UNIX_epoch = -220924792.000; /* 1970.01.01_00:00:00_UTC */
		TIME t_obs, t_rec, date__obs, mjd, now;
		double t_step;
		double exptime, mjd_day, mjd_time;
		t_rec = drms_getkey_time(inRec, "T_REC", &status);
		if (status && verbose)fprintf(stderr,"*** ACT_TIME drms_getkey_time T_REC status=%d, t_rec=%f\n",status,t_rec);
		t_step = drms_getkey_double(outRec, "T_REC_step", &status); /* note from outRec */
		if (status && verbose)fprintf(stderr,"*** ACT_TIME drms_getkey_double T_REC_step status=%d, t_step=%f\n",status,t_step);
		// exptime = t_step; /* note - for lev1.5 */
                exptime = drms_getkey_double(inRec, "INTERVAL", &status);
		t_obs = drms_getkey_time(inRec, "T_OBS", &status);
		if (status && verbose)fprintf(stderr,"*** ACT_TIME drms_getkey_time T_OBS status=%d, t_obs=%f\n",status,t_obs);
                if (status == DRMS_ERROR_UNKNOWNKEYWORD) // T_OBS is not present, skip this record.
		date__obs = t_obs - exptime/2.0;
		mjd = date__obs - MJD_epoch; /* sign error corrected by tplarson 2008.05.29 */
		mjd_day = floor(mjd / 86400.0);
		mjd_time = mjd - 86400.0 * mjd_day;
		now = (double)time(NULL) + UNIX_epoch;
		drms_setkey_time(outRec, "T_REC", t_rec);
		drms_setkey_time(outRec, "T_OBS", t_obs);
		drms_setkey_double(outRec, "EXPTIME", exptime);
		drms_setkey_double(outRec, "CADENCE", t_step);
		drms_setkey_double(outRec, "MJD", mjd_day);
		drms_setkey_double(outRec, "TIME", mjd_time);
                // allow either string or time types for DATE and DATE_OBS
                if (drms_keyword_type(drms_keyword_lookup(outRec, "DATE__OBS", 1)) == DRMS_TYPE_STRING)
                  {
		  sprint_time(timebuf, date__obs, "ISO", 0);
		  drms_setkey_string(outRec, "DATE__OBS", timebuf);
                  }
                else
                  drms_setkey_time(outRec, "DATE__OBS", date__obs);
                if (drms_keyword_type(drms_keyword_lookup(outRec, "DATE", 1)) == DRMS_TYPE_STRING)
                  {
		  sprint_time(timebuf, now, "ISO", 0);
		  drms_setkey_string(outRec, "DATE", timebuf);
		  }
                else
                  drms_setkey_time(outRec, "DATE", now);

		break;
		}
	  case ACT_AU:
		{
# define AU_m (149597870691.0)
//#define AU_m (1.49597892e11) bad
		double au;
		au = drms_getkey_double(inRec, "OBS_DIST", &status);
		if (status && verbose)fprintf(stderr,"*** ACT_AU drms_getkey_double OBS_DIST status=%d,au=%f\n",status,au);
		if (status != DRMS_ERROR_UNKNOWNKEYWORD)
		    drms_setkey_double(outRec, "DSUN_OBS", au * AU_m);
		break;
		}
          case ACT_NOT_FOUND:
          default:
                /* name not in table, just take same name from input series */
                {
		DRMS_Value_t inValue = {DRMS_TYPE_STRING, NULL};
                DRMS_Keyword_t *outKey = drms_keyword_lookup(outRec, keyName, 0);
                if (drms_keyword_isconstant(outKey))
			break;
                inValue = drms_getkey_p(inRec, keyName, &status);
		if (status == DRMS_ERROR_UNKNOWNKEYWORD)
			break;
		if (status && verbose)fprintf(stderr,"*** DEFAULT drms_getkey_p %s status=%d\n",keyName, status);
                drms_setkey_p(outRec, keyName, &inValue);
		if ((inValue.type == DRMS_TYPE_STRING) && inValue.value.string_val)
		  free(inValue.value.string_val);
		inValue.value.string_val = NULL;
                break;
                }
          }
        }

      /* assume only one segment */
	DataFile = drms_getkey_string(inRec,"DATAFILE",&status);
	if (status && verbose)fprintf(stderr,"*** Segment Read DATAFILE status=%d\n",status);
	char filepath[DRMS_MAXPATHLEN];
	inSeg = drms_segment_lookupnum(inRec, 0);
        if (inSeg)
	  drms_segment_filename(inSeg, filepath);
        else 
          filepath[0] = '\0';
	if (*DataFile && access(filepath, R_OK | F_OK) == 0)
	  {
          outSeg = drms_segment_lookupnum(outRec, 0);
          if (inSeg && outSeg)
            {
            DRMS_Array_t *data;
            /* read the data ad doubles so allow rescaling on output */
            data = drms_segment_read(inSeg, DRMS_TYPE_DOUBLE, &status);
            if (!data)
                  {
                  fprintf(stderr, "Bad data record %d\n",iRec);
                  DIE_status("giveup\n");
                  }
            /* use the zero and offset values in the JSD for the new record segment */
            data->bscale = outSeg->bscale;
            data->bzero = outSeg->bzero;
            drms_segment_write(outSeg, data, 0);
            drms_free_array(data);
            Record_OK = 1;
            }
          else
            DIE("Bad data segment lookup, in or out\n");
	  }
        else
	  { /* record is missing */
 	  int qualstat = 0;
          int quality = drms_getkey_int(outRec, "QUALITY", &qualstat);
	  if (!qualstat)
	    drms_setkey_int(outRec, "QUALITY", 0X80000000 | quality); 
	  if (drms_keyword_lookup(outRec, "DATAVALS", 0))
	    drms_setkey_int(outRec, "DATAVALS", 0);
          if (SkipMissingFiles)
             {
             Record_OK = 0;
             if (verbose) 
               fprintf(stderr,"DSDS Record %d has no datafile, T_REC=%s, set missing.\n", iRec, drms_getkey_string(outRec,"T_REC",NULL));
             }
          else
             Record_OK = 1;
	  }

      /* loop through all target links */
      //while ( (outLink=(DRMS_Keyword_t *)hiter_getnext(&link_list)) )
        //{
        ///* assume no links - do nothing here now. */
        //}
      drms_close_records(outRecSet,(Record_OK ? DRMS_INSERT_RECORD : DRMS_FREE_RECORD));
      }

   drms_close_records(inRecSet,DRMS_FREE_RECORD);

   return(DRMS_SUCCESS);
   }

