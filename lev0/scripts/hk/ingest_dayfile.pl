#!/usr/bin/perl
##############################################################################
# Name:        ingest_dayfile.pl - Ingest hsb or moc or egsefm dayfiles      #
# Description: Used to ingest high spreed bus formatted dayfiles to DRMS     #
#              or moc dayfiles from moc server or egsefm dayfiles from LMSAL.#
#              Will ingest xml from moc. Currently no xml for hsb and egsefm.#
#              The script can gather a list of dayfiles based on arguments   #
#              used for script and then write those file to DRMS. There are  #
#              a few different ways to gather up files based on apid and     #
#              and dates. Set path to day files using environment variable   #
#              DF_HSB_DAYFILE_DIRECTORY in this file. Switch on a small      #
#              amount of debug using DF_INGEST_HSB_DEBUG set to 1 below.     #
# Execution:   (1)The run options are show in help listing:                  #
#              ingest_dayfile -h                                             #
# Limitation:  Setup required environment variables at top of file based on  #
#              where running. Script works using hsb,moc  dayfile file       #
#              format. Here are example input day file based on source of    #
#              dayfiles:                                                     #
#                   hsb_0445_2008_04_15_10_41_00.hkt  <hsb>                  #
#                   0022_2008_050_02.hkt              <moc>                  #
#                   200805030_0x01d.hkt               <egsefm>               #
#             Here is formt of moc xml files:                                #
#                   0022_2008_050_02.hkt.xml          <moc>                  #
# Author:      Carl                                                          #
# Date:        Move from EGSE to JSOC software environment on April 15, 2008 #
##############################################################################
# main program                                                               #
##############################################################################
  #set environment variables specific for this script
  $ENV{'DF_INGEST_HSB_DEBUG'}="0";

  #common setting for all environments
  $ENV{'SUMSERVER'}="d02.Stanford.EDU";
  $hm=$ENV{'HOME'};
  $ENV{'MAILTO'}="";
  $ENV{'DF_DRMS_EXECUTABLES'}="$hm/cvs/JSOC/bin/linux_x86_64";
  $script_dir="$hm/cvs/JSOC/proj/lev0/scripts/hk";
  $ENV{'PATH'}="/usr/local/bin:/bin:/usr/bin:.:$script_dir:$ENV{'DF_DRMS_EXECUTABLES'}";

  # set debug flag 1 to turn on and 0 to turn off
  $dflg=$ENV{'DF_INGEST_HSB_DEBUG'};

  #check for any arguments passed in command
  &check_arguments();

  # set log file based on sdo, moc, or egsefm
  if ($source eq "hsb")
  {
    $logfile="$hm/cvs/JSOC/proj/lev0/scripts/hk/log-df-hsb";
  }
  elsif ($source eq "moc")
  {
    $logfile="$hm/cvs/JSOC/proj/lev0/scripts/hk/log-df-moc";
  }
  elsif ($source eq "egsefm")
  {
    $logfile="$hm/cvs/JSOC/proj/lev0/scripts/hk/log-df-egsefm";
  }

  # open log file
  open(LF,">>$logfile") || die "Can't Open $logfile: $!\n";
  print LF `date`;

  # set path to dayfiles using source -these vary based on where dayfiles are placed
 if ($source eq "hsb")
 {
   #$ENV{'DF_DAYFILE_DIRECTORY'}="/home1/carl/cvs/JSOC/proj/lev0/apps/data/hk_hsb_dayfile";
   $ENV{'DF_DAYFILE_DIRECTORY'}="/surge/production/lev0/hk_hsb_dayfile";
   #for production#
   #$ENV{'DF_DAYFILE_DIRECTORY'}="/tmp21/production/lev0/hk_hsb_dayfile";
 }
 elsif ($source eq "moc")
 {
   #$ENV{'DF_DAYFILE_DIRECTORY'}="/home/carl/cvs/JSOC/proj/lev0/scripts/hk/SDO";
   #for production#
   $ENV{'DF_DAYFILE_DIRECTORY'}="/tmp21/production/lev0/hk_moc_dayfile";
 }
 elsif ($source eq "egsefm")
 {
   $ENV{'DF_DAYFILE_DIRECTORY'}="/tmp20/production/hmi_hk";
   #for production#
   #$ENV{'DF_DAYFILE_DIRECTORY'}="/tmp21/production/lev0/hk_egsefm_dayfile";
 }

  #get list of initial jsoc definition files to use
  &get_dayfile_list();

  #get list of apids to do 
  &get_apid_to_do();

  #get list of apids to do 
  &get_date_to_do();

  #go through day files in delivery directory looking for all files for given apid
  #create a list of files to ingest
  &get_list_to_ingest();

  #get lookup table of data series names that go with each APID
  &get_dsn_list();

  #ingest files in list as day files
  &ingest_day_files();
 
  #close logfile
  print LF `date`;
  close LF;

#############################################################################
# subroutine check arguments and set flags                                  #
#############################################################################
sub check_arguments()
{
  $help_flg= "0";
  $apid_list_flg="0";
  $date_range_flg="0";
  $view_flg="0";
  
  if ($#ARGV < 0)
  {
    $help_flg="1";
  }
    
  if ($#ARGV >= 0)
  {
    if ($ARGV[0] eq "-h" || $ARGV[0] eq "-help" || $ARGV[0] eq "-H")
    {
      $help_flg = "1";
    }
    elsif (substr($ARGV[0],0,2) eq "-v" )
    {
      #view only dayfiles want to load
      $view_flg="1";

      if (substr($ARGV[1],0,9) eq "apidlist=" )
      {
        #use file to get list of apids to create map files for
        $apid_list_flg = "1";
      }
      elsif (substr($ARGV[1],0,5) eq "apid=" )
      {
        #use apid following -a to create map files
        #push decimal character value in this format dddd. Example: -a 0001
        $apid_list_flg = "2";
      }
 
    }
    elsif (substr($ARGV[0],0,9) eq "apidlist=" )
    {
      #use file to get list of apids to create map files for
      $apid_list_flg = "1";
    }
    elsif (substr($ARGV[0],0,5) eq "apid=" )
    {
      #use apid following -a to create map files 
      #push decimal character value in this format dddd. Example: -a 0001
      $apid_list_flg = "2";
    }
  }
  if ($#ARGV >= 1 && $view_flg eq "0")
  {
    if ( (substr($ARGV[1],0,6) eq "start=") and (substr($ARGV[2],0,4) eq "end=") )
    {
      #use file to get list of apids to create map files for
      $date_range_flg = "1";
    }
    elsif (substr($ARGV[1],0,8) eq "dsnlist=" )
    {
      #use file to get list of apids to create map files for
      $ds_list_flg = "2";
      if ( substr($ARGV[2],0,4) eq "src=")
      {
        $source=substr($ARGV[2],4,6);
      }
      else
      {
         print  "WARNING: Did not use src argument. Exiting script\n";
         exit;
      }
    }
    else
    {
      print  "Warning:arguments entered not correct\n";
      exit;
    }
  }
  elsif ($#ARGV >= 2 && $view_flg eq "1")
  {
    if ( (substr($ARGV[2],0,6) eq "start=") and (substr($ARGV[3],0,4) eq "end=") )
    {
      #use file to get list of apids to create map files for
      $date_range_flg = "1";
    }
    elsif (substr($ARGV[2],0,8) eq "dsnlist=" )
    {
      #use file to get list of apids to create map files for
      $ds_list_flg = "2";
      if ( substr($ARGV[3],0,4) eq "src=")
      {
        $source=substr($ARGV[3],4,6);
      }
      else
      {
         print  "WARNING: Did not use src argument. Exiting script\n";
         exit;
      }
    }
    else
    {
      print  "Warning:arguments entered not correct\n";
      exit;
    }
 }


  if ($#ARGV >= 3 && $view_flg eq "0")
  {
    if ( substr($ARGV[3],0,8) eq "dsnlist=") 
    {
      #use file to get list of apids to create map files for
      $ds_list_flg = "1";
       if ( substr($ARGV[4],0,4) eq "src=")
       {
         $source=substr($ARGV[4],4,6);
       }
       else
       {
         print  "WARNING: Did not use src argument. Exiting script\n";
         exit;
       }
    }
    else
    {
       print  "Warning argument 3 is not correct: $ARGV[3]\n";
       print  "This is the data series name list. \n";
       print  "Existing. Enter correct value. \n";
       exit;
    }
  }
  elsif ($#ARGV >= 4 && $view_flg eq "1")
  {
    if ( substr($ARGV[4],0,8) eq "dsnlist=") 
    {
      #use file to get list of apids to create map files for
      $ds_list_flg = "1";
       if ( substr($ARGV[5],0,4) eq "src=")
       {
         $source=substr($ARGV[5],4,6);
       }
       else
       {
         print  "WARNING: Did not use src argument. Exiting script\n";
         exit;
       }
    }
    else
    {
       print  "Warning argument 3 is not correct: $ARGV[3]\n";
       print  "This is the data series name list. \n";
       print  "Existing. Enter correct value. \n";
       exit;
    }
  }

  if ( $help_flg eq "1")
  {
     &show_help_info;
  }
}
#############################################################################
# subroutine show_help_info: show help information                          #
#############################################################################
sub show_help_info
{
  print "Help Listing\n";
  print "(1)Ways to Execute Perl Script: \n";
  print "(1a)Ingest Day Files using apidfile option will ingest all files based on apids contained in file:\n
        ingest_dayfile.pl apidlist=<filename containing APID List to do> dsnlist=<file with ds lookup list> src=<source of data>\n";
  print "        Example: ingest_dayfile.pl apidlist=./df_apid_list_day_file_hsb dsnlist=./df_apid_ds_list_for_hsb src=hsb\n";
  print "        Example: ingest_dayfile.pl apidlist=./df_apid_list_day_file_moc dsnlist=./df_apid_ds_list_for_moc src=moc\n";
  print "        Example: ingest_dayfile.pl apidlist=./df_apid_list_day_file_egsefm dsnlist=./df_apid_ds_list_for_src src=egsefm\n\n";
  print "(1b)Ingest Day Files using apid option will ingest all files for given apid value:\n
        ingest_hsb_dayfile.pl apid=<apid-value in decimal format> dsnlist=<file with ds lookup list> src=<source of data>\n";
  print "        Example: ingest_dayfile.pl apid=029  dsnlist=./df_apid_ds_list_for_moc src=moc\n";
  print "        Example: ingest_dayfile.pl apid=445  dsnlist=./df_apid_ds_list_for_hsb src=hsb\n";
  print "        Example: ingest_dayfile.pl apid=029  dsnlist=./df_apid_ds_list_for_egsefm src=egsefm\n\n";
  print "(1c)Ingest Day Files using date range with apidfile option:\n
        ingest_dayfile.pl apidlist=<file> start=<yyyymmdd> end=< yyyymmdd> dsnlist=<file with ds lookup list> src=<source of data>\n";
  print "        Example: ingest_dayfile.pl apidlist=./df_apid_list_day_file_moc start=20080518 end=20080530 dsnlist=./df_apid_ds_list_for_moc src=moc\n\n";
  print "(1d)Ingest Day Files using date range with apid option:\n
        ingest_dayfile.pl apid=<apid in decimal>  start=<yyyymmdd> end=<yyyymmdd>  dsnlist=<file with ds lookup list> src=<source of data>\n";
  print "        Example: ingest_dayfile.pl apid=445 start=20070216  end=20070218 dsnlist=./df_apid_ds_list_for_egsefm src=egsefm\n\n";
  print "(1e)Get Help Information:\n
         ingest_hsb_dayfile.pl -h  or  ingest_hsb_dayfile.pl -help\n\n";
  print "(1f)View what going to save in data series by adding -v as first argument. This should work for 1a,1b,1c,and 1d cases.:\n
        ingest_dayfile.pl -v apidlist=<file> start=<yyyymmdd> end=< yyyymmdd> dsnlist=<file with ds lookup list> src=<source of data>\n";
  print "        Example: ingest_dayfile.pl -v apidlist=./df_apid_list_day_file_hsb start=20070216 end=20070218 dsnlist=./df_apid_ds_list_for_hsb src=hsb\n\n";
  print "*Note:Currently using option ():\n" ;
  print "(2) Requires setup of JSD file and create series before running script.\n";
  print "(3) Requires setup of environment variable DF_DAYFILE_DIRECTORY\n";
  print "(3a)Used to store location of input day files of this script.\n";
  print "(3b)Example setting: setenv DF_DAYFILE_DIRECTORY /tmp20/production/hmi_hk \n";
  print "(4) Requires setup of apid list in file when use \"apidlist\" option\n";
  print "(4a)Enter values in file in decimal format(i.e.,0001,0015,0021,0445,0475).\n";
  print "(4b)Example format and file of apid list is located in this current file: ./df_apid_list_day_files\n";
  print "(5) Requires setup of data series name and apid list in file when use \"dsname\" argument.\n";
  print "(5a)Enter values in file in decimal format for APID value and this data series name(i.e.,0445  hmi_ground.hk_dayfile).\n";
  print "(5b)Example format and file of apid list is located in this current file: ./df_apid_ds_list_for_hsb\n";
  print "(5c)Create a file for different types of data to save files in correct series names.\n";
  print "(5d)For example, create df_apid_ds_list_for_hsb to save hsb dayfiles in series names that are for hmi.hk_dayfile and aia.hk_dayfile data series.View example.\n";
  print "(5e)Other Examples files:df_apid_ds_list_for_hsb, df_apid_ds_list_for_egsefm, df_apid_ds_list_for_moc, df_apid_ds_list_for_egseem, etc.\n";
  print "(6)****Limitation****: a)Works only on hsb dayfile formats(i.e., hsb_0445_2007_11_08_16_52_11_00.hkt.\n";
  print "                       b)Works only on moc dayfile formats(i.e., 0029_2007_150_02.hkt.\n";
  print "                       c)Enter arguments in specified order when running script.\n";
  exit;
}
#############################################################################
# subroutine get_dayfile_list: get list lm day file brought over            #
#############################################################################
sub get_dayfile_list()
{
  #Open input data files
  $dir_init_df=$ENV{'DF_DAYFILE_DIRECTORY'};
  #open directory file handle to read in initial jsd files.
  opendir(DIR_DF, $dir_init_df) || die "Can't open-1:$!\n";
  #get list of day files
  @df_files=readdir( DIR_DF );
  #close directory file handle
  closedir DIR_DF; 
  push(@df_files, "");
  if ($dflg eq "1") {print LF "get_dayfile_list():dayfile_list is @df_files\n";}
}
#############################################################################
# subroutine get_apid_to_do: gets list of apid to create maps files for     #
#############################################################################
sub get_apid_to_do
{
  # create list of files to process using apid values in file
  if ($apid_list_flg eq "1")
  {
    if ($view_flg eq "0")
    {
      $fn= substr($ARGV[0],9);
    }
    else
    {
      $fn= substr($ARGV[1],9);
    }
    open(FILE_APID_LIST, "$fn") || die "Can't Open-2: $fn file: $!\n";
    while (<FILE_APID_LIST>)
    {
      push(@all_apids, $_) ;
    }
    close FILE_APID_LIST ;

  if ($dflg eq "1") { print LF "get_apid_to_do():...doing decimal formatted apids\n @all_apids";}
  }
  elsif ($apid_list_flg eq "2")
  {
    #push decimal character value in this format dddd.Example 0001
    if ($view_flg eq "0")
    {
      push(@all_apids, substr($ARGV[0],5));
    }
    else
    {
      push(@all_apids, substr($ARGV[1],5));

    }
    if ($dflg eq "1") { print LF "get_apid_to_do():...doing decimal formatted apids @all_apids\n";}
  }
  else 
  {
    print LF "WARNING: Not valid apid list flag value\n";
  }
}
#############################################################################
# subroutine get_date_to_do: gets date range to do                          #
#############################################################################
sub get_date_to_do
{
  # create list of files to process using apid values in file
  if ($date_range_flg eq "1")
  {
    if ($view_flg eq "0")
    {
      $date_r1= substr($ARGV[1],6,8);
      $date_r2= substr($ARGV[2],4,8);
    }
    else
    {
      $date_r1= substr($ARGV[2],6,8);
      $date_r2= substr($ARGV[3],4,8);
    }
  }
}
#############################################################################
#############################################################################
# subroutine get_list_to_ingest                                             #
#############################################################################
sub get_list_to_ingest()
{
  #go through day files in delivery directory looking for first apid
  #create a list of files to ingest
  foreach $apid (@all_apids)
  {
    $strapid=  $apid;
    foreach $file (@df_files)
    {
      if ($file eq "")
      {
        if ($dflg eq "1") {print LF "get_list_to_ingest:breaking. final item in list \n"};
        break;
      }

      #compare apid value in list with files
      if ($source eq "hsb")
      {
        $find_str= sprintf("hsb_%04s", $strapid);
      }
      elsif ($source eq "moc")
      {
        $find_str= sprintf("%04s_", $strapid);
      }
      elsif ($source eq "egsefm")
      {
        $find_str= sprintf(".%s%04s", "0x", $strapid);
      }
      if ($dflg eq "1") { print LF "get_list_to_ingest:find str is $find_str\n";}
      if ($apid_list_flg eq "1")
      {
        #chop $find_str;
        $find_str =~ s/\n//g;
      }
      if ($dflg eq "1") {print LF "get_list_to_ingest:compare file:<$file> vs apid:<$find_str>\n"};

      # check filenames and skip those in if clause
      if (( index  $file, $find_str ) != -1)
      {
        # take out filename with x at end of filename.
        # sort out 20070302_2157_text_ls.0x0002 or 20070302.0x0002x
        # or 20070216vac_cal_open.0x0002 or 20070221_1840_text.0x0002
        $new=substr($file,15,1);
        if ( (( index  $file, "ls" ) == -1) and ( ( index  $file, "vac" ) == -1) and
             (( index  $file, "text" ) == -1) and ( $new ne "x" ) and (index  $file, "xml") == -1 )
        {
          #if found one file with apid and date range then push file on list of lists
          if ($date_range_flg eq "1")
          {
            # check if file is within date range 
            # first get date from filename
            if ($source eq "hsb")
            {
              $filedate= substr($file,9,10);
              $filedate =~ s/_//g;
            }
            elsif  ($source eq "moc")
            {
              $year= substr($file,5,4);
              $dyear= substr($file,10,3);
              if($dflg eq "1") {print LF "get_list_to_ingest:year is $year\n"};
              if ($dflg eq "1") {print LF "get_list_to_ingest:dyear is $dyear\n"};
              ($filedate)=get_df_date($year, $dyear);
              $filedate =~ s/\.//g;
            }
            elsif ($source eq "egsefm")
            {
               $filedate=substr($file,0,8); 
            }
            if ($dflg eq "1") {print LF "get_list_to_ingest:filedate is $filedate\n"};

            # check filename date is within range of dates entered as arguments
            if( (int $date_r1)  <= (int $filedate) and 
                (int $date_r2)  >= (int $filedate ) )  
            {   
              push( @list, $file);
              if ($dflg eq "1") { print LF "get_list_to_ingest:found one push file on list: $file\n"};
            }
          }
          else
          {
            #if no date then push on list based only on apid
            push( @list, $file);
            if ($dflg eq "1") { print LF "get_list_to_ingest:found one push file on list: $file\n"};
          }
        }
      }
    }#for each file find apid
  }#for each apid to do
}
#############################################################################
# subroutine get_dsn_list(): get list of data series name for each APID     #
#############################################################################
sub get_dsn_list()
{
  my($fn);
  if ( $ds_list_flg  eq "1")
  { 
    if ($view_flg eq "0")
    {
      $fn= substr($ARGV[3],8);
    }
    else
    {
      $fn= substr($ARGV[4],8);
    }
  }
  elsif ( $ds_list_flg  eq "2")
  { 
    if ($view_flg eq "0")
    {
      $fn= substr($ARGV[1],8);
    }
    else
    {
      $fn= substr($ARGV[2],8);
    }
  }
  open(FILE_DSN_LIST, "$fn") || die "Can't Open-3: $fn file: $!\n";
  while (<FILE_DSN_LIST>)
  {
    push(@all_dsn_list, $_) ;
  }
  close FILE_DSN_LIST;
  if($dflg eq "1") {print LF "get_dsn_list:Using data series names for each APID: \n @all_dsn_list\n";}
}
#############################################################################
# subroutine get_series_name(): get data series name by looking up apid     #
#############################################################################
sub get_series_name()
{
  foreach $dsn_line (@all_dsn_list)
  {
    $dsnapid = substr ($dsn_line, 0, 4);
    if ($dsnapid eq $fapid)
    {
      #get series name
      @s_line = split / \s*/,$dsn_line;
      $dsn=$s_line[1];
      $dsn =~ s/\n//g;
      return  $dsn;
    }
  }
}
#############################################################################
# subroutine ingest_day_files                                               #
#############################################################################
#($foundflg)=&check_apid_list(@apidslist,$currapid);
#sub check_apid_list (@$)
sub ingest_day_files()
{
  $file=""; $xfile="";
  foreach $file (@list)
  {
    #get command
    $command="set_keys -c ";

    # get series name
    if ($source eq "hsb")
    {
      $fapid = substr($file, 4,4);
    }
    elsif ($source eq "moc")
    {
      $fapid = substr($file, 0,4);
    }
    elsif ($source eq "egsefm")
    {
      $fapid = substr($file, 11,4);
    }
    else
    {
      print LF "-->Warning:Unknown source <$source>\n";
      print LF "-->Warning:exiting setting dayfile in series\n";
      exit;
    }
    $dsn=get_series_name();
    $arg1=sprintf("%s%s","ds=", $dsn);

    # get date value
    if ($source eq "hsb")
    {
      $year=substr($file,9,4);
      $month=substr($file,14,2);
      $day=substr($file,17,2);
      $skdate=sprintf("%s.%s.%s",$year,$month,$day); 
    }
    elsif ($source eq "moc")
    {
      $year=substr($file,5,4);
      $dyear=substr($file,10,3);
      ($skdate)=get_df_date($year,$dyear);
    }
    elsif ($source eq "egsefm")
    {
      $year=substr($file,0,4);
      $month=substr($file,4,2);
      $day=substr($file,6,2);
      $skdate=sprintf("%s.%s.%s",$year,$month,$day); 
    }
    #DATE=2008.05.29_00:00:00.000_TAI
    # fix string format to DATE in time variable
    $time_skdate=sprintf("%10.10s_00:00:00_TAI",$skdate);

    #$arg2="DATE=$skdate";
    $arg2="DATE=$time_skdate";

    #get apid
    if ($source eq "hsb")
    {
      $aid=substr($file,4,4);
    }
    elsif  ($source eq "moc")
    {
      $aid=substr($file,0,4);
    }
    elsif ($source eq "egsefm")
    {
      $aid= hex substr($file,11, 4);
    }
    $arg3="APID=$aid";

    #get source
    $arg4="SOURCE=$source";

    #get day file name
    $arg5="file=$dir_init_df/$file";
    if($dflg eq "1") {print LF  "ingest_dayfiles:dayfile arg:<$arg5>\n";}

    #get xml file name
    if ($source eq "hsb")
    {
      #$xfile=sprintf("%s/%s.xml",$dir_init_df,$file);
      #$arg6="xmlfile=$xfile";
      $arg6="";
    }
    elsif  ($source eq "moc")
    {
      $xfile=sprintf("%s/%s.xml",$dir_init_df,$file);
      $arg6="xmlfile=$xfile";
    }
    elsif ($source eq "egsefm")
    {
      #$xfile=sprintf("%s/%sx",$dir_init_df,$file);
      #$arg6="xmlfile=$xfile";
      $arg6="";
    }
    if($dflg eq "1") {print LF "ingest_dayfiles:xmlfile arg: <$arg6>\n";}

    # check command line to load dayfiles in DRMS day file data series
    @args=("$command","$arg1 ","$arg2 ","$arg3 ","$arg4 " ,"$arg5", "$arg6");
    print LF "-->Load dayfile command: <@args>\n";


    if ($view_flg eq "1")
    {
      #print " skip executing load and viewing data to be loaded in data series only\n";
    }
    else
    {

      # load dayfile 
      system(" @args ") == 0 or die "system(\"  @args\")\nFailure cause:Need to create data series. Check if exists:describe_series $dsn\nFailed:$?" ; 

      # check if files loaded, if are add to file for deleting.
      # create command line
      $ckcommand="show_keys";
      @checkargs=("$ckcommand","$arg1\[$time_skdate\]\[$aid\]\[$source\]","-p seg=file,xmlfile \| grep -v file");
      if($dflg eq "1") {print LF "ingest_dayfiles:check arguments are <@checkargs>\n";}
      # execute check for files loaded in drms
      $check_ret_values=`@checkargs`;
      # check values received
      $check_ret_values =~ s/\n//g;
      if($dflg eq "1") {print LF "ingest_dayfiles:<$check_ret_values>\n";}
      # split file and xmlfile values returned
      @s_ret_values = split ' ', $check_ret_values;
      if($dflg eq "1") {print LF "ingest_dayfiles:first is <$s_ret_values[0]>\n";};
      if($dflg eq "1") {print LF "ingest_dayfiles:second is <$s_ret_values[1]>\n";}
      # check if files exist
      if ( -e $s_ret_values[0])
      {
         if($dflg eq "1") {print LF "ingest_dayfiles:file exists:$s_ret_values[0]\n";}
         #parse just filename
         my($directory, $filename) = $s_ret_values[0]=~ m/(.*\/)(.*)$/;
         if($dflg eq "1") {print LF "ingest_dayfiles:filename is <$filename>\n";}
         #add just filename to delete_file list
         if($filename ne "")
         {
           open(DELFILE,">>$script_dir/DF_DELETE_FILE_LIST") || die "(6)Can't Open $script_dir/DF_DELETE_FILE_LIST file: $!\n";
           print DELFILE "$filename\n";
           close DELFILE;
         }
      }
      if ( -e $s_ret_values[1])
      {
        if($dflg eq "1") {print LF "ingest_dayfiles:file exists:$s_ret_values[1]\n";}
        #parse just filename
        my($directory, $xmlfilename) = $s_ret_values[1]=~ m/(.*\/)(.*)$/;
        if($dflg eq "1") {print LF "ingest_dayfiles:xmlfilename is <$xmlfilename>\n";}
        #add just filename to delete_file list
        if($xmlfilename ne "")
        {
          open(DELFILE, ">>$script_dir/DF_DELETE_FILE_LIST") || die "(6)Can't Open $script_dir/DF_DELETE_FILE_LIST file: $!\n";
          print DELFILE "$xmlfilename\n";
          close DELFILE;
        }
      }


      #$log=`@args`;
      #print LF $log;
      if($dflg eq "1") {print LF "ingest_dayfiles:loading data to be loaded in data series only\n";}

    }
  }
  if ($view_flg eq "1")
  {
      print LF "ingest_dayfile:skipping executing load of dayfiles above and viewing data to be loaded in data series only\n";
  }
}
#############################################################################
# subroutine get_df_date                                                    #
#############################################################################
sub get_df_date($$)
{
  use Time::Local 'timelocal';
  use POSIX;

  # set day in year to day1
  $day1=$dyear ;

  # set month to 0 to use day of year to get time
  $mon1=0;

  # set year from filename
  $year1=$year - 1900;

  # call mktime
  $unixtime=mktime('','','',$day1,$mon1,$year1,'','');
  if($dflg eq "1") {print LF "get_df_date:unixtime is $unixtime\n"};

  #get broke up time
  ($sec,$min,$hour,$mday,$monoffset,$yearoffset,$wday,$yday,$isdst) = localtime($unixtime);

  # set year using year offset
  $year= $yearoffset + 1900;

  # set month using month offset
  $mon=$monoffset + 1;

  # create DATE for index for dayfile
  $date_for_dayfile=sprintf("%-4.4d.%-02.2d.%-02.2d",$year,$mon,$mday);
  if($dflg eq "1") {print LF "get_df_date:date for dayfile is $date_for_dayfile\n"};

  # get time in seconds
  $tm_seconds=timelocal($sec,$min,$hour,$mday,$mon,$year);
  if($dflg eq "1") {print LF "get_df_date:timelocal returned seconds are <$tm_seconds>\n"};

  #return value yyyy.mm.dd
  return ($date_for_dayfile);

}
