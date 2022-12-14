/******************************************************************************/
/*
 *  mtrack.c						~rick/src/mtrack
 *
 *  Responsible:  Rick Bogart				rick@sun.stanford.edu
 *
 *  Generate multiple mapped tracked data cubes at different locations from
 *    a common time sequence of solar images
 *
 *  (Main module body begins around line 1940)
 *
 *  Parameters: (type   default         description)
 *	in	DataSet none            Input dataset
 *				A series of images of all or part of the
 *				solar disc in a "plate" coordinate system
 *				(helioprojective geometry). It may be
 *				specified as either a data set query or
 *				just a data series name, in which case
 *				additional selection parameters are required
 *				The dataset is assumed to include in its
 *				ancillary data the parameters required for
 *				heliographic mapping, namely the observation
 *				time and heliographic location.
 *      segment string	-		Name of the input data segment (only
 *				required if the input series has multiple
 *				segments and segment not in specifier)
 *      out     DataSer none            Output data series name
 *				The output series must have prime keys that
 *				can distinguish individual output records by
 *				LatHG, LonHG, and either MidTime or CarrTime
 *				(= CarrRot:CMLon)
 *	sdofix	DataSer	sdo.location	Data series containing
 *				corrected SDO observer ephemeris
 *      bckgn	DataRecord -		If specified, a data record segment
 *				to be pre-subtracted from each input image
 *	reject	string	-		If specified, name of a file with a
 *				list of input images to be rejected regardless
 *				of quality mask values
 *	qmask	int	0x80000000	Quality mask for data acceptability;
 *				records rejected if (qmask & qkey:value) != 0
 *	cvok	string	any		64-bit mask of acceptable values for
 *				calibration veraion of input; default -> all
 *				values acceptable
 *	cvno	string	none		64-bit mask of acceptable values for
 *				calibration veraion of input; default -> no
 *				values unacceptable
 *	max_miss int	All		Tolerance threshold for number of blank
 *				values in image (assumed to exclude crop)
 *	tmid	string	-		Midpoint of target tracking interval;
 *				ignored if input specified as data set; defined
 *				as string because it can be specified in either
 *				regular time format or as CR:CL
 *	length	int	-		Length of tracking interval, in units
 *				of data cadence; ignored if input specified as
 *				data set
 *	tstart	time	-		Start of target tracking interval;
 *				ignored if input specified as data set, or if
 *				tmid and length are specified
 *	tstop	time	-		End of target tracking interval;
 *				ignored if input specified as data set, or if
 *				tmid and length are specified
 *	tstep	double	-		Time step size (if present, overrides
 *				input series cadence)
 *	lat	float*	[0.0]		Target heliographic latitude(s)
 *	lon	float*	[0.0]		Target heliographic longitude(s)
 *      map     enum  "Postel"               Mapping option:
 *				recognized values are "carree", "Cassini",
 *				"Mercator", "cyleqa", "sineqa", "gnomonic",
 *				"Postel", "stereographic", "orthographic",
 *				and "Lambert" (and possibly others).
 *      interp  enum  "cubiconv"           Interpolation option:
 *				recognized values are "cubiconv" (and possibly
 *				others)
 *      fillopt	enum  "interp"           Option for fill of frames for times
 *				of missing images: recognized values are
 *				"interp", "zero", and "nan"
 *      scale   float   0.0             Scale of map (heliographic degrees /
 *				pixel) at location appropriate for mapping
 *				option; a 0 value implies autoscaling to best
 *				scale of image.  Typical values would be
 *				about 0.057 * resolution [arcsec / pixel]
 *				for solar radii of 1000 arcsec, or about
 *				0.12 for MDI or GONG or MtWilson
 *				0.03 for HMI or AIA
 *	cols	int	0		Columns in output maps; 0 -> rows
 *	rows	int	0		Rows in output maps; 0 -> cols
 *      map_pa  float*  [0.0]           The angle(s) between heliographic
 *				north and "up" on the output map (in the
 *				direction of increasing rows) [deg[, in the
 *				sense that a positive position angle represents
 *				a clockwise displacement of the north axis.
 *      a0      float   -0.02893	Coefficients in sin^2 (latitude)
 *      a2      float   -0.3441		expansion of rotation rate minus
 *      a4      float   -0.5037		Carrington rotation (urad/sec)
 *				ignored if -c or -n flag
 *         Some representative values to use:
 *                 a0       a2      a4
 *		-0.02893 -0.3441 -0.5037	Snodgrass 82/84 (default)       
 *		-0.02713 -0.3014 -0.5263	Snodgrass 67/84
 *		-0.02533 -0.3079 -0.5278	used by Haber & Hill
 *		-0.02833 -0.4100 -0.4189	Ulrich et al. SP 117, 291
 *		-0.0853	 -0.351  -0.443		Howard & Harvey
 *		 0.03749 -0.5596  0		Newton & Nunn
 *		 0.0137  -0.339  -0.485		Snodgrass & Ulrich ApJ 351, 309
 *		 0.1067  -0.484  -0.361		Snodgrass & Ulrich " (66-87)
 *      merid_v float   0.0		Meridional rate for tracking
 *				urad/sec, +ve northward
 *	bscale	float	NaN		Value scaling parameter for output
 *	bzero	Float	NaN		value offset parameter for output
 *	trec_key string	T_REC		Keyname of time type prime keyword for
 *				input data series, assumed slotted
 *	tobs_key string	T_OBS		Keyname of time type keyword describing
 *				observation time (midpoint) of each input image
 *	tstp_key string	Cadence		Keyname of float type keyword describing
 *				observing cadence of input data
 *	qual_key string	Quality		Keyname of int type keyword describing
 *				record quality as bitmask
 *	clon_key string	CRLN_OBS	Keyname of float type keyword describing
 *				centre Carrington longitude of each input image
 *	clat_key string	CRLT_OBS	Keyname of float type keyword describing
 *				centre Carrington latitude of each input image
 *	crot_key string	CAR_ROT		Keyname of int type keyword describing
 *				Carrington Rotation number associated with the
 *				centre Carrington longitude of each input image
 *	rsun_key string	R_SUN		Keyname of float type keyword describing
 *				image semi-diameter in pixels
 *	apsd_key string	RSUN_OBS	Keyname of float type keyword describing
 *				apparent solar semidiameter of each image in arcsec
 *	dsun_key string	DSUN_OBS	Keyname of double type keyword describing
 *				distance from sun for of each image
 *	cvkey	 string	CalVer64	Key name of 64-bit mask type keyword
 *				describing calibration version used
 *	idkey	 string	Ident		Key name of keyword whose string value
 *				is to be set to value of ident (if specified)
 *	cvok	string	any		Acceptable value of cvkey
 *	cvno	string	none		Uncceptable value of cvkey
 *      mai	 float*  NaN	MAI (Magnetic Activity Index) value to
 *				set as keyword
 *	ident	string	Not Specified	Identifier string (user defined)
 *
 *  Flags
 *	-c	track at Carrington rate (a0 = a2 = a4 = merid_v = 0)
 *	-n	turn off tracking, just correct for anomalous observer motion
 *		(a0 = -2.8653291, a2 = a4 = merid_v = 0
 *	-o	remove line-of-sight component of observer velocity
 *	-r	remove line-of-sight component of solar rotation
 *	-v	run verbose
 *	-w	experimental run (do not actually read image data nor remap
 *		nor insert output records; just rapidly process input record
 *		key information)
 *	-x	experimental run (do not insert output records)
 *	-G	use geocentric ephemeris for time of meridian crossing,
 *		regardless of data platform
 *	-M	use MDI keywords for input and correct for MDI distortion
 *	-S	suppress correction of SDO Carrington Longitude
 *	-Z	log times in UT (default is TAI)
 *
 *  Notes:
 *    This module is a DRMS-based version of fastrack, which it has replaced
 *
 *  Bugs:
 *    The code does not check that the output data segments match those of
 *	the output series when the segment protocol is variable
 *    Checks for validity of tstart and tstop parameters are unreliable, due
 *	to bugs in DRMS treatment of scans of invalid strings for times
 *	(ticket #177)
 *    Should free log, map
 *    Will not accept an input dataset specification of @*
 *    The image foreshortening corrections are appropriate for 1 AU, independent
 *	of DSUN_OBS
 *    The input of the ellipse position angle has not been verified, but then
 *	neither has the correction for ellipticity of the image altogether
 *    If there are multiple data segments in the input series and a segment
 *	has been explicitly specified, the correct segment may not be selected;
 *	need code like ndimsegments in ~rick/lsm/src/dopresid.c
 *    If there are multiple data segments of rank 3 in the output series, a
 *	warning is generated and the first one is used
 *    If there are multiple data segments in the background subtraction record,
 *	a warning is issued and the first that can be read is used
 *    The protocol of the Log segment, if present in the output series, is not
 *	verified to be generic
 *    There is no verification that numerous essential key data are actually
 *	in the input dataset, in particular trec_key, tobs_key, and
 *	crot_key. (clon_key and clat_key are checked)
 *    When a target time and length are given, the actual number of records
 *	may differ by one from the expected number; this only occurs if
 *	the target time differs by a small but non-zero amount (< 0.1 sec)
 *	from either the data record time or the midway point between data
 *	record times. This happens for example, if the length is odd and
 *	the target time differs from an actual observation time by more than
 *	0.1 usec but less than 0.05 sec, with a 1-minute cadence
 *    The input data are unconditionally read in as floats (single-precision),
 *	and the output data written as scaled shorts
 *    There is evidently no WCS conventional name for the Cassini-Soldner
 *	(transverse plate carree) projection; CAS is arbitrarily used; the
 *	alternative would be to interchange HGLT and HGLN, but that would
 *	necessitate a change in the position angle
 *    When the target cadence is much larger than the input cadence, many
 *	input records are read needlessly; likewise, if there are multiple
 *	records for the same reference time
 *    The removal of observer velocity is performed on mapped images, not
 *	the input, and the removed velocity applies only to the value at
 *	the map center. This is efficient for a small number of tracked
 *	regions, but implies field inaccuracies over large regions
 *    Image geometry for cases with unequal scales CDELti is not to be trusted
 *    The function set_stat_keys tries to set DataVals and MissVals as int's,
 *	even though the relevant values are long long, since there is no
 *	long long checking set key function in keystuff
 *    It is assumed that CROTA2 is opposite to the AIPS convention
 *    Only a single value can be specified for cvok or cvno (which are treated
 *	as string arguments for local parsing, although in principle they
 *	should be arrays of ints)
 *    The function ndimsegments(), which really belongs in a utility
 *	library, is not well tested.
 *    The options for filling of gaps longer than tsept with 0 or NaN rather
 *	than temporal interpolation do not apply to extrapolations at start
 *	or end of tracking interval.
 *
 *  Possible Future Updates
 *    Add option for tracking from interpolated set of target locations
 *    Add ability to track from remapped images, not just helioprojective
 *    Create new data series as needed
 *    Check for MDI source input data and consistency of flag
 *
 *  Revision history is at end of file
 */
/******************************************************************************/

/******************** defines, includes, and global declarations **************/
#define MODULE_VERSION_NUMBER	("2.6")
#define CARTOGRAPHY_VERSION "cartography_v11.c"
#define KEYSTUFF_VERSION "keystuff_v11.c"
#define EARTH_EPHEM_VERSION "earth_ephem_v12.c"
#define SOHO_EPHEM_VERSION "soho_ephem_v11.c"

#include <jsoc_main.h>

#include CARTOGRAPHY_VERSION
#include KEYSTUFF_VERSION
#include EARTH_EPHEM_VERSION
#include SOHO_EPHEM_VERSION
						       /*  module identifier  */
char *module_name = "mtrack";
char *module_desc = "track multiple regions from solar image sequences";
char *version_id = MODULE_VERSION_NUMBER;

#define CARR_RATE       (2.86532908457)
#define RSUNM		(6.96e8)
#define INTERP_NEAREST_NEIGHBOR	(1)
#define INTERP_BILINEAR	(2)
#define FILL_BY_INTERP	(0)
#define FILL_WITH_ZERO	(1)
#define FILL_WITH_NAN	(2)
#define OBS_CRLN_FIXED	(0x10000000)
#define OBS_CROT_FIXED	(0x00000010)

ModuleArgs_t module_args[] = {
  {ARG_STRING,	"in", "", "input data series or dataset"}, 
  {ARG_STRING,	"segment", "Not Specified",
      "input data series segment; ignored if series only has one segment"}, 
  {ARG_STRING,	"out",	"", "output data series"}, 
  {ARG_STRING,	"bckgn", "Not Specified",
      "background record-segment image to be subtracted from input data"}, 
  {ARG_STRING,	"sdofix", "sdo.location",
      "Data series containing corrected SDO observer ephemeris"},
  {ARG_STRING,	"reject", "Not Specified", "file containing rejection list"}, 
  {ARG_INT,	"qmask", "0x80000000", "quality bit mask for image rejection"},
  {ARG_INT,	"max_miss", "All",
      "missing values threshold for image rejection"},
  {ARG_STRING,	"tmid", "Not Specified", "midpoint of tracking interval"}, 
  {ARG_INT,	"length", "Not Specified",
      "target length of tracking interval [in units of input cadence]"}, 
  {ARG_TIME,	"tstart", "Not Specified", "start of tracking interval"}, 
  {ARG_TIME,	"tstop", "Not Specified", "end of tracking interval"}, 
  {ARG_FLOAT,	"tstep", "Not specified", "temporal cadence for output"},
  {ARG_FLOATS,	"lat", "[0.0]",
      "heliographic latitude(s) of tracking center(s) [deg]"},
  {ARG_FLOATS,	"lon", "[0.0]",
      "heliographic longitude(s) of tracking center(s) [deg]"},
  {ARG_NUME, "map", "Postel", "map projection",
      "carree, Cassini, Mercator, cyleqa, sineqa, gnomonic, Postel, stereographic, orthographic, Lambert"},
  {ARG_NUME, "interp", "cubiconv", "interpolation option",
      "cubiconv, nearest, bilinear"},
  {ARG_NUME, "fillopt", "interp", "missing frame fill option",
      "interp, zero, nan"},
  {ARG_FLOAT,	"scale", "Not specified", "map scale at center [deg/pxl]"},
  {ARG_INT,     "cols", "0", "columns in output map(s)"},
  {ARG_INT,     "rows", "0", "rows in output map(s)"},
  {ARG_FLOATS,	"map_pa", "0.0", "position angle(s) of north on output map"},
  {ARG_FLOAT,	"a0", "-0.02893",
      "equatorial rotation - Carrington rate [urad/sec]"},
  {ARG_FLOAT,	"a2", "-0.3441", "solar rotation parameter A2 [urad/sec]"},
  {ARG_FLOAT,	"a4", "-0.5037", "solar rotation parameter A4 [urad/sec]"},
  {ARG_FLOAT,	"merid_v", "0.0",
      "solar meridional velocity [urad/sec]; 0.0014368 * rate in m/s"},
  {ARG_FLOAT,	"bscale", "Segment Default", "output scaling factor"},
  {ARG_FLOAT,	"bzero", "Segment Default", "output offset"},
  {ARG_STRING,	"trec_key", "T_REC", "keyname of (slotted) prime key"}, 
  {ARG_STRING,	"tobs_key", "T_OBS", "keyname for image observation time"}, 
  {ARG_STRING,	"tstp_key", "CADENCE",  "keyname for observation cadence"}, 
  {ARG_STRING,	"qual_key", "Quality",  "keyname for 32-bit image quality field"}, 
  {ARG_STRING,	"clon_key", "CRLN_OBS", "keyname for image central longitude"}, 
  {ARG_STRING,	"clat_key", "CRLT_OBS", "keyname for image central latitude"}, 
  {ARG_STRING,	"crot_key", "CAR_ROT", "keyname for image Carrington rotation"}, 
  {ARG_STRING,	"rsun_key", "R_SUN", "keyname for image semi-diameter (pixel)"}, 
  {ARG_STRING,	"apsd_key", "RSUN_OBS", "keyname for apparent solar semi-diameter (arcsec)"}, 
  {ARG_STRING,	"dsun_key", "DSUN_OBS", "keyname for observer distance"}, 
  {ARG_STRING,	"cvkey", "CalVer64", "keyname for Calibration Version key"}, 
  {ARG_STRING,  "cvok", "any", "Acceptable value of cvkey"},
  {ARG_STRING,  "cvno", "none", "Unacceptable value of cvkey"},
  {ARG_FLOATS,	"mai", "NaN", "Magnetic Activity Indices"},
  {ARG_STRING,	"id_key", "Ident",
      "keyword whose value is to value of ident (if specified)"}, 
  {ARG_STRING,	"ident", "Not Specified", "identifier"}, 
  {ARG_FLAG,	"c",	"", "track at Carrington rate"}, 
  {ARG_FLAG,	"n",	"", "turn off tracking"}, 
  {ARG_FLAG,	"o",	"",
      "remove line-of-sight component of observer velocity"}, 
  {ARG_FLAG,	"r",	"",
      "remove line-of-sight component of solar rotation"}, 
  {ARG_FLAG,	"v",	"", "verbose mode"}, 
  {ARG_FLAG,	"w",	"", "experimental mode (do not save output nor process images)"}, 
  {ARG_FLAG,	"x",	"", "experimental mode (do not save output)"}, 
  {ARG_FLAG,	"G",	"",
  	"use geocentric times for meridian crossing time and locations"}, 
  {ARG_FLAG,	"M",	"",
      "use MDI keywords for input and correct for MDI distortion"}, 
  {ARG_FLAG,	"S",	"", "suppress correction of SDO Carrington longitude"}, 
  {ARG_FLAG,	"Z",	"", "log times in UTC rather than TAI"}, 
  {}
};
       /*  list of keywords to propagate (if possible) from input to output  */
char *propagate[] = {"CONTENT"};

#include "imginfo.c"
#include "mdistuff.c"

typedef enum {LOC_UNKNOWN, LOC_GEOC, LOC_MWO, LOC_GONG_MR, LOC_GONG_LE,
    LOC_GONG_UD, LOC_GONG_TD, LOC_GONG_CT, LOC_GONG_TC, LOC_GONG_BB,
    LOC_GONG_ML, LOC_SOHO, LOC_SDO} platloc;

typedef struct {
  TIME trec;
  TIME tobs;
  double clon;
  double clat;
  double vr;
  double vw;
  double vn;
  double dist;
  double apsd;
  int crot;
} OBS_EPHEMERIS;

char *prime_keylist[] = {"CarrRot", "CMLon", "LonHG", "LatHG", "Duration",
    "Width", "Height", "Source", "ZonalTrk", "MeridTrk", "MapProj", "MapScale"};

		 /*  global declaration of missing to be initialized as NaN  */
float missing_val;

/***************************** internal functions *****************************/
/*
 *  function table of contents
 *    Functions are declared in the order in which they or their ancestors
 *    are called in the main module or in their ancestors.
 *    This list excludes functions in included files
 *	  function			  called in
 *	params_get_mask			main
 *	key_params_from_dspec		track_record_selection
 *	drms_key_is_slotted		get_cadence, track_record_selection
 *	get_cadence			track_record_selection
 *	platform_info			track_record_selection
 *	time_from_crcl			track_record_selection
 *	select_segment			track_record_selection
 *	track_record_selection		main
 *	check_cvbits			main
 *	verify_keys			main
 *	ndimsegments			main
 *	interp_crlfix			adjust_obs2rec
 *	interp_hmi_fixes		adjust_obs2rec
 *	adjust_obs2rec			main
 *	ephemeris_vector		main
 *	interp_crcfromtvec		adjust_sdo_clon
 *	adjust_sdo_clon			getlon_span; main
 *	getctrloc_from_time		getlon_span, main
 *	getlon_span			main
 *	fgetline			read_reject_list
 *	read_reject_list		main
 *	free_all			main
 *	fsphere2img			perform_mappings
 *	nearest_val			array_imaginterp
 *	linint2				array_imaginterp
 *	ccker				ccint2
 *	ccint2				array_imaginterp
 *	array_imaginterp		perform_mappings
 *	perform_mappings		main
 *	calc_limb_distance		main
 *	ephemeris_params		main
 *	adjust_for_observer_velocity	main
 *	adjust_for_solar_rotation	main
 *	create_prime_key		main
 *	set_stat_keys			main
 */
/******************************************************************************/
			  /*  the following belong in external utility files  */
long long params_get_mask (CmdParams_t *params, char *arg,
    long long defval) {
/*
 *  This function parses the string associated with the command parameters
 *    argument "arg" as the hexadecimal representation of an unsigned 64-bit
 *    integer, which it returns. The string may consist of up to 16 hexadecimal
 *    characters. (They can optionally be preceded by '0x', but the string is
 *    treated as a hexadecimal representation regardless.)  If there are any
 *    extra or illegal characters in the string, the value "defval" is returned.
 *  There is another copy in datavg.c
 */
  long long retval;
  const char *str = params_get_str (params, arg);
  char *ext;

  retval  = strtoull (str, &ext, 16);
  if (strlen (ext)) retval = defval;
  return retval;
}

int key_params_from_dspec (const char *dspec) {
/*
 *  Establish whether target times are determined from dataset specifier
 *    assume that if a bracket is found in the dataset specifier it is a
 *    set of well-specified records containing a properly ordered input
 *    dataset; otherwise, a dataseries to be queried
 */
  int n, nt = strlen (dspec);

  for (n = 0; n < nt; n++) if (dspec[n] == '[') return 1;
  return 0;
}

int drms_key_is_slotted (DRMS_Env_t *drms_env, const char *keyname,
    const char *dsname) {
  DRMS_Record_t *rec;
  DRMS_Keyword_t *key;
  int status;

  rec = drms_template_record (drms_env, dsname, &status);
  if (status) return 0;
  key = drms_keyword_lookup (rec, keyname, 1);
  if (!key) return 0;
  status = (key->info->recscope > 1);
  drms_free_keyword_struct (key);
  return status;
}

int get_cadence (DRMS_Record_t *rec, const char *source, const char *tstp_key,
    const char *trec_key, double *tstep, double *cadence) {
			      /*  attempt to determine uniform input cadence  */
/*
 *  If the data cadence keyword tstp_key ("Cadence" by default) is present
 *    in the input series and is constant, use it
 *  If it is variable, or missing, and the time keyword trec_key ("T_REC" by
 *    default) exists and is slotted, use its step length as the cadence
 *  If tstp_key is missing or variable and trec_key is missing or unslotted,
 *    use (and require) the output step size *cadence
 */
  DRMS_Keyword_t *keywd;
  int status;

  if ((keywd = drms_keyword_lookup (rec, tstp_key, 1))) {
					      /*  cadence should be constant  */
    *cadence = drms_getkey_double (rec, tstp_key, &status);
    if (keywd->info->recscope != 1) {
      fprintf (stderr, "Warning: %s is variable in input series %s\n",
	  tstp_key, source);
      drms_free_keyword_struct (keywd);
      if ((keywd = drms_keyword_lookup (rec, trec_key, 1))) {
	if (drms_key_is_slotted (drms_env, trec_key, source)) {
	  char *stepkey = malloc (strlen (trec_key) + 8);
	  sprintf (stepkey, "%s_step", trec_key);
	  *cadence = drms_getkey_double (rec, stepkey, &status);
	  free (stepkey);
	} else {
	  fprintf (stderr, "         and %s is not slotted\n", trec_key);
	  fprintf (stderr, "         output cadence will be %f\n", *tstep);
	  *cadence = *tstep;
	}
	drms_free_keyword_struct (keywd);
      } else {
	fprintf (stderr, "         and %s is not present\n", trec_key);
	fprintf (stderr, "         output cadence will be %f\n", *tstep);
	*cadence = *tstep;
      }
    }
    if (*tstep > 0) *tstep *= *cadence;
    else *tstep = *cadence;
  } else {
			     /* cadence key missing, use trec_key if slotted  */
    drms_free_keyword_struct (keywd);
    if ((keywd = drms_keyword_lookup (rec, trec_key, 1))) {
      if (drms_key_is_slotted (drms_env, trec_key, source)) {
	char *stepkey = malloc (strlen (trec_key) + 8);
	sprintf (stepkey, "%s_step", trec_key);
	*cadence = drms_getkey_double (rec, stepkey, &status);
	free (stepkey);
      } else if (isnan (*tstep)) {
	fprintf (stderr,
            "Error: data cadence keyword %s not in input series %s\n",
	    tstp_key, source);
	fprintf (stderr, "       and %s is not slotted\n", trec_key);
	fprintf (stderr, "       Specify desired output cadence as tstep\n");
	return 1;
      }
      drms_free_keyword_struct (keywd);
    } else if (isnan (*tstep)) {
			    /*  output cadence not specified either: give up  */
      fprintf (stderr,
          "Error: data cadence keyword %s not in input series %s\n",
	  tstp_key, source);
      fprintf (stderr, "       and %s is not present\n", trec_key);
      fprintf (stderr, "       Specify desired output cadence as tstep\n");
      return 1;
    } else *cadence = *tstep;
  }
		       /*  set a missing output cadence to the input cadence  */
  if (isnan (*tstep)) *tstep = *cadence;
  return 0;
}

static platloc platform_info (DRMS_Record_t *rec, char *source_series) {
  platloc platform = LOC_UNKNOWN;
  int status;
  DRMS_Keyword_t *keywd;

  if ((keywd = drms_keyword_lookup (rec, "TELESCOP", 1))) {
						      /*  should be constant  */
    if (keywd->info->recscope != 1)
      fprintf (stderr, "Warning: TELESCOP is variable in input series %s\n",
	  source_series);
    if (!strcmp (drms_getkey_string (rec, "TELESCOP", &status), "SDO/HMI") ||
      !strcmp (drms_getkey_string (rec, "TELESCOP", &status), "SDO/AIA"))
      platform = LOC_SDO;
    else if (!strcmp (drms_getkey_string (rec, "TELESCOP", &status), "SOHO"))
      platform = LOC_SOHO;
    else if (!strcmp (drms_getkey_string (rec, "TELESCOP", &status),
	"NSO-GONG")) {
      if ((keywd = drms_keyword_lookup (rec, "SITE", 1))) {
	if (!strcmp (drms_getkey_string (rec, "SITE", &status), "MR"))
	  platform = LOC_GONG_MR;
	else if (!strcmp (drms_getkey_string (rec, "SITE", &status), "LE"))
	  platform = LOC_GONG_LE;
	else if (!strcmp (drms_getkey_string (rec, "SITE", &status), "UD"))
	  platform = LOC_GONG_UD;
	else if (!strcmp (drms_getkey_string (rec, "SITE", &status), "TD"))
	  platform = LOC_GONG_TD;
	else if (!strcmp (drms_getkey_string (rec, "SITE", &status), "CT"))
	  platform = LOC_GONG_CT;
	else if (!strcmp (drms_getkey_string (rec, "SITE", &status), "BB"))
	  platform = LOC_GONG_BB;
	else if (!strcmp (drms_getkey_string (rec, "SITE", &status), "ML"))
	  platform = LOC_GONG_ML;
	else {
	  platform = LOC_GONG_MR;
	}
      } else {
        fprintf (stderr, "Warning: unspecified GONG site: MR assumed\n");
	platform = LOC_GONG_MR;
      }
    }
  }
  return platform;
}

TIME time_from_crcl (int cr, double cl, int from_soho) {
  if (from_soho)
    return SOHO_meridian_crossing (cl, cr);
  else
    return earth_meridian_crossing (cl, cr);
}
/*
 *  This function implements the segment selection rules of the specification
 *    for mtrack v 2.3 and is intended to be used in substitution for the code
 *    in the module body; the iterator logic is necessary because the
 *    drms_segment_lookup* functions do not follow links when the record is
 *    not instantiated (i.e. a template record).
 */ 
char *select_segment (DRMS_Record_t *trec, char *parsed_segname) {
  DRMS_Segment_t *seg;
  HIterator_t hit;
  int n, segct;

  int found = 0;
  char *first_seg = NULL;
  segct = drms_record_numsegments (trec);
  if (parsed_segname) {
       /*  if a segment has been specified, check that it exists and is okay  */
    for (n = 0; n < segct; n++) {
      hiter_new_sort (&hit, &trec->segments, drms_segment_ranksort);
      while ((seg = (DRMS_Segment_t *)hiter_getnext (&hit))) {
	if (seg->info->segnum == n) break;
	if (seg->info->segnum > n) {
	  seg = NULL;
	  break;
	}
      }
      hiter_free (&hit);
      if (strcasecmp (seg->info->name, parsed_segname)) continue;
      if (seg->info->naxis != 2) return NULL;
      if (seg->info->protocol == DRMS_GENERIC) return NULL;
      return parsed_segname;
    }
    return NULL;
  } else {
				       /*  look for first 2-dim, warn if more */
    for (n = 0; n < segct; n++) {
      hiter_new_sort (&hit, &trec->segments, drms_segment_ranksort);
      while ((seg = (DRMS_Segment_t *)hiter_getnext (&hit))) {
	if (seg->info->segnum == n) break;
	if (seg->info->segnum > n) {
	  seg = NULL;
	  break;
	}
      }
      hiter_free (&hit);
      if (seg->info->naxis == 2 && seg->info->protocol != DRMS_GENERIC) {
        if (!found) first_seg = seg->info->name;
	found++;
      }
    }
    if (found > 1) {
      fprintf (stderr, "Warning: multiple 2-d segments in series %s\n",
	  seg->record->seriesinfo->seriesname);
      fprintf (stderr, "         using %s\n", first_seg);
    }
    return first_seg;
  }
}
/*
 *  This function implements the record set selection rules of the
 *    specification for the current version of mtrack, making use of
 *    the DRMS localized serverdefs to support Carrington times
 */ 
DRMS_RecordSet_t *track_record_selection (const char *inspec,
    const char *tstrt_str, const char *tstop_str, const char *tmid_str,
    int length, const char *tstp_key, const char *trec_key, double *tstep,
    double *cadence, int geo_times, char *reqseg, char **segname,
    char **input_set, int *tstrtstop, int *time_cr, double *time_cl,
    int *need_crcl, platloc *platform, int verbose) {
  DRMS_RecordSet_t *ds = NULL;
  DRMS_Record_t *trec;
  TIME pkeybase, tbase, tmid, tstrt, tstop;
  double dt, phase, pkeystep;
  int tstrt_ind, tstop_ind;
  int slotted, status;
  char *eoser, *eoseg, *segnames, *parsed_segname;
  char rec_query[DRMS_MAXQUERYLEN];
  char tbuf[64], ptbuf[64], pttbuf[64];

  int explicit = 0;
  char *source_series = strdup (inspec);
  char *pkindx = malloc (strlen (trec_key) + 8);
  char *pkbase = malloc (strlen (trec_key) + 8);
  char *pkstep = malloc (strlen (trec_key) + 8);

  *tstrtstop = 0;
  segnames = strchr (source_series, '{');
  if (segnames) {
    eoseg = strchr (segnames, '}');
    if (eoseg) *eoseg = '\0';
    parsed_segname = segnames + 1;
    if (strchr (segnames, ',')) {
      fprintf (stderr, "Error: only a single segment can be selected\n");
      return ds;
    }
				  /*  check for conflicting segment requests  */
    if (strcmp (reqseg, "Not Specified")) {
      if (strcasecmp (reqseg, parsed_segname)) {
	fprintf (stderr, "Error: conflicting segment names %s and %s\n",
	    reqseg, parsed_segname);
	fprintf (stderr, "       only one can be selected\n");
	return ds;
      }
    }
  } else parsed_segname = (strcmp (reqseg, "Not Specified")) ? reqseg : NULL;
  sprintf (rec_query, "");
  if (key_params_from_dspec (inspec)) {
				    /*  input specified as specific data set  */
			/*  this overrides any time specification via the
				arguments tstart, tstop, tmid, and/or length  */
    if (strcmp (tstrt_str, "Not Specified") ||
	strcmp (tstop_str, "Not Specified") ||
        strcmp (tmid_str, "Not Specified") || (length > 0)) {
      fprintf (stderr, "Warning: input record set explicitly specified:\n");
      fprintf (stderr,
	  "         tstart, tstop, tmid and length values ignored\n");
    }
    eoser = strchr (source_series, '[');
    if (eoser) *eoser = '\0';
    eoser = strchr (source_series, '{');
    if (eoser) *eoser = '\0';
    strcpy (rec_query, inspec);
    trec = drms_template_record (drms_env, source_series, &status);
    get_cadence (trec, source_series, tstp_key, trec_key, tstep, cadence);
    dt = *cadence;
    sprintf (pkindx, "%s_index", trec_key);
    *platform = platform_info (trec, source_series);
    explicit = 1;
  } else {
				/*  only the input data series is named,
				    get record specifications from arguments  */
    eoser = strchr (source_series, '{');
    if (eoser) *eoser = '\0';
    trec = drms_template_record (drms_env, source_series, &status);
    if (!trec) {
      fprintf (stderr, "Error: unable to find requested series \"%s\"\n",
	  source_series);
      return ds;
    }
    if (get_cadence (trec, source_series, tstp_key, trec_key, tstep, cadence))
      return ds;
    dt = *cadence;
    *platform = platform_info (trec, source_series);
    if (slotted = drms_key_is_slotted (drms_env, trec_key, source_series)) {
      sprintf (pkindx, "%s_index", trec_key);
      sprintf (pkbase, "%s_epoch", trec_key);
      sprintf (pkstep, "%s_step", trec_key);
      tbase = pkeybase = drms_getkey_time (trec, pkbase, &status);
      pkeystep = drms_getkey_double (trec, pkstep, &status);
    }
    if (strcmp (tmid_str, "Not Specified")) {
      if (strcmp (tstrt_str, "Not Specified") ||
	  strcmp (tstop_str, "Not Specified")) {
	fprintf (stderr, "Warning: with tmid and length specified,\n");
	fprintf (stderr,
	  "         tstart and tstop values ignored\n");
      }
      if (length <= 0) {
	fprintf (stderr, "Error: if tmid is specified,");
	fprintf (stderr, " length in units of cadence must also be specified\n");
	return ds;
      }
	/*  determine start and stop times from length (in units of tstep)
			and midtime (which can be CR:CL as well as date_time) */
      if (sscanf (tmid_str, "%d:%lf", time_cr, time_cl) == 2) {
			   /*  tmid specified as CR:CL : need ephemeris info  */
	*need_crcl = 0;
	if (geo_times || *platform != LOC_SOHO) {
#ifndef JPL_EPHEM_TABLEDIR
	  fprintf (stderr,
	      "Error: no ephemeris info, cannot specify time in CT format\n");
	  return ds;
#endif
	  tmid = time_from_crcl (*time_cr, *time_cl, 0);
	} else {
#ifndef SOHO_EPHEM_TABLE
	  fprintf (stderr,
	      "Error: no ephemeris info, cannot specify time in CT format\n");
	  return ds;
#endif
	  tmid = time_from_crcl (*time_cr, *time_cl, 1);
	}
	if (time_is_invalid (tmid)) {
	  fprintf (stderr,
	      "Error: Unable to determine time from Carrington longitude\n");
	  return ds;
	}
      } else		       /*  tmid specified as normal date-time string  */
	tmid = sscan_time ((char *)tmid_str);
      sprint_time (ptbuf, tmid, "TAI", 0);
      if (slotted) {
	phase = fmod ((tmid - tbase), dt);
	if (length % 2 == 0) phase += 0.5 * dt;
	if (phase > dt) phase -= dt;
	tmid -= phase;
	if (verbose) {
	  sprint_time (tbuf, tmid, "TAI", 0);
	  if (*need_crcl) {
	    if (strcmp (ptbuf, tbuf))
	      printf ("Target time %s adjusted to %s\n", tmid_str, tbuf);
	  } else {
	    sprint_time (tbuf, tmid, "TAI", 1);
	    printf ("Target time %d:%05.1f = %s adjusted to\n\t%s\n",
	      *time_cr, *time_cl, ptbuf, tbuf);
	  }
	}
      }
      tstrt = tmid - (length / 2) * *tstep;
      if (length % 2 == 0) tstrt += 0.5 * dt;
      tstop = tstrt + (length - 1) * *tstep;
    } else {
      if (sscanf (tstrt_str, "%d:%lf", time_cr, time_cl) == 2) {
			   /*  time specified as CR:CL : need ephemeris info  */
	if (geo_times || *platform != LOC_SOHO) {
#ifndef JPL_EPHEM_TABLEDIR
	  fprintf (stderr,
	      "Error: no ephemeris info, cannot specify time in CT format\n");
	  return ds;
#endif
	  tstrt = time_from_crcl (*time_cr, *time_cl, 0);
	} else {
#ifndef SOHO_EPHEM_TABLE
	  fprintf (stderr,
	      "Error: no ephemeris info, cannot specify time in CT format\n");
	  return ds;
#endif
	  tstrt = time_from_crcl (*time_cr, *time_cl, 1);
	}
	if (time_is_invalid (tstrt)) {
	  fprintf (stderr,
	      "Error: Unable to determine time from Carrington longitude\n");
	  return ds;
	}
        sprint_time (ptbuf, tstrt, "TAI", 0);
      } else {		       /*  time specified as normal date-time string  */
        *tstrtstop = 1;
	tstrt = sscan_time ((char *)tstrt_str);
      }
      if (time_is_invalid (tstrt)) {
	fprintf (stderr,
	    "Error: either a specific data record set must be selected as input\n");
	fprintf (stderr,
	    "       or (tmid and length) or (tstart and tstop) must be specified\n");
	return ds;
      }
      if (sscanf (tstop_str, "%d:%lf", time_cr, time_cl) == 2) {
			   /*  time specified as CR:CL : need ephemeris info  */
	if (geo_times || *platform != LOC_SOHO) {
#ifndef JPL_EPHEM_TABLEDIR
	  fprintf (stderr,
	      "Error: no ephemeris info, cannot specify time in CT format\n");
	  return ds;
#endif
	  tstop = time_from_crcl (*time_cr, *time_cl, 0);
	} else {
#ifndef SOHO_EPHEM_TABLE
	  fprintf (stderr,
	      "Error: no ephemeris info, cannot specify time in CT format\n");
	  return ds;
#endif
	  tstop = time_from_crcl (*time_cr, *time_cl, 1);
	}
	if (time_is_invalid (tstop)) {
	  fprintf (stderr,
	      "Error: Unable to determine time from Carrington longitude\n");
	  return ds;
	}
	*tstrtstop = 0;
      } else		       /*  time specified as normal date-time string  */
	tstop = sscan_time ((char *)tstop_str);
      if (time_is_invalid (tstop)) {
	fprintf (stderr,
	    "Error: either a specific data record set must be selected as input\n");
	fprintf (stderr,
	    "       or (tmid and length) or (tstart and tstop) must be specified\n");
	return ds;
      }
      sprint_time (pttbuf, tstop, "TAI", 0);
      if (slotted) {
		 /*  assume that prime key specified by trec_key is slotted!  */
	phase = fmod ((tstrt - tbase), dt);
	tstrt -= phase;
	if (phase > 0.5 * dt) tstrt += dt;
	phase = fmod ((tstop - tbase), dt);
	tstop -= phase;
	if (phase > 0.5 * dt) tstop += dt;
      }
    }
    if (slotted) {
		 /*  assume that prime key specified by trec_key is slotted!  */
      tstrt_ind = (tstrt - pkeybase + 0.5 * pkeystep) / pkeystep;
      tstop_ind = (tstop - pkeybase + 0.5 * pkeystep) / pkeystep;
      snprintf (rec_query, 256, "%s[?%s between %d and %d?]", source_series,
	  pkindx, tstrt_ind, tstop_ind);
    } else {
      snprintf (rec_query, 256, "%s[?%s between %23.16e and %23.16e?]",
	  source_series, trec_key, tstrt - 0.01 * dt, tstop + 0.01 * dt);
    }
  }
						     /*  check input segment  */
		     /*  doesn't work properly if there are linked segments!  */
  *segname = select_segment (trec, parsed_segname);
  if (*segname == NULL) {
    if (parsed_segname)
      fprintf (stderr, "Error: segment \"%s\" is not in series, or is wrong type\n",
	  parsed_segname);
    else fprintf (stderr, "Error: no appropriate segments in series %s\n",
	source_series);
    return ds;
  }
  *input_set = strdup (rec_query);
  if (!(ds = drms_open_records (drms_env, rec_query, &status))) {
    fprintf (stderr, "Error: unable to open selected data set \"%s\"\n",
	rec_query);
    fprintf (stderr, "       status = %d\n", status);
    return ds;
  }
  if (explicit) {
					/*  for explictly named dataset,
				check if record spec implies reduced cadence  */
    int cmult = (ds->n < 2) ? 1 :
	drms_getkey_int (ds->records[1], pkindx, &status) -
	drms_getkey_int (ds->records[0], pkindx, &status);
    if (!status) *cadence *= cmult;
  }
  free (pkindx);
  free (pkbase);
  free (pkstep);
  return ds;
}
/*
 *  Check for Calibration Version bits signifying key corrections
 *  If the first record encountered has the corresponding bit set,
 *    no correction is assumed needed, otherwise it is assumed needed.
 *  If any subsequent record has a different CalVer pattern, even if
 *    the relevant bit is the same, the function returns 1, indicating
 *    that each record should be inspected.
 *  It would be simpler just to fill an array of need_fix values
 */
int check_cvbits (DRMS_RecordSet_t *ds, char *key, unsigned int keyval,
    int *need_fix) {
  DRMS_Record_t *rec;
  long long calver;
  int status;
  int n = 0, recct = ds->n;

  rec = ds->records[n];
  calver = drms_getkey_longlong (rec, key, &status);
  *need_fix = (calver & keyval) ? 0 : 1;
  for (n = 1; n < recct; n++) {
    rec = ds->records[n];
    if (calver != drms_getkey_longlong (rec, key, &status))
      return 1;
  }
  return 0;
}

int verify_keys (DRMS_Record_t *rec, const char *clon,
    const char *clat, double *keyscale) {
  DRMS_Keyword_t *keywd;
  
  keywd = drms_keyword_lookup (rec, clon, 1);
  if (!keywd) {
    fprintf (stderr,
	"Error: Keyword \"%s\" for Carrington longitude of observer not found\n",
	clon);
    fprintf (stderr, "       Must supply an appropriate value for clon_key\n");
    fprintf (stderr, "       (Carrington longitude of disc center in deg)\n");
    return -1;
  }
  if (keywd->info->type == DRMS_TYPE_TIME ||
      keywd->info->type == DRMS_TYPE_STRING ||
      keywd->info->type == DRMS_TYPE_RAW) {
    fprintf (stderr,
	"Error: Keyword \"%s\" for observer Carrington longitude is of wrong type\n",
	clon);
    fprintf (stderr, "       Must supply an appropriate value for clon_key\n");
    fprintf (stderr, "       (Carrington longitude of disc centre in deg)\n");
    return -1;
  }
  if (strncasecmp (keywd->info->unit, "deg", 3)) {
    fprintf (stderr,
	"Warning: Keyword \"%s\" for observer Carrington longitude has unit of \"%s\"\n",
	clon, keywd->info->unit);
    fprintf (stderr, "         ignored, \"deg\" assumed\n");
  }

  keywd = drms_keyword_lookup (rec, clat, 1);
  if (!keywd) {
    fprintf (stderr,
	"Error: Keyword \"%s\" for observer heliographic latitude not found\n",
	clat);
    fprintf (stderr, "       Must supply an appropriate value for clat_key\n");
    fprintf (stderr, "       (heliographic latitude of disc centre in deg)\n");
    return -1;
  }
  if (keywd->info->type == DRMS_TYPE_TIME ||
      keywd->info->type == DRMS_TYPE_STRING ||
      keywd->info->type == DRMS_TYPE_RAW) {
    fprintf (stderr,
	"Error: Keyword \"%s\" for observer heliographic latitude is of wrong type\n",
	clat);
    fprintf (stderr, "       Must supply an appropriate value for clat_key\n");
    fprintf (stderr, "       (heliographic latitude of disc centre in deg)\n");
    return -1;
  }
  if (strncasecmp (keywd->info->unit, "deg", 3)) {
    fprintf (stderr,
	"Warning: Keyword \"%s\" for observer heliographic latitude has unit of \"%s\"\n",
	clat, keywd->info->unit);
    fprintf (stderr, "         ignored, \"deg\" assumed\n");
  }

  return 0;
}

int *ndimsegments (DRMS_Record_t *rec, int ndim, int *ct) {
/*  
 *  Returns a list of the segnums of the actually available segments (in case
 *    segments have been named in the dataset specification) of rank ndim
 *    in the record rec
 */
  DRMS_Record_t *temprec;
  DRMS_Segment_t *seg;
  static int *seglist = NULL;
  int found, n, segct, status;

  temprec = drms_template_record (drms_env, rec->seriesinfo->seriesname, &status);
  segct = drms_record_numsegments (temprec);
  if (!seglist) seglist = (int *)realloc (seglist, segct * sizeof (int));
  found = 0;
  for (n = 0; n < segct; n++) {
    seg = drms_segment_lookupnum (rec, n);
    if (!(seg = drms_segment_lookupnum (rec, n))) continue;
    if (seg->info->naxis == ndim) seglist[found++] = n;
  }
  *ct = found;
  return seglist;
}
/*
 *  Interpolate Carrington longitude of disc center, and if necessary
 *    Carrington Rotation number, from tobs to trec
 */
int interp_crlfix (double *tobs, double trec, int nt, double **crcl,
    double *clfix, int *crfix) {
  double clt, dt, dl, fac;
  int n = 0;
			      /*  if target out of bounds, can't interpolate  */
  if (trec < tobs[0] || trec > tobs[nt-1]) return 1;
  while (trec >= tobs[n] && n < nt) n++;
  if (n == nt) return 1;
					  /*  likewise if obstime is missing  */
  if (time_is_invalid ((TIME)(tobs[n])) ||
      time_is_invalid ((TIME)(tobs[n-1]))) return 1;
  dt = tobs[n] - tobs[n-1];
  if (isnan (dt)) return 1;
  fac = (trec - tobs[n-1]) / dt;
  dl = crcl[1][n] - crcl[1][n-1];
  *crfix = crcl[0][n-1] + 0.001;
  if (dl > 180.0) {
    dl -= 360.0;
    *crfix += 1;
  }
  if (dl < -180.0) {
    dl += 360.0;
    *crfix -= 1;
  }

  clt = crcl[1][n-1] + fac * dl;
  if (clt < 0.0) {
    clt += 360.0;
    *crfix += 1;
  }
  *clfix = clt;
  return 0;
}
/*
 *  Interpolate standard, non-periodic variables from tobs to trec
 */
int interp_hmi_fixes (double *x, double x0, int nx, double **yt, double *y,
    int yct) {
  double dx, dy, fac;
  int i;
  int n = 0;
			      /*  if target out of bounds, can't interpolate  */
  if (x0 < x[0] || x0 > x[nx-1]) return 1;
  while (x0 >= x[n] && n < nx) n++;
  if (n == nx) return 1;
					  /*  likewise if obstime is missing  */
  if (time_is_invalid ((TIME)(x[n])) ||
      time_is_invalid ((TIME)(x[n-1]))) return 1;
  dx = x[n] - x[n-1];
  fac = (x0 - x[n-1]) / dx;
  for (i = 0; i < yct; i++) {
    dy = yt[i][n] - yt[i][n-1];
    y[i] = yt[i][n-1] + fac * dy;
  }
  return 0;
}
/*
 *  Adjust ephemeris values applicable to T_OBS to T_REC by interpolation
 */
void adjust_obs2rec (OBS_EPHEMERIS *rec, DRMS_RecordSet_t *ids, int ct,
    char *trec_key, char *tobs_key, char *crot_key, char *clon_key,
    char *clat_key, char *dsun_key, char *apsd_key, TIME tstrt, TIME tstop,
    double data_cadence, char *source_series) {
  DRMS_Record_t *irec;
  DRMS_Array_t *sdoephvec, *sdocrlvec;
  double **efixp, **crlfix;
  double *tfix, *efix;
  double loncfix;
  int l, m, n, tblct, valct;
  int nr, status;
  int fixct, efixct, crfix;
  char rec_query[DRMS_MAXQUERYLEN];
  char keylist[256], tbuf0[64], tbuf1[64];
				   /*  fill ephemeris vector from key values  */
  for (nr = 0; nr < ct; nr++) {
    irec = ids->records[nr];
    rec[nr].trec = drms_getkey_time (irec, trec_key, &status);
    rec[nr].tobs = drms_getkey_time (irec, tobs_key, &status);
    rec[nr].crot = drms_getkey_int (irec, crot_key, &status);
    rec[nr].clon = drms_getkey_double (irec, clon_key, &status);
    rec[nr].clat = drms_getkey_double (irec, clat_key, &status);
    rec[nr].vr = drms_getkey_double (irec, "OBS_VR", &status);
    rec[nr].vw = drms_getkey_double (irec, "OBS_VW", &status);
    rec[nr].vn = drms_getkey_double (irec, "OBS_VN", &status);
    rec[nr].dist = drms_getkey_double (irec, dsun_key, &status);
    rec[nr].apsd = drms_getkey_double (irec, apsd_key, &status);
  }
  sprint_time (tbuf0, tstrt - data_cadence, "TAI", 0);
  sprint_time (tbuf1, tstop + data_cadence, "TAI", 0);
  sprintf (rec_query, "%s[%s-%s]\n", source_series, tbuf0, tbuf1);
  sprintf (keylist, "%s,%s,OBS_VR,OBS_VW,OBS_VN,%s,%s", tobs_key, clat_key,
        dsun_key, apsd_key);
  sdoephvec = drms_record_getvector (drms_env, rec_query, keylist,
	DRMS_TYPE_DOUBLE, 0, &status);
  tblct = sdoephvec->axis[1];
  valct = sdoephvec->axis[0];
  tfix = (double *)malloc (tblct * sizeof (double));
  for (n = 0; n < tblct; n++)
    tfix[n] = ((double *)sdoephvec->data)[n];
  efixp = (double **)malloc (valct * sizeof (double *));
  efix = (double *)malloc (valct * sizeof (double));
  for (l = 0; l < valct; l++) {
    efixp[l] = (double *)malloc (tblct * sizeof (double));
    for (m = 0; m < tblct; m++, n++)
      efixp[l][m] = ((double *)sdoephvec->data)[n];
  }
  drms_free_array (sdoephvec);
  fixct = tblct;
  efixct = valct;
  sprint_time (tbuf0, tstrt - data_cadence, "TAI", 0);
  sprint_time (tbuf1, tstop + data_cadence, "TAI", 0);
  sprintf (rec_query, "%s[%s-%s]\n", source_series, tbuf0, tbuf1);
  sprintf (keylist, "%s,%s,%s", tobs_key, crot_key, clon_key);
  sdocrlvec = drms_record_getvector (drms_env, rec_query, keylist,
      DRMS_TYPE_DOUBLE, 0, &status);
  valct = sdocrlvec->axis[0];
  crlfix = (double **)malloc (valct * sizeof (double *));
  n = tblct;
  for (l = 0; l < valct; l++) {
    crlfix[l] = (double *)malloc (tblct * sizeof (double));
    for (m = 0; m < tblct; m++, n++)
      crlfix[l][m] = ((double *)sdocrlvec->data)[n];
  }
  drms_free_array (sdocrlvec);
  for (nr = 0; nr < ct; nr++) {
    status = interp_crlfix (tfix, (double)(rec[nr].trec), fixct,
	crlfix, &loncfix, &crfix);
    if (status) continue;
    rec[nr].crot = crfix;
    rec[nr].clon = loncfix;
    status = interp_hmi_fixes (tfix, (double)(rec[nr].trec), fixct,
	efixp, efix, efixct);
    if (status) continue;
    rec[nr].clat = efix[0];
    rec[nr].vr = efix[1];
    rec[nr].vw = efix[2];
    rec[nr].vn = efix[3];
    rec[nr].dist = efix[4];
    rec[nr].apsd = efix[5];
  }
}
/*
 *  Create interpolation vector of requested ephemeris value from
 *    observer coordinates series; tstrt and tstop should extend
 *    at least one time step in correction series beyond times needed
 *    for linear interpolation
 */
DRMS_Array_t *ephemeris_vector (char *ephem_series, char *trec_key,
    char *ephem_key, TIME tstrt, TIME tstop) {
  DRMS_Array_t *ephemvec;
  DRMS_Record_t *trec;
  double cadence;
  int status;
  char rec_query[DRMS_MAXQUERYLEN];
  char keylist[256], tbuf0[64], tbuf1[64], key[32];

  trec = drms_template_record (drms_env, ephem_series, &status);
  if (status) return NULL;
  sprintf (key, "%s_step", trec_key);
  cadence = drms_getkey_double (trec, key, &status);
  if (status) return NULL;

  sprint_time (tbuf0, tstrt - cadence, "TAI", 0);
  sprint_time (tbuf1, tstop + cadence, "TAI", 0);
  sprintf (rec_query, "%s[%s-%s]\n", ephem_series, tbuf0, tbuf1);
  sprintf (keylist, "%s,%s", trec_key, ephem_key);
  ephemvec = drms_record_getvector (drms_env, rec_query, keylist,
      DRMS_TYPE_DOUBLE, 0, &status);
  return ephemvec;
}
/*
 *  Linear interpolation in a time-ordered table of an angular variable
 *    represented mod 360
 *  It is assumed that the function is called with time targets ordered
 *    in the same direction as the table
 */
int interp_crcfromtvec (TIME t0, double *vfix, TIME *tblt, double *tblv,
    int tblct) {
  double dt, dv, fac, vt;
  int n = 0;

  if (t0 < tblt[0] || t0 > tblt[tblct-1]) return 1;
  while (t0 >= tblt[n] && n < tblct) n++;
  if (n == tblct) return 1;
  dt = tblt[n] - tblt[n-1];
  if (isnan (dt)) return 1;
  fac = (t0 - tblt[n-1]) / dt;
  dv = tblv[n] - tblv[n-1];
  if (dv > 180.0) dv -= 360.0;
  if (dv < -180.0) dv += 360.0;
  vt = tblv[n-1] + fac * dv;
  if (vt < 0.0) vt += 360.0;
  *vfix = vt;
  return 0;
}
/*
 *  Correct the value of CRLN_OBS, either by reference to a table or
 *    by subtraction of a constant
 */
double adjust_sdo_clon (double cmlon_orig, TIME trgt, int use_table,
    TIME *fixt, double *fixv, int fixct, int *status) {
  char rec_query[DRMS_MAXQUERYLEN];

  double cmlon = cmlon_orig;

  if (use_table) {
    *status = interp_crcfromtvec (trgt, &cmlon, fixt, fixv, fixct);
    if (*status) {
      fprintf (stderr, "Warning: unable to interpolate CMLon from table\n");
      fprintf (stderr, "         using constant adjustment\n");
      cmlon -= 0.081894;
    }
  } else {
    *status = 0;
    cmlon -= 0.081894;
  }
  if (cmlon < 0.0) cmlon += 360.0;
  if (cmlon >= 360.0) cmlon -= 360.0;
  return cmlon;
}
/*
 *  Infer the Sun center location (Carrington longitude and latitude,
 *    as well as the rotation number) from the observation time and ephemeris
 *    for the given platform
 *
 *  All known platforms other than SOHO return geocentric values
 */
int getctrloc_from_time (TIME t, double *img_lat, double *cm_lon, int *cr,
    platloc platform) {
  double rsun, vr, vn, vw;
  TIME table_mod_time;
  int status = 0;

  if (platform == LOC_SOHO) {
    status = soho_ephemeris (t, &rsun, img_lat, cm_lon, &vr, &vn, &vw, &table_mod_time);
    *cr = carrington_rots (t, 1);
  } else if (platform == LOC_UNKNOWN) return 1;
  else {
    status = earth_ephemeris (t, &rsun, img_lat, cm_lon, &vr, &vn, &vw);
    *cr = carrington_rots (t, 0);
  }
  return status;
}
/*
 *  Determine the Carrington central-meridian-longitude span of the input
 *    dataset from (a) the clon and crot keywords from the records that
 *    correspond to the start and stop times if they are valid; otherwise
 *    (b) the computed values from the start and stop time if the required
 *    tables are present; otherwise (c) time extrapolation of the relevant
 *    keyword values for the first and last records with valid values.
 *    If there are no valid values for the central meridian longitude
 *    and Carrington rotation a NaN is returned, signaling that not mapping
 *    and tracking could be performed (although strictly the Carrington
 *    rotation number would not really be required).
 */
double getlon_span (DRMS_RecordSet_t *ds, TIME tstrt, TIME tstop,
    double cadence, platloc platform, int geo_times, char *tobs_key,
    char *clon_key, char *crot_key, int sdo_fixclon, int *use_sdofixtbl,
    TIME *fixt, double *fixv, int fixct) {
  DRMS_Record_t *rec;
  TIME tobs, tfrst, tlast;
  double img_lat, cm_lon_strt, cm_lon_stop, cm_lon_frst, cm_lon_last;
  int cr_strt, cr_stop, cr_frst, cr_last;
  int nr, status;

  double lon_span = 0.0 / 0.0;
  int extrapolate_lonstrt = 0, extrapolate_lonstop = 0;
  int recct = ds->n;

  cm_lon_strt = cm_lon_stop = 0.0 / 0.0;
  cr_strt = cr_stop = -1;
  nr = 0;
  tfrst = tobs = drms_getkey_time (ds->records[nr], tobs_key, &status);
  if (fabs (tobs - tstrt) < cadence) {
    rec = ds->records[nr];
    cr_frst = drms_getkey_int (rec, crot_key, &status);
    cm_lon_frst = drms_getkey_double (rec, clon_key, &status);
    if (status || isnan (cm_lon_frst) || cr_frst < 0) {
		  /*  in unlikely case that obstime available, but not CMLon  */
      if (geo_times) status = getctrloc_from_time (tstrt, &img_lat,
	  &cm_lon_strt, &cr_strt, LOC_GEOC);
      else status = getctrloc_from_time (tstrt, &img_lat, &cm_lon_strt,
	  &cr_strt, platform);
      extrapolate_lonstrt = status;
    } else {
		  /*  in unlikely case that obstime available, but not CMLon  */
      cr_strt = cr_frst;
      cm_lon_strt = cm_lon_frst;
    }
    if (sdo_fixclon) {
      double cmlon_orig = cm_lon_strt;
      cm_lon_strt = adjust_sdo_clon (cmlon_orig, tobs, *use_sdofixtbl, fixt,
	fixv, fixct, &status);
      if (status) *use_sdofixtbl = 0;
      if (fabs (cm_lon_strt - cmlon_orig) > 180.0) cr_strt--;
    }
  } else {
				       /*  no valid obs time for first record */
    extrapolate_lonstrt = 1;
    for (nr = 1; nr < recct; nr++) {
      rec = ds->records[nr];
      tobs = drms_getkey_time (rec, tobs_key, &status);
      if (time_is_invalid (tobs)) continue;
      cm_lon_frst = drms_getkey_double (rec, clon_key, &status);
      if (status || isnan (cm_lon_frst)) continue;
      cr_frst = drms_getkey_int (rec, crot_key, &status);
      if (status || cr_frst < 0) continue;
      break;
    }
    if (nr == recct) return lon_span;
    tfrst = tobs;
    if (sdo_fixclon) {
      double cmlon_orig = cm_lon_frst;
      cm_lon_frst = adjust_sdo_clon (cmlon_orig, tfrst, *use_sdofixtbl, fixt,
	  fixv, fixct, &status);
      if (status) *use_sdofixtbl = 0;
      if (fabs (cm_lon_frst - cmlon_orig) > 180.0) cr_frst--;
    }
  }
  nr = recct - 1;
  tlast = tobs = drms_getkey_time (ds->records[nr], tobs_key, &status);
  if (fabs (tobs - tstop) < cadence) {
    rec = ds->records[nr];
    cr_last = drms_getkey_int (rec, crot_key, &status);
    cm_lon_last = drms_getkey_double (rec, clon_key, &status);
    if (status|| isnan (cm_lon_stop) || cr_stop < 0) {
		  /*  in unlikely case that obstime available, but not CMLon  */
      if (geo_times) status = getctrloc_from_time (tstop, &img_lat,
	  &cm_lon_stop, &cr_stop, LOC_GEOC);
      else status = getctrloc_from_time (tstop, &img_lat, &cm_lon_stop,
	  &cr_stop, platform);
      extrapolate_lonstop = status;
    } else {
      cr_stop = cr_last;
      cm_lon_stop = cm_lon_last;
      if (sdo_fixclon) {
	double cmlon_orig = cm_lon_stop;
	cm_lon_stop = adjust_sdo_clon (cmlon_orig, tobs, *use_sdofixtbl, fixt,
	    fixv, fixct, &status);
	if (status) *use_sdofixtbl = 0;
	if (fabs (cm_lon_stop - cmlon_orig) > 180.0) cr_stop--;
      }
    }
  } else {
				       /*  no valid obs time for last record */
    extrapolate_lonstop = 1;
    for (; nr; --nr) {
      rec = ds->records[nr];
      tobs = drms_getkey_time (rec, tobs_key, &status);
      if (time_is_invalid (tobs)) continue;
      cm_lon_last = drms_getkey_double (rec, clon_key, &status);
      if (status || isnan (cm_lon_last)) continue;
      cr_last = drms_getkey_int (rec, crot_key, &status);
      if (status || cr_last < 0) continue;
      break;
    }
    if (nr <= 0) return lon_span;
    tlast = tobs;
    if (sdo_fixclon) {
      double cmlon_orig = cm_lon_last;
      cm_lon_last = adjust_sdo_clon (cmlon_orig, tlast, *use_sdofixtbl, fixt,
	  fixv, fixct, &status);
      if (status) *use_sdofixtbl = 0;
      if (fabs (cm_lon_last - cmlon_orig) > 180.0) cr_last--;
    }
  }
		/*  If CM longitudes at start and stop of tracking interval
			could not be read from data or calculated from time,
			       extrapolate from known first and last values  */
  if (extrapolate_lonstrt || extrapolate_lonstop) {
    double dlon, rotrate;
    double tfrst_last = tlast - tfrst;
    if (sdo_fixclon) {
      double cmlon_orig = cm_lon_frst;
      cm_lon_frst = adjust_sdo_clon (cmlon_orig, tfrst, *use_sdofixtbl, fixt,
	  fixv, fixct, &status);
      if (status) *use_sdofixtbl = 0;
      if (fabs (cm_lon_frst - cmlon_orig) > 180.0) cr_frst--;
      cmlon_orig = cm_lon_last;
      cm_lon_last = adjust_sdo_clon (cmlon_orig, tlast, *use_sdofixtbl, fixt,
	  fixv, fixct, &status);
      if (status) *use_sdofixtbl = 0;
      if (fabs (cm_lon_last - cmlon_orig) > 180.0) cr_last--;
    }
    dlon = cm_lon_frst - cm_lon_last;
    dlon += 360 * (cr_last - cr_frst);
    if (dlon < 0.0) dlon += 360.0;
    rotrate = dlon / tfrst_last;
    if (extrapolate_lonstrt) {
      cm_lon_strt = cm_lon_frst + rotrate * (tfrst - tstrt);
      while (cm_lon_strt > 360.0) {
	cm_lon_strt -= 360.0;
	cr_strt--;
      }
    }
    if (extrapolate_lonstop) {
      cm_lon_stop = cm_lon_last - rotrate * (tstop - tlast);
      while (cm_lon_strt < 0.0) {
	cm_lon_stop += 360.0;
	cr_stop++;
      }
    }
  }
  lon_span = cm_lon_strt - cm_lon_stop;
  if (lon_span < 0.0) lon_span += 360.0;
  return lon_span;
}
/*
 *  Functions to support reading from a specified rejection list of the
 *    appropriate format
 */
int fgetline (FILE *in, char *line, int max) {
  if (fgets (line, max, in) == NULL) return 0;
  else return (strlen (line));
}

int read_reject_list (FILE *file, int **list) {
  int ds, sn, rec, last_rec;
  int allocd = 1024, ct = 0, gap = 0;
  char line[1024], t_str[64], estr[16];

  *list = (int *)malloc (allocd * sizeof (int));
  while (fgetline (file, line, 1024)) {
    if (strlen (line) == 1) continue;
    if (line[0] == '#') continue;
    if (sscanf (line, "%d %d %d %s", &ds, &sn, &rec, t_str) != 4) {
      if (sscanf (line, "%d %s", &rec, t_str) != 2) {
        sscanf (line, "%s", estr);
        if (strcmp (estr, "...")) continue;
        gap = 1;
        last_rec = rec;
        continue;
      }
    }
    if (gap) {
      while (rec > last_rec) {
	last_rec++;
	(*list)[ct++] = last_rec;
	if (ct >= allocd) {
	  allocd += 1024;
	  *list = (int *)realloc (*list, allocd * sizeof (int));
	}
      }
      gap = 0;
      continue;
    }
    (*list)[ct++] = rec;
    if (ct >= allocd) {
      allocd += 1024;
      *list = (int *)realloc (*list, allocd * sizeof (int));
    }
  }
  return ct;
}

static void free_all (float *clat, float *clon, float *mai, double *delta_rot,
    float *maps, float *last, double *maplat, double *maplon, double *mapsinlat,
    unsigned char *offsun, DRMS_Record_t **orecs, FILE **log, int *rejects) {
  free (clat);
  free (clon);
  free (mai);
  free (delta_rot);
  free (maps);
  free (last);
  free (maplat);
  free (maplon);
  free (mapsinlat);
  free (offsun);
  free (orecs);
  free (log);
  free (rejects);
}
/*
 *  Optimized version of sphere2img() for case when lat and pa do not
 *    change frequently
 */
static int fsphere2img (double sin_lat, double cos_lat, double lon,
	double latc, double lonc, double *x, double *y,
	double xcenter, double ycenter, double rsun,
	double cospa, double sinpa, double ecc, double chi, int ellipse,
	int xinvrt, int yinvrt) {
  static double sin_asd = 0.004660, cos_asd = 0.99998914;
                                                   /*  appropriate to 1 AU  */
  static double last_latc = 0.0, cos_latc = 1.0, sin_latc = 0.0;
  double r, cos_cang, xr, yr;
  double cos_lat_lon;
  double squash, cchi, schi, c2chi, s2chi, xp, yp;
  int hemisphere;

  if (latc != last_latc) {
      sin_latc = sin (latc);
      cos_latc = cos (latc);
      last_latc = latc;
  }
  cos_lat_lon = cos_lat * cos (lon - lonc);

  cos_cang   = sin_lat * sin_latc + cos_latc * cos_lat_lon;
  hemisphere = (cos_cang < 0.0) ? 1 : 0;
  r = rsun * cos_asd / (1.0 - cos_cang * sin_asd);
  xr = r * cos_lat * sin (lon - lonc);
  yr = r * (sin_lat * cos_latc - sin_latc * cos_lat_lon);
                                        /*  Change sign for inverted images  */
  if (xinvrt) xr *= -1.0;
  if (yinvrt) yr *= -1.0;
                          /*  Correction for ellipsoidal squashing of image  */
  if (ellipse) {
    squash = sqrt (1.0 - ecc * ecc);
    cchi = cos (chi);
    schi = sin (chi);
    s2chi = schi * schi;
    c2chi = 1.0 - s2chi;
    xp = xr * (s2chi + squash * c2chi) - yr * (1.0 - squash) * schi * cchi;
    yp = yr * (c2chi + squash * s2chi) - xr * (1.0 - squash) * schi * cchi;
    xr = xp;
    yr = yp;
  }
  *x = xr * cospa - yr * sinpa;
  *y = xr * sinpa + yr * cospa;

  *y += ycenter;
  *x += xcenter;
  return (hemisphere);
}
					  /*  nearest value "interpolation"  */
float nearest_val (float *f, int cols, int rows, double x, double y) {
  int col, row;
  if (x < -0.5 || x >= (cols - 0.5) || y < -0.5 || y >= (rows - 0.5))
    return missing_val;
  if (isnan (x) || isnan (y)) return missing_val;
  col = x + 0.5;
  row = y + 0.5;
  return f[col + row * cols];
}
						 /*  Bilinear interpolation  */
float linint2 (float *f, int cols, int rows, double x, double y) {
  double p, q, val;
  int col = (int)x, row = (int)y;
  int onerow = cols * row;
  int colp1 = col + 1, onerowp1 = onerow + cols;
  if (x < 0.0 || x > cols - 1.0 || y < 0.0 || y > rows - 1.0)
    return missing_val;
  if (isnan (x) || isnan (y)) return missing_val;
  p = x - col;
  q = y - row;
  val = (1 - p) * (1 - q) * f[col + onerow]
      + p * (1 - q) * f[colp1 + onerow]
      + (1 - p) * q * f[col + onerowp1]
      + p * q * f[colp1 + onerowp1];
  return val;
}
				/*  adapted from SOI libM/interpolate.c  */
/*
 *  Cubic convolution interpolation, based on GONG software, received Apr-88
 *  Interpolation by cubic convolution in 2 dimensions with 32-bit float data
 *
 *  Reference:
 *    R.G. Keys in IEEE Trans. Acoustics, Speech, and Signal Processing, 
 *    Vol. 29, 1981, pp. 1153-1160.
 *
 *  Usage:
 *    float ccint2 (float *f, int nx, int ny, double x, double y)
 *    double ccint2d (double *f, int nx, int ny, double x, double y)
 *
 *  Bugs:
 *    Currently interpolation within one pixel of edge is not
 *      implemented.  If x or y is in this range or off the image, the
 *      function value returned is NaN.
 */
				/*  Calculate the interpolation kernel.  */
static void ccker (double *u, double s) {
  double s2, s3;

  s2= s * s;
  s3= s2 * s;
  u[0] = s2 - 0.5 * (s3 + s);
  u[1] = 1.5*s3 - 2.5*s2 + 1.0;
  u[2] = -1.5*s3 + 2.0*s2 + 0.5*s;
  u[3] = 0.5 * (s3 - s2);
}
					/*  Cubic convolution interpolation  */
float ccint2 (float *f, int nx, int ny, double x, double y) {
  double  ux[4], uy[4];
  double sum;
  int ix, iy, ix1, iy1, i, j;

  if (x < 1.0 || x >= (float)(nx-2) || y < 1.0 || y >= (float)(ny-2))
    return missing_val;
  if (isnan (x) || isnan (y)) return missing_val;
  ix = (int)x;
  ccker (ux,  x - (double)ix);
  iy = (int)y;
  ccker (uy,  y - (double)iy);

  ix1 = ix - 1;
  iy1 = iy - 1;
  sum = 0.;
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      sum = sum + f[(iy1+i) * nx + ix1 + j] * uy[i] * ux[j];
  return (float)sum;
}
				/*  adapted from SOI functions/sds_interp.c  */
float array_imaginterp (DRMS_Array_t *img, double x, double y, int schema) {
/*
 *  Interpolate to an arbitrary grid location {x, y} from a DRMS Array
 *    containing a projected solar image.  The aim of this function is
 *    is to provide an ideal interpolation weighted by foreshortening,
 *    limb darkening, and vector projection, but for now this is simply
 *    a stub function that extracts information from the attributes of
 *    the dataset and calls a simple two-dimensional cubic convolutional
 *    interpolation function.
 *
 *  x and y are in the range [-1,-1] at the "lower left" of the first pixel
 *    to [1,1] at the "upper right" of the last pixel in the image.
 *  (These are converted to the ccint2 conventions, with x and y in
 *    the range [0,0] at the "center" of the first pixel to
 *    [cols-1, rows-1] at the "center" of the last pixel.)
 *  Interpolation near the edges is not allowed.
 *
 *  Bugs:
 *    Interpolation within one pixel of edge is not implemented.  If x or y
 *	is in this range or off the image, the function returns zero.
 *    The function assumes a fixed scale in both directions, so that if one
 *	dimension is larger than another the scale is applied to the larger.
 *    Only floating point data are supported by the function, and there is
 *	not even any testing for validity.
 */
  double xs, ys;
  int cols, rows, mdim;

  cols = img->axis[0];
  rows = img->axis[1];
  mdim = (cols > rows) ? cols : rows;
  xs = 0.5 * (x + 1.0) * mdim - 0.5;
  ys = 0.5 * (y + 1.0) * mdim - 0.5;
  if (schema == INTERP_NEAREST_NEIGHBOR)
    return nearest_val (img->data, cols, rows, xs, ys);
  else if (schema == INTERP_BILINEAR)
    return linint2 (img->data, cols, rows, xs, ys);
  else return ccint2 (img->data, cols, rows, xs, ys);
}

static void perform_mappings (DRMS_Array_t *img, float *maps, double *delta_rot,
    int mapct, double *maplat, double *maplon,
    double *map_coslat, double *map_sinlat, int pixct,
    double delta_time, unsigned char *offsun,
    double latc, double lonc, double xc, double yc,
    double a0, double a2, double a4, double merid_v, double radius, double pa,
    double ellipse_e, double ellipse_pa, int x_invrt, int y_invrt,
    int interpolator, int MDI_correct_distort) {
/*
 *  Perform the mappings from the target heliographic coordinate sets
 *    appropriate to each output cube into the image coordinates (as
 *    corrected) for spatial interpolation of the data values
 */
		/*  STUB function: checked so far: delta_time, delta_lat,
	latc, lonc, xc, yc, x_invrt, y_invrt, ellipse_e, delta_lon, radius, pa  */
  static double sin_asd = 0.004660, cos_asd = 0.99998914;
                                                   /*  appropriate to 1 AU  */
  double lat, lon, cos_lat, sin_lat;
  double xx, yy;
  float interpval;
  int m, n, mset;

  double delta_lat = merid_v * delta_time;
  int plate_cols = img->axis[0];
  int plate_rows = img->axis[1];
  double plate_width = (plate_cols > plate_rows) ? plate_cols : plate_rows;
  int no_merid_v = (merid_v == 0.0);
  double cos_pa = cos (pa);
  double sin_pa = sin (pa);
  int fit_ellipse = (ellipse_e > 0.0 && ellipse_e < 1.0);
double degrad = 180.0 / M_PI;

  mset = 0;
  xc *= 2.0 / plate_width;
  yc *= 2.0 / plate_width;
  radius *= 2.0 / plate_width;

  if (no_merid_v) {
    if (x_invrt || y_invrt || fit_ellipse) {
      for (m = 0; m < mapct; m++) {
	double delta_lon = delta_rot[m] * delta_time;
	for (n = 0; n < pixct; n++) {
	  if (offsun[n + mset]) {
	    maps[n + mset] = missing_val;
            continue;
	  }
       /*  Calculate heliographic coordinates corresponding to map location  */
	  sin_lat = map_sinlat[n + mset];
	  cos_lat = map_coslat[n + mset];
	  lon = maplon[n + mset] + delta_lon;
     /*  Calculate plate coordinates corresponding to heliocentric location  */
	  if (fsphere2img (sin_lat, cos_lat, lon, latc, lonc, &xx, &yy, xc, yc,
	      radius, cos_pa, sin_pa, ellipse_e, ellipse_pa, fit_ellipse,
	      x_invrt, y_invrt)) {
	    maps[n + mset] = missing_val;
	    continue;
	  }
	  if (plate_cols > plate_rows) yy -= 1.0 - plate_rows / plate_width;
	  if (plate_rows > plate_cols) xx -= 1.0 - plate_cols / plate_width;
				  /*  Correction for MDI distortion and tip  */
       /*  should be replaced by call to MDI_correct_plateloc when verified  */
	  if (MDI_correct_distort) {
	    mtrack_MDI_image_tip (&xx, &yy, 1, 1);
	    mtrack_MDI_image_stretch (&xx, &yy, 1, 1);
	  }
	  interpval = array_imaginterp (img, xx, yy, interpolator);
	  maps[n + mset] = (isnan (interpval)) ? missing_val : interpval;
	}
	mset += pixct;
      }
    } else {
      double r, cos_cang, xr, yr, cos_lat_lon;
      double cos_latc = cos (latc);
      double sin_latc = sin (latc);
      for (m = 0; m < mapct; m++) {
	double delta_lon = delta_rot[m] * delta_time;
	for (n = 0; n < pixct; n++) {
	  if (offsun[n + mset]) {
	    maps[n + mset] = missing_val;
	    continue;
	  }
       /*  Calculate heliographic coordinates corresponding to map location  */
	  sin_lat = map_sinlat[n + mset];
	  cos_lat = map_coslat[n + mset];
	  lon = maplon[n + mset] + delta_lon;
	  cos_lat_lon = cos_lat * cos (lon - lonc);
	  cos_cang  = sin_lat * sin_latc + cos_latc * cos_lat_lon;
	  if (cos_cang < 0.0) {
	    maps[n + mset] = missing_val;
	    continue;
	  }
	  r = radius * cos_asd / (1.0 - cos_cang * sin_asd);
	  xr = r * cos_lat * sin (lon - lonc);
	  yr = r * (sin_lat * cos_latc - sin_latc * cos_lat_lon);
	  xx = xr * cos_pa - yr * sin_pa;
	  yy = xr * sin_pa + yr * cos_pa;
	  yy += yc;
	  xx += xc;
		  /*  should take tests outside loop, just modify xc and yc  */
	  if (plate_cols > plate_rows) yy -= 1.0 - plate_rows / plate_width;
	  if (plate_rows > plate_cols) xx -= 1.0 - plate_cols / plate_width;
				  /*  Correction for MDI distortion and tip  */
       /*  should be replaced by call to MDI_correct_plateloc when verified  */
	  if (MDI_correct_distort) {
	    mtrack_MDI_image_tip (&xx, &yy, 1, 1);
	    mtrack_MDI_image_stretch (&xx, &yy, 1, 1);
	  }
	  interpval = array_imaginterp (img, xx, yy, interpolator);
	  maps[n + mset] = (isnan (interpval)) ? missing_val : interpval;
        }
	mset += pixct;
      }
    }
    return;
  }
/*
 *  only executed if there is a meridional component to the tracking rate
 */
  for (m = 0; m < mapct; m++) {
    double delta_lon = delta_rot[m] * delta_time;
    for (n = 0; n < pixct; n++) {
      if (offsun[n + mset]) {
        maps[n + mset] = missing_val;
        continue;
      }
       /*  Calculate heliographic coordinates corresponding to map location  */
      lat = maplat[n + mset] + delta_lat;
      lon = maplon[n + mset] + delta_lon;
      map_coslat[n + mset] = cos (lat);
      map_sinlat[n + mset] = sin (lat);
    /*  Calculate plate coordinates corresponding to heliocentric location  */
      if (sphere2img (lat, lon, latc, lonc, &xx, &yy, xc, yc, radius, pa,
          ellipse_e, ellipse_pa, x_invrt, y_invrt)) {
	maps[n + mset] = missing_val;
	continue;
      }
      if (plate_cols > plate_rows) yy -= 1.0 - plate_rows / plate_width;
      if (plate_rows > plate_cols) xx -= 1.0 - plate_cols / plate_width;
				  /*  Correction for MDI distortion and tip  */
      if (plate_cols > plate_rows) yy -= 1.0 - plate_rows / plate_width;
      if (plate_rows > plate_cols) xx -= 1.0 - plate_cols / plate_width;
				  /*  Correction for MDI distortion and tip  */
      if (MDI_correct_distort) {
	mtrack_MDI_image_tip (&xx, &yy, 1, 1);
	mtrack_MDI_image_stretch (&xx, &yy, 1, 1);
      }
      interpval = array_imaginterp (img, xx, yy, interpolator);
      maps[n + mset] = (isnan (interpval)) ? missing_val : interpval;
    }
    mset += pixct;
  }
}

void calc_limb_distance (double *delta_rot, int mapct, double *maplat,
    double *maplon, double delta_time, double merid_v,unsigned char *offsun,
    double latc, double lonc, double *mu, double *ctrx, double *ctry) {
/*
 *  Return instantaneous center-limb distances and position angles for selected
 *    target locations
 *  N.B. there is no finite distance correction
 */
  static double degrad = 180.0 / M_PI;
  double lat, lon, cos_lat, cos_lon, cos_lat_lon;
  double cos_cang;
  int m;

  double cos_latc = cos (latc);
  double sin_latc = sin (latc);
  double delta_lat = merid_v * delta_time;
  double missing_val = 0.0 / 0.0;

  for (m = 0; m < mapct; m++) {
    mu[m] = missing_val;
    double delta_lon = delta_rot[m] * delta_time;
    if (offsun[m]) continue;
    lat = maplat[m] + delta_lat;
    lon = maplon[m] + delta_lon;
    cos_lat = cos (lat);
    cos_lon = cos (lon - lonc);
    cos_lat_lon = cos_lat * cos_lon;
    cos_cang  = sin (lat) * sin_latc + cos_latc * cos_lat_lon;
    if (cos_cang < 0.0) continue;
    mu[m] = cos_cang;
    ctrx[m] = cos_lat * sin (lon - lonc);
    ctry[m] = sin (lat) * cos_latc - sin_latc * cos_lat * cos_lon;
  }
}

int ephemeris_params (DRMS_Record_t *img, double *vr, double *vw, double *vn) {
  int status, kstat = 0;

  *vr = drms_getkey_double (img, "OBS_VR", &status);
  kstat += status;
  *vw = drms_getkey_double (img, "OBS_VW", &status);
  kstat += status;
  *vn = drms_getkey_double (img, "OBS_VN", &status);
  kstat += status;
  if (kstat) *vr = *vw = *vn = 0.0;
  return kstat;
}
/*
 *  Adjust all values for various components of relative line-of-sight
 *    velocity at center of map (does not require image geometry, only
 *    target heliographic coordinates)
 */
void adjust_for_observer_velocity (float *map, int mapct, float *mapclat,
    float *mapclon, int pixct, double latc, double lonc, double orbvr,
    double orbvw, double orbvn, double semidiam, int radial_only) {
/*
 *  Adjust values for heliocentric observer motion
 */
  double vobs;
  double chi, clon, coslat, sig, x, y;
  double coslatc = cos (latc), sinlatc = sin (latc);
  int m, n, mset = 0;

  double tanang_r = tan (semidiam);
				       /*  No correction for foreshortening  */
  for (m = 0; m < mapct; m++) {
    coslat = cos (mapclat[m]);
    clon = mapclon[m] - lonc;
    x = coslat * sin (clon);
    y = sin (mapclat[m]) * coslatc - sinlatc * coslat * cos (clon);
    chi = atan2 (x, y);
    sig = atan (hypot (x, y) * tanang_r);
    vobs = orbvr * cos (sig);
    if (!radial_only) {
      vobs -= orbvw * sin (sig) * sin (chi);
      vobs -= orbvn * sin (sig) * cos (chi);
    }
    for (n = 0; n < pixct; n++) map[n + mset] -= vobs;
    mset += pixct;
  }
}
/*
 *  Adjust all values for line-of-sight components of standard solar rotational
 *    velocity at center of map (does not require image geometry, only the
 *    target heliographic coordinates)
 */
static void adjust_for_solar_rotation (float *map, int mapct, float *mapclat,
    float *mapclon, int pixct, double latc, double lonc) {
  static double a0 = -0.02893, a2 = -0.3441, a4 = -0.5037;
  static double RSunMm = 1.0e-6 * RSUNM;
  double vrot, uonlos;
  double chi, clon, coslat, sinlat, sin2lat, sinth, x, y;
  double coslatc = cos (latc), sinlatc = sin (latc);
  int m, n, mset = 0;

  for (m = 0; m < mapct; m++) {
    coslat = cos (mapclat[m]);
    sinlat = sin (mapclat[m]);
    sin2lat = sinlat * sinlat;
    clon = mapclon[m] - lonc;
    x = coslat * sin (clon);
    y = sin (mapclat[m]) * coslatc - sinlatc * coslat * cos (clon);
    chi = atan2 (x, y);
    sinth = hypot (x, y);
		     /*  Line-of-sight component of zonal surface component  */
    uonlos = (coslat > 1.e-8) ? sinth * coslatc * sin (chi) / coslat : 0.0;
    vrot = (a0 + CARR_RATE) *  RSunMm * coslat * uonlos;
    vrot += a2 *  RSunMm * coslat * uonlos * sin2lat;
    vrot += a4 *  RSunMm * coslat * uonlos * sin2lat * sin2lat;

    for (n = 0; n < pixct; n++) map[n + mset] -= vrot;
    mset += pixct;
  }
  return;
}

char *create_prime_key (DRMS_Record_t *rec) {
  int pct = sizeof (prime_keylist) / sizeof (char *);

  return create_primekey_from_keylist (rec, prime_keylist, pct);
}

int set_stat_keys (DRMS_Record_t *rec, long long ntot, long long valid,
    double vmn, double vmx, double sum, double sum2, double sum3, double sum4) {
  double scal, avg, var, skew, kurt;
  int kstat = 0;

  kstat += check_and_set_key_longlong (rec, "DATAVALS", valid);
  kstat += check_and_set_key_longlong (rec, "MISSVALS", ntot - valid);
  if (valid <= 0) return kstat;

  kstat += check_and_set_key_double (rec, "DATAMIN", vmn);
  kstat += check_and_set_key_double (rec, "DATAMAX", vmx);

  scal = 1.0 / valid;
  avg = scal * sum;
  var = scal * sum2 - avg * avg;
  skew = scal * sum3 - 3 * var * avg - avg * avg * avg;
  kurt = scal * sum4 - 4 * skew * avg - 6 * avg * avg * var -
      avg * avg * avg * avg;
  kstat += check_and_set_key_double (rec, "DATAMEAN", avg);
  if (var < 0.0) return kstat;
  kstat += check_and_set_key_double (rec, "DATARMS", sqrt (var));
  if (var == 0.0) return kstat;
  skew /= var * sqrt (var);
  kurt /= var * var;
  kurt -= 3.0;
  kstat += check_and_set_key_double (rec, "DATASKEW", skew);
  kstat += check_and_set_key_double (rec, "DATAKURT", kurt);

  return kstat;
}
/******************************************************************************/
			/*  module body begins here  */
/******************************************************************************/
int DoIt (void) {
  CmdParams_t *params = &cmdparams;
  DRMS_RecordSet_t *ids = NULL, *ods = NULL, *bckgds = NULL;
  DRMS_Record_t **orecs;
  DRMS_Record_t *irec, *orec;
  DRMS_Segment_t *record_segment, *oseg, *logseg;
  DRMS_Array_t *data_array = NULL, *map_array = NULL, *bckg_array;
  DRMS_Keyword_t *keywd;
  FILE **log;
  OBS_EPHEMERIS *obsval;
  TIME trec, tobs, tmid, tbase, tfrst, tlast, ttrgt;
  double *maplat, *maplon, *map_coslat, *map_sinlat = NULL;
  double *cmaplat, *cmaplon, *ctrmu, *ctrx, *ctry, *muavg, *xavg, *yavg, *latavg;
  double *delta_rot, *map_pa;
  double *minval, *maxval, *datamean, *datarms, *dataskew, *datakurt;
  double *cos_phi, *sin_phi;
  double min_scaled, max_scaled;
  double carr_lon, cm_lon_strt, cm_lon_stop, cm_lon_last, lon_span;
  double data_cadence, coverage;
  double lat, lon, img_lat, img_lon;
  double t_cl, tmid_cl, lon_cm;
  double x, y, x0, y0, xstp, ystp;
  double img_xc, img_yc, img_xscl, img_yscl, img_radius, img_pa;
  double ellipse_e, ellipse_pa;
  double kscale;
  float *data, *clat, *clon, *mai, *bck = NULL, *zero;
  long long *datavalid, *origvalid;
  long long *cvgolist, *cvnolist, *cvlist;
  long long nn, nntot, calver;
  unsigned int quality;
  int *mnlatct, *muct, *cvfound, *seglist;
  int axes[3], slice_start[3], slice_end[3];
  int recct, rgn, rgnct, segct, valid, cvct, cvgoct, cvnoct, cvused, cvmaxct;
  int t_cr, tmid_cr, cr_strt, cr_stop, tstrtstop;
  int col, row, pixct, dxy, i, found, n, nr, or;
  int blankvals, no_merid_v, rejects, status, verbose_logs, setmais;
  int x_invrt, y_invrt;
  int need_ephem, need_limb_dist, need_stats;
  int MDI_correct, MDI_correct_distort, SDO_fixephem, SDO_fixclon, SDO_fixcrot;
  int bscale_override, bzero_override, data_scaled;
  int badtkey, badqual, badfill, badtime, badcv, blacklist, qualcheck;
  int check_crlnfix, check_crotfix, need_crlnfix, need_crotfix;
  unsigned char *offsun, *ctroffsun;
  char *input, *source_series, *eoser, *segspec, *osegname, *pkeyval;
  char *parsed_segname;
  char logfilename[DRMS_MAXPATHLEN], ctimefmt[DRMS_MAXFORMATLEN];
  char rec_query[DRMS_MAXQUERYLEN];
  char module_ident[64], key[64], tbuf[64], ptbuf[64], ctime_str[16];

  TIME *crlnfixt = NULL;
  double *crlnfixv = NULL;
  double raddeg = M_PI / 180.0;
  double degrad = 1.0 / raddeg;
  float *maps = NULL, *last = NULL;
  platloc platform = LOC_UNKNOWN;
  int *reject_list = NULL;
  int crlnfixct = 0;
  int need_crcl = 1;
  int check_platform = 0;
  int use_sdofixtbl = 1;
  int segnum = 0;
  int extrapolate = 1;
  char *mapname[] = {"PlateCarree", "Cassini-Soldner", "Mercator",
      "LambertCylindrical", "Sanson-Flamsteed", "gnomonic", "Postel",
      "stereographic", "orthographic", "LambertAzimuthal"};
  char *interpname[] = {"Cubic Convolution", "Nearest Neighbor", "Bilinear"};
  char *wcscode[] = {"CAR", "CAS", "MER", "CEA", "GLS", "TAN", "ARC", "STG",
      "SIN", "ZEA"};

  missing_val = 0.0 / 0.0;
						 /*  process command params  */
  char *inspec = strdup (params_get_str (params, "in"));
  char *outser = strdup (params_get_str (params, "out"));
  char *sdofixser = strdup (params_get_str (params, "sdofix"));
  char *bckgn = strdup (params_get_str (params, "bckgn"));
  char *rejectfile = strdup (params_get_str (params, "reject"));
  char *seg_name = strdup (params_get_str (params, "segment"));
  unsigned int qmask = cmdparams_get_int64 (params, "qmask", &status);
  long long cvaccept = params_get_mask (params, "cvok", -1);
  long long cvreject = params_get_mask (params, "cvno", 0);
  char *calverkey = strdup (params_get_str (params, "cvkey"));
  int max_miss = params_get_int (params, "max_miss");
  char *tmid_str = strdup (params_get_str (params, "tmid"));
  char *tstrt_str = strdup (params_get_str (params, "tstart"));
  char *tstop_str = strdup (params_get_str (params, "tstop"));
  TIME tstrt = params_get_time (params, "tstart");
  TIME tstop = params_get_time (params, "tstop");
  int length = params_get_int (params, "length");
  double tstep = params_get_double (params, "tstep");
  int latct = params_get_int (params, "lat_nvals");
  int lonct = params_get_int (params, "lon_nvals");
  int maict = params_get_int (params, "mai_nvals");
  int pact = params_get_int (params, "map_pa_nvals");
  int proj = params_get_int (params, "map");
  int intrpopt = params_get_int (params, "interp");
  int fillopt = params_get_int (params, "fillopt");
  double map_scale = params_get_double (params, "scale");
  int map_cols = params_get_int (params, "cols");
  int map_rows = params_get_int (params, "rows");
  double a0 = params_get_double (params, "a0");
  double a2 = params_get_double (params, "a2");
  double a4 = params_get_double (params, "a4");
  double merid_v = params_get_double (params, "merid_v");
  double bscale = params_get_double (params, "bscale");
  double bzero = params_get_double (params, "bzero");
  char *identifier = strdup (params_get_str (params, "ident"));

  char *trec_key = strdup (params_get_str (params, "trec_key"));
  char *tobs_key = strdup (params_get_str (params, "tobs_key"));
  char *tstp_key = strdup (params_get_str (params, "tstp_key"));
  char *qual_key = strdup (params_get_str (params, "qual_key"));
  char *clon_key = strdup (params_get_str (params, "clon_key"));
  char *clat_key = strdup (params_get_str (params, "clat_key"));
  char *crot_key = strdup (params_get_str (params, "crot_key"));
  char *rsun_key = strdup (params_get_str (params, "rsun_key"));
  char *apsd_key = strdup (params_get_str (params, "apsd_key"));
  char *dsun_key = strdup (params_get_str (params, "dsun_key"));
  char *id_key = strdup (params_get_str (params, "id_key"));

  int carr_track = params_isflagset (params, "c");
  int no_track = params_isflagset (params, "n");
  int remove_obsvel = params_isflagset (params, "o");
  int remove_rotation = params_isflagset (params, "r");
  int verbose = params_isflagset (params, "v");
  int no_proc = params_isflagset (params, "w");
  int dispose = (no_proc | params_isflagset (params, "x")) ? DRMS_FREE_RECORD :
      DRMS_INSERT_RECORD;
  int geo_times = params_isflagset (params, "G");
  int MDI_proc = params_isflagset (params, "M");
  int SDO_fixkeys = (params_isflagset (params, "S")) ? 0 : 1;
  int ut_times = params_isflagset (params, "Z");
  int filt_on_calver = (cvreject || ~cvaccept);

  int obs2recfix = strcasecmp (trec_key, tobs_key);

  snprintf (module_ident, 64, "%s v %s", module_name, version_id);
  if (verbose) printf ("%s: JSOC version %s\n", module_ident, jsoc_version);
  verbose_logs = (dispose == DRMS_INSERT_RECORD) ? verbose : 0;
					/*  process and check arguments for
					 (un)acceptable calibration versions  */
  cvused = 0;
  cvmaxct = 64;
  cvfound = (int *)malloc (cvmaxct * sizeof (int));
  cvlist = (long long *)malloc (cvmaxct * sizeof (long long));
  if (filt_on_calver) {
    cvgoct = 1;
    cvgolist = (long long *)malloc (cvgoct * sizeof (long long));
    cvgolist[0] = cvaccept;
    cvnoct = 1;
    cvnolist = (long long *)malloc (cvnoct * sizeof (long long));
    cvnolist[0] = cvreject;
    if (~cvaccept) cvgoct = 0;
    if (!cvreject) cvnoct = 0;
  }
	   /*  get lists of latitudes and longitudes defining region centers  */
  rgnct = (latct > lonct) ? latct : lonct;
  if (rgnct > 300) {
    fprintf (stderr,
	"Error: requested number of regions (%d) exceeds cfitsio limit\n",
	rgnct);
    fprintf (stderr, "       of 300 open file pointers.\n");
    return 1;
  }
  setmais = (maict == rgnct);
  if (isnan (map_scale) || map_scale == 0.0) {
    fprintf (stderr,
	"Error: auto scaling from image resolution not implemented;\n");
    fprintf (stderr, "       scale parameter must be set.\n");
    return 1;
  }
							  /*  tracking rates  */
  if (no_track) {
    a0 = -(CARR_RATE);
    a2 = a4 = merid_v = 0.0;
    if (carr_track) {
      fprintf (stderr, "Error: inconsistent flags -c and -n\n");
      return 1;
    }
  }
  if (carr_track) a0 = a2 = a4 = merid_v = 0.0;
				       /*  input expected in microradian/sec  */
  a0 *= 1.0e-6;
  a2 *= 1.0e-6;
  a4 *= 1.0e-6;
  merid_v *= 1.0e-6;
  no_merid_v = (merid_v == 0.0);
  if (map_cols < 1) map_cols = map_rows;
  if (map_rows < 1) map_rows = map_cols;
  if (map_rows < 1) {
    fprintf (stderr, "Error: at least one of \"cols\" or \"rows\" must be set\n");
    return 1;
  }
  MDI_correct = MDI_correct_distort = MDI_proc;
  pixct = map_rows * map_cols;
  xstp = ystp = map_scale * raddeg;
  x0 = 0.5 * (1.0 - map_cols) * xstp;
  y0 = 0.5 * (1.0 - map_rows) * ystp;
		       /*  check validity and compatability of output series  */
  orec = drms_template_record (drms_env, outser, &status);
  if (status) {
    fprintf (stderr, "Error: no information about output series %s\n", outser);
    return 1;
  }
  segct = drms_record_numsegments (orec);
  found = 0;
  for (n = 0; n < segct; n++) {
    record_segment = drms_segment_lookupnum (orec, n);
    if (record_segment->info->naxis != 3) continue;
    if (record_segment->info->scope == DRMS_CONSTANT) continue;
    if (record_segment->info->scope == DRMS_VARIABLE) {
      if (record_segment->axis[0] != map_cols ||
	  record_segment->axis[1] != map_rows ||
	  record_segment->axis[2] != length) continue;
    }
    if (!found) osegname = strdup (record_segment->info->name);
    found++;
  }
  if (found < 1) {
    fprintf (stderr, "Error: no data segment of dimension 3 and ");
    fprintf (stderr, "appropriate size in output series\n  %s\n", outser);
    return 1;
  }
  record_segment = drms_segment_lookup (orec, osegname);
  if (found > 1) {
    fprintf (stderr, "Warning: multiple data segments of dimension 3 and ");
    fprintf (stderr, "appropriate size in output series\n  %s\n", outser);
    fprintf (stderr, "       using \"%s\"\n", osegname);
  }
	     /*  use output series default segment scaling if not overridden  */
  bscale_override = 1;
  if (isnan (bscale) || bscale == 0.0) {
    bscale = record_segment->bscale;
    bscale_override = 0;
  }
  bzero_override = 1;
  if (isnan (bzero)) {
    bzero = record_segment->bzero;
    bzero_override = 0;
  }
					   /*  check for segment named "Log"  */
  if (verbose_logs) {
    logseg = drms_segment_lookup (orec, "Log");
    if (!logseg) {
      fprintf (stderr,
	  "Warning: segment \"Log\" not present in output series %s\n", outser);
      fprintf (stderr, "         verbose logging turned off\n");
      verbose_logs = 0;
    }
  }
  if ((keywd = drms_keyword_lookup (orec, "CMLon", 1)))
    sprintf (ctimefmt, "%%d:%s", keywd->info->format);
  else sprintf (ctimefmt, "%%d:%%07.3f");
					   /*  scaling check initializations  */
  need_stats = (drms_keyword_lookup (orec, "DATAMIN", 1) ||
      drms_keyword_lookup (orec, "DATAMAX", 1) ||
      drms_keyword_lookup (orec, "DATAMEAN", 1) ||
      drms_keyword_lookup (orec, "DATARMS", 1) ||
      drms_keyword_lookup (orec, "DATASKEW", 1) ||
      drms_keyword_lookup (orec, "DATAKURT", 1) ||
      drms_keyword_lookup (orec, "DATAVALS", 1) ||
      drms_keyword_lookup (orec, "MISSVALS", 1));
  data_scaled = ((record_segment->info->type == DRMS_TYPE_CHAR) ||
      (record_segment->info->type == DRMS_TYPE_SHORT) ||
      (record_segment->info->type == DRMS_TYPE_INT) ||
      (record_segment->info->type == DRMS_TYPE_LONGLONG));
  need_stats |= data_scaled;
  if (data_scaled) {
    max_scaled = (record_segment->info->type == DRMS_TYPE_CHAR) ?
	SCHAR_MAX : (record_segment->info->type == DRMS_TYPE_SHORT) ?
	SHRT_MAX : (record_segment->info->type == DRMS_TYPE_INT) ?
	INT_MAX : LLONG_MAX;
    min_scaled = - max_scaled;
    max_scaled *= bscale;
    min_scaled *= bscale;
    max_scaled += bzero;
    min_scaled += bzero;
  }
					/*  check if limb distance is needed  */
  need_limb_dist = (drms_keyword_lookup (orec, "MeanMu", 1) ||
      drms_keyword_lookup (orec, "MeanPA", 1));

  if (!(ods = drms_create_records (drms_env, rgnct, outser, DRMS_PERMANENT,
      &status))) {
    fprintf (stderr, "Error: unable to create %d records in series %s\n",
	rgnct, outser);
    fprintf (stderr, "       drms_create_records() returned status %d\n", status); 
    return 1;
  }
  if (verbose) printf ("creating %d record(s) in series %s\n", rgnct, outser);
  if (verbose && dispose == DRMS_FREE_RECORD)
      printf ("experimental run, output records will not be saved\n");
  orec = drms_recordset_getrec (ods, 0);
  if (!orec) {
    fprintf (stderr, "Error accessing record %d in series %s\n", 0, outser);
    drms_close_records (ods, DRMS_FREE_RECORD);
    return 1;
  }
						    /*  replacement function  */
  ids = track_record_selection (inspec, tstrt_str, tstop_str, tmid_str,
      length, tstp_key, trec_key, &tstep, &data_cadence, geo_times, seg_name,
      &parsed_segname, &input, &tstrtstop, &tmid_cr, &tmid_cl, &need_crcl,
      &platform, verbose);
  if (!ids) {
    fprintf (stderr, "Error (%s): no input data set\n", module_ident);
    return 1;
  }
  seg_name = parsed_segname;
  if ((recct = ids->n) < 2) {
    fprintf (stderr, "Error: (%s) <2 records in selected input set\n",
	module_ident);
    fprintf (stderr, "       %s\n", input);
    drms_close_records (ids, DRMS_FREE_RECORD);
    return 1;
  }
  irec = ids->records[0];
  source_series = strdup (irec->seriesinfo->seriesname);
  if (platform != LOC_SDO) SDO_fixkeys = 0;
  SDO_fixclon = SDO_fixcrot = SDO_fixkeys;
  check_crlnfix = need_crlnfix = check_crotfix = need_crotfix = SDO_fixkeys;
  if (filt_on_calver || SDO_fixkeys) {
    keywd = drms_keyword_lookup (irec, calverkey, 1);
    if (keywd) {
      check_crlnfix = check_cvbits (ids, calverkey, OBS_CRLN_FIXED,
	  &need_crlnfix);
      check_crotfix = check_cvbits (ids, calverkey, OBS_CROT_FIXED,
	  &need_crotfix);
    } else {
      fprintf (stderr, "Warning: required keyword %s not found\n", calverkey);
      fprintf (stderr, "         no calibration version filtering applied\n");
      filt_on_calver = 0;
      check_crlnfix = need_crlnfix = check_crotfix = need_crotfix = 0;
      if (SDO_fixkeys)
	fprintf (stderr, "         %s and %s values assumed valid\n", clon_key,
	    crot_key);
    }
  }
					     /*  check for required keywords  */
  status = verify_keys (irec, clon_key, clat_key, &kscale);
  if (status) {
    drms_close_records (ids, DRMS_FREE_RECORD);
    return 1;
  }
  if (tstrtstop) {
    tstrt = params_get_time (params, "tstart");
    tlast = tstop = params_get_time (params, "tstop");
  } else {
    tstrt = drms_getkey_time (irec, trec_key, &status);
    tlast = tstop = drms_getkey_time (ids->records[recct - 1], trec_key, &status);
  }
  tmid = 0.5 * (tstrt + tstop);
  if (length <= 0)
    length = (tstop - tstrt + 1.01 * data_cadence) / data_cadence;
  seglist = ndimsegments (irec, 2, &segct);
  if (verbose) {
    sprint_time (ptbuf, tstrt, "TAI", 0);
    sprint_time (tbuf, tstop, "TAI", 0);
    printf ("tracking data from %s - %s at cadence of %.1f s\n", ptbuf, tbuf,
	tstep);
  }
		   /*  adjust ephemeris values for mapping from tobs to trec  */
  if (obs2recfix) {
    obsval = (OBS_EPHEMERIS *)malloc (recct * sizeof (OBS_EPHEMERIS));
    adjust_obs2rec (obsval, ids, recct, trec_key, tobs_key, crot_key, clon_key,
        clat_key, dsun_key, apsd_key, tstrt, tstop, data_cadence, source_series);
					/*  check ephemeris correction table  */
  }
		/*  To prefetch SUMS records as needed, without blocking
			(individual segment reads should take care of that)
		call  drms_stage_records (ids, 1, 0) here; however, this was
	    removed in v 1.1, as it seemed to be causing crashes at the time  */
    /*  Get Carrington times for first, midtime (if needed), and last images
					      from input records if possible  */
  if (need_crlnfix) {
    if (strcasecmp (sdofixser, "Not Specified")) {
      DRMS_Record_t *rec = drms_template_record (drms_env, sdofixser, &status);
      if (status || !rec) {
	fprintf (stderr,
	  "Warning: can\'t  open ephemeris correction series %s\n", sdofixser);
	fprintf (stderr,
	  "         Carrington longitudes will be adjusted by constant\n");
	use_sdofixtbl = 0;
      } else {
	DRMS_RecordSet_t *ds = NULL;
	sprint_time (tbuf, tstrt - data_cadence, "TAI", 0);
	sprintf (rec_query, "%s[%s]\n", sdofixser, tbuf);
	if (ds = drms_open_records (drms_env, rec_query, &status)) {
          if (ds->n < 1) {
	    fprintf (stderr, "Warning: series %s does not include %s\n",
	        sdofixser, tbuf);
	    fprintf (stderr,
	        "         Carrington longitudes will be adjusted by constant\n");
	    use_sdofixtbl = 0;
	    drms_close_records (ds, DRMS_FREE_RECORD);
	  } else {
	    sprint_time (tbuf, tstop + data_cadence, "TAI", 0);
	    sprintf (rec_query, "%s[%s]\n", sdofixser, tbuf);
	    ds = drms_open_records (drms_env, rec_query, &status);
	    if (ds->n < 1) {
	      fprintf (stderr, "Warning: series %s does not include %s\n",
		  sdofixser, tbuf);
	      fprintf (stderr,
	        "         Carrington longitudes will be adjusted by constant\n");
	      use_sdofixtbl = 0;
	    }
	    drms_close_records (ds, DRMS_FREE_RECORD);
	  }
        } else {
	  fprintf (stderr, "Error: unable to open selected data set \"%s\"\n",
	      rec_query);
          fprintf (stderr, "       status = %d\n", status);
	  fprintf (stderr,
	      "         Carrington longitudes will be adjusted by constant\n");
	  use_sdofixtbl = 0;
	}
      }
    } else use_sdofixtbl = 0;
  }
  if (use_sdofixtbl) {
    DRMS_Array_t *crlnfixvec = ephemeris_vector (sdofixser, "T_REC",
	"CRLN_OBS", tstrt - data_cadence, tstop + data_cadence);
    if (crlnfixvec) {
      int vct = crlnfixvec->axis[0];
      crlnfixct = crlnfixvec->axis[1];
      if (vct != 2 || crlnfixct < 2) {
        use_sdofixtbl = 0;
        fprintf (stderr,
	    "Warning: unable to create interpolation grid from %s\n",
	    sdofixser);
        fprintf (stderr,
	  "         CRLN_OBS will be adjusted by constant as needed\n");
      }
      crlnfixt = (TIME *)malloc (crlnfixct * sizeof (TIME));
      crlnfixv = (double *)malloc (crlnfixct * sizeof (double));
      for (n = 0; n < crlnfixct; n++)
        crlnfixt[n] = ((TIME *)crlnfixvec->data)[n];
      for (int m = 0; m < crlnfixct; m++, n++)
        crlnfixv[m] = ((double *)crlnfixvec->data)[n];
      drms_free_array (crlnfixvec);
    } else {
      DRMS_Array_t *crlnfixvec = ephemeris_vector (sdofixser, "T_REC",
	  "CRLN", tstrt - data_cadence, tstop + data_cadence);
      if (crlnfixvec) {
	int vct = crlnfixvec->axis[0];
	crlnfixct = crlnfixvec->axis[1];
	if (vct != 2 || crlnfixct < 2) {
          use_sdofixtbl = 0;
          fprintf (stderr,
	      "Warning: unable to create interpolation grid from %s\n",
	      sdofixser);
          fprintf (stderr,
	      "         CRLN_OBS will be adjusted by constant as needed\n");
	}
	crlnfixt = (TIME *)malloc (crlnfixct * sizeof (TIME));
	crlnfixv = (double *)malloc (crlnfixct * sizeof (double));
	for (n = 0; n < crlnfixct; n++)
        crlnfixt[n] = ((TIME *)crlnfixvec->data)[n];
	for (int m = 0; m < crlnfixct; m++, n++)
        crlnfixv[m] = ((double *)crlnfixvec->data)[n];
	drms_free_array (crlnfixvec);
      } else {
	use_sdofixtbl = 0;
	fprintf (stderr,
	    "Warning: unable to create interpolation grid from %s\n",
	    sdofixser);
	fprintf (stderr,
	    "         CRLN_OBS will be adjusted by constant as needed\n");
      }
    }
  }
  lon_span = getlon_span (ids, tstrt, tstop, data_cadence, platform, geo_times,
      tobs_key, clon_key, crot_key, SDO_fixclon, &use_sdofixtbl, crlnfixt,
      crlnfixv, crlnfixct);
  if (isnan (lon_span)) {
    drms_close_records (ids, DRMS_FREE_RECORD);
    return 1;
  }
  if (need_crcl) {
	     /*  midtime target not specified in CT, need CR and CL for keys  */
    int interpcrcl = 0;
    if (recct % 2) {
      nr = recct / 2;
      irec = ids->records[nr];
      tmid_cr = drms_getkey_int (irec, crot_key, &status);
      tmid_cl = drms_getkey_double (irec, clon_key, &status);
      if (status || isnan (tmid_cl) || tmid_cr < 0) {
	if (geo_times)
	  status = getctrloc_from_time (tmid, &img_lat, &tmid_cl, &tmid_cr, LOC_GEOC);
	else
	  status = getctrloc_from_time (tmid, &img_lat, &tmid_cl, &tmid_cr, platform);
	if (status) interpcrcl = 1;
      } else {
	if (SDO_fixclon) {
	  double cmlon_orig = tmid_cl;
	  tmid_cl = adjust_sdo_clon (cmlon_orig, tmid, use_sdofixtbl, crlnfixt,
	      crlnfixv, crlnfixct, &status);
	  if (status) use_sdofixtbl = 0;
	  if (tmid_cl - cmlon_orig < 180.0) tmid_cr++;
	  if (tmid_cl - cmlon_orig < -180.0) tmid_cr--;
	}
      }
    } else {
      if (geo_times)
	status = getctrloc_from_time (tmid, &img_lat, &tmid_cl, &tmid_cr, LOC_GEOC);
      else
	status = getctrloc_from_time (tmid, &img_lat, &tmid_cl, &tmid_cr, platform);
      if (status) interpcrcl = 1;
    }
    if (interpcrcl) {
				/*  no ephemeris for midtime record and can't 
					get ephemeris from time; interpolate  */
      TIME t0, t1;
      double cl0, cl1;
      int cr0, cr1;
      nr = recct / 2 + 1;
      for (; nr < recct; nr++) {
	irec = ids->records[nr];
	t1 = drms_getkey_time (irec, tobs_key, &status);
			/*  if correction table for hmi lon and lat available,
							 this should suffice  */
	cr1 = drms_getkey_int (irec, crot_key, &status);
	cl1 = drms_getkey_double (irec, clon_key, &status);
        if (status || isnan (cl1) || cr1 < 0) continue;
	if (SDO_fixclon) {
	  double cmlon_orig = cl1;
	  cl1 = adjust_sdo_clon (cmlon_orig, t1, use_sdofixtbl, crlnfixt,
	      crlnfixv, crlnfixct, &status);
	  if (status) use_sdofixtbl = 0;
	  if ((cl1 - cmlon_orig) > 180.0) cr1--;
	  if ((cl1 - cmlon_orig) < -180.0) cr1++;
	}
	break;
      }
      nr = recct / 2 - 1;
      for (; nr; nr--) {
	irec = ids->records[nr];
	t0 = drms_getkey_time (irec, tobs_key, &status);
								/*  as above  */
	cr0 = drms_getkey_int (irec, crot_key, &status);
	cl0 = drms_getkey_double (irec, clon_key, &status);
        if (status || isnan (cl0) || cr0 < 0) continue;
	if (SDO_fixclon) {
	  double cmlon_orig = cl0;
	  cl0 = adjust_sdo_clon (cmlon_orig, t0, use_sdofixtbl, crlnfixt,
	      crlnfixv, crlnfixct, &status);
	  if (status) use_sdofixtbl = 0;
	  if ((cl0 - cmlon_orig) > 180.0) cr0--;
	  if ((cl0 - cmlon_orig) < -180.0) cr0++;
	}
	break;
      }
      if (cr0 < cr1) cl0 += 360;
      tmid_cr = cr0;
      tmid_cl = cl1 + (cl0 - cl1) * (tmid - t0) / (t1 - t0);
      if (tmid_cl > 360.0) {
        tmid_cl -= 360.0;
	tmid_cr++;
      }
    }
  }
  axes[0] = map_cols;
  axes[1] = map_rows;
  axes[2] = 1;
  slice_start[0] = slice_start[1] = 0;
  slice_end[0] = axes[0] - 1;
  slice_end[1] = axes[1] - 1;
  nntot = (long long)recct * pixct;
					  /*  allocate map parameter arrays  */
  clat = (float *)malloc (rgnct * sizeof (float));
  clon = (float *)malloc (rgnct * sizeof (float));
  mai = (float *)malloc (rgnct * sizeof (float));
  map_pa = (double *)malloc (rgnct * sizeof (double));
  cos_phi = (double *)malloc (rgnct * sizeof (double));
  sin_phi = (double *)malloc (rgnct * sizeof (double));
  delta_rot = (double *)malloc (rgnct * sizeof (double));
  for (i = 0; i < latct; i++) {
    snprintf (key, 64, "lat_%d_value", i);
    clat[i] = params_get_float (params, key) * raddeg;
  }
  for (i = 0; i < lonct; i++) {
    snprintf (key, 64, "lon_%d_value", i);
    clon[i] = params_get_float (params, key) * raddeg;
  }
  for (i = latct; i < rgnct; i++) clat[i] = clat[i-1];
  for (i = lonct; i < rgnct; i++) clon[i] = clon[i-1];
  for (i = 0; i < pact; i++) {
    snprintf (key, 64, "map_pa_%d_value", i);
    map_pa[i] = params_get_double (params, key) * raddeg;
    cos_phi[i] = cos (map_pa[i]);
    sin_phi[i] = sin (map_pa[i]);
  }
  for (; i < rgnct; i++) {
    map_pa[i] = map_pa[i-1];
    cos_phi[i] = cos_phi[i-1];
    sin_phi[i] = sin_phi[i-1];
  }
  if (setmais) {
    for (i = 0; i < rgnct; i++) {
      snprintf (key, 64, "mai_%d_value", i);
      mai[i] = params_get_float (params, key);
    }
  }
  for (rgn = 0; rgn < rgnct; rgn++) {
    double sin_lat = sin (clat[rgn]);
    sin_lat *= sin_lat;
    delta_rot[rgn] =  a0 + sin_lat * (a2 + a4 * sin_lat);
  }
  log = (FILE **)malloc (rgnct * sizeof (FILE *));
	/*  create output memory structures: data arrays and file pointers,
					     one per region (output record)  */
  orecs = (DRMS_Record_t **)malloc (rgnct * sizeof (DRMS_Record_t *));
  if (need_stats) {
    minval = (double *)malloc (rgnct * sizeof (double));
    maxval = (double *)malloc (rgnct * sizeof (double));
    datamean = (double *)calloc (rgnct, sizeof (double));
    datarms = (double *)calloc (rgnct, sizeof (double));
    dataskew = (double *)calloc (rgnct, sizeof (double));
    datakurt = (double *)calloc (rgnct, sizeof (double));
    datavalid = (long long *)calloc (rgnct, sizeof (long long));
    origvalid = (long long *)calloc (rgnct, sizeof (long long));
  }
  maps = (float *)malloc (pixct * rgnct * sizeof (float));
  last = (float *)malloc (pixct * rgnct * sizeof (float));
		      /*  allocate mapping info arrays for all output cubes  */
  maplat = (double *)malloc (pixct * rgnct * sizeof (double));
  maplon = (double *)malloc (pixct * rgnct * sizeof (double));
  if (no_merid_v) {
    map_coslat = maplat;
    map_sinlat = (double *)malloc (pixct * rgnct * sizeof (double));
  } else {
    map_coslat = (double *)malloc (pixct * rgnct * sizeof (double));;
    map_sinlat = (double *)malloc (pixct * rgnct * sizeof (double));
  }
  offsun = (unsigned char *)malloc (pixct * rgnct * sizeof (char));
  latavg = (double *)calloc (rgnct, sizeof (double));
  mnlatct = (int *)calloc (rgnct, sizeof (int));
  if (need_limb_dist) {
    cmaplat = (double *)malloc (rgnct * sizeof (double));
    cmaplon = (double *)malloc (rgnct * sizeof (double));
    ctrmu = (double *)malloc (rgnct * sizeof (double));
    ctrx = (double *)malloc (rgnct * sizeof (double));
    ctry = (double *)malloc (rgnct * sizeof (double));
    muavg = (double *)calloc (rgnct, sizeof (double));
    muct = (int *)calloc (rgnct, sizeof (int));
    xavg = (double *)calloc (rgnct, sizeof (double));
    yavg = (double *)calloc (rgnct, sizeof (double));
    ctroffsun = (unsigned char *)malloc (rgnct * sizeof (char));
  }
    /*  Calculate heliographic coordinates corresponding to map location(s)  */
  n = 0;
  for (rgn = 0; rgn < rgnct; rgn++) {
    double xrot, yrot;
    for (row=0, y=y0; row < map_rows; row++, y +=ystp) {
      for (col=0, x=x0; col < map_cols; col++, x +=xstp, n++) {
	xrot = x * cos_phi[rgn] - y * sin_phi[rgn];
	yrot = y * cos_phi[rgn] + x * sin_phi[rgn];
	offsun[n] = plane2sphere (xrot, yrot, clat[rgn], clon[rgn], &lat, &lon,
	    proj);
	maplat[n] = lat;
	maplon[n] = lon;
	if (no_merid_v) {
	  map_coslat[n] = cos (lat);
	  map_sinlat[n] = sin (lat);
	}
	if (!offsun[n]) {
	  mnlatct[rgn]++;
	  latavg[rgn] += lat;
	}
      }
    }
    if (mnlatct[rgn]) latavg[rgn] /= mnlatct[rgn];
    if (need_stats) {
      minval[rgn] = 1./ 0.;
      maxval[rgn] = -minval[rgn];
      datavalid[rgn] = nntot;
      origvalid[rgn] = nntot;
    }
    if (need_limb_dist) {
      ctroffsun[rgn] = plane2sphere (0.0, 0.0, clat[rgn], clon[rgn], &lat, &lon,
	  proj);
      cmaplat[rgn] = lat;
      cmaplon[rgn] = lon;
    }
  }
				    /*  this should not really be necessary  */
  zero = (float *)calloc (pixct, sizeof (float));
  map_array = drms_array_create (DRMS_TYPE_FLOAT, 3, axes, (void *)zero, &status);
  map_array->bscale = bscale;
  map_array->bzero = bzero;

  dxy = 0;
  if (strcmp (bckgn, "Not Specified")) {
                                       /*  read in average background image  */
    if (bckgds = drms_open_records (drms_env, bckgn, &status)) {
      if (bckgds->n == 1) {
        DRMS_Record_t *bckrec = bckgds->records[0];
	segct = drms_record_numsegments (bckrec);
	if (segct) {
	  for (segnum = 0; segnum < segct; segnum++) {
	    DRMS_Segment_t *recseg = drms_segment_lookupnum (bckrec, segnum);
	    if (recseg) {
	      bckg_array = drms_segment_read (recseg, DRMS_TYPE_FLOAT, &status);
	      if (bckg_array) {
		if (bckg_array->naxis) dxy = 1;
		for (n = 0; n < bckg_array->naxis; n++)
		  dxy *= bckg_array->axis[n];
		bck = (float *)bckg_array->data;
		if (segct > 1)
		  fprintf (stderr, "using background segment %d (%s)\n",
		      segnum, recseg->info->name);
		break;
	      } else continue;
	    }
	  }
	  if (segnum >= segct) {
	    fprintf (stderr,
	        "Error: unable to open background record segment \"%s\"\n", bckgn);
	    drms_close_records (bckgds, DRMS_FREE_RECORD);
	    drms_close_records (ids, DRMS_FREE_RECORD);
	    drms_close_records (ods, DRMS_FREE_RECORD);
	    return 1;
	  }
	} else {
	  fprintf (stderr, "Warning: background data record %s\n", bckgn);
	  fprintf (stderr, "       contains %d segments; ignored\n", segct);
	}
      } else {
	fprintf (stderr, "Warning: background data set %s\n", bckgn);
	fprintf (stderr, "       contains %d records; ignored\n", bckgds->n);
      }
      drms_close_records (bckgds, DRMS_FREE_RECORD);
    } else {
      fprintf (stderr, "Warning: unable to open background data set \"%s\"\n",
	  bckgn);
      fprintf (stderr, "       drms_open_records() returned %d; ignored\n",
	  status);
    }
  }
		  /*  support special hack of reading of rejection list file  */
  rejects = 0;
  if (strcmp (rejectfile, "Not Specified")) {
    FILE *rejectfp = fopen (rejectfile, "r");
    if (rejectfp) rejects = read_reject_list (rejectfp, &reject_list);
    else fprintf (stderr,
	"Warning: could not open rejection list %s; ignored\n", rejectfile);
  }

  found = 0;
  for (rgn = 0; rgn < rgnct; rgn++) {
    orec = orecs[rgn] = drms_recordset_getrec (ods, rgn);
    if (!orec) {
      fprintf (stderr, "Error accessing record %d in series %s\n", rgn, outser);
      drms_close_records (ids, DRMS_FREE_RECORD);
      drms_close_records (ods, DRMS_FREE_RECORD);
      free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
	  map_sinlat, offsun, orecs, log, reject_list);
      if (bck) free (bck);
      return 1;
    }
    if (verbose_logs) {
      logseg = drms_segment_lookup (orec, "Log");
      if (logseg) drms_segment_filename (logseg, logfilename);
      log[rgn] = fopen (logfilename, "w");
      if (!log[rgn]) {
        fprintf (stderr, "Error: unable to open log file \"%s\"\n", logfilename);
	drms_close_records (ids, DRMS_FREE_RECORD);
	drms_close_records (ods, DRMS_FREE_RECORD);
	free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
	    map_sinlat, offsun, orecs, log, reject_list);
	if (bck) free (bck);
	return 1;
      }
    }
		      /*  set segment axis lengths and scaling if necessary  */
    oseg = drms_segment_lookup (orec, osegname);
    if (oseg->info->scope == DRMS_VARDIM) {
      oseg->axis[0] = map_cols;
      oseg->axis[1] = map_rows;
      oseg->axis[2] = length;
    }
    if (bscale_override) oseg->bscale = bscale;
    if (bzero_override) oseg->bzero = bzero;
  }
					     /*  loop through input records  */
  valid = 0;
  ttrgt = tstrt;
  or = 0;
  badtkey = badqual = badfill = badtime = badcv = blacklist = 0;
  if (verbose) fflush (stdout);

  for (nr = 0; nr < recct; nr++) {
    irec = ids->records[nr];
    trec = drms_getkey_time (irec, trec_key, &status);
    if (ut_times) sprint_time (tbuf, trec, "UTC", 0);
    else sprint_time (tbuf, trec, "TAI", 0);
    tbuf[strlen (tbuf) - 4] = '\0';
    tobs = drms_getkey_time (irec, tobs_key, &status);
    quality = drms_getkey_int (irec, qual_key, &status);
    if (status) qualcheck = 0;
    else if (quality & qmask) {
					      /*  bad or missing image, skip  */
      badqual++;
      continue;
    } else qualcheck = 1;
    blankvals = drms_getkey_int (irec, "MISSVALS", &status);
    if ((max_miss >= 0) && (blankvals > max_miss) && !status) {
						     /*  partial image, skip  */
      badfill++;
      continue;
    }
    if (tobs == tlast) {
					  /*  same time as last record, skip  */
      badtime++;
      continue;
    }
			  /*  check for record quality, reject as applicable  */
    if (rejects) {
      int idrec = drms_getkey_int (irec, "T_REC_index", &status);
      int match = 0;
      if (status) {
	fprintf (stderr, "Warning: \"T_REC_index\" keyword not found\n");
	fprintf (stderr, "         up to %d bad images could be processed\n",
	  rejects);
	rejects = 0;
      }
      for (n = 0; n < rejects; n++) {
	if (idrec == reject_list[n]) {
	  match = 1;
	  break;
	}
      }
      if (match) {
        blacklist++;
        continue;
      }
    }
	      /*  check for record calibration version; reject as applicable  */
    calver = drms_getkey_longlong (irec, calverkey, &status);
    if (filt_on_calver) {
      if (status) {
		       /*  this probably never happens if the keyword exists  */
	badcv++;
	continue;
      }
      for (cvct = 0; cvct < cvgoct; cvct++)
	if (calver == cvgolist[cvct]) break;
      if (cvct >= cvgoct) {
	badcv++;
	continue;
      }
      for (cvct = 0; cvct < cvnoct; cvct++) {
 	if (calver == cvnolist[cvct]) {
	  badcv++;
	  continue;
	}
      }
      for (cvct = 0; cvct < cvnoct; cvct++) {
 	if (calver == cvnolist[cvct]) {
	  badcv++;
	  continue;
	}
      }
    }
    if (time_is_invalid (tobs)) {
						      /*  no time info, skip  */
      badtkey++;
      continue;
    }
    for (cvct = 0; cvct < cvused; cvct++) {
      if (calver == cvlist[cvct]) {
        cvfound[cvct]++;
        break;
      }
    }
    if (cvct == cvused) {
      if (cvused < cvmaxct) {
        cvlist[cvct] = calver;
	cvfound[cvct] = 1;
	cvused++;
      }
    }
    if (check_crlnfix) need_crlnfix = (calver & OBS_CRLN_FIXED) ? 0 : 1;
			    /*  get needed info from record keys for mapping  */
    if (obs2recfix) {
      cm_lon_last = img_lon = raddeg * obsval[nr].clon;
      img_lat = raddeg * obsval[nr].clat;;
    } else {
      cm_lon_last = img_lon =
	  raddeg * drms_getkey_double (irec, clon_key, &status);
      img_lat = raddeg * drms_getkey_double (irec, clat_key, &status);
    }
    if (SDO_fixclon && need_crlnfix) {
      double cmlon_orig = drms_getkey_double (irec, clon_key, &status);
      cm_lon_last = img_lon = raddeg * adjust_sdo_clon (cmlon_orig, trec,
	  use_sdofixtbl, crlnfixt, crlnfixv, crlnfixct, &status);
      if (status) use_sdofixtbl = 0;
    }
    status = solar_image_info (irec, &img_xscl, &img_yscl, &img_xc, &img_yc,
	&img_radius, rsun_key, apsd_key, &img_pa, &ellipse_e, &ellipse_pa,
	&x_invrt, &y_invrt, &need_ephem, 0);
    if (status & NO_SEMIDIAMETER) {
      int keystat = 0;
      double dsun_obs = drms_getkey_double (irec, dsun_key, &keystat);
      if (keystat) {
	fprintf (stderr, "Error: one or more essential keywords or values missing; skipped\n");
	fprintf (stderr, "solar_image_info() returned %08x\n", status);
	continue;
      }
			       /*  set image radius from scale and distance  */
      img_radius = asin (RSUNM / dsun_obs);
      img_radius *= 3600.0 * degrad;
      img_radius /= (img_xscl <= img_yscl) ? img_xscl : img_yscl;
      status &= ~NO_SEMIDIAMETER;
    }
    if (status) {
      fprintf (stderr, "Error: one or more essential keywords or values missing; skipped\n");
      fprintf (stderr, "solar_image_info() returned %08x\n", status);
      continue;
    }
    if (SDO_fixcrot) img_pa -= 0.00702 * raddeg;
    if (obs2recfix) img_radius = obsval[nr].apsd / img_xscl;
    if (MDI_correct) {
      mtrack_MDI_correct_imgctr (&img_xc, &img_yc, img_radius);
      mtrack_MDI_correct_pa (&img_pa);
    }
    record_segment = drms_segment_lookup (irec, seg_name);
    if (!no_proc) {
		  /*  actual image data manipulation - read input data image  */
      data_array = drms_segment_read (record_segment, DRMS_TYPE_FLOAT, &status);
      if (status) {
	if (ut_times) sprint_time (tbuf, trec, "UTC", 0);
	else sprint_time (tbuf, trec, "TAI", 0);
	fprintf (stderr, "Error: segment read failed for record %s\n", tbuf);
	if (qualcheck) {
	  if (data_array) drms_free_array (data_array);
	  drms_free_array (map_array);
	  drms_close_records (ods, DRMS_FREE_RECORD);
	  free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
		map_sinlat, offsun, orecs, log, reject_list);
	  return 1;
	} else continue;
      }
      data = (float *)data_array->data;
				   /*  remove average background from image  */
      if (dxy) {
	for (nn = 0; nn < dxy; nn++) {
	  if (isnan (data[nn])) continue;
	  data[nn] -= bck[nn];
	}
      }
      img_xc -= 0.5 * (data_array->axis[0] - 1);
      img_yc -= 0.5 * (data_array->axis[1] - 1);

      if (!extrapolate)
	memcpy (last, maps, rgnct * pixct * sizeof (float));
		      /*  loop through output records, appending time slice  */
      perform_mappings (data_array, maps, delta_rot, rgnct, maplat,
	  maplon, map_coslat, map_sinlat, pixct, tobs - tmid, offsun,
	  img_lat, img_lon, img_xc, img_yc, a0, a2, a4, merid_v, img_radius,
	  img_pa, ellipse_e, ellipse_pa, x_invrt, y_invrt, intrpopt,
	  MDI_correct_distort);
      if (need_limb_dist) {
	calc_limb_distance (delta_rot, rgnct, cmaplat, cmaplon, tobs - tmid,
	    merid_v, ctroffsun, img_lat, img_lon, ctrmu, ctrx, ctry);
	for (rgn = 0; rgn < rgnct; rgn++) {
          if (isfinite (ctrmu[rgn])) {
	    muct[rgn]++;
	    muavg[rgn] += ctrmu[rgn];
	    xavg[rgn] += ctrx[rgn];
	    yavg[rgn] += ctry[rgn];
	  }
	}
      }
      if (need_stats) {
	double v, v2;
	if (data_scaled) {
	  for (rgn = 0; rgn < rgnct; rgn++) {
            nn = rgn * pixct;
	    for (n = 0; n < pixct; n++) {
	      v = maps[n + nn];
	      if (!isfinite (v)) {
		origvalid[rgn]--;
		datavalid[rgn]--;
	      } else if (v < min_scaled || v > max_scaled) {
		datavalid[rgn]--;
	      } else {
		if (v > maxval[rgn]) maxval[rgn] = v;
		if (v < minval[rgn]) minval[rgn] = v;
		datamean[rgn] += v;
		v2 = v * v;
		datarms[rgn] += v2;
		dataskew[rgn] += v2 * v;
		datakurt[rgn] += v2 * v2;
	      }
	    }
	  }
	} else {
	  for (rgn = 0; rgn < rgnct; rgn++) {
            nn = rgn * pixct;
	    for (n = 0; n < pixct; n++) {
	      v = maps[n + nn];
	      if (!isfinite (v)) {
		origvalid[rgn]--;
		datavalid[rgn]--;
	      } else {
		if (v > maxval[rgn]) maxval[rgn] = v;
		if (v < minval[rgn]) minval[rgn] = v;
		datamean[rgn] += v;
		v2 = v * v;
		datarms[rgn] += v2;
		dataskew[rgn] += v2 * v;
		datakurt[rgn] += v2 * v2;
	      }
	    }
	  }
	}
      }
    }
	    /*  extrapolate first image backward to start time if necessary  */
    if (extrapolate) {
      int ntot = rgnct * pixct;
      int fillunset = 1;
      float *val = (float *)malloc (ntot * sizeof (float));

      while (ttrgt < trec) {
	if (fillopt == FILL_BY_INTERP) { 
	  if (fillunset)
	    for (n = 0; n < ntot; n++) val[n] = maps[n];
	  fillunset = 0;
	  if (verbose) printf ("step %d extrapolated from image %s\n", or, tbuf);
	} else if (fillopt == FILL_WITH_ZERO) {
	  if (fillunset)
	    for (n = 0; n < ntot; n++) val[n] = (isfinite (maps[n])) ? 0.0 :
		missing_val;
	  fillunset = 0;
	  if (verbose) printf ("step %d zero filled\n", or);
	} else if (fillopt == FILL_WITH_NAN) {
	  if (fillunset)
	    for (n = 0; n < ntot; n++) val[n] = missing_val;
	  fillunset = 0;
	  if (verbose) printf ("step %d blank filled\n", or);
	}
	if (verbose && !(or % 64)) fflush (stdout);
	if (!no_proc) {
	  for (rgn = 0; rgn < rgnct; rgn++) {
	    orec = orecs[rgn];
	    oseg = drms_segment_lookup (orec, osegname);
	    memcpy (map_array->data, &val[rgn*pixct], pixct * sizeof (float));
	    slice_start[2] = slice_end[2] = or;
	    status = drms_segment_writeslice (oseg, map_array, slice_start,
	        slice_end, 0);
	    if (status) {
	      fprintf (stderr, "Error writing data to record %d in series %s\n",
		rgn, outser);
	      fprintf (stderr, "      series may not have appropriate structure\n");
	      drms_free_array (map_array);
	      drms_close_records (ods, DRMS_FREE_RECORD);
	      free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
		  map_sinlat, offsun, orecs, log, reject_list);
	      return 1;
	    }
	    if (verbose_logs) fprintf (log[rgn],
	        "step %d extrapolated from image %s\n", or, tbuf);
	  }
	}
	or++;
	if (or >= length) {
	  fprintf (stderr, "Error: reached output length limit\n");
	  drms_close_records (ids, DRMS_FREE_RECORD);
	  drms_close_records (ods, DRMS_FREE_RECORD);
	  free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
	      map_sinlat, offsun, orecs, log, reject_list);
	  if (bck) free (bck);
	  return 1;
	}
	ttrgt += tstep;
      }
      extrapolate = 0;
      free (val);
    }
    if (remove_obsvel) {
      double obsvr, obsvw, obsvn, apsd;
      if (obs2recfix) {
	obsvr = obsval[nr].vr;
	obsvw = obsval[nr].vw;
	obsvn = obsval[nr].vn;
      } else {
	  if (ephemeris_params (irec, &obsvr, &obsvw, &obsvn)) {
	  obsvr = obsvw = obsvn = 0.0;
	  remove_obsvel = 0;
	  fprintf (stderr, "Warning: expected keywords not found;\n");
	  fprintf (stderr, "         no correction will be made for orbital velocity.\n");
	}
      }
      apsd = img_xscl * img_radius * raddeg / 3600.0;
      adjust_for_observer_velocity (maps, rgnct, clat, clon, pixct,
	  img_lat, img_lon, obsvr, obsvw, obsvn, apsd, 0);
    }
			 /*  remove solar rotation signal from Doppler data  */
    if (remove_rotation) adjust_for_solar_rotation (maps, rgnct, clat, clon,
	pixct, img_lat, img_lon);

    if (ttrgt < trec) {
	/*  linearly interpolate individual pixels between last valid map
								and current  */
      double f, g;
      int ntot = rgnct * pixct;
      float *val = (float *)malloc (ntot * sizeof (float));
      char *skip = (char *)malloc (ntot * sizeof (char));
      
      for (n = 0; n < ntot; n++) {
	skip[n] = (isnan (last[n]) || isnan (maps[n]));
	val[n] = missing_val;
      }
      while (ttrgt < trec) {
	if (no_proc) {
	  if (verbose) {
	    if (fillopt == FILL_BY_INTERP || fabs (ttrgt - tlast) <= tstep) {
	      printf ("step %d not interpolated from images %s and %s\n", or,
		  ptbuf, tbuf);
	    } else if (fillopt == FILL_WITH_ZERO)
	      printf ("step %d zero filled\n", or);
	    else if (fillopt == FILL_WITH_NAN)
	      printf ("step %d blank filled\n", or);
	    if (!(or % 64)) fflush (stdout);
	  }
	} else {
	  if (fillopt == FILL_BY_INTERP || fabs (ttrgt - tlast) <= tstep) {
            f = (ttrgt - tlast) / (tobs - tlast);
	    g = 1.0 - f;
  	    for (n = 0; n < ntot; n++)
	      if (!skip[n]) val[n] = g * last[n] + f * maps[n];
	    if (verbose) {
	      printf ("step %d interpolated from images %s and %s\n", or, ptbuf,
		  tbuf);
	      if (!(or % 64)) fflush (stdout);
	    }
	  } else if (fillopt == FILL_WITH_ZERO) {
	    for (n = 0; n < ntot; n++) if (isfinite (val[n])) val[n] = 0.0;
	    if (verbose) printf ("step %d zero filled\n", or);
	  } else if (fillopt == FILL_WITH_NAN) {
	    for (n = 0; n < ntot; n++) val[n] = missing_val;
	    if (verbose) printf ("step %d blank filled\n", or);
	  }
	  for (rgn = 0; rgn < rgnct; rgn++) {
	    orec = orecs[rgn];
	    oseg = drms_segment_lookup (orec, osegname);
	    memcpy (map_array->data, &val[rgn*pixct], pixct * sizeof (float));
	    slice_start[2] = slice_end[2] = or;
	    status = drms_segment_writeslice (oseg, map_array, slice_start,
		slice_end, 0);
	    if (status) {
	      fprintf (stderr, "Error writing data to record %d in series %s\n",
		  rgn, outser);
	      fprintf (stderr,
		  "      series may not have appropriate structure\n");
	      drms_free_array (map_array);
	      drms_close_records (ods, DRMS_FREE_RECORD);
	      free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
		  map_sinlat, offsun, orecs, log, reject_list);
	      return 1;
	    }
	    if (verbose_logs) fprintf (log[rgn],
		"step %d interpolated from images %s and %s\n", or, ptbuf, tbuf);
	  }
	}
	or++;
	if (or >= length) {
	  if (nr < recct - 1) fprintf (stderr,
	      "Warning: reached output limit before last input record processed\n");
	  ttrgt = tstop;
	}
	ttrgt += tstep;
      }
      free (val);
      free (skip);
    }

    if (ttrgt == trec) {
      if (no_proc) {
	if (verbose) {
	  printf ("step %d not mapped from image %s [#%lld]\n", or, tbuf,
	      irec->recnum);
	  if (!(or % 64)) fflush (stdout);
	}
      } else {
	if (verbose) {
	  printf ("step %d mapped from image %s [#%lld]\n", or, tbuf,
	      irec->recnum);
	  if (!(or % 64)) fflush (stdout);
	}
	for (rgn = 0; rgn < rgnct; rgn++) {
	  orec = orecs[rgn];
	  oseg = drms_segment_lookup (orec, osegname);
	  memcpy (map_array->data, &maps[rgn*pixct], pixct * sizeof (float));
	  slice_start[2] = slice_end[2] = or;
	  status = drms_segment_writeslice (oseg, map_array, slice_start,
	      slice_end, 0);
	  if (status) {
	    fprintf (stderr, "Error writing data to record %d in series %s\n",
	        rgn, outser);
	    fprintf (stderr, "      series may not have appropriate structure\n");
	    drms_free_array (map_array);
	    drms_close_records (ods, DRMS_FREE_RECORD);
	    free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
	        map_sinlat, offsun, orecs, log, reject_list);
	    return 1;
	  }
	  if (verbose_logs)
            fprintf (log[rgn], "step %d mapped from image %s [#%lld]\n", or,
	        tbuf, irec->recnum);
	}
      }
      or++;
      ttrgt += tstep;
    }
    tlast = trec;
    strcpy (ptbuf, tbuf);
    drms_free_array (data_array);
    data_array = NULL;
    valid++;
  }
  if (bck) free (bck);
  for (rgn = 0; rgn < rgnct; rgn++) {
       /*  propagate designated key values from last input record to output  */
    int kstat = 0;
    int keyct = sizeof (propagate) / sizeof (char *);
    orec = orecs[rgn];
    kstat += propagate_keys (orec, irec, propagate, keyct);
    if (kstat) {
      fprintf (stderr, "Error writing key value(s) to record %d in series %s\n",
	  rgn, outser);
      fprintf (stderr, "      series may not have appropriate structure\n");
      drms_free_array (map_array);
      drms_close_records (ids, DRMS_FREE_RECORD);
      drms_close_records (ods, DRMS_FREE_RECORD);
      free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
	  map_sinlat, offsun, orecs, log, reject_list);
      return 1;
    }
    if ((keywd = drms_keyword_lookup (orec, "COMMENT", 1)))
      append_args_tokey (orec, "COMMENT");
    else if ((keywd = drms_keyword_lookup (orec, "HISTORY", 1)))
      append_args_tokey (orec, "HISTORY");
  }
  drms_close_records (ids, DRMS_FREE_RECORD);

  if (!valid) {
    fprintf (stderr, "Error: no valid records in input dataset %s\n", input);
    if (badtkey) fprintf (stderr,
	"    %d of %d records rejected for invalid values of %s\n",
	badtkey, recct, tobs_key);
    if (badqual) fprintf (stderr,
	"    %d of %d records rejected for quality matching %08x\n",
	badqual, recct, qmask);
    if (badfill) fprintf (stderr,
	"    %d  of %d records rejected for missing values exceeding %d\n",
	badfill, recct, max_miss);
    if (badtime) fprintf (stderr,
	"    %d of %d records rejected for duplicate values of %s\n",
	badtime, recct, trec_key);
    if (badcv) fprintf (stderr,
	"    %d of %d records rejected for unacceptabel values of %s\n",
	badcv, recct, calverkey);
    drms_close_records (ods, DRMS_FREE_RECORD);
    free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
	map_sinlat, offsun, orecs, log, reject_list);
    return 1;
  }
					 /*  extend last image if necessary  */
  if (or < length) {
    int fillunset = 1;
    int ntot = rgnct * pixct;
    float *val = (float *)malloc (ntot * sizeof (float));

    while (ttrgt <= tstop && or < length) {
      if (fillopt == FILL_BY_INTERP) {
	if (fillunset)
	  for (n = 0; n < ntot; n++) val[n] = last[n];
	fillunset = 0;
	if (verbose) printf ("step %d extrapolated from image %s\n", or, ptbuf);
      } else if (fillopt == FILL_WITH_ZERO) {
	if (fillunset)
	  for (n = 0; n < ntot; n++) if (isfinite (val[n])) val[n] = 0.0;
	fillunset = 0;
	if (verbose) printf ("step %d zero filled\n", or);
      } else if (fillopt == FILL_WITH_NAN) {
	if (fillunset)
	  for (n = 0; n < ntot; n++) val[n] = missing_val;
	fillunset = 0;
     	if (verbose) printf ("step %d blank filled\n", or);
      }
      if (verbose && !(or % 64)) fflush (stdout);
      for (rgn = 0; rgn < rgnct; rgn++) {
	orec = orecs[rgn];
	oseg = drms_segment_lookup (orec, osegname);
	memcpy (map_array->data, &val[rgn*pixct], pixct * sizeof (float));
	slice_start[2] = slice_end[2] = or;
	status = drms_segment_writeslice (oseg, map_array, slice_start,
	    slice_end, 0);
	if (status) {
	  fprintf (stderr, "Error writing data to record %d in series %s\n",
	      rgn, outser);
	  fprintf (stderr, "      series may not have appropriate structure\n");
	  drms_free_array (map_array);
	  drms_close_records (ods, DRMS_FREE_RECORD);
	  free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
	      map_sinlat, offsun, orecs, log, reject_list);
	  return 1;
	}
	if (verbose_logs) {
	  if (fillopt == FILL_BY_INTERP)
	    fprintf (log[rgn], "step %d extrapolated from image %s\n", or, ptbuf);
	  else if (fillopt == FILL_WITH_ZERO)
	    fprintf (log[rgn], "step %d zero filled\n", or);
	  else if (fillopt == FILL_WITH_NAN)
	    fprintf (log[rgn], "step %d blank filled\n", or);
	}
      }
      ttrgt += tstep;
      or++;
    }
    free (val);
  }

  coverage = (double)valid / (double)length;
  if (tstep != data_cadence) coverage *= data_cadence / tstep;
  if (verbose) {
    printf ("End tracking: %d of %d possible input records accepted\n",
        valid, recct);
    printf ("              effective coverage = %.3f\n", coverage);
    if (badtkey) printf
	("    %d input records rejected for invalid values of %s\n",
	badtkey, tobs_key);
    if (badqual) printf
	("    %d input records rejected for quality matching %08x\n",
	badqual, qmask);
    if (blacklist) printf
	("    %d input records rejected from rejection list\n", blacklist);
    if (badfill) printf
	("    %d input records rejected for missing values exceeding %d\n",
	badfill, max_miss);
    if (badtime) printf
	("    %d input records rejected for duplicate values of %s\n",
	badtime, trec_key);
    if (badcv) printf
	("    %d input records rejected for unacceptable values of %s\n",
	badcv, calverkey);
  }
				     /*  check for scaled data out of range  */
  if (data_scaled) {
    long long totor = 0;
    int orrgn = 0;
    for (rgn = 0; rgn < rgnct; rgn++) {
      totor += origvalid[rgn] - datavalid[rgn];
      orrgn++;
    }
    if (totor) {
      fprintf (stderr,
  	  "Warning: %lld valid values scaled out of representable range in %d records\n",
	  totor, orrgn);
      for (rgn = 0; rgn < rgnct; rgn++) {
	if (origvalid[rgn] != datavalid[rgn])  {
	  if (verbose_logs) fprintf (log[rgn],
	      "         %lld valid values scaled out of representable range\n",
	      origvalid[rgn] - datavalid[rgn]);
	  if (verbose)
	      printf ("         %lld valid values scaled out of representable range in region #%d\n",
	      origvalid[rgn] - datavalid[rgn], rgn);
	} 
      }
    }
  }
      /*  adjust CR:CL if necessary for slotted output series to avoid lon 0  */						      /*  write out records  */
  if (tmid_cl <= 0.0) {
    tmid_cl += 360.0;
    tmid_cr++;
  }
  orec = orecs[0];
  if ((keywd = drms_keyword_lookup (orec, "CMLon", 1))) {
    if (keywd->info->recscope > 1) {
      double kstep;
      sprintf (key, "CMLon_step");
      kstep = fabs (drms_getkey_double (orec, key, &status));
      if (tmid_cl <= 0.5 * kstep) {
	tmid_cl += 360.0;
	tmid_cr++;
      }
    }
  }
  for (rgn = 0; rgn < rgnct; rgn++) {
							  /*  set key values  */
    int kstat = 0;
    orec = orecs[rgn];
    kstat += check_and_set_key_str   (orec, "WCSNAME", "Carrington Heliographic/Time");
    kstat += check_and_set_key_int   (orec, "WCSAXES", 3);
    snprintf (key, 64, "CRLN-%s", wcscode[proj]);
    kstat += check_and_set_key_str   (orec, "CTYPE1", key);
    snprintf (key, 64, "CRLT-%s", wcscode[proj]);
    kstat += check_and_set_key_str   (orec, "CTYPE2", key);
    kstat += check_and_set_key_str   (orec, "CTYPE3", "TIME");
    kstat += check_and_set_key_str   (orec, "CUNIT1", "deg");
    kstat += check_and_set_key_str   (orec, "CUNIT2", "deg");
    kstat += check_and_set_key_str   (orec, "CUNIT3", "s");
    kstat += check_and_set_key_float (orec, "CRPIX1", 0.5 * map_cols + 0.5);
    kstat += check_and_set_key_float (orec, "CRPIX2", 0.5 * map_rows + 0.5);
    kstat += check_and_set_key_float (orec, "CRPIX3", 0.5 * length + 0.5);
    kstat += check_and_set_key_float (orec, "CRVAL1", clon[rgn] * degrad);
    kstat += check_and_set_key_float (orec, "CRVAL2", clat[rgn] * degrad);
    kstat += check_and_set_key_double(orec, "CRVAL3", tmid);
    kstat += check_and_set_key_float (orec, "CDELT1", map_scale);
    kstat += check_and_set_key_float (orec, "CDELT2", map_scale);
    kstat += check_and_set_key_float (orec, "CDELT3", tstep);
    kstat += check_and_set_key_float (orec, "LonHG", clon[rgn] * degrad);
    kstat += check_and_set_key_float (orec, "LatHG", clat[rgn] * degrad);
    kstat += check_and_set_key_str   (orec, "MapProj", mapname[proj]);
    kstat += check_and_set_key_str   (orec, "Interp", interpname[intrpopt]);
    kstat += check_and_set_key_time  (orec, "MidTime", tmid);
    snprintf (ctime_str, 16, ctimefmt, tmid_cr, tmid_cl);
    kstat += check_and_set_key_str   (orec, "CarrTime", ctime_str);
    kstat += check_and_set_key_int   (orec, "CarrRot", tmid_cr);
    kstat += check_and_set_key_float (orec, "CMLon", tmid_cl);
    carr_lon = (360.0 * tmid_cr) + 360.0 - tmid_cl;
    kstat += check_and_set_key_double(orec, "CarrLon", carr_lon);
    lon_cm = clon[rgn] * degrad - tmid_cl;
    while (lon_cm < -180.0) lon_cm += 360.0;
    while (lon_cm > 180.0) lon_cm -= 360.0;
    kstat += check_and_set_key_float (orec, "LonCM", lon_cm);
    kstat += check_and_set_key_time  (orec, "T_START", tstrt);
    kstat += check_and_set_key_time  (orec, "T_STOP", tstop);
    kstat += check_and_set_key_float (orec, "LonSpan", lon_span);
    kstat += check_and_set_key_float (orec, "Coverage", coverage);
    kstat += check_and_set_key_float (orec, "ZonalTrk", delta_rot[rgn] * 1.0e6);
    kstat += check_and_set_key_float (orec, "ZonalVel",
	(delta_rot[rgn] + CARR_RATE * 1.0e-6) * RSUNM * cos (clat[rgn]));
    kstat += check_and_set_key_float (orec, "LonDrift",
	delta_rot[rgn] * degrad * length * tstep);
    kstat += check_and_set_key_float (orec, "MeridTrk", merid_v * 1.0e6);
    kstat += check_and_set_key_float (orec, "MeridVel", merid_v * RSUNM);
    kstat += check_and_set_key_float (orec, "LatDrift",
	merid_v * degrad * length * tstep);
    kstat += check_and_set_key_str   (orec, "Module", module_ident);
    kstat += check_and_set_key_str   (orec, "BLD_VERS", jsoc_version);
    kstat += check_and_set_key_str   (orec, "Source", source_series);
    kstat += check_and_set_key_str   (orec, "Input", input);
    kstat += check_and_set_key_time  (orec, "Created", CURRENT_SYSTEM_TIME);
    if (strcmp (bckgn, "Not Specified"))
      kstat += check_and_set_key_str (orec, "Backgrnd", bckgn);
    if (strcmp (rejectfile, "Not Specified"))
      kstat += check_and_set_key_str (orec, "RejectList", rejectfile);
    kstat += check_and_set_key_float (orec, "MapScale", map_scale);
    kstat += check_and_set_key_float (orec, "Cadence", tstep);
    kstat += check_and_set_key_float (orec, "Duration", length * tstep);
    kstat += check_and_set_key_float (orec, "Width", map_cols * map_scale);
    kstat += check_and_set_key_float (orec, "Height", map_rows * map_scale);
    kstat += check_and_set_key_float (orec, "Size", sqrt (map_rows * map_cols) * map_scale);
    kstat += check_and_set_key_float (orec, "Map_PA", map_pa[rgn] / raddeg);
    kstat += check_and_set_key_float (orec, "RSunRef", 1.0e-6 * RSUNM);
    if (setmais && isfinite (mai[rgn]))
      kstat += check_and_set_key_float (orec, "MAI", mai[rgn]);
    if (bscale_override) {
      sprintf (key, "%s_bscale", osegname);
      kstat += check_and_set_key_double (orec, key, bscale);
    }
    if (bzero_override) {
      sprintf (key, "%s_bzero", osegname);
      kstat += check_and_set_key_double (orec, key, bzero);
    }
    if (MDI_correct)
      kstat += check_and_set_key_float (orec, "MDI_PA_Corr", MDI_IMG_SOHO_PA);
    if (pkeyval = create_prime_key (orec))
      kstat += check_and_set_key_str (orec, "PrimeKeyString", pkeyval);
    if (strcmp (identifier, "Not Specified"))
      kstat += check_and_set_key_str (orec, id_key, identifier);
    if (need_stats) kstat += set_stat_keys (orec, nntot, datavalid[rgn],
        minval[rgn], maxval[rgn], datamean[rgn], datarms[rgn], dataskew[rgn],
	datakurt[rgn]);
    if (need_limb_dist) {
      if (muct[rgn]) {
	kstat +=
	    check_and_set_key_float (orec, "MeanMU", muavg[rgn] / muct[rgn]);
	xavg[rgn] /= muct[rgn];
	yavg[rgn] /= muct[rgn];
	kstat += check_and_set_key_float (orec, "MeanPA",
	    degrad * atan2 (xavg[rgn], yavg[rgn]));
      }
    }
    if (mnlatct[rgn])
      kstat += check_and_set_key_float (orec, "MeanLat", latavg[rgn] * degrad);
    if (kstat) {
      fprintf (stderr, "Error writing key value(s) to record %d in series %s\n",
	  rgn, outser);
      fprintf (stderr, "      series may not have appropriate structure\n");
      drms_free_array (map_array);
      drms_close_records (ods, DRMS_FREE_RECORD);
      free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon,
	  map_sinlat, offsun, orecs, log, reject_list);
      return 1;
    }
    if (verbose_logs) {
      if (badtkey) fprintf (log[rgn],
	  "    %d input records rejected for invalid values of %s\n",
	  badtkey, tobs_key);
      if (badqual) fprintf (log[rgn],
	  "    %d input records rejected for quality matching %08x\n",
	  badqual, qmask);
      if (blacklist) fprintf (log[rgn],
	  "    %d input records rejected from rejection list\n", blacklist);
      if (badfill) fprintf (log[rgn],
	  "    %d  of %d records rejected for missing values exceeding %d\n",
	  badfill, recct, max_miss);
      if (badtime) fprintf (log[rgn],
	  "    %d of %d records rejected for duplicate values of %s\n",
	  badtime, recct, trec_key);
      if (badcv) {
        fprintf (log[rgn],
	    "    %d input records rejected for calib version matching %016llx\n",
	    badcv, cvreject);
	fprintf (log[rgn],
	    "                     or failing to match %016llx\n", cvaccept);
      }
      fprintf (log[rgn], "%s values used:", calverkey);
      for (cvct = 0; cvct < cvused; cvct++) {
        if (cvct) fprintf (log[rgn], ",");
        fprintf (log[rgn], " %016llx (%d)", cvlist[cvct], cvfound[cvct]);
      }
      fprintf (log[rgn], "\n");
      fclose (log[rgn]);
    }
  }
  drms_close_records (ods, dispose);
  drms_free_array (map_array);
  free_all (clat, clon, mai, delta_rot, maps, last, maplat, maplon, map_sinlat,
      offsun, orecs, log, reject_list);
  return 0;
}
/******************************************************************************/
/*
 *  Revision History (all mods by Rick Bogart unless otherwise noted)
 *
 *  08.04.22 	created this file, based on fastrack
 *  v 0.1	add all args, modify as appropriate for DRMS;
 *	add all output keywords, fill as possible from args (except dataset)
 *    0.2	add input dataset processing, fill output keys as necessary
 *    0.3	process input record list (dataset) info, reflect in keys
 *	add platform info and ephemeris for SOHO
 *    0.4	open and close input segments, add background image processing
 *    0.5	create and write to output segments
 *    0.6	create interpolation targets, fix some key value bugs
 *    0.7	interpolate input data to output maps using crude keyword
 *	search; add interpolation of skipped images; close most memory leaks;
 *	fixed possible fastrack bug in calculation of f in interpolate_images()
 *    0.8	added keywords for output cadence, start/stop specification,
 *	interpolation option;
 *	extensive modifications to allow for time interpolation to different
 *		cadence from input, missing records and non-uniformly spaced
 *		records in input;
 *	removed restriction on output segment name;
 *	added correction for MDI image distortion and PA and image center
 *		(with -M flag), but not yet correction for plate scale if
 *		necessary;
 *	added processing of personal MDI image quality rejection lists
 *	added setting of optional PrimeKeyString
 *	adjust midtime to phase of input when specified as CR:CL
 *    0.9	write output by slices, avoiding massive memory allocation
 *	added code for nearest-value "interpolation" (untested)
 *	added processing of optional OBS_ASD key in place of R_SUN if missing
 *	changed defaults for output scaling to use output series defaults
 *	change Duration key type from int to float
 *	reject images with MISSVALS != 0 (if present)
 *	fixed setting of ZonalTrk and MeridTrk keys, added corresponding Vel
 *		keys and RSunRef; added MDI_PA_Corr key; fixed setting of
 *		Duration and CDELT3 keys
 *	adjusted Carrington rate (by 5 parts in 10^9)
 *	fixed setting of scaling when overrides in effect; support vardim
 *		output by setting segment axis
 *	fixed a few icc11 compiler warnings
 *	added argument rsun_key; verification of some essential keys
 *	added recognition of GONG site data origins, support for JPL table-based
 *		ephemerides, processing of quality keyword against qmask
 *	added optional processing of MAI value(s) into keyword
 *	changed constituent of primekeystring from CarrTime to CarrRot and
 *		CMLon
 *	added setting of Size keyword (geometric mean of Height and Width) and
 *		Cadence (= CDELT3)
 *  v 0.9 frozen 2010.02.08
 *    1.0	more careful treatment of strings from cmdparams; trap bad
 *		length request; fix global missing_val to be NaN
 *	Added "Created" keyword
 *	Fixed writing of Log to multiple output records, though it is
 *		inefficient
 *	Added JSOC build version to verbose output and keyword BLD_VERS
 *	Added max_miss argument for blank values threshold
 *	Added dsun_key argument for observer distance keyword, apsd_key for
 *		apparent solar semidiameter keyword, and mechanism for
 *		extracting semi-diameter from observer distance if missing
 *	Introduced check for images at same time in case HMI data duplicated
 *		for separate cameras
 *	Added propagation key list
 *	Improved midpoint elements calculation from record info
 *	Incorporated option for removal of observer velocity; however, there
 *	  is a bug in the removal of the transverse components, so those are
 *	  not included
 *	Changed default qmask value to 0x80000000
 *	Added test for no acceptable records, counts of certain rejection
 *	  filters, and verbose or error status reporting of same
 *	Added recognition of platform SDO, at least for HMI
 *	Modified reading of rejection list to accept alternate form for DRMS
 *	Added occasional flushing of stdout for monitoring verbose output in
 *	  batch runs
 *  v 1.0 frozen 2010.04.23
 *    1.1	Removed reading of background image as FITS file with local
 *	code; require that it be in DRMS
 *		Changed output keyword from PosAng to Map_PA
 *		Fixed bug in determining tstep when no input cadence available
 *		Transverse component of observer velocity *is* included; the
 *	remark for v 1.0 was erroneous
 *		Added option for bilinear interpolation
 *		Changed default value of bzero to use segment default
 *		Added calculation as needed (but not yet setting) of minimum
 *	and maximum data values for each output cube
 *		Support specification of tstart and/or tstop as well as tmid
 *	in CR:CL format; any observer location other than SOHO uses geocentric
 *	ephemeris
 *		Added ident parameter
 *		Added check for failed segment reads (SUMS errors or missing
 *	segments from records expected to have them)
 *		Added prefetch to streamline SUMS interface; backed out
 *		Minor output format fixes to properly escape quoted strings
 *		Added mai, log, orecs, and reject_list to free_all arguments
 *		Fixed  bug in setting of MAI values
 *  v 1.1 frozen 2010.08.20
 *    1.2	Added setting of statistics keywords
 *		Added calculation of center-limb distances for region centers
 *	for keyword setting
 *		Added calculation of mean latitude for regions for key setting
 *		Added warnings about redundant unused parameters
 *		Added reporting of additional reasons for failure to find any
 *	valid records
 *		Added scaling check initializations
 *  v 1.2 frozen 2011.06.06
 *    1.3	First record for series info is first recnum, not first pkey
 *		Fixed bug in determination of segment from multiple candidates
 *		Removed unused -G flag, added -Z flag for UT time logging
 *  v 1.3 frozen 2011.11.09
 *    1.4	Require that output CMLon key value >= 0, and > 0 if key is
 *	slotted
 *		Allow setting of long long key values as appropriate
 *		Removed needless scope declarations on helper functions
 *		Added argument to call to solar_image_info to (always) specify
 *		CROTA2 keyword as opposite to AIPS convention
 *		Added calculation of mean position angle for region centers
 *	for keyword setting (not tested)
 *  v 1.4 frozen 2012.04.23
 *    1.5	Fixed bug in bilinear interpolation function (failure to
 *		properly detect out of range)
 *  v 1.5 frozen 2012.10.16
 *    1.6	Added recording of recnums of mapped images to log
 *		Added recording of calling params info to comment or history key
 *		Added support for acceptance and/or rejection of records with
 *	certain values of CalVer64 (or other equivalent key)
 *		Added logging of image rejection cause summaries and CalVer64
 *	values used
 *		Fixed bscale-bzero overrides
 *  v 1.6 frozen 2013.04.08
 *    1.7	Added optional removal of solar rotation Doppler signal from
 *	input (2013.06.03)
 *    		Fixed bug in determination of midpoint elements when keying
 *	from date/time and using T_REC as tobs_key and when elements missing
 *	for the midpoint input record (2013.08.30)
 *  v 1.7 frozen 2013.09.04
 *    1.8	Removed functions drms_appendstr_tokey() and append_args_tokey()
 *	(now in keystuff) (2014.10.19)
 *  v 1.8 frozen 2015.01.13
 *    1.9	Added option for position angle of output maps to be
 *	individually specified rather than uniform (2015.08.20)
 *		Changed default for max_miss from 0 to "All" (i.e. no check)
 *	(2015.09.21)
 *		Made mechanisms for determination of data cadence more
 *	inclusive and systematic, using slotted trec_key info, as well as using
 *	slotted trec_key info for midtime/length searches; support the
 *	specification of input segments as part of dataset specifier as well
 *	as by using the segment parameter; added option for zero- or nan-filling
 *	long gaps rather than per-pixel temporal interpolation; fixed bug in
 *	extrapolation exposed when interval is expressed in midtime/length
 *	form and tmid is in date_time format and initial image is missing
 *	ephemeris information (2016.03.07)
 *  v 1.9 frozen 2016.06.20
 *  v 2.0	Simplification of Carrington time determinations, to correct
 *	rare bug of setting wrong Carrington rotation key value; start and end
 *	times printed to nearest second rather than minute; Added -w flag for
 *	rapid testing of just record key processing; Added -G flag to force
 *	use of geocentric ephemeris for meridian crossing times, and specific
 *	location for geocenter
 *  v 2.0 frozen 2016.11.29
 *  v 2.1	Fixed bug requiring tstep to be explictly set when input
 *	dataset fully specified; fixed reporting of coverage when output
 *	and input step sizes differ; added versioning of some included code
 *	(2019.11.04)
 *  v 2.1 frozen 2019.11.26
 *  v 2.2	Added -G flag to arg list for description; fixed bug causing
 *	failure to properly set keys for Carrington coordinates when target
 *	midpoint specified as date_time or by dataset specification and
 *	input record for midpoint missing data; added check for limb-distance
 *	calculation; fixed bug requiring tstep to be explictly set when
 *	when input dataset fully specified (not fully fixed in previous
 *	version
 *  v 2.2 frozen 2019.01.14
 *  v 2.3	Fixed declaration of platform_info() to make certain compilers
 *	happy, and body to return correct info for AIA; Extensive changes to
 *	use functions track_record_selection () and select_segment() to better
 *	implement data selection rules, and subsidiary functions to use DRMS
 *	localized serverdefs as needed; Fixed bug that could have caused use
 *	of wrong segment when input series has multiple 2-d segments and
 *	background subtraction is done; Fixed bug in getctrloc_from_time()
 *	that was always calculating for SOHO; Fixed the setting of the MidTime
 *	keyword to correct for a bias of half the cadence; Modified to avoid
 *	interpolation between adjacent acceptable input steps when dataset
 *	selection is by tmid/length, regardless of phase of target time and
 *	parity of length
 *  v 2.3 frozen 2020.07.03
 *  v 2.4	Fixed use of localized serverdefs.
 *	Added mechanism for correction of HMI Carrington longitude of disc
 *	  center as necessary;
 *	Modified to use key specified by trec_key as target time rather than
 *	  tobs_key, so that it is no longer necessary to specify T_REC as
 *	  tobs_key when T_REC is the prime key
 *	Modified to interpolate ephemeris values appropriate to obs time to
 *	  rec time; the interpolation is suppressed by specifying tobs_key
 *	  as T_REC
 *	Added option to suppress correction of SDO Carrington longitude,
 *	  argument for name of ephemeris correction series
 *	Some code reorganization
 *	Added check of calver bit for position angle correction, and
 *	  fixing as needed
 *  v 2.4 frozen 2020.09.22
 *  v 2.5	Fixed a comment; Rewrite getlon_span() to fix bad returns
 *	when data missing at endpoints
 *	Changed default for sdofix series
 *	Added trap for NaN values in map target locations which can be
 *	  returned in rare circumstances by version 10 of plane2sphere()
 *	  without setting the offsun flag to the interpolation routines
 *	Use v 11 of cartographic routines
 *	Either CRLN or CRLN_OBS accepted in sdofix series
 *	Fixed verbose reporting bug when extrapolating at end
 *  v 2.5 frozen 2020.12.14
 *	Changed declaration of index into correction table from static to
 *	  dynamic in interp_crlfix() and in interp_hmi_fixes()
 *	Added requirement of need_crlnfix true on record-by-record basis for
 *	  call to adjust_sdo_clon() in main module body to avoid over-correction
 *	  when table not in use
 *  v 2.5 refrozen 2020.12.28
 *  v 2.6	Fixed non-initialization bug exposed when merid_v != 0.
 *	Added option for specifying different name of identifier key
 *	Removed potentially dangerous frees of template records
 *	Updated version controlled included ephemeris code and other utils,
 *	using DRMS localization defs only
 *  v 2.6 frozen 2021.11.05
 */
/******************************************************************************/

