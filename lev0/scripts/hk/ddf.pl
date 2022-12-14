#!/usr/bin/perl
##############################################################################
# Name:        ddf.pl - decode day files                                     #
#              Simplifed script to decode dayfile for today only without     #
#              using elaborate arguments used in gdfdrms.pl script.          #
#              CRON to get hmi,sdo,aia dayfiles from DRMS & send to for      #
#              decoding keywords and then write keywords to DRMS hk data     #
#              series by apid. This script sets values to process dayfiles   #
#              for either hmi,aia, or sdo  dayfiles for today only. Sets up  #
#              environment variable for hmi,aia, or sdo input dayfile series.#
#              Can turn on debug flag using DF_GDFDRMS_DEBUG and this is     #
#              passed to gdfdrms.pl. Can turn on  report flag which will     #
#              turn on reporting in gdfdrms.pl.                              #
#              APIDs to process determined by input arguments to gdfdrms.pl. #
#              Currently input args to gdfdrms.pl are for today, for apids   #
#              set in apidlist argument, and for source set in src argument. #
#              Log file is set using $logfile and is passed to gdfdrms.pl.   #
#              Setup and create apidlist files:ddf_apid_list_sdo_rtmon,      #
#              ddf_apid_list_hmi_hsb, ddf_apid_list_aia_hsb, etc.            #
#              This script can be run at command line too.                   #
# Execution:   (1)Run option to process dayfiles for today:                  #
#                                                                            #
#                 perl ddf.pl <project name of day file series> <source>     #
#                                                                            #
#              (2)The run options are show in help listing:                  #
#                                                                            #
#                 perl ddf.pl  -h                                            #
#                                                                            #
# Examples Execution(Possible cases as of today):                            #
#              perl ddf.pl hmi egsefm                                        #
#              perl ddf.pl hmi hsb                                           #
#              perl ddf.pl hmi moc                                           #
#              perl ddf.pl aia egsefm                                        #
#              perl ddf.pl aia hsb                                           #
#              perl ddf.pl aia moc                                           #
#              perl ddf.pl sdo moc                                           #
#              perl ddf.pl sdo rtmon                                         #
#              perl ddf.pl hmi rtmon                                         #
#              perl ddf.pl aia rtmon                                         #
# Limitation:  Setup required environment variables at top of file.          #
#               Must have apidlist file.                                     #
# Author:      Carl                                                          #
# Date:        Move from EGSE to JSOC software environment on May,7, 2008    #
##############################################################################
# main program                                                               #
##############################################################################
#set environment variables for HMI RELATED DECODE DAYFILE PROCESSING

#(0)get source argument
&check_agruments();

#(1)set paths
$hm=$ENV{'HOME'};
$script_dir=$ENV{'HK_SCRIPT_DIR'}="$hm/cvs/JSOC/proj/lev0/scripts/hk";
$ENV{'PATH'}="/usr/local/bin:/bin:/usr/bin:.:$script_dir";

#(2)cron setting
$ENV{'MAILTO'}="";

#(3)set debug mode
$dflg=$ENV{'DF_GDFDRMS_DEBUG'}=0;

#(4)setup log file for with name based on input argument(hmi,aia,sdo), 
$logfile=$ENV{'HK_DF_LOGFILE'}="$script_dir/log-$dspnm-ddf";

#(5)open log and record log info if in debug mode
if ($dflg) {open(LF,">>$logfile") || die "ERROR in ddf.pl:Can't Open <$logfile>: $!\n; exit;";}
if ($dflg) {print LF `date`;}
if ($dflg) {print LF "--->ddf.pl:debug:log file is <$ENV{'HK_DF_LOGFILE'}>\n";}
if ($dflg) {print LF "--->ddf.pl:debug:source argument is <$src>\n";}

#(6)set report mode on or off. if turn on, set report name want to use below.Creates gzipped report.
$rptflg=$ENV{'DF_REPORT_FLAG'}=0;

#(7)set report naming convention for using input argument(hmi,aia,sdo) if reporting is turned on
$ENV{'DF_PROJECT_NAME_FOR_REPORT'}="$dspnm";
$ENV{'DF_DATA_TYPE_NAME_FOR_REPORT'}="lev0";

####log info####
if ($dflg) {print LF "--->ddf.pl:debug:ARGV[0]: $ARGV[0]\n";}
if ($dflg) {print LF "--->ddf.pl:debug:ARGV[1]: $ARGV[1]\n";}
if ($dflg) {print LF "--->ddf.pl:debug:ARGV[2]: $ARGV[2]\n";}

#(8)get apid list to use
$list=&get_apid_list();

#(9)set up command for  getting dayfile from drms and then writing keywords to series
# execute and retrieve dayfile marked with today's time stamp only and decode keywords.
$command="perl $script_dir/gdfdrms.pl  apidlist=$script_dir/$list  src=$src";

#Note:Used to test with known data. do:perl ddf.pl hmi egsefm
#$command="perl $script_dir/gdfdrms.pl  apidlist=$script_dir/$list  start=20081001 end=20081001 src=$src";

####log info####
if ($dflg) {print LF "--->ddf.pl:debug:Running:Command running:<$command>\n";}

#(10)execute to decode dayfiles using APIDs in apidlist
$log=`$command`;

####log info####
if($dflag) {print LF "--->ddf.pl:series name for input used was <$dsnm>\n";}
if ($dflg) {print LF "--->ddf.pl:debug:Finished. Command Log is <$log>\n";}
if ($dflg) {print LF `date`;}
if ($dflg) {close(LF);}


#############################################################################
# get_apid_list()
#############################################################################
sub get_apid_list
{
  #as convention keep apid list in file using this format so can easily add or delete apids
  return( "ddf_apid_list_$dspnm\_$src");
}

##############################################################################
# check_arguments()                                                          #
##############################################################################
sub check_agruments()
{
  #data series project name(i.e.,sdo,hmi,aia) for hk_dayfile data series
  $dspnm=$ARGV[0]; #data series project name(i.e.,sdo,hmi,aia)
  #source index in hk_dayfile data series
  $src=$ARGV[1];
  #check arguments
  if ($#ARGV <= 0 || $#ARGV > 2 )
  {
    print "Function Description:Decode Dayfile script decodes keywords from dayfile into HK By APID data series for todays dayfiles.\n";
    print "Usage: perl ddf.pl <project name of input dayfile> <dayfile source>\n";
    print "\-where project name of input dayfile which is either sdo, aia, or hmi\(i.e.,sdo=sdo.hk_dayfile,hmi=hmi.hk_dayfile,aia=aia.hk_dayfile\).\n";
    print "\-where source is source index value into dayfile data series and is either:hsb,moc,egsefm or rtmon.\n";
    exit;
  }
  elsif ("-h" eq substr($ARGV[0],0,2) )
  {
     print "Usage: perl ddf.pl <project name of input dayfile> <dayfile source>\nwhere project name is hmi,aia,sdo.\nwhere source is either:hsb,moc,egsefm,or rtmon.\n";
     exit;
  }
  elsif("hmi" eq substr($ARGV[0],0,3) or "aia" eq substr($ARGV[0],0,3) or "sdo" eq  substr($ARGV[0],0,3))
  {
    if("hsb" eq substr($ARGV[1],0,3) or "egsefm" eq substr($ARGV[1],0,6) or "egseem" eq  substr($ARGV[1],0,6) or "moc" eq  substr($ARGV[1],0,3) or "rtmon" eq  substr($ARGV[1],0,5) or "lmcmd" eq  substr($ARGV[1],0,5)   )
    {
      #print "okay";
    }
    else
    {
      print "ERROR: Entered incorrect source value. Use egsefm,hsb, or moc.\n";
      print "Usage: perl ddf.pl <project name of input dayfile> <dayfile source>\nwhere project name is hmi,aia,sdo.\nwhere source is either:hsb,moc,egsefm or rtmon.\n";
      exit;
    }
  }
  else
  {
     print "ERROR: Entered incorrect project name for data series value. Use sdo,hmi or aia\n";
     print "Usage: perl ddf.pl <project name of input dayfile> <dayfile source>\nwhere project name is hmi,aia,sdo.\nwhere source is either:hsb,moc,egsefm or rtmon.\n";
     exit;
  }
}
