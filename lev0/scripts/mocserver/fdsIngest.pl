#!/usr/bin/perl -w 
#
# ingestFDS.pl takes FDS data files in the directory specified and imports them into
# DRMS as generic data segments.  The script groups data files into data products, as
# defined by 464-GS-ICD-0068.

# DRMS series in which to place the data files
my($series) = "sdo.moc_fds";
#my($series) = "su_arta.testfds";

# Primary key
my(@primaryKey);
$primaryKey[0] = "FDS_DATA_PRODUCT";
$primaryKey[1] = "FDS_PRODUCT_COMP";
$primaryKey[2] = "OBS_DATE";
$primaryKey[3] = "DATA_FORMAT";
$primaryKey[4] = "FILE_VERSION";

# Return codes
$RET_OK = 12251966;
$RET_BADUSAGE = 1;
$RET_BADINPUT = 2;
$RET_SHOWKEYS = 3;
$RET_COPYFAIL = 4;
$RET_TIMECONV = 5;
$RET_SETKEYS = 6;

# Segment name(s)
my(@segmentName);
$segmentName[0] = "FILENAME";

# map from filename prefix to type
# commented out types are products provided by the MOC Product Server,
# but not needed by SU.
my(%fdsTypeMap);
$fdsTypeMap{"ECLIPSE"} = "predEclipse";
$fdsTypeMap{"ECLIPSEATM"} = "predEclipse";
#$fdsTypeMap{"BETAANGLE"} = "predBetaAngle";
#$fdsTypeMap{"EWDRIFT"} = "predLongDrift";
#$fdsTypeMap{"ANODES"} = "predAnDnNodes";
#$fdsTypeMap{"DNODES"} = "predAnDnNodes";
#$fdsTypeMap{"OMNI_BOTH"} = "predAttDepGSVis";
#$fdsTypeMap{"OMNI_NONE"} = "predAttDepGSVis";
$fdsTypeMap{"HGA_NEG_VIEW"} = "predHGAView";
$fdsTypeMap{"HGA_POS_VIEW"} = "predHGAView";
#$fdsTypeMap{"HGA_GIMBALS"} = "predHGAGimbals";
#$fdsTypeMap{"PRED_ATT"} = "predAtt";
$fdsTypeMap{"RFI_STWS"} = "predRFI";
$fdsTypeMap{"RFI_STSS"} = "predRFI";
$fdsTypeMap{"ORBIT_GEO"} = "predGeoOrb";
$fdsTypeMap{"ORBIT_HELIO"} = "predHelioOrb";
#$fdsTypeMap{"CMD_SK"} = "maneuverCmd";
#$fdsTypeMap{"CMD_ASC"} = "maneuverCmd";
$fdsTypeMap{"PLAN_SK"} = "predOrbitManeuverPlan";
#$fdsTypeMap{"SENSORIF"} = "predSensorVisAndInt";
#$fdsTypeMap{"GRNDTRK"} = "predGrndTrack";
#$fdsTypeMap{"PROPELLANT1"} = "propellantRem";
#$fdsTypeMap{"PROPELLANT2"} = "propellantRem";
$fdsTypeMap{"MNVRSUM"} = "orbitManeuver";
#$fdsTypeMap{"RANGE"} = "predRange";
#$fdsTypeMap{"SCLOC_TIME"} = "predLocTime";
#$fdsTypeMap{"EPV"} = "extPrecVector";
$fdsTypeMap{"2LINE_ELEM"} = "twoLineElem";
#$fdsTypeMap{"IIRV"} = "impInterrangeVector";
$fdsTypeMap{"SCI_FOV"} = "predCelBodInFOV";
$fdsTypeMap{"COMP_ORBIT"} = "compOrbitSoln";
$fdsTypeMap{"VALATT"} = "compAttValidation";
#$fdsTypeMap{"VALORB"} = "compOrbValidation";
#$fdsTypeMap{"CMD_MOMENTUM"} = "momentMgmntCmd";
#$fdsTypeMap{"SPICE"} = "spice";
$fdsTypeMap{"SUN_MOON_ANGLES"} = "predLunTrans";
#$fdsTypeMap{"CMD_ENG"} = "engSlewCmd";
$fdsTypeMap{"CMD_HROLL"} = "calManeuverCmd";
$fdsTypeMap{"CMD_EFOV"} = "calManeuverCmd";
$fdsTypeMap{"CMD_ECRUC"} = "calManeuverCmd";
$fdsTypeMap{"CMD_HMIAIAOFF"} = "calManeuverCmd";
$fdsTypeMap{"CMD_GTPZT"} = "calManeuverCmd";
#$fdsTypeMap{"LOF"} = "locOscillatorFreq";
#$fdsTypeMap{"DSSCAL"} = "calUplinkTable";
#$fdsTypeMap{"STCAL"} = "calUplinkTable";
#$fdsTypeMap{"IRUCAL"} = "calUplinkTable";
#$fdsTypeMap{"HGACAL"} = "calUplinkTable";
#$fdsTypeMap{"DSSFOVCAL"} = "calUplinkTable";
#$fdsTypeMap{"TDRSE_EPHEM"} = "tdrsEphem";
#$fdsTypeMap{"TDRSW_EPHEM"} = "tdrsEphem";
#$fdsTypeMap{"TDRSIO_EPHEM"} = "tdrsEphem";
$fdsTypeMap{"PLAN_MM"} = "predMomMgmntManeuverPlan";
$fdsTypeMap{"SOLAR_TRANSIT"} = "predSolarTrans";
#$fdsTypeMap{"HGA_GIMBAL_OFFSETS"} = "hgaGimbalOffCmd";
#$fdsTypeMap{"LINK_MARGIN"} = "predLinkMargin";

# timespan enum
my(%fileFormatMap);
$fileFormatMap{"kFileFormatEXCEL"} = "spreadsheet";
$fileFormatMap{"kFileFormatSHORT"} = "shortTerm";
$fileFormatMap{"kFileFormatLONG"} = "longTerm";
$fileFormatMap{"kFileFormatCOMPRESSED"} = "compressed";
$fileFormatMap{"kFileFormatVARIABLE"} = "variable";

# get input directory
my($inputDir);
my($inputFile);
my(@contents);
my($line);

my($argc) = scalar(@ARGV);
if ($argc != 1)
{
    PrintUsage();
    exit($RET_BADUSAGE);
}
else
{
    if (-d $ARGV[0])
    {
	$inputDir = $ARGV[0];
    }
    elsif (-f $ARGV[0])
    {
	$inputFile = $ARGV[0];
    }
    else
    {
	print STDERR "Could not find input file/directory $ARGV[0].\n";
	exit($RET_BADINPUT);
    }
}

if (defined($inputDir))
{
    print STDOUT "Input dir is \"$inputDir\".\n";

    # enumerate all files
    if (!open(INPUTDIR, "/bin/ls -1 $inputDir |"))
    {
	print STDERR "Could not read from directory \"$inputDir\".\n";
	exit($RET_BADINPUT);
    }

    while ($line = <INPUTDIR>)
    {
	chomp($line);
	$contents[scalar(@contents)] = $line;
    }

    close(INPUTDIR);
}
else
{
    print STDOUT "Input file is \"$inputFile\".\n";
    $contents[scalar(@contents)] = $inputFile;
}

my($fileBase);
my($ext);
my($prefix);

my($dataType);
my($prodComp);
my($date);
my($fileFormat);
my($fileVersion);

my($numFilesToAdd) = 0;
my($numFiles) = 0;
my($numRecsAdded) = 0;
my($numFilesAdded) = 0;

my(%skCmds);

foreach $filename (@contents)
{
    my($err) = 0;

    $numFiles++;

    print STDOUT "Analyzing file $filename...\n";

    # Get filename base and extension
    if ($filename =~ /(.+)\.(.+)/)
    {
	$fileBase = $1;
	$ext = $2;
    }
    else
    {
	$err = 1;
	print STDOUT "  Filename $filename is not a recognized format.\n";
    }

    # get prefix and date
    if (!$err)
    {
	if ($fileBase =~ /(.+)_([0-9][0-9][0-9][0-9][0-9][0-9][0-9])/)
	{
	    $prefix = $1;
	    $date = $2;
	}
	else
	{
	    $err = 1;
	    print STDOUT "  Filename $filename is not a recognized format.\n";
	}


	if (!$err)
	{
	    # get file modifiers
	    if ($ext =~ /xls/i)
	    {
		$fileFormat = "kFileFormatEXCEL";
		$fileVersion = "0";
	    }
	    elsif ($ext =~ /(S|L|C)([0-9][0-9])/)
	    {
		my($dur) = $1;
		if ($dur eq "S")
		{
		    $fileFormat = "kFileFormatSHORT";
		}
		elsif ($dur eq "L")
		{
		    $fileFormat = "kFileFormatLONG";
		}
		elsif ($dur eq "C")
		{
		    $fileFormat= "kFileFormatCOMPRESSED";
		}		

		$fileVersion = $2;
	    }
	    elsif ($ext =~ /([0-9][0-9])/)
	    {
		$fileFormat = "kFileFormatVARIABLE";
		$fileVersion = $1;
	    }
	    else
	    {
		$err = 1;
		print STDOUT "  $ext is not a recognized format.\n";
	    }
	}
    }

    if ($err)
    {
	# skip this file - not a data file
	next;
    }

    # Map the prefix to the data product type
    if ($dataType = $fdsTypeMap{$prefix})
    {	
	my($filePath);
	my($skKey);

	$prodComp = $prefix;

	if (defined($inputDir))
	{
	    if ($inputDir =~ /.+\/$/)
	    {
		$filePath = $inputDir . $filename;
	    }
	    else
	    {
		$filePath = $inputDir . "/" . $filename;
	    }
	}
	else
	{
	    $filePath = $inputFile;
	}
	
	$skKey = "$dataType.$prodComp.$date.$fileFormat.$fileVersion";
	
	if (defined($skCmds{$skKey}))
	{
	    print STDOUT "  $dataType.$prodComp.$date.$fileFormat.$fileVersion already defined\n";
	}
	else
	{
	    $skCmds{$skKey} = $filePath;
	    $numFilesToAdd++;
	}
    }
    else
    {
	print STDOUT "  Data type $prefix is not a recognized type.\n";
    }
}

print STDOUT "\nNumber of files to add to DRMS: $numFilesToAdd\n";
print STDOUT "Number of total files examined: $numFiles\n";

# Call all set_key commands, one for each data product (which may contain more than one data file)
my(@skCmdsKeys) = keys(%skCmds);

foreach $oneKey (@skCmdsKeys)
{
    my($oneVal) = $skCmds{$oneKey};
    CallSetKey($oneKey, $oneVal);
}

print STDOUT "\nNumber of records added: $numRecsAdded\n";
print STDOUT "Number of files (segments) added: $numFilesAdded\n";

exit($RET_OK);

sub PrintUsage
{
    print STDOUT "Usage:\n";
    print STDOUT "\tingestFDS <input directory>\n";
}

sub CallSetKey
{
    my($key, $filePath) = @_;

    # Extract data type, product component, date, fileFormat, and fileVersion from $key
    my($dataType);
    my($prodComp);
    my($fileYr);
    my($fileD);
    my($fileFormat);
    my($fileVersion);

    if ($key =~ /(.+)\.(.+)\.([0-9][0-9][0-9][0-9])([0-9][0-9][0-9])\.(\S+)\.(\S+)/)
    {
	$dataType = $1;
	$prodComp = $2;
	$fileYr = $3;
	$fileD = $4;
	$fileFormat = $fileFormatMap{$5};
	$fileVersion = $6;

	print STDOUT "\nAdding record for <$dataType, $prodComp, $fileYr$fileD, $fileFormat, $fileVersion>\n";

	# acceptable JSOC date format is YYYY.MM.DD, but FDS files contain ordinal dates (YYYYDDD)
	my($convTime);
	my($tcCmdLine) = "time_convert ord=${fileYr}\.${fileD}_UT o=cal zone=UT |";

	if (!open(TIMECONV, $tcCmdLine))
	{
	    print STDERR "Couldn't run time_conv: $tcCmdLine\n";
	    exit($RET_TIMECONV);
	}
	
	if (defined($convTime = <TIMECONV>))
	{
	    my($skCmd);
	    my($jsocDate);
	    
	    chomp($convTime);
	    $jsocDate = $convTime;
	    
	    $skCmd = "set_keys -c ds=$series $primaryKey[0]=$dataType $primaryKey[1]=$prodComp $primaryKey[2]=$jsocDate $primaryKey[3]=$fileFormat $primaryKey[4]=$fileVersion $segmentName[0]=$filePath";
	    print STDOUT "  Running $skCmd\n";
	    if (system($skCmd) != 0)
	    {
		print STDERR "Error calling set_keys: $?\n";
		exit($RET_SETKEYS);
	    }

	    $numRecsAdded++;
	    $numFilesAdded++;
	    # XXX - this doesn't work anymore
	    VerifyFileCopy($series, $dataType, $prodComp, $jsocDate, $fileFormat, $fileVersion, $filePath);
	}
	
	close (TIMECONV);
    }
}

sub VerifyFileCopy
{
    my($series, $dataType, $prodComp, $jsocDate, $dataFormat, $fileVersion, $srcFilePath) = @_;

    my($skResultLine);
    my($skCmdLine) = "show_keys -pq ds=$series\[$primaryKey[0]=$dataType\]\[$primaryKey[1]=$prodComp\]\[$primaryKey[2]=$jsocDate\]\[$primaryKey[3]=$dataFormat\]\[$primaryKey[4]=$fileVersion\] seg=$segmentName[0] |";

    if (!open(SHOWKEYS, $skCmdLine))
    {
	print STDERR "Couldn't run show_keys: $skCmdLine\n";
	exit($RET_SHOWKEYS);
    }

    if (defined($skResultLine = <SHOWKEYS>))
    {
	print STDOUT "showkeys: $skResultLine";
	# there should be only one line returned by show_keys
	chomp($skResultLine);
	if ($skResultLine =~ /^\s*(\S+)\s*$/)
	{
	    my($fileListRet) = $1;
	    my($dstFilePath);

	    while ($fileListRet =~ /^\s*(\S+)(.+)?/)
	    {
		if ($dstFilePath)
		{
		    # error - show_keys returned more than one file
		    print STDERR "  File $1 in DRMS unexpected.\n";
		    close(SHOWKEYS);
		    exit($RET_SHOWKEYS);
		}

		$dstFilePath = $1;
		if (defined($2))
		{
		    $fileListRet = $2;
		}
		else
		{
		    last;
		}
	    }
	    
	    my($strLoc);
	    my($oneSrcSegFile);
	    my($oneDstSegFile);

	    $strLoc = rindex($srcFilePath, "/");
	    if ($strLoc >= 0)
	    {
		$oneSrcSegFile = substr($srcFilePath, $strLoc + 1);
	    }
	    else
	    {
		$oneSrcSegFile = $srcFilePath;
	    }
	    
	    $strLoc = rindex($dstFilePath, "/");
	    if ($strLoc >= 0)
	    {
		$oneDstSegFile = substr($dstFilePath, $strLoc + 1);
	    }
	    else
	    {
		$oneDstSegFile = $dstFilePath;
	    }
	    
	    if ($oneSrcSegFile ne $oneDstSegFile)
	    {
		print STDERR "  File in DRMS unexpected.\n";
		print STDERR "    Expected: $oneSrcSegFile, Actual: $oneDstSegFile\n";
		close(SHOWKEYS);
		exit($RET_COPYFAIL);
	    }
	    else
	    {
		# segment file names match - now compare files
		my($cmpCmd) = "cmp $srcFilePath $dstFilePath";
		my($cres);
		if (system($cmpCmd) != 0)
		{
		    print STDERR "  $srcFilePath  not successfully copied into DRMS.\n";
		    exit($RET_COPYFAIL);
		}

		$cres = $? >> 8;
		if ($cres == 0)
		{
		    print STDOUT  "  $srcFilePath successfully copied into DRMS.\n";
		}
		else
		{
		    print STDERR  "  $srcFilePath NOT successfully copied into DRMS.\n";
		    exit($RET_COPYFAIL);
		}
	    }
	}
    }
    else
    {
	# show_keys failure
	print STDERR "show_keys didn't find any records for <$primaryKey[0]=$dataType, $primaryKey[1]=$prodComp, $primaryKey[2]=$jsocDate, $primaryKey[3]=$dataFormat, $primaryKey[4]=$fileVersion>.\n";
	close(SHOWKEYS);
	exit($RET_SHOWKEYS);
    }

    close(SHOWKEYS);
}
