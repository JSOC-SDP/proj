/*****************************************************************************
 * Filename: decode_hk.c                                                     *
 * Author: Carl                                                              *
 * Create Date: August, 5, 2005                                              *
 * Description: This file contains modules to decode housekeeping data.      *
 * (C) Stanford University, 2005                                             *
 *                                                                           *
 * ----------------------   ------------------------- ---------------------  *
 * Type                     Name                       Description           *
 * ----------------------   ------------------------- ---------------------  *
 *  
 *                                                                           *
 * Revision History:                                                         *
 * --------------  -------------  ----------------------------------------   *
 * Draft/Revision  Date           Description                                *
 * --------------  -------------  ----------------------------------------   *
 * 0.0               8/5/2005     Created                                    *
 * 0.1               10/19/2005   Pass draft 0 revision 1 to Rasmus to       *
 *                                integrate with L0 code                     *
 * 0.2               10/27/2005   Pass back code after Rasmus did a first try*
 *                                at integrating code with L0 code.          *
 *                                Add reread_all_files function call which   *
 *                                reads all config files when a configuration*
 *                                data for version number does not exist.    *
 *                                Updated code to handle version number in   *
 *                                structures as string buffer rather than as *
 *                                float value.                               *
 * 0.3              11/14/2005    Updated get_version_number(),              *
 *                                decode_hk_keywords() and load_hk_value()   *
 *                                functions to integrate with                *
 *                                Rasmus's new api's and structures.         *
 *                                Create new function load_engr_values() to  *
 *                                load engr values into HK_Keyword_t         *
 *                                structure. Removed compiler warnings.      *
 * 0.4              11/16/2005    After Rasmus took code to integrate added: *
 *                                deallocate_hk_keywords_format() call to    *
 *                                deallocate memory nodes in link list.      *
 *                                Removed deallocate_pointers_hk_keywords()  *
 *                                function.                                  *
 * 0.5              2005/11/28    Re-wrote linked list handling. Removed     *
 *                                global variables. Shortened variable names.*
 * 0.6   carl      11/30/2005     Added code to fill in mnemonic value in    *
 *                                name and keyword value in fitsname in the  *
 *                                HK_Keyword_t structure.                    *
 * 0.7   carl      01/19/2006     Added environment variable for finding     *
 *                                config data for apid 431. Added error      *
 *                                when cannot find config data initially in  *
 *                                the link list of config data. Update call  *
 *                                to check_version_number() to check_packet_ *
 *                                version_number().                          *
 * 0.8   carl      04/04/2006     Added code to load configuration data on   *
 *                                demand. Added code to load conv_type in    *
 *                                the HK_Config_Format link list.            *
 *                                Added code to check and print warning if   *
 *                                keyword is a DSC value                     *
 * 0.9   carl      07/10/2006     Added code to decode analog and digital    *
 *                                type of keywords. Added code to decode     *
 *                                SB, IL1, and IS1 types of keywords. Added  *
 *                                code to print warning when digital line    *
 *                                values do not exist for digital keyword    *
 *                                types. Added code to read APID line's      *
 *                                packet type parameter to determine if      *
 *                                packet version number is HMI or AIA ID.    *
 *                                Confirmed deallocate_hk_keyword_format()   *
 *                                function deallocates nodes.                *
 *                                Updated code to handle case when no ACON   *
 *                                data line(no coeff values) for ALG type    *
 *                                keyword which in this case set engr value  *
 *                                to zero. Formatted structures in file.     *
 * 0.91  carl      08/22/2006     Updated line in get_version_number function*
 *                                to covert hex value to decimal values for  *
 *                                the packet version number.                 *
 * 0.92  carl      01/19/2007     Updated  load_hk_values() to get raw value *
 *                                for bits stream counting bit start position*
 *                                from left-to-right or from high bits to low*
 *                                bits. Initial release counted low to high. *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h> 
#include <unistd.h>
#include <libgen.h>
#include "decompress.h"
#include "hmi_compression.h"
#include "decode_hk.h"
#include "printk.h"


/***************************** extern global variables ***********************/
extern APID_Pointer_HK_Configs *global_apid_configs;
extern GTCIDS_Version_Number*   global_gtcids_vn;
/**************************static function prototypes ************************/
static void get_version_number(unsigned short *wd_ptr, char *ptr_vn);
HK_Keywords_Format * load_hk_configs(HK_Config_Files *found_config);/*encode_hk uses-keep non-static*/
static int load_hk_values(unsigned short *word_ptr, HK_Keywords_Format *hk_keywords);
static int load_engr_values(HK_Keywords_Format *hk_keywords, HK_Keyword_t **kw_head);
static void deallocate_hk_keywords_format(HK_Keywords_Format *head);   


/**********************housekeeping telemetry decode routines*****************/
/***************************************************************************** 
 * Decode HK Keywords
 * Module Name: decode_hk_keywords 
 * Description: This is the top level function to decode housekeeping keywords.
 * Status decode_hk_keywords(): Tested and Reviewed
 *****************************************************************************/
int decode_hk_keywords(unsigned short *word_ptr, int apid, HK_Keyword_t **kw_head) 
{
#define ERRMSG(__msg__) printkerr("ERROR at %s, line %d: " #__msg__"\n",__FILE__,__LINE__);

  /* declarations */
  HK_Keywords_Format *ptr_hk_keywords;
  APID_Pointer_HK_Configs *apid_configs;
  HK_Config_Files *config_files;
  int status;
  char version_number[MAX_CHAR_VERSION_NUMBER]; 
  HK_Config_Files *matching_config;
  char *init_hdr_apid;
  char *init_hdr_pkt_version;

  /* init values */
  matching_config= (HK_Config_Files *)NULL;

  /* get environmental variables */
  init_hdr_apid= getenv("HK_INITIAL_HEADER_APID");
  init_hdr_pkt_version= getenv("HK_INITIAL_HEADER_PKT_VERSION");
  if( !init_hdr_apid || !init_hdr_pkt_version)
  {
    ERRMSG("Please set. Could not find environment variables:  <HK_INITIAL_HEADER_APID> <HK_INITIAL_HEADER_PKT_VERSION>");
    return HK_DECODER_ERROR_UNKNOWN_ENV_VARIABLE;
  }

  /* Get Version Number  from byte 8 and byte 9  bit stream for hk packet  */
  if (apid == 431) /*inital packet header is 8 bytes and called apid 431 */ 
  {
    strcpy(version_number,init_hdr_pkt_version);/*parameter version # from packet*/ 
  }
  else
  {
    get_version_number(word_ptr, version_number); 
  }

  /* check if global pointer to configuration link list exists*/
  if (!global_apid_configs )
  {
    load_all_apids_hk_configs(version_number);
  }

  /* get pointer to HK_Config_File structure based on apid # */
  apid_configs = global_apid_configs;
  while(apid_configs)   
  {
    if (apid_configs->apid == apid)  
      break; /*Found pointer apid pointer correct HK_Config_File structures */
    else
      apid_configs = apid_configs->next;
  }

  if ( apid_configs == NULL) 
  {

    printkerr("ERROR at %s, line %d: This apid <%x> does not have valid "
              "config data to decode packet. Check if config files exists "
              "for packet version number %s. "
              "If don't exist, run make_hkpdf.pl script to create files.\n",
               __FILE__, __LINE__, apid, version_number);
printf("decode_hk:  HK_DECODER_ERROR_NO_CONFIG_DATA apid:<%d>pvn:<%s>\n", apid,version_number);
 
    return HK_DECODER_ERROR_NO_CONFIG_DATA;
  }

  /*set pointer to configs to tmp pointer to get config data for this apid*/
  config_files = apid_configs->ptr_hk_configs;     

  /**************************************************************
   * Check if version number exists for hk-configurations       *
   * There are two version numbers:                             *
   * Version number for STANFORD_TLM_HMI_AIA file               *
   * Version number for parameter                               *
   **************************************************************/
  printf("APID = <%x> PACKET VERSION NUMBER = < %s >\n",apid,version_number);
  matching_config = check_packet_version_number(config_files,version_number);
  if ( matching_config == NULL )
  {
    /* Could not find matching config. Try to reload the config files. */
    printkerr("WARNING at %s, line %d: For apid <%x> and packet version number <%s> "
	      "-Cannot find config data to decode packet!!Rereading data files.\n",
              __FILE__, __LINE__, apid, version_number,version_number);
    
    config_files = reread_all_files(apid_configs, version_number);
    matching_config = check_packet_version_number(config_files,version_number);
    if ( matching_config != NULL ) 
    {
      printkerr("WARNING at %s, line %d: Found config for apid <%x> and version number <%s>\n",
                 __FILE__, __LINE__,apid, version_number);
       ;
    }
  }
  if ( matching_config == NULL )
  {
    /* Still no matching config information. Return an error code. */
    //Took out-Carl-10-12-2007:
    ERRMSG("Could not find  version number even after re-reading hk config files.");
    return HK_DECODER_ERROR_CANNOT_FIND_VER_NUM; 
  }
  /* Load hk configs in HK_Keywords_Format structure  */
  if (ptr_hk_keywords = load_hk_configs( matching_config ))
  {
    /* Load hk values in HK_Keyword_struct  */
    if (status = load_hk_values( word_ptr, ptr_hk_keywords))
      return status;
    /* set engr values and raw values in HK_Keyword_t structure which is used for Lev0 Processing*/
    if (!( status = load_engr_values(ptr_hk_keywords, kw_head)))
    {
      /* deallocate HK_Keyword_Format structure */
      deallocate_hk_keywords_format(ptr_hk_keywords);
      status = HK_DECODER_SUCCESSFUL; /*successful!!*/
      deallocate_apid_ptr_hk_config_nodes();/*check setting before doing*/
      return status;
    }
  }
  else  
  {
    ERRMSG("Could not find config data for this version number");
    status = HK_DECODER_ERROR_CANNOT_LOAD_HK_VALUES;
  }
  return status;
}/*Module:decode_hk_keywords*/


/***************************************************************************** 
 * GET VERSION NUMBER 
 * Module Name: get_version_number
 * Description: This is a utility to get version number from  pointer to hk 
 *              packet.
 * Status get_version_number(): Reviewed and Tested
 *****************************************************************************/
static void get_version_number(unsigned short *wd_ptr, char *ptr_vn) 
{
  /* declarations */
  unsigned short w;
  w = wd_ptr[7];
  /* parse high and low bytes of version number and convert from hex to decimal value */
  sprintf(ptr_vn, "%d.%d",  w & 0x00FF, w >> 8  & 0x00FF );
}

/***************************************************************************** 
 * Load Engineering Values
 * Module Name: load_engr_values
 * Status load_engr_values(): Tested and  Reviewed
 *****************************************************************************/
static int load_engr_values(HK_Keywords_Format *hk_keywords, HK_Keyword_t **kw_head)
{
  /* declarations */
  HK_Keyword_t  *kwt=NULL;
  DSC_Conversion *dsc;
  int i;
  /* Loop thru HK_Keywords_Format link list structure and find 
     first value to load */
  while(hk_keywords)
  {
    if (!kwt)
      *kw_head = kwt = (HK_Keyword_t*) malloc (sizeof (HK_Keyword_t));
    else
    {
      kwt->next = (HK_Keyword_t*) malloc (sizeof (HK_Keyword_t));
      kwt = kwt->next;
    }
    memset(kwt,0,sizeof(HK_Keyword_t));
    kwt->next = NULL;
    /* load keyword name */ 
    strcpy(kwt->fitsname, hk_keywords->keyword_name); 
    strcpy(kwt->name, hk_keywords->telemetry_mnemonic_name); 
    /* load raw value */
    kwt->raw_value = hk_keywords->keyword_value;
    /* load engr value and type  */
    if ( hk_keywords->conv_type == 'R')
    {
      if (!strcmp( hk_keywords->type , "UB"))
      {
        kwt->eng_type = KW_TYPE_UINT8;
        kwt->eng_value.uint8_val = (unsigned char)hk_keywords->keyword_value;
      }
      else if (!strcmp( hk_keywords->type , "SB"))
      {
        kwt->eng_type = KW_TYPE_INT8;
        kwt->eng_value.int8_val = (signed char)hk_keywords->keyword_value;
      }
      else if (!strcmp( hk_keywords->type , "IU1"))
      {
        kwt->eng_type = KW_TYPE_UINT16;
        kwt->eng_value.uint16_val = (unsigned short)hk_keywords->keyword_value;

      }
      else if (!strcmp( hk_keywords->type , "IS1"))
      {
        kwt->eng_type = KW_TYPE_INT16;
        kwt->eng_value.int16_val = (short)hk_keywords->keyword_value;
      }
      else if (!strcmp( hk_keywords->type , "IL1"))
      {
        kwt->eng_type = KW_TYPE_INT32;
        kwt->eng_value.int32_val = hk_keywords->keyword_value;
      }
      else if (!strcmp( hk_keywords->type , "UL1"))
      {
        kwt->eng_type = KW_TYPE_UINT32;
        kwt->eng_value.uint32_val = hk_keywords->keyword_value;
      }
      else 
      {
        printkerr("ERROR at %s, line %d: Type '%s' not handled.\n", __FILE__, __LINE__, hk_keywords->type);
        return ERROR_HK_UNHANDLED_TYPE;
      }
    }/* end-if R type */
    else if ( hk_keywords->conv_type == 'D')
    {
      if (!strcmp( hk_keywords->type,"UB") || !strcmp( hk_keywords->type ,"IU1") || !strcmp( hk_keywords->type,"UL1"))
      {
        kwt->eng_type = KW_TYPE_STRING;
        for (dsc = hk_keywords->dsc; dsc ; dsc= dsc->next)
        {
          if  (  hk_keywords->keyword_value  >= dsc->low_range &&
                 hk_keywords->keyword_value  <=  dsc->high_range )
          {
	    kwt->eng_value.string_val =(char *) malloc(sizeof(dsc->dsc_value) + 1);
            strcpy( kwt->eng_value.string_val , dsc->dsc_value);
            break;
          }
        }
        if ( !kwt->eng_value.string_val)
        {
	   kwt->eng_value.string_val =(char *) malloc(sizeof("NO_VALUE") + 1);
           strcpy( kwt->eng_value.string_val , "NO_VALUE");
           printkerr("WARNING at %s, line %d: There are no DSC data lines to set digital type keyword '%s'.\n"
                     "Setting value to NO_VALUE. Check config files and run script to update config files.\n",
                     __FILE__, __LINE__, hk_keywords->keyword_name);
        }
      }
      else 
      {
        printkerr("ERROR at %s, line %d: Type '%s' not handled.\n", __FILE__, __LINE__, hk_keywords->type);
        return ERROR_HK_UNHANDLED_TYPE;
      }
    } /* end else if D type */
    else if (hk_keywords->conv_type == 'A')
    {
      /* Calculation is as follows: 
         (1) get raw value
         (2) get number of coeffs
         (3) get each coeff value
         (4) engr value = coeff[0] * pow(raw, 0) + coeff[1] * pow(raw, 1) + .. coeff[4] * pow(raw, 4);
        */
      if (!strcmp( hk_keywords->type,"UB") || !strcmp( hk_keywords->type ,"IU1") || !strcmp( hk_keywords->type,"UL1"))
      {
        if(hk_keywords->alg)
        { 
          for (i=0; i  <  hk_keywords->alg->number_of_coeffs; i++)
          {
            kwt->eng_value.double_val += hk_keywords->alg->coeff[i] * pow(hk_keywords->keyword_value, i);
          }
        }
        else
        { /* handle case when no ACON line which contains coeff. values in config data */
          kwt->eng_value.double_val = 0;
          printkerr("WARNING at %s, line %d: Missing ACON line for keyword '%s'."
                    " Default engr.value set to zero.\n"
                    "Check config files for missing ACON lines for keyword.\n",
                     __FILE__, __LINE__, hk_keywords->keyword_name);
        }
        kwt->eng_type = KW_TYPE_DOUBLE;/*set to given type or double */
      }
      else
      {
        printkerr("ERROR at %s, line %d: Engr Type '%s' not handled.\n", __FILE__, __LINE__, hk_keywords->type);
        return ERROR_HK_UNHANDLED_TYPE;
      }
    }/* else if A type */
    else
    {
          printkerr("ERROR at %c, line %d: Conversion Type '%s' not handled.\n", __FILE__, __LINE__, hk_keywords->conv_type);
          return ERROR_HK_UNHANDLED_TYPE;
    }
    hk_keywords = hk_keywords->next;
  } /*while-loop*/
  return HK_SUCCESSFUL; /* successful*/
}



/***************************************************************************** 
 * LOAD HK CONFIGS
 * Module Name: load_hk_configs
 * Description: This is the top level function to decode housekeeping keywords.
 *              This function loads in config data for decoding raw value and
 *              engr. values.
 * Status load_hk_configs(): updated code- not Tested and Reviewed
 *****************************************************************************/
HK_Keywords_Format *load_hk_configs(HK_Config_Files *config) 
{
  HK_Keywords_Format *new_kw, *new_kw_head;
  Keyword_Parameter *config_kw;
  DSC_Conversion *config_dsc, *prev_new_dsc, *new_dsc;
  ALG_Conversion *config_alg, *new_alg;
  int i;
  /* Initialized variables */
  config_kw=config->keywords;
  /* Allocate memory to HK_Keywords_Format structure */
  if (config_kw == NULL)      
  {
    ERRMSG("Null pointer input.");
    return NULL;
  }
  /* load values to HK_Keywords_Format structure while not null */
  new_kw_head = new_kw = NULL;
  while (config_kw)  
  {
    if (!new_kw)
      new_kw = new_kw_head = malloc(sizeof(HK_Keywords_Format));
    else
    {
      new_kw->next = malloc(sizeof(HK_Keywords_Format));
      new_kw = new_kw->next;
    }
    new_kw->next = NULL;
    /* Load config values */
    strcpy(new_kw->keyword_name, config_kw->keyword_name);
    /*for debugging only */
    strcpy(new_kw->telemetry_mnemonic_name,
	   config_kw->telemetry_mnemonic_name);
    /* for parsing later */
    new_kw->start_byte = config_kw->start_byte;  
    new_kw->start_bit = config_kw->start_bit_number;  
    /* for parsing later */
    strcpy( new_kw->type, config_kw->type);
    /* for parsing later */
    new_kw->bit_length = config_kw->bit_length;
    new_kw->conv_type = config_kw->conv_type;
    /* get DSC values for parsing later and setting engr values */
    if( config_kw->conv_type == 'D')
    {
      new_kw->dsc = (DSC_Conversion*)NULL;/*ADDED 6-26-2006 */
      config_dsc=config_kw->dsc_ptr;
      prev_new_dsc=NULL;
      while (config_dsc)
      {
        /* Set low and high range and string value */
        new_dsc=(DSC_Conversion*) malloc(sizeof(DSC_Conversion));
        if (!prev_new_dsc) 
        {
          new_kw->dsc= new_dsc;
        }
        new_dsc->low_range = config_dsc->low_range;
        new_dsc->high_range = config_dsc->high_range;
        strcpy(new_dsc->dsc_value,config_dsc->dsc_value);
        new_dsc->next = (DSC_Conversion *)NULL;
        if(prev_new_dsc)
        {
          prev_new_dsc->next = new_dsc;
        }
        /* Go to next node for values */
        config_dsc= config_dsc->next;
        prev_new_dsc=new_dsc; 
        new_dsc= new_kw->dsc->next;
      }
    }
    else
    {
       new_kw->dsc = (DSC_Conversion*)NULL;
    }
    /* get ALG values for setting engr values */
    if( config_kw->conv_type == 'A')
    {
      config_alg=config_kw->alg_ptr;
      if (config_alg)
      {
        /* create node & set low and high range and string value */
        new_alg = (ALG_Conversion*) malloc(sizeof(ALG_Conversion));
        new_kw->alg= new_alg;
        new_alg->number_of_coeffs = config_alg->number_of_coeffs;
        for(i=0; i < new_alg->number_of_coeffs; i++)
        {
          new_alg->coeff[i] = config_alg->coeff[i];
        }
      }
      else 
      {
        /* set to null -missing ACON line case */
        new_kw->alg= (ALG_Conversion*)NULL;
      }

    }
    else
    {
       new_kw->alg = (ALG_Conversion*)NULL;
    }

    /* Go to next keyword-parameter link list 
       node in HK_Config_Files structure */
    config_kw = config_kw->next;
  }
  return new_kw_head;
} 



/***************************************************************************** 
 * Load HK Values
 * Module Name: load_hk_values
 * Description: This function load housekeeping keywords from input data stream.
 * Status load_hk_values():Review and tested
 *****************************************************************************/
static int load_hk_values(unsigned short *word_ptr, HK_Keywords_Format *ptr_hk_keywords)    
{
  /* declarations */
  HK_Keywords_Format *top_hk_keywords;
  unsigned int *w;
  unsigned char *byte_ptr;
  unsigned int keep_bits; /* 32 bits */

  /* initialized variables */
  top_hk_keywords = ptr_hk_keywords;

  /* Initialize variable */
  byte_ptr= (unsigned char*)(word_ptr+3);

  /* Loop  thru HK_Keywords_Format link list structure and find  first value to load */
  while ( ptr_hk_keywords != NULL )  
  {
    if(!strcmp(ptr_hk_keywords->type, "UB") || !strcmp(ptr_hk_keywords->type, "SB") ) 
    {
      /* set keep bits to zero */
      keep_bits=0;
      /* get raw value */
      w = (unsigned int*) &(byte_ptr[ptr_hk_keywords->start_byte]);
      /* calculate keep bits mask to use*/
      keep_bits = (unsigned int) (pow( 2, ptr_hk_keywords->bit_length) - 1) ;
      /* adjust based on bit length  and bit position */
      ptr_hk_keywords->keyword_value = (unsigned  char)(( *w >> (8 - (ptr_hk_keywords->start_bit + ptr_hk_keywords->bit_length))) & keep_bits);
    } 
    else if( (!strcmp(ptr_hk_keywords->type, "IS1") || !strcmp(ptr_hk_keywords->type, "IU1")) && ptr_hk_keywords->bit_length <= 16) 
    {
      /* set keep bits to zero */
      keep_bits=0;
      /* get raw value */
      w = (unsigned int*) &(byte_ptr[ptr_hk_keywords->start_byte]);
      /* calculate keep bits mask to use*/
      keep_bits = (unsigned int) (pow( 2, ptr_hk_keywords->bit_length) - 1) ;
      /* set 0th to 7th bits */
      ptr_hk_keywords->keyword_value =  (unsigned  short int) (( (*w  ) >> 8) & 0x00FF );
      /* set 8th to 15th bits */
      ptr_hk_keywords->keyword_value |= (unsigned  short int) (( (*w ) << 8 ) & 0xFF00 );
      /* adjust based on bit length  and bit position */
      ptr_hk_keywords->keyword_value =  (unsigned int) (( ptr_hk_keywords->keyword_value >> (16 - (ptr_hk_keywords->start_bit + ptr_hk_keywords->bit_length))) &  keep_bits);
    }
    else if (!strcmp(ptr_hk_keywords->type, "UL1") || !strcmp(ptr_hk_keywords->type, "IL1")) 
    {  
      /* set keep bits to zero */
      keep_bits=0;
      /* calculate keep bits mask to use*/
      keep_bits = (unsigned int) (pow( 2, ptr_hk_keywords->bit_length ) - 1 ) ;
      /* get raw value */
      w = (unsigned int*) &(byte_ptr[ptr_hk_keywords->start_byte]);
      /* set 0th to 7th bits */
      ptr_hk_keywords->keyword_value =	(unsigned int) ( (*w) >> 24 & 0x000000FF);
      /* set 8th to 15th bits */
      ptr_hk_keywords->keyword_value |=	(unsigned int) ( (*w) >> 8 & 0x0000FF00);
      /* set 24th to 31th bits */
      ptr_hk_keywords->keyword_value |=	(unsigned int) ( (*w) << 24 & 0xFF000000);
      /* set 16th to 23th bits */
      ptr_hk_keywords->keyword_value |=	(unsigned int) ( (*w) << 8 & 0x00FF0000);
      /* adjust for bit length */
      ptr_hk_keywords->keyword_value =  (ptr_hk_keywords->keyword_value >> (32 - (ptr_hk_keywords->start_bit + ptr_hk_keywords->bit_length ))) & keep_bits ; 
    }                 
    else  
    {
      printkerr("ERROR at %s, line %d: Did not find this bit length for keyword %s\n",
                 __FILE__, __LINE__, ptr_hk_keywords->keyword_name );
      //return ERROR_HK_INVALID_BITFIELD_LENGTH;
    }
  ptr_hk_keywords = ptr_hk_keywords->next;           
  } 
  ptr_hk_keywords= top_hk_keywords;
  return (HK_SUCCESSFUL); /*no errors */
} /*END-module :load_hk_values()*/


/***************************************************************************** 
 * DeAllocate HK Keywords Format
 * Module Name: deallocate_hk_keywords_format()
 * Description: This functions deallocates keywords-nods in
 *              HK_Keywords_Format link list structures.
 * Status deallocate_hk_keywords_format():  Tested and Reviewed 6-28-2006
 *****************************************************************************/
static void deallocate_hk_keywords_format(HK_Keywords_Format *head)   
{
  HK_Keywords_Format *tmp;
  DSC_Conversion *d_tmp, *d_head;

  while(head)
  {
    /*clear dsc nodes*/
    d_head=head->dsc;
    while (d_head)
    {
      d_tmp=d_head->next;
      free(d_head);
      d_head= d_tmp;
    }

    /*clear alg nodes*/
    if (head->alg)
    {
      free(head->alg);    
    }

    /*clear HK_Keyword_Format nodes */
    tmp = head->next;
    free(head);
    head = tmp;
  }
}

/***************************************************************************** 
 * DeAllocate HK Keyword_t link list structure 
 * Module Name: deallocate_hk_keywords_t
 * Description: This functions deallocates keywords-nodes in
 *              HK_Keywords_t Structure.
 * Status deallocate_hk_keyword_t(): Tested and Reviewed. 
 *****************************************************************************/
void deallocate_hk_keyword(HK_Keyword_t *head)   
{
  HK_Keyword_t *tmp;

  while(head)
  {
    tmp = head->next;
    if (head->eng_type == KW_TYPE_STRING && head->eng_value.string_val)
    {
      free(head->eng_value.string_val);
    }
    free(head);
    head = tmp;
  }
}



/***************************************************************************** 
 * COPY HK Keywords 
 *
 *****************************************************************************/
HK_Keyword_t *copy_hk_keywords(HK_Keyword_t *head)   
{
  HK_Keyword_t *newhead, *p;

  if (head)
  {
    assert(newhead = malloc(sizeof(HK_Keyword_t)));
    p = newhead;
    memcpy(p, head, sizeof(HK_Keyword_t));
    if (head->eng_type == KW_TYPE_STRING)
      p->eng_value.string_val = strdup(head->eng_value.string_val);
    head = head->next;
    while(head)
    {      
      assert(p->next = malloc(sizeof(HK_Keyword_t)));
      p = p->next;
      memcpy(p, head, sizeof(HK_Keyword_t));
      if (head->eng_type == KW_TYPE_STRING)
	p->eng_value.string_val = strdup(head->eng_value.string_val);
      head = head->next;      
    }   
  }
  else
    newhead = NULL;

  return newhead;
}
