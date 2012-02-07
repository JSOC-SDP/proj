#!/bin/sh -e
# (line below is for qsub)
#$ -S /bin/sh
#
# Top-level driver for deriving HARPs from a series of HMI masks (e.g., hmi.Marmask_720s).
#
# Invokes HARP finder/tracker and ingests resulting HARPs into a harp data series.  Uses
# a filesystem directory (dest_dir) transfer results.  Essential aspects of the dest_dir 
# intermediate results ("checkpoint files") are stored into harp_log_series in case dest_dir 
# is accidentally mangled.
#
# Usage:
#
#  track_and_ingest_mharp.sh [ -i N ] [ -n ] [ -d ] dest_dir mask_series harp_series harp_log_series
#
# where:
#   dest_dir is a staging area in the filesystem
#   mask_series is an input data series qualified by T_REC 
#   harp_series is an output data series to put tracks
#
# and:
#   -i introduces an optional argument indicating an initial run with 
#      new tracks numbered starting from N.  Otherwise, a run that picks
#      up where an earlier one left off, according to a checkpoint file
#      in `dest_dir', is assumed.
#      IF GIVEN, `-i N' CLEARS ALL EXISTING RESULTS.
#   -n indicates near-real-time (NRT) mode.  Less track history is retained,
#      allowing faster startup and more compact checkpoint files.  And, both
#      finalized and currently-pending tracks are ingested into JSOC.
#   -d is a flag which signals to use the developer MATLABPATH (~turmon) rather
#      than the regular production path (~jsoc).
#
# Note that we require a T_REC to be supplied with mask_series.  
# 
# Typical command line:
# [monthly]
#   track_and_ingest_mharp.sh -i 1 /tmp/harp/monthly hmi.Marmask_720s[2011.10.01/10d]
#     hmi.Mharp_720s hmi.Mharp_log_720s
# [nrt]
#   track_and_ingest_mharp.sh -i 1 -n /tmp/harp/nrt hmi.Marmask_720s_nrt[2011.10.01_12:36_TAI]
#     hmi.Mharp_720s_nrt hmi.Mharp_log_720s_nrt
# Don't forget to omit the -i 1 argument on subsequent runs, in both cases!
#
# This routine calls: track_hmi_production_driver_stable.sh, ingest_mharp, ingest_mharp_log
#

# turmon jul-sep 2011

# echo command for debugging
#set -x

progname=`basename $0`
# under SGE, $0 is set to a nonsense name, so use our best guess instead
if [ `expr "$progname" : '.*track.*'` == 0 ]; then progname=track_and_ingest_mharp; fi
USAGE="Usage: $progname [ -i N ] [ -n ] [ -d ] dest_dir mask_series harp_series harp_log_series"
TEMPROOT=/tmp/harps-${progname}
# creates the dir, and returns the dir name
TEMP_DIR=`mktemp -d $TEMPROOT-XXXXXXXX`
# we have not begun tracking yet, so errors are not problematic
HAVE_BEGUN=0

# echo the arguments, for repeatability
echo "${progname}: Invoked as:" "$0" "$@"

# alas, we require a temp file
function cleanup() {
    # clean the whole temp dir
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

# usage: die LINENO MESSAGE CODE
# all arguments optional
function die() {
    echo "${progname}: Error near line ${1:- \?}: ${2:-(no message)}" 1>&2
    if [ $HAVE_BEGUN = 1 ]; then
	echo "${progname}: Note: Before running $progname again," \
             "you likely must roll back the checkpoint file in $dest_dir" 1>&2
    fi
    exit ${3:- 1}
}

# Error message selection upon exit
cmd="shell code"
trap 'die $LINENO "Running $cmd"'     ERR
trap 'die $LINENO "Received SIGHUP"'  SIGHUP
trap 'die $LINENO "Received SIGINT"'  SIGINT
trap 'die $LINENO "Received SIGTERM"' SIGTERM

# get options -- pass them down to tracker/ingestor
make_movie=1
first_track=0
other_path=0
track_opts=""
nrt_mode=0
while getopts "hndi:" o; do
    case "$o" in
	i)    first_track="$OPTARG"
	      track_opts="$track_opts -i $first_track";;
	d)    other_path=1
	      track_opts="$track_opts -d";;
	n)    nrt_mode=1;;
	[?h]) echo "$USAGE" 1>&2
	      exit 2;;
    esac
done
shift `expr $OPTIND - 1`

# Disable globbing from here forward, to prevent wildcard chars, esp. within
# $mask_series, from expanding into files
set -f

# handle setup for NRT versus definitive mode
if [ "$nrt_mode" -eq 1 ]; then
    # tracker options:
    #   retain only r days of history
    track_opts="$track_opts -r 1"
    # ingest both new and pending regions
    listfiles="track-new.txt,track-pending.txt"
    # ingester options:
    #   ingest only most recent trec; no temporal padding
    #   do not filter HARPs based on number of appearances (tmin=1)
    #   do not output match information
    ingest_opts="trec=1 tpad=0 tmin=1 match=0"
    do_match=0
else
    # tracker options:
    #   retain all history
    track_opts="$track_opts -r inf"
    # ingest only new regions
    listfiles="track-new.txt"
    # ingester options:
    #   accept default trec, tpad
    #   filter HARPs having fewer than tmin appearances
    #   do output match information
    ingest_opts="tmin=3 match=1"
    do_match=1
fi
# movie vs. no movie
if [ "$make_movie" -eq 1 ]; then
    # -m makes a movie, its absence does not
    track_opts="$track_opts -m"
fi

# get arguments
if [ "$#" -ne 4 ]; then
    echo "$USAGE" 1>&2
    exit 2
fi
# ok to set up args
dest_dir="$1"
mask_series="$2"
harp_series="$3"
harp_log_series="$4"

# input data series, without T_REC qualifier
mask_series_only=`echo $mask_series | sed 's/\[.*$//'`

# SGE/OpenMP setup
SGE_ROOT=/SGE;     export SGE_ROOT
OMP_NUM_THREADS=1; export OMP_NUM_THREADS
KMP_BLOCKTIME=10;  export KMP_BLOCKTIME

uname=`uname -m`
if [ "X$uname" = Xi686 ]; then
    PATH="${PATH}:$SGE_ROOT/bin/lx24-x86"
elif [ "X$uname" = Xx86_64 ]; then
    PATH="${PATH}:$SGE_ROOT/bin/lx24-amd64"
elif [ "X$uname" = Xx86_64 ]; then
    PATH="${PATH}:$SGE_ROOT/bin/lx24-ia64"
else
    die $LINENO "Could not find system '$uname' to set up SGE"
fi

#############################################################
#
# Simple error checks, while it's painless
#
#############################################################

# if not -i, check that dest_dir exists and is writable
if [ $first_track = 0 ]; then
    if [ ! -d "$dest_dir/Tracks/jsoc" ]; then
	die $LINENO "Not initial run, but could not find Tracks/jsoc within dest_dir"
    fi
    if [ ! -w "$dest_dir/Tracks/jsoc" ]; then
	die $LINENO "Not initial run: need write permission on Tracks/jsoc within dest_dir"
    fi
fi

# for show_info, ingestor, and log-ingestor
# there is no sh version of .setJSOCenv, so we use this to get JSOC_MACHINE
jsoc_mach=`csh -f -c 'source /home/jsoc/.setJSOCenv && echo $JSOC_MACHINE'`
PATH="${PATH}:$HOME/cvs/JSOC/bin/$jsoc_mach"

echo "${progname}: show_info is at:" `which show_info`
echo "${progname}: ingest_mharp is at:" `which ingest_mharp`

# check that mask_series exists
cmd="show_info -s $mask_series"
$cmd > /dev/null
# check that harp*series exists
cmd="show_info -s $harp_series"
$cmd > /dev/null
cmd="show_info -s $harp_log_series"
$cmd > /dev/null

#############################################################
#
# Main processing starts
#
#############################################################

# filenames must match the deletion pattern in cleanup() above
# hold mask list
TEMPMASK="$TEMP_DIR/mask.list"

# stage data (include T_REC)
#   (this does not fail if the series is empty)
echo "${progname}: Staging masks."
cmd="show_info -q -ip $mask_series"
$cmd > $TEMPMASK
echo "${progname}: Finished staging masks."
# find first T_REC, for later
trec_frst=`awk 'NR == 1 {print $1}' $TEMPMASK | sed -e 's/.*\[//' -e 's/\].*//'`
trec_last=`awk 'NR == 1 {print $1}' $TEMPMASK | sed -e 's/.*\[//' -e 's/\].*//'`
# no longer needed
rm -f "$TEMPMASK"

# after this point, simple re-runs may not work because tracker alters $dest_dir
HAVE_BEGUN=1

# driver invokes matlab to perform tracking
#  -- note, track_opts is unquoted
echo "${progname}: Beginning tracking."
cmd=track_hmi_production_driver_stable.sh 
$cmd $track_opts "$mask_series" "$dest_dir"
echo "${progname}: Finished tracking."

echo "${progname}: Ingesting tracks."

# ingest data
#  -- note, ingest_opts is unquoted
cmd=ingest_mharp 
$cmd $ingest_opts "root=$dest_dir/Tracks/jsoc" "lists=$listfiles" "out=$harp_series" "mask=$mask_series_only" verb=1
# preserve NOAA AR match data
if [ $do_match = 1 ]; then
    # name for saved match info
    ext="$trec_frst"
    # current and new location for NOAA AR match data
    matchdir="$dest_dir/Tracks/jsoc/Match"
    matchold="$dest_dir/Tracks/jsoc/Match-old"
    # -p also suppresses errors if the dir exists already
    mkdir -p "$matchold"
    mv "$matchdir" "$matchold/Match-$ext"
fi

# ingest log and checkpoint file
#   name is a string indicating run name; we just use the mask series
#   which contains the time interval.  This is *not* interpreted as a 
#   data series by ingest_mharp_log.  It is just an identifying string.
cmd=ingest_mharp_log
$cmd root="$dest_dir/Tracks/jsoc" out="$harp_log_series" trec="$trec_frst" name="$mask_series"

echo "${progname}: Finished ingesting tracks."

# Exit OK
echo "${progname}: Done."
exit 0
