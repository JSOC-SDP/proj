#include "jsoc_main.h"
#include "drms_types.h"

/*	Populate keywords from ME inversion results to 
 *	ME_patch, B, B_patch, ME_HARP, B_HARP, etc.
 *
 *	Upon success, return 0
 *	Otherwise return number of keywords failed to populate
 *	If record fails, return -1
 *
 *	Prime keys like T_REC, PNUM needs to be set upfront in the main code
 *
 *	Usage:
 *	For full disk image population, use:
 *		copy_me_keys() and copy_geo_keys()
 *	For patch population without remapping, use:
 *		copy_me_keys() and copy_geo_keys() and copy_patch_keys()
 *	For patch with remapping, use:
 *		copy_me_keys() and copy_patch_keys()
 *	For B series, additionally use:
 *		copy_ambig_keys()
 *
 *	Written by X. Sun, Mar 01 2011
 * 
 *      Added keywords DATE, BLD_VERS, QUIET, OFFDISK, INVNFCTI, 
 *                     INVNFCTQ, INVNFCTU, INVNFCTV
 */

#define ARRLENGTH(ARR) (sizeof(ARR) / sizeof(ARR[0]))

const char *meKeys[] =
{
	"QUALITY", "QUAL_S", "QUALLEV1", "INSTRUME", "CAMERA",		// info
	"T_OBS", "CADENCE", "DATE_S", "DATE__OBS", "DATE", "DATE_M",											// time
	"DSUN_OBS", "CRLN_OBS", "CRLT_OBS", "CAR_ROT",
	"OBS_VR", "OBS_VW", "OBS_VN", "RSUN_OBS",			// geometry
	"INVCODEV", "INVDOCU", "INVITERA", "INVLMBDA", "INVLMBDF",
	"INVTUNEN", "INVSVDTL", "INVCHIST", "INVPOLTH",
	"INVPJUMP", "INVLMBDM", "INVLMBD0", "INVLMBDB",
	"INVDLTLA", "INVLMBDS", "INVLMBMS",
        "INVLYOTW", "INVWNARW", "INVWSPAC",
	"INVINTTH", "INVNOISE", "INVCONTI", "INVWGHTI",
	"INVWGHTQ", "INVWGHTU", "INVWGHTV", "INVSTLGT", "INVFREEP",
	"INVFLPRF", "INVPHMAP", "INVVLAVE", "INVBLAVE", "INVBBAVE",
	"INVNPRCS", "INVNCNVG",
	"INVKEYS1", "INVKEYS2", "INVKEYS3", 
	"INVKEYI1", "INVKEYI2", "INVKEYI3", 
	"INVKEYD1", "INVKEYD2", "INVKEYD3", 				 // inversion, removed Bunit Nov 18
	"INVNFCTI", "INVNFCTQ", "INVNFCTU", "INVNFCTV",
	"HFLID", "HCFTID", "QLOOK", "TINTNUM", "SINTNUM",
	"DISTCOEF", "ROTCOEF", "POLCALM", "SOURCE",
	"CODEVER0", "CODEVER1", "CODEVER2", "CODEVER3", "CODEVER4", "CODEVER5", "CODEVER6","BLD_VERS","CALVER64"	// misc
};

const char *patchKeys[] = 
{
	"PATCHNUM","ARMCODEV","ARMDOCU","HRPCODEV","HRPDOCU",
	"CRSIZE1","CRSIZE2","ACTIVE","NCLASS","ON_PATCH","MASK","ARM_QUAL","ARM_NCLN","H_MERGE","H_FAINT",
	"ARM_MODL","ARM_EDGE","ARM_BETA","TKP_KWID","TKP_KLAT","TKP_TAU","TKP_TAU2",
	"TKP_ACTV","TKP_FNUM","TKP_FTIM","TKP_MAPR","TKP_RUNN","TKP_RUNT",
	"LATDTMIN","LONDTMIN","LATDTMAX","LONDTMAX","OMEGA_DT",
	"NPIX","SIZE","AREA","NACR","SIZE_ACR","AREA_ACR",
	"MTOT","MNET","MPOS_TOT","MNEG_TOT","MMEAN","MSTDEV","MSKEW","MKURT",
	"LAT_MIN","LON_MIN","LAT_MAX","LON_MAX","LAT_FWT","LON_FWT","LAT_FWTPOS","LON_FWTPOS","LAT_FWTNEG","LON_FWTNEG",
	"T_FRST","T_FRST1","T_LAST1","T_LAST","N_PATCH","N_PATCH1","N_PATCHM","NOAA_AR","NOAA_NUM","NOAA_ARS","OFFDISK", "QUIET"
};

const char *geoKeys[] =
{
	"CRPIX1", "CRPIX2", "CRVAL1", "CRVAL2",
	"CDELT1", "CDELT2", "CROTA2",
	"CRDER1", "CRDER2", "CSYSER1", "CSYSER2",
	"IMCRPIX1", "IMCRPIX2"
};

const char *ambigKeys[] =
{
  "AMBCODEV", "AMBDOCU",
	"AMBGMTRY", "AMBWEAK", "AMBNEROD", "AMBNGROW",
	"AMBNPAD", "AMBNAP", "AMBNTX", "AMBNTY",
	"AMBBTHR0", "AMBBTHR1", "AMBSEED", "AMBNEQ",
	"AMBLMBDA", "AMBTFCT0", "AMBTFCTR", "AMBPATCH"
};



int copy_me_keys (DRMS_Record_t *inRec, DRMS_Record_t *outRec)

{
	int failCount = 0;
	int iKey, nKeys = ARRLENGTH(meKeys);
	
	if (inRec == NULL || outRec == NULL) {
		return -1;
	}
	
	for (iKey = 0; iKey < nKeys; iKey++)
	{
		failCount += drms_copykey(outRec, inRec, meKeys[iKey]);
	}
	
	return failCount;
}



int copy_patch_keys (DRMS_Record_t *inRec, DRMS_Record_t *outRec)

{
	int failCount = 0;
	int iKey, nKeys = ARRLENGTH(patchKeys);
	
	if (inRec == NULL || outRec == NULL) {
		return -1;
	}
	
	for (iKey = 0; iKey < nKeys; iKey++)
	{
		failCount += drms_copykey(outRec, inRec, patchKeys[iKey]);
	}
	
	return failCount;
}



int copy_geo_keys (DRMS_Record_t *inRec, DRMS_Record_t *outRec)

{
	int failCount = 0;
	int iKey, nKeys = ARRLENGTH(geoKeys);
	
	if (inRec == NULL || outRec == NULL) {
		return -1;
	}
	
	for (iKey = 0; iKey < nKeys; iKey++)
	{
		failCount += drms_copykey(outRec, inRec, geoKeys[iKey]);
	}
	
	return failCount;
}



int copy_ambig_keys (DRMS_Record_t *inRec, DRMS_Record_t *outRec)

{
	int failCount = 0;
	int iKey, nKeys = ARRLENGTH(ambigKeys);
	
	if (inRec == NULL || outRec == NULL) {
		return -1;
	}
	
	for (iKey = 0; iKey < nKeys; iKey++)
	{
		failCount += drms_copykey(outRec, inRec, ambigKeys[iKey]);
	}
	
	return failCount;
}
