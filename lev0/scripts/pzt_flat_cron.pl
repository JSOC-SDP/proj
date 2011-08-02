#!/usr/bin/perl
#pzt_flat_cron.pl
#
#Run Richards pzt_flatfield IDL program.
#The input in IDL is like:
# pzt_flatfield, 2010, 10, 07
#
#We will echo this into idl for todays date.
#
$PID = getppid;
$host = `hostname -s`;
chomp($host);
print "host = $host\n";
$DB = jsoc;
$HOSTDB = "hmidb";      #host where DB runs
$PGPORT=5432;
$user = $ENV{'USER'};
$QSUBDIR = "/scr21/production/qsub/flat";


#if($host ne "n02") {
#  print "Error: This must be run on n02\n";
#  exit;
#}
$ENV{'JSOC_MACHINE'} = "linux_x86_64";
$JSOC_MACHINE = "linux_x86_64";
$ENV{'PATH'} = "/home/production/cvs/JSOC/bin/$JSOC_MACHINE:/home/production/cvs/JSOC/scripts:/bin:/usr/bin:/SGE/bin/lx24-amd64:/home/production/STAGING/bin/_linux4:";

$ENV{'SGE_ROOT'} = "/SGE";
$sgeroot = $ENV{'SGE_ROOT'};
#print "SGE_ROOT = $sgeroot\n";
$path = $ENV{'PATH'};
#print "path = $path\n";  #!!!TEMP
$mach = $ENV{'JSOC_MACHINE'};
#print "JSOC_MACHINE = $mach\n";  #!!!TEMP

$pztdate = &labeldate;
print "pztdate = $pztdate\n";
$logfile = "$QSUBDIR/pzt2.$PID.log";
open(LOG, ">$logfile") || die "Can't Open: $logfile $!\n";

$cmd = "cd /home/production/cvs/JSOC/proj/lev0/apps/pztflat; echo \"pzt_flatfield, $pztdate\" | /usr/local/bin/idl 1>> $QSUBDIR/pzt2.$PID.log 2>&1";
print LOG "$cmd\n";
print "$cmd\n";
`$cmd`;
close(LOG);


sub labeldate {
  local($sec,$min,$hour,$mday,$year,$wday,$yday,$isdst,$date,$sec2,$min2,$hour2,$mday2);
  ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime(time);
  if($year % 4 == 0 && $year % 100 != 0 || $year % 400 == 0) { #leap year
    $leap = 1;
  }
  else {
    $leap = 0;
  }
  $sec2 = sprintf("%02d", $sec);
  $min2 = sprintf("%02d", $min);
  $hour2 = sprintf("%02d", $hour);
  $mday2 = sprintf("%02d", $mday);
  $mon2 = sprintf("%02d", $mon+1);
  $dayofmo = $mday2;                    #set global value
  $year4 = sprintf("%04d", $year+1900);
  $date = $year4.",".$mon2.",".$mday2;
  return($date);
}

