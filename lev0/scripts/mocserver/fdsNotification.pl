#!/usr/bin/perl -w 

$LOGALL = "LOGALL: ";
$MAILSUBJ = "\"Error downloading/ingesting MOC FDS data products\"";

my($arg);
my($logFile);
my($notifyList) = "";
my($mailMsg) = "";
my($nlines) = 0;

while ($arg = shift(@ARGV))
{
    if ($arg eq "-l")
    {
	# log file to parse
	$logFile = shift(@ARGV);
    }

    if ($arg eq "-n")
    {
	# list of people to notify
	my($npeeps) = 0;
	my($peep);
	while (defined($peep = shift(@ARGV)))
	{
	    $notifyList = "$notifyList$peep ";
	    $npeeps++;
	}

	if ($npeeps < 1)
	{
	    PrintUsage();
	    exit(1);
	}
    }
}

if (!defined($logFile))
{
    PrintUsage();
    exit(1);
}

open(LOGFILE, "< $logFile") || die "Couldn't read log file $logFile.\n";

while (defined($line = <LOGFILE>))
{
    chomp($line);

    if ($line =~ /^${LOGALL}(.+)/)
    {
#	print STDOUT "$1\n";
	$mailMsg = "${mailMsg}$1\n";
	$nlines++;
    }
}

close(LOGFILE);

if ($nlines > 0)
{
    open(MAILPIPE, "| /bin/mail -s $MAILSUBJ $notifyList") || die "Couldn't open 'mail' pipe.\n";
    print MAILPIPE $mailMsg;
    close(MAILPIPE);
}

sub PrintUsage
{
    print "fdsNotification.pl -l <logfile> -n <notification list>\n";
    print "  <notification list> is a space separated list of email addresses.\n";
    return;
}
