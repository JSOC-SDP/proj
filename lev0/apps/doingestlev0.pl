OLD. Replaced by proj/lev0/scripts/doingestlev0_[AIA,HMI].pl
#!/usr/bin/perl 
#/home/production/cvs/JSOC/proj/lev0/apps/start_lev0.pl
#Start up the four ingest_lev0 programs and periodically cause them
#to exit and then restart them.
#
$user = "388";
if($user ne "388") {
  print "You must be user production to run\n";
  exit;
}
$host = `hostname`;
if(!($host =~ /cl1n001/)) {
  print "This can only be run on the cl1n001 pipeline machine.\n";
  exit;
}
@vcnames = ("VC01", "VC04", "VC02", "VC05"); #primary channels
$ldate = &labeldate();
@lognames = ("VC01_$ldate.log", "VC04_$ldate.log", "VC02_$ldate.log", "VC05_$ldate.log");

$rmcmd0 = "/bin/rm -f /usr/local/logs/lev0/@vcnames[0]_stop /usr/local/logs/lev0/@vcnames[0]_exit";
$rmcmd1 = "/bin/rm -f /usr/local/logs/lev0/@vcnames[1]_stop /usr/local/logs/lev0/@vcnames[1]_exit";
$rmcmd2 = "/bin/rm -f /usr/local/logs/lev0/@vcnames[2]_stop /usr/local/logs/lev0/@vcnames[2]_exit";
$rmcmd3 = "/bin/rm -f /usr/local/logs/lev0/@vcnames[3]_stop /usr/local/logs/lev0/@vcnames[3]_exit";

#clean up the signal files
print "$rmcmd0\n";
if(system($rmcmd0)) {
  print "Failed: $rmcmd0\n";
  exit;
}
print "$rmcmd1\n";
if(system($rmcmd1)) {
  print "Failed: $rmcmd1\n";
  exit;
}
print "$rmcmd2\n";
if(system($rmcmd2)) {
  print "Failed: $rmcmd2\n";
  exit;
}
print "$rmcmd3\n";
if(system($rmcmd3)) {
  print "Failed: $rmcmd3\n";
  exit;
}

#Now do the initial start of the 4 ingest_lev0
$cmd0 = "ingest_lev0  vc=@vcnames[0] indir=/dds/soc2pipe/aia logfile=/usr/local/logs/lev0/@lognames[0] &";
$cmd1 = "ingest_lev0  vc=@vcnames[1] indir=/dds/soc2pipe/aia logfile=/usr/local/logs/lev0/@lognames[1] &";
$cmd2 = "ingest_lev0  vc=@vcnames[2] indir=/dds/soc2pipe/hmi logfile=/usr/local/logs/lev0/@lognames[2] &";
$cmd3 = "ingest_lev0  vc=@vcnames[3] indir=/dds/soc2pipe/hmi logfile=/usr/local/logs/lev0/@lognames[3] &";
print "$cmd0\n";
if(system($cmd0)) {
  print "Failed: $cmd0\n";
}
print "$cmd1\n";
if(system($cmd1)) {
  print "Failed: $cmd1\n";
}
print "$cmd2\n";
if(system($cmd2)) {
  print "Failed: $cmd2\n";
}
print "$cmd3\n";
if(system($cmd3)) {
  print "Failed: $cmd3\n";
}

print "\nTo cleanly stop all ingest_lev0 and commit any open images, call:\n";
print "stop_lev0.pl\n";

#$SIG{INT} = \&catchc;
#$SIG{INT} = 'IGNORE';

while(1) {
  for($i=0; $i < 720; $i++) {  #run this for 3600 seconds
    sleep 5;
    &ckingest;		#see if any ingest_lev0 are still running
    if(!$ifound) {
      $ldate = &labeldate();
      print "$ldate\n";
      $stopfile = "/usr/local/logs/lev0/VC02_stop";
      #if find a stop file, assume stop_lev0.pl was called and exit
      if(-e $stopfile) {
        print "\nAll ingest_lev0 have been stopped. Exit doingestlev0.pl\n\n";
        exit(0);
      }
      print "\nAll ingest_lev0 are already stopped. Restart...\n\n";
      last;  #!!TEMP this didn't seem to work here ???
      $i = 720;
    }
  }
  $cmd = "touch /usr/local/logs/lev0/@vcnames[0]_stop"; #tell ingest to stop
  `$cmd`;
  $cmd = "touch /usr/local/logs/lev0/@vcnames[1]_stop";
  `$cmd`;
  $cmd = "touch /usr/local/logs/lev0/@vcnames[2]_stop";
  `$cmd`;
  $cmd = "touch /usr/local/logs/lev0/@vcnames[3]_stop";
  `$cmd`;
  while(1) {
    $found = 0;
    #wait until they're all stopped
    @ps_prod = `ps -ef | grep ingest_lev0`;
    while($_ = shift(@ps_prod)) {
      if(/vc VC01/ || /vc VC02/ || /vc VC04/ || /vc VC05/) {
        $found = 1;
        sleep 1;
        last;
      }
    }
    if(!$found) { last; }
  }
  sleep(5);			#make sure previous commit to db is done

      `$rmcmd0`;
      $cmd = "ingest_lev0 -r vc=@vcnames[0] indir=/dds/soc2pipe/aia logfile=/usr/local/logs/lev0/@lognames[0] &";
      if(system($cmd)) {
        print "Failed: $cmd\n";
      }
      `$rmcmd1`;
      $cmd = "ingest_lev0 -r vc=@vcnames[1] indir=/dds/soc2pipe/aia logfile=/usr/local/logs/lev0/@lognames[1] &";
      if(system($cmd)) {
        print "Failed: $cmd\n";
      }
      `$rmcmd2`;
      $cmd = "ingest_lev0 -r vc=@vcnames[2] indir=/dds/soc2pipe/hmi logfile=/usr/local/logs/lev0/@lognames[2] &";
      if(system($cmd)) {
        print "Failed: $cmd\n";
      }
      `$rmcmd3`;
      $cmd = "ingest_lev0 -r vc=@vcnames[3] indir=/dds/soc2pipe/hmi logfile=/usr/local/logs/lev0/@lognames[3] &";
      if(system($cmd)) {
        print "Failed: $cmd\n";
      }
}

sub ckingest {
    $ifound = 0;
    #see if they're all stopped
    @ps_prod = `ps -ef | grep ingest_lev0`;
    while($_ = shift(@ps_prod)) {
      if(/vc VC01/ || /vc VC02/ || /vc VC04/ || /vc VC05/) {
        $ifound = 1;
        last;
      }
    }
}

#Return date in form for a label e.g. 1998.01.07_14:42:00
sub labeldate {
  local($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst,$date,$sec2,$min2,$hour2,$mday2);
  ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
  $sec2 = sprintf("%02d", $sec);
  $min2 = sprintf("%02d", $min);
  $hour2 = sprintf("%02d", $hour);
  $mday2 = sprintf("%02d", $mday);
  $mon2 = sprintf("%02d", $mon+1);
  $year4 = sprintf("%04d", $year+1900);
  $date = $year4.".".$mon2.".".$mday2._.$hour2.":".$min2.":".$sec2;
  return($date);
}

