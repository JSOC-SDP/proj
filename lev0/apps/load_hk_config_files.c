/*****************************************************************************
 * Filename: load_hk_config_files.c                                          *
 * Author: Carl Cimilluca                                                    *
 * Create Date: August, 5, 2005                                              *
 * Description: This file contains modules to load from files the            *
 *               housekeeping data into memory and into a link list data     *
 *               structure for modules in file decode_hk.c to use to decode  *
 *                housekeeping data from the stream of packets from a EGSE   *
 *               telemetry simulator or from DDS                             *
 * Global Variables:                                                         *
 * ----------------------   ------------------------- ---------------------  *
 * Type                     Name                       Description           *
 * ----------------------   ------------------------- ---------------------  *
 * APID_Pointer_HK_Configs* global_apid_configs        Top pointer to the    *
 *                                                     link list used to hold*
 *                                                     configuration data    *
 *                                                     Used in 4 places to   *
 *                                                     make deallocate easier*
 *                                                     from other functions  *
 *                                                     to make it easier for *
 *                                                     decode_hk functions to*
 *                                                     use data in list      *
 * GTCIDS_Version_Number*   global_gtcids_vn           Top pointer to the    *
 *                                                     link list used to hold*
 *                                                     the Ground to code ids*
 *                                                     file's file-version-  *
 *                                                     number and packet-    *
 *                                                     version-number data   *
 *                                                     pairs. Use in 4 places*
 *                                                     to make deallocate    *
 *                                                     easier from other     *
 *                                                     functions.            *
 * (C) Stanford University, 2005                                             *
 * Revision History:                                                         *
 * ------- ------  -------------  ----------------------------------------   *
 * Draft/   Who    Date           Description                                *
 * Revision                                                                  *
 * ------- ------  -------------  ----------------------------------------   *
 * 0.0     Carl      8/5/2005     Created                                    *
 * 0.1     Carl      10/19/2005   Pass draft 0 revision 1 to Rasmus to       *
 *                                integrate with L0 code                     *
 * 0.2     Carl      10/31/2005   Pass back code after Rasmus did a first try*
 *                                at integrating code with L0 code.          *
 *                                Add reread_all_files function which re-    *
 *                                reads all config files when a configuration*
 *                                data for version number does not exist.    *
 *                                Updated code to load version number in     *
 *                                structures as string buffer rather than a  *
 *                                float.                                     *
 * 0.3   Rasmus/     11/18/2005   Almost completely re-written so Rasmus'    *
 *       Carl                     eyes don't bleed so much when he tries to  *
 *                                read it.                                   * 
 * 0.4   Rasmus      2005/11/28   Changed names of global variables for no   *
 *                                apparent reason.                           *
 * 0.5   Carl        11/29/2005   Also added fix for APID 431. Now can read  *
 *                                and process packets with APID 431.         *
 *                                Checked in to integrate with Jim and Rasmus*
 *                                code.                                      *
 * 0.6   Carl        11/30/2005   Added code to load in telemetry_name values*
 *                                Added code to read in the number apids to  *
 *                                to process based on files present in       *
 *                                HK_CONFIG_DIRECTORY                        *
 * 0.7   Carl        01/19/2006   Added code to load "real" Ground TO CODE   *
 *                                IDS file. Set next value in the GTCIDS_    *
 *                                Version_Number structure to null.          *
 *                                Added code to load new format for apid-#-  *
 *                                version-# files with KWD, DCON, ACON       *
 *                                reserve words in file.                     *
 * 0.8   Carl        01/19/2006   Added additional error message when cannot *
 *                                open file like cvs version 1.6. Also did   *
 *                                exit.                                      *
 * 0.9   Carl        01/25/2006   Add default path for GTCIDS file directory *
 *                                 ../../tables/hk_config_file               *
 * 0.10  Carl        03/23/2006   Currently apid-#-version-# files are read  *
 *                                into memory during initialization. This    *
 *                                updated code will read in apid-#-version-# *
 *                                files on demand. Removed some comments from* 
 *                                from code that did not make sense.         *
 *                                Rewrote reread function.                   *
 * 0.11  Carl        07/10/2006   Updated load of apid line for loading      *
 *                                packet_id_type to be used to determine     *
 *                                which packet version number to use in      *
 *                                GROUND_to_CODE_ids.txt file.               *
 *                                Added functions to load, create and free   *
 *                                nodes for  ALG and DSC  keywords data from *
 *                                ACON and DCON lines in the apid-#-version-#*
 *                                files. Added feature to check to clear     *
 *                                in-memory config data structure when file  *
 *                                exists HK_FREE_CONFIG_FLAG_FILE in         *
 *                                directory  ../../table/hk_config_data.     *
 *                                This ensures memory can be cleared while   *
 *                                process is running.                        *
 * 0.12  Carl        07/10/2006   Updated breaks to format file and added    *
 *                                comment                                    *
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
#include "printk.h"
#include "decode_hk.h"
#include <dirent.h>

#define LOAD_HK_C
#include "load_hk_config_files.h"
#undef LOAD_HK_C


/***************************** function prototypes ****************************/
APID_HKPFD_Files * allocate_apid_hkpfd_files_nodes(int *ptrFirstTimeFlag);
void load_gtcids_data( GTCIDS_Version_Number* top_ptr_gtcids_data,
                       APID_Pointer_HK_Configs *top_apid_ptr_hk_configs);
void deallocate_apid_hkpfd_files_nodes(APID_HKPFD_Files *top_ptr_apid_hkpfd_files_ptr) ;
char * find_file_version_number(GTCIDS_Version_Number *top,char f_version_number[]);
/***************************** global variables ******************************/
APID_Pointer_HK_Configs *global_apid_configs;
GTCIDS_Version_Number *global_gtcids_vn;



/**********housekeeping load configuration and formats routines***************/
/***************************************************************************** 
 * Load All APIDS HK Configs
 * Module Name: 
 * Description: This is the top level function to decode housekeeping keywords.
 * Status load_all_apids_hk_configs(): Tested
 *****************************************************************************/
int load_all_apids_hk_configs(char version_number[])    
{
  /*  declarations  */
  APID_Pointer_HK_Configs *apid_ptr_hk_configs;
  APID_Pointer_HK_Configs *top_apid_ptr_hk_configs;
  APID_HKPFD_Files *top_apid_hkpfd_files; 
  APID_HKPFD_Files *hkpfd_files;
  GTCIDS_Version_Number *top_ptr_gtcids_data;
  int apid_array[MAX_APID_POINTERS];
  int number_of_apids, i;
  char file_version_number[50];
  char *ptr_fvn;
  //HK_Config_Files *hk_configs;  /* used to print results which is commented out*/
  // Keyword_Parameter *hk_keyword;/* used to print results which is commented out*/

  /* Init parameter */
  ptr_fvn=file_version_number;
  /* Load data from ground to code ids file */
  top_ptr_gtcids_data = read_gtcids_hk_file(top_apid_ptr_hk_configs); 
  /* Check for file version number in ground to code ids file */
  ptr_fvn=find_file_version_number(top_ptr_gtcids_data, version_number);
  /* load HK_Config_Files structures for each apid */
  if ((top_apid_hkpfd_files = read_all_hk_config_files(ptr_fvn)) == NULL)
    return ERROR_HK_NOSUCHDIR;
  /* get list of unique apids to read and allocate space for*/
  number_of_apids = 0;
  memset(apid_array, 0, sizeof(apid_array));
  for(hkpfd_files=top_apid_hkpfd_files; hkpfd_files; 
      hkpfd_files=hkpfd_files->next)
  {
    /* See if we already have this apid. */
    for (i=0; i<number_of_apids; i++)
    {
      if (apid_array[i] == hkpfd_files->apid)
        break;
    }
    if (i >= number_of_apids) \
    {
      /* Nope this was a new one. Insert it in the list. */
      apid_array[i]= hkpfd_files->apid;
      number_of_apids++;
    }
  }
  /* load apid pointer to HK Configs structure apid-value and allocate nodes */  
  for ( i=0; i < number_of_apids; i++)     
  {
    if ( i == 0)    
    { 
      /* create and load first node */
      apid_ptr_hk_configs = (APID_Pointer_HK_Configs *)malloc(sizeof(APID_Pointer_HK_Configs));
      top_apid_ptr_hk_configs = apid_ptr_hk_configs;
    }
    else    
    { 
      /* create and load next node */
      apid_ptr_hk_configs->next = (APID_Pointer_HK_Configs *)malloc(sizeof(APID_Pointer_HK_Configs));
      apid_ptr_hk_configs = apid_ptr_hk_configs->next;      
    } 
    apid_ptr_hk_configs->apid = apid_array[i];
    apid_ptr_hk_configs->next= NULL;
    apid_ptr_hk_configs->ptr_hk_configs= (HK_Config_Files*)NULL;
  }             
  /* set moving pointer to top of list */
  apid_ptr_hk_configs = top_apid_ptr_hk_configs;
  for(i=0; i < number_of_apids; i++)  
  {
    load_config_data( top_apid_hkpfd_files, apid_ptr_hk_configs);
    apid_ptr_hk_configs = apid_ptr_hk_configs->next;
  }  /*End for*/
  /* load file_version_number based on packet version number */
  load_gtcids_data(top_ptr_gtcids_data, top_apid_ptr_hk_configs);
  /* set global variable to use for decode_hk modules */
  global_apid_configs = top_apid_ptr_hk_configs;
  /* deallocate  top_apid_hkpfd_files link list */
  deallocate_apid_hkpfd_files_nodes(top_apid_hkpfd_files) ;
/******PRINT RESULTS **********/
/***
for (apid_ptr_hk_configs=global_apid_configs; apid_ptr_hk_configs;
     apid_ptr_hk_configs=apid_ptr_hk_configs->next)
{
   if( apid_ptr_hk_configs->apid == 445)
   {
      printf("APID_Pointer_HK_Configs apid is %d\n", apid_ptr_hk_configs->apid);
      for (hk_configs=apid_ptr_hk_configs->ptr_hk_configs; hk_configs;
            hk_configs=hk_configs->next)
      {
          printf("HK_Configs file version number  is %s\n", hk_configs->file_version_number );
          if (!strcmp(hk_configs->file_version_number, "1.38"))
          {
            printf("HK_Configs file version number  is %s\n", hk_configs->file_version_number );
            for ( hk_keyword = hk_configs->keywords;hk_keyword;  
                  hk_keyword = hk_keyword->next)
            {
              printf("Keywords  is %s\n", hk_configs->keywords->keyword_name );
            }
          }
      }
   }
}
***/
/******END PRINT RESULTS*******/
  return HK_SUCCESSFUL;  
}/*End Module: load_all_apids_hk_configs */



/***************************************************************************** 
 * Load Config Data
 * Module Name:  load_config_data
 * Description: This module uses filenames loaded in structure to open files
 *              and read data in HK_Config_Files structure. This includes 
 *              reading in apid and version # and all keyword values.
 * Status load_config_data(): Tested and Reviewed
 *****************************************************************************/
void load_config_data(APID_HKPFD_Files *hkpfd_files,
		      APID_Pointer_HK_Configs *hk_configs)  
{
  /*declarations*/
  APID_HKPFD_Files* p;
  int apid;
  char filename[MAX_FILE_NAME];
  FILE* file_ptr; 

  /*intialized variables*/
  int err_status=0;

  /* FOR LOOP through all files in directory */
  apid = hk_configs->apid;
  p = hkpfd_files;
  while(p)
  {
    if ( p->apid == apid )
    {
      sprintf(filename,"%s/%s",p->directory_name, p->filename);
      file_ptr = fopen( filename ,"r");
      err_status = save_hdpf_new_formats(file_ptr, hk_configs);
      fclose(file_ptr);
    }
    p = p->next;
  }    
}

/***************************************************************************** 
 * Load Filenames From Directory
 * Module Name:  load_filenames_from_directory
 * Description: This module opens a directory and loads all apid-<apid#>-
 *              version<version#>.txt files and GTCIDS file. Assume all
 *              files will be in one directory called for now 
 *              .../HK-CONFIG-FILES.
 * Status load_filenames_from_directory(): Tested
 *****************************************************************************/
APID_HKPFD_Files* read_all_hk_config_files(char f_version_number[])
{
  /*declarations */
  char dirname[200];
  char *dn;
  APID_HKPFD_Files *top, *p; 
  DIR *dir_p;
  struct dirent *dir_entry_p;

  /* intialize variables */
  int file_loaded_flag=0;
  memset(dirname, 0x0, sizeof(dirname));

  /* get directory name */
  dn = getenv("HK_CONFIG_DIRECTORY");
  if(dn == NULL) 
  {
    printkerr("Error at %s, line %d: Could not get directory environment\n"
              "variable:<HK_CONFIG_DIRECTORY>. Set the env variable "
	      "HK_CONFIG_DIRECTORY to point to the config file directory.\n",
              __FILE__,__LINE__,dn);
    return NULL;
  }
  /* Add file version number to directory path */
  strcpy( dirname, dn);
  strcat(dirname, f_version_number);
  strcat(dirname,"\0");

  /* open directory */
  if ((dir_p = opendir(dirname)) == NULL)
  {
    printkerr("Error at %s, line %d: Could not open directory <%s>. "
	      "The directory with < %s > version number does not exist. "
	      "Run make_hkpdf.pl script to create directory and config files.\n",
              __FILE__,__LINE__,dirname, f_version_number);
    return NULL;
  }
  /* read each entry until NULL.*/
  top = NULL;
  while( (dir_entry_p = readdir(dir_p)) != NULL ) 
  {
    if( strncmp(dir_entry_p->d_name,"apid",4) )
      continue; /* not an APID config file - skip*/
    if( top == NULL )   
      top = p = malloc(sizeof(APID_HKPFD_Files));
    else                
    {
      p->next = malloc(sizeof(APID_HKPFD_Files));
      p = p->next;
    } 
    p->next = NULL;
    /* load dir and filename in structure link list */
    strcpy(p->filename, dir_entry_p->d_name);
    strcpy(p->directory_name, dirname);
    /* parse filename to get apid and version number*/
    sscanf(p->filename, "%*4s-%x-%*7s-%d",
	   &p->apid, p->version_number); 
    file_loaded_flag=1;
  }
  closedir(dir_p);
  /* check if at least one file loaded and exists in directory*/
  if ( !file_loaded_flag) 
  {
    /*Carl-10-12-2007:Took out too many messages in log
    printkerr("Error at %s, line %d: Could not find config file(s) "
	      "in directory < %s >. Check if file(s) exists for "
              "file version number <%s>, if don't exist, then "
              "run make_hkpdf.pl script to create config files.\n",
               __FILE__,__LINE__,dirname, f_version_number);
    */
  }
  return top;
}/* END-Module: load_hk_filenames_from_directory  */



/***************************************************************************** 
 * save New HDPF Formats
 * Module Name: save_hdpf_new_formats
 * Description: 
 *   This is the top level function to decode housekeeping keywords.
 * Parameters Passed: 
 *   File* file_ptr = Reference pointer to 
 *   HK_Config_Files *ptr_config_node= Reference pointer
 *   HK_Config_Files *top_ptr_config_node= Reference pointer
 * Functions Called: 
 * Status save_hdpf_new_formats(): Tested
 *
 *****************************************************************************/
int save_hdpf_new_formats(FILE* file_ptr,APID_Pointer_HK_Configs *p_apid_ptr_hk_configs) 
{
  /* declarations */
  int err_status[3];
  HK_Config_Files *ptr_config_node;
  HK_Config_Files *previous_ptr_config_node;
  int status;
  int i, j, k;
  char line[MAXLINE_IN_FILE];
  char keyword_lines[MAX_NUM_KW_LINES][MAXLINE_IN_FILE];
  char acon_lines[MAX_NUM_ACON_LINES][MAXLINE_ACON_IN_FILE];
  char dcon_lines[MAX_NUM_DCON_LINES][MAXLINE_DCON_IN_FILE];

  /*Assume format contents of file using functional spec formats */
  /* Check if first node exists*/
  if (p_apid_ptr_hk_configs->ptr_hk_configs == NULL)   
  {
    /* create first HK Config node */
    p_apid_ptr_hk_configs->ptr_hk_configs =malloc( sizeof(HK_Config_Files));
    ptr_config_node = p_apid_ptr_hk_configs->ptr_hk_configs;
    ptr_config_node->next = NULL; 
    ptr_config_node->keywords = NULL;
  } /*END-if first node exists */
  else    
  {
    /* search for new null node in HK_Config Link list */
    for(ptr_config_node = p_apid_ptr_hk_configs->ptr_hk_configs,
	previous_ptr_config_node = ptr_config_node;
	ptr_config_node;
	previous_ptr_config_node = ptr_config_node,
	ptr_config_node = ptr_config_node->next) 
    {
      ;/* continue loop*/
    }         
    /* create next node */
    ptr_config_node = malloc( sizeof(HK_Config_Files));
    previous_ptr_config_node->next = ptr_config_node;
    ptr_config_node->next = NULL;
    ptr_config_node->keywords = NULL;
  }
  /* load keyword, dsc, or alg lines in hk configuration structures */
  for(i=0, j=0, k=0; fgets(line, MAXLINE_IN_FILE, file_ptr) != NULL; ) 
  {
    if (!strncmp(line, "#", 1))
    {
      continue; /*skip comments */
    }
    else if (!strncmp(line, "KWD", 3))
    {
      strcpy( keyword_lines[i++], line);
    }
    else if (!strncmp(line, "DCON", 4))
    {
      
      if(j >= MAX_NUM_DCON_LINES)
      {
        printkerr("WARNING: Maximum lines for array exceeded.\n"
                  "         Skipping saving DCON config data since array too small.\n"
                  "         Update define variable MAX_NUM_DCON_LINES. \n");
      }
      else
      {
        strncpy( dcon_lines[j++], line, MAXLINE_DCON_IN_FILE);
      }
    }
    else if (! strncmp(line, "ACON", 4))
    {
      if(k >= MAX_NUM_ACON_LINES)
      {
        printkerr("WARNING: Maximum lines for array exceeded.\n"
                  "         Skipping saving ACON config data since array too small.\n"
                  "         Update define variable MAX_NUM_ACON_LINES. \n");
      }
      else
      {
        strcpy( acon_lines[k++], line);
      }
    }
    else if (!strncmp(line, "FILE", 4))
    {
      sscanf(line," %*s %*s  %s ", ptr_config_node->file_version_number);
    }
    else if (!strncmp(line, "APID", 4))
    {
      if( strstr(line, "HMI")  )
      {
        sscanf( line,"%*s %x %d %s %s", 
                &(ptr_config_node->apid_number), 
                &(ptr_config_node->number_bytes_used), 
                ptr_config_node->packet_id_type, 
                ptr_config_node->date);
      }
      else if( strstr(line, "AIA") )
      {
        sscanf( line,"%*s %x %d %s %s", 
                &(ptr_config_node->apid_number), 
                &(ptr_config_node->number_bytes_used), 
                ptr_config_node->packet_id_type, 
                ptr_config_node->date);
      }
      else if( strstr(line, "SSIM") )
      {
        sscanf( line,"%*s %x %d %*s %s", 
                &(ptr_config_node->apid_number), 
                &(ptr_config_node->number_bytes_used), 
                ptr_config_node->date);
        /* set to default HMI */
        strcpy(ptr_config_node->packet_id_type,"HMI");
      }
      else
      {
        /* Backward compatible case for apid-#-version-# created*
         * using older(before 6-30-2006) make_hpf.pl script     */ 
        sscanf( line,"%*s %x %d %s", 
                &(ptr_config_node->apid_number), 
                &(ptr_config_node->number_bytes_used), 
                ptr_config_node->date);
        /* set to default HMI */
        strcpy(ptr_config_node->packet_id_type,"HMI");
      }
    }
    else
    {
      printkerr("WARNING: Could not find line with this keyword in apid-#-version-# file. Possible bad data in STANFORD file.\n");
      printkerr("WARNING: Line tried to parse is < %s >\n", line);
    }
  }
  /* Set to null the end of array*/
  strcpy(keyword_lines[i],"");
  strcpy(dcon_lines[j],"");
  strcpy(acon_lines[k],"");
  /*Load keyword, alg and dsc lines values in configuration structures */
  err_status[0] = load_hdpf_keyword_lines(keyword_lines,i,ptr_config_node);
  err_status[1] = load_hdpf_dsc_lines(dcon_lines,j,ptr_config_node);  
  err_status[2] = load_hdpf_alg_lines(acon_lines,k,ptr_config_node);  
  /* return values */
  /* return top_hk_config_nodes by setting value if first node*/
  /* return error status when do error checks. Where
     status 0 = no errors and  status 1 equals errors */
  for ( i=0; i < sizeof(err_status); i++)  
  {
    if ( err_status[i] != ERROR_LOADING_HK_DATA )  
      continue;
    else 
      status= err_status[i];
  }/* END-for */
  return status;
}


/***************************************************************************** 
 * Create Memory Load HDPF keyword lines 
 * Module Name: load_hdpf_keyword_lines 
 * Description: This function loads the keyword config data in HK_Config_Files 
 *              structure's Keyword_Parameter link list nodes.
 * Status load_hdpf_keyword_lines(): Tested
 *****************************************************************************/
int load_hdpf_keyword_lines(char keyword_lines[MAX_NUM_KW_LINES][MAXLINE_IN_FILE],int number_of_lines,HK_Config_Files *ptr_hk_config_node) 
{
  /* declarations */
  Keyword_Parameter *keyword_node;
  int k=0;

  /*create space for keyword_parameter link list nodes */
  keyword_node= create_hdpf_keyword_nodes( ptr_hk_config_node, number_of_lines );
  /* load keyword values in Keyword_Parameter nodes */
  for(k=0;  keyword_lines[k][0] != '\0'; 
      k++,keyword_node=keyword_node->next) 
  {
    sscanf(keyword_lines[k]," %*s %s %s %d %d %d %s %c %*s",
	   keyword_node->keyword_name,
	   keyword_node->telemetry_mnemonic_name,
	   &(keyword_node->start_byte),
	   &(keyword_node->start_bit_number),
	   &(keyword_node->bit_length),
	   keyword_node->type,
	   &keyword_node->conv_type);
    keyword_node->dsc_ptr = (DSC_Conversion *)NULL;
    keyword_node->alg_ptr = (ALG_Conversion *)NULL;
  }
  return HK_SUCCESSFUL;
} /*END-module: load_hdpf_keyword_lines */


/***************************************************************************** 
 * Load HDPF dsc lines 
 * Module Name: load_hdpf_dsc_lines 
 * Description: This function loads the dsc config data in HK_Config_Files 
 *              structure's Keyword_Parameter link list nodes.
 * Status load_hdpf_dsc_lines(): entered code & Need to test and Review
 *****************************************************************************/
int load_hdpf_dsc_lines(char dsc_lines[MAX_NUM_DCON_LINES][MAXLINE_DCON_IN_FILE ], int number_of_lines, HK_Config_Files *ptr_hk_config_node) 
{
  /* declarations */
  DSC_Conversion *dsc_node;
  int i=0;
  char tlm_name[HK_MAX_TLM_NAME];
  Keyword_Parameter *kw;

  /* loop throught each of the telemetry names in keywords node link list 
     with conv type equal to D. Search for line in array with telemetry 
     name equal to keyword node in link list. When found, create DSC node 
     and load values from line. 
   */
  for ( kw= ptr_hk_config_node->keywords ; kw; kw=kw->next)
  {
    /* find D type KWDs */
    if ( kw->conv_type == 'D')
    {
      kw->dsc_ptr=(DSC_Conversion*)NULL;
      /* find line with tlm name the same as Keywords tlm name */
      for (i=0; i < number_of_lines; i++)
      {
        sscanf ( dsc_lines[i], "%*s  %s  %*d  %*d  %*d  %*d  %*d  %*d", tlm_name);
        strcat( tlm_name, "");
        if( !strcmp( kw->telemetry_mnemonic_name, tlm_name))
        {
          /* create dsc node memory */
          dsc_node  = create_hdpf_dsc_nodes( kw);
          /* load values in dsc node*/
          sscanf(dsc_lines[i]," %*s %*s %d %d %s %*s", 
            &(dsc_node->low_range), &(dsc_node->high_range),
            dsc_node->dsc_value); 
          dsc_node->next = NULL; /*set link list to null at end node*/
        } /* end of if tlm name equal */
      } /* end of for number of lines */
    } /* end of if D type keyword */
  } /* end of for each keyword */
  return 0;
} /*END-module: load_hdpf_dsc_lines */



/***************************************************************************** 
 * Create Memory Load HDPF alg lines 
 * Module Name: load_hdpf_alg_lines 
 * Description: This function loads the keyword config data in HK_Config_Files 
 *              structure's Keyword_Parameter link list node which points to 
 *              ALG_Conversion node.
 * Status load_hdpf_alg_lines():Reviewed and Tested.
 *****************************************************************************/
int load_hdpf_alg_lines(char alg_lines[MAX_NUM_ACON_LINES][MAXLINE_ACON_IN_FILE],int number_of_lines,HK_Config_Files *ptr_hk_config_node) 
{
  /* declarations */
  ALG_Conversion *alg_node;
  int i, k;
  char tlm_name[HK_MAX_TLM_NAME];
  Keyword_Parameter *kw;

  /* Loop throught each of the telemetry names in keywords node link list  
     with conv type equal to A.Search for line in array with telemetry 
     name equal to keyword node in link list. When found, create ALG node
     and load values from liner.
    */
  for ( kw= ptr_hk_config_node->keywords ; kw; kw=kw->next)
  {
    /* find "A" type KWDs */
    if ( kw->conv_type == 'A')
    {
      /* find line with tlm name the same as Keywords tlm name */
      for (i=0; i < number_of_lines; i++)
      {
        /* get tlm name from line */
        sscanf ( alg_lines[i], "%*s  %s  %*d  %*d  %*d  %*d  %*d  %*d  %*d", tlm_name);
        strcat( tlm_name, "");
        if( !strncmp( kw->telemetry_mnemonic_name, tlm_name, sizeof(tlm_name)))
        {
          /* create alg node memory */
          kw->alg_ptr = (ALG_Conversion*)malloc (sizeof (ALG_Conversion));
          alg_node= kw->alg_ptr;

          /* load values in alg node-assume number of coeffs to be 5*/
          k=0;
          sscanf(alg_lines[i]," %*s %*s %d %lf %lf %lf %lf %lf %*lf %*s",
            &(alg_node->number_of_coeffs), &(alg_node->coeff[k++]),
            &(alg_node->coeff[k++]), &(alg_node->coeff[k++]),
            &(alg_node->coeff[k++]), &(alg_node->coeff[k]) );
          break;
        } /* end of if tlm name equal */
      } /* end of for number of lines */
    } /* end of if "A" type keyword */
  } /* end of for each keyword */
  return HK_SUCCESSFUL;
} /*END-module: load_hdpf_alg_lines */



/***************************************************************************** 
 * Create (Memory) HDPF keyword Nodes
 * Module Name: create_hdfd_keyword_nodes
 * Description: This function creates memory for Keyword_Parameter structure
 *              nodes
 * Status create_hdfd_keyword_nodes(): Reviewed and Tested
 *****************************************************************************/
Keyword_Parameter* create_hdpf_keyword_nodes(HK_Config_Files *ptr_hk_config,
                                             int number_of_lines)   
{
  /* declarations */
  Keyword_Parameter *p;
  int k;

  /* initialize variables */
  p = ptr_hk_config->keywords = NULL;

  for( k=0; k<number_of_lines;  k++)
  {
    if(ptr_hk_config->keywords == NULL) 
    {
      /* Create first node for keyword */
      p = ptr_hk_config->keywords = malloc( sizeof(Keyword_Parameter) );
    } 
    else     
    {
      /*Create all other keyword_parameter nodes required */
      p->next = malloc(sizeof(Keyword_Parameter ));
      p = p->next;
    } 
    p->next = NULL;
  }
  return (ptr_hk_config->keywords);
}



/***************************************************************************** 
 * Create Memory For DSC Nodes
 * Module Name: create_hdfd_dsc_nodes
 * Description: This function creates memory for DSC_Conversion structure nodes.
 * Status create_hdfd_dsc_nodes(): Tested and Reviewed
 *****************************************************************************/
DSC_Conversion* create_hdpf_dsc_nodes(Keyword_Parameter *ptr_hk_keyword)
{
  /* declarations */
  DSC_Conversion *p;

  /* initializations */
  p = ptr_hk_keyword->dsc_ptr ;

  if ( p == NULL)
  {
    /*create first node for dsc data */
    p =  (DSC_Conversion *) malloc(sizeof(DSC_Conversion)); 
    ptr_hk_keyword->dsc_ptr= p; /* add connection 5-17-2006*/
  }
  else
  {
    while ( p->next  != (DSC_Conversion *)(NULL))
    {
      p= p->next;
    }
    /*create all other DSC Conversion nodes */
    p->next = (DSC_Conversion *) malloc (sizeof(DSC_Conversion));
    p=p->next;
  }
  p->next= NULL;
  return(p);
}/*end-create_hdpf_dsc_nodes module */



/***************************************************************************** 
 * Check Packet Version Number
 * Module Name: check_file_version_number
 * Description: This function checks version number in the HK_Config_Files
 *              structure. For encode_hk checked parameter-version-numbers.
 * Status check_version_number():Reviewed & Tested-used by decode_hk functions
 *****************************************************************************/
HK_Config_Files* check_packet_version_number( HK_Config_Files *ptr_to_configs,
				       char *version_number )       
{
  while(ptr_to_configs != NULL )  
  {
    if(!strcmp(ptr_to_configs->parameter_version_number, version_number))
    {
      break; 
    }
    ptr_to_configs = ptr_to_configs->next;
  }
  return ptr_to_configs;
}
/***************************************************************************** 
 * Check File Version Number
 * Module Name: check__file_version_number
 * Description: This function checks version number in the HK_Config_Files
 *              structure. For encode_hk checked file-version-numbers.
 * Status check_version_number():Reviewed & Tested-used by decode_hk functions
 *****************************************************************************/
HK_Config_Files* check_file_version_number( HK_Config_Files *ptr_to_configs,
				       char *version_number )       
{
  while(ptr_to_configs != NULL )  
  {
    if(!strcmp(ptr_to_configs->file_version_number, version_number)) 
      break; 
    ptr_to_configs = ptr_to_configs->next;
  }
  return ptr_to_configs;
}

/***************************************************************************** 
 * CHECK FREE CONFIG FLAG
 * Module Name:check_free_configs_flag
 * Description:This function is used to decide whether to free config structures
 *             dynamically by check if file name exists in directory.
 *             If file name exist returns 1 and if not returns 0.
 * Status check_free_configs_flag: Reviewd and Tested on 6-28-2006
 *****************************************************************************/
int check_free_configs_flag(void)
{
  /*declarations */
  int status;
  FILE *file_ptr;
  char *directory ;
  char *filename;
  char directory_filename[MAX_FILE_NAME];

  /* get directory and file name */
  directory = getenv("HK_CONFIG_DIRECTORY");
  filename= getenv("HK_FREE_FLAG_FILE");
  if(filename == NULL) 
    filename = "HK_FREE_CONFIG_FLAG_FILE";
  if(directory == NULL) 
    directory = "../../tables/hk_config_file";
  sprintf(directory_filename, "%s/%s", directory, filename); 
  /*open file-if exists then clear configs each time*/
  file_ptr = fopen(directory_filename, "r");
  if (file_ptr == NULL)
  {
    status =0;
  }
  else 
  {
    status = 1;
    fclose(file_ptr);
  }
  return status;
}



/***************************************************************************** 
 * Free Allocated Memory for HK_Configs, etc
 * Module Name:deallocate_apid_ptr_hk_config_nodes 
 * Description:This function free any memory in APID_Pointer_HK_Configs
 *             HK_Config_Files, Keyword_Parameter, DSC_Conversion, and 
 *             ALG Conversion link list structures. This structure is used to 
 *             hold in-memory configuration data used decode keywords. The call
 *             to this function frees everything that was read in since the 
 *             process has been up and running.  
 * Status deallocate_apid_ptr_hk_config_nodes(): Tested and Reviewed
 *****************************************************************************/
void deallocate_apid_ptr_hk_config_nodes(void)     
{
  extern APID_Pointer_HK_Configs *global_apid_configs;
  APID_Pointer_HK_Configs *tmp_apid_ptr_hk_configs;
  APID_Pointer_HK_Configs *prev_apid_ptr_hk_configs;
  HK_Config_Files *tmp_ptr_hk_configs;
  HK_Config_Files *prev_ptr_hk_configs;
  Keyword_Parameter *tmp_keyword_node;
  Keyword_Parameter *prev_keyword_node;
  DSC_Conversion *tmp_dsc_node;
  DSC_Conversion *prev_dsc_node;
  ALG_Conversion *tmp_alg_node;
  int free_flag=0;

  /* check if want to free all stored configurations */
  if (!(free_flag= check_free_configs_flag()))
  {
     /*skip deallocating configuration data kept in memory*/
     return;
  }

  /* assign top of link list of config data to tmp ptr */
  tmp_apid_ptr_hk_configs=global_apid_configs;

  /* Loop throught and free nodes */
  for(prev_apid_ptr_hk_configs=tmp_apid_ptr_hk_configs;
      tmp_apid_ptr_hk_configs;
      prev_apid_ptr_hk_configs = tmp_apid_ptr_hk_configs )  
  {
    for(prev_ptr_hk_configs=tmp_ptr_hk_configs=tmp_apid_ptr_hk_configs->ptr_hk_configs;
	tmp_ptr_hk_configs;
	prev_ptr_hk_configs =tmp_ptr_hk_configs)  
    {
      for(prev_keyword_node=tmp_keyword_node=
	    tmp_ptr_hk_configs->keywords;
	  tmp_keyword_node;
	  prev_keyword_node =tmp_keyword_node)  
      {
        /* free dsc nodes */
        for(prev_dsc_node=tmp_dsc_node=tmp_keyword_node->dsc_ptr;
            tmp_dsc_node; prev_dsc_node = tmp_dsc_node)
        {
          tmp_dsc_node= tmp_dsc_node->next;
          free( prev_dsc_node);
        }
        /* free alg node */
        tmp_alg_node=tmp_keyword_node->alg_ptr;
        if (tmp_keyword_node)
        {
          free( tmp_alg_node );
        }
	tmp_keyword_node=tmp_keyword_node->next;
	free(prev_keyword_node);
      }
      tmp_ptr_hk_configs =tmp_ptr_hk_configs->next;
      free(prev_ptr_hk_configs);
    }
    tmp_apid_ptr_hk_configs = tmp_apid_ptr_hk_configs->next ;
    free(prev_apid_ptr_hk_configs);
  }
  //ADDED 6-28 to test deallocate
  global_apid_configs=NULL;

}


/***************************************************************************** 
 * Read GTCIDS(Ground_to_code_ids) HK Files
 * Module Name: read_gtcids_hk_file
 * Description: This function reads the GTCIDS file and loads data into the
 *              GTCIDS_Version_Number structure.
 * Status read_gtcids_hk_fi1e(): Reviewed and Tested
 *****************************************************************************/
GTCIDS_Version_Number * read_gtcids_hk_file(APID_Pointer_HK_Configs *top_apid_ptr)
{
  /*declarations*/
  char *hk_gtcids_directory ;
  char *hk_gtcids_filename;
  char hk_gtcids_directory_filename[MAX_FILE_NAME];
  FILE *file_ptr;
  char line[MAXLINE_IN_FILE];
  GTCIDS_Version_Number *top_ptr_gtcids_vn;
  GTCIDS_Version_Number *ptr_gtcids_vn;

  /* get directory and file name */
  hk_gtcids_directory = getenv("HK_CONFIG_DIRECTORY");
  hk_gtcids_filename= getenv("HK_GTCIDS_FILE");
  if(hk_gtcids_filename == NULL) 
    hk_gtcids_filename = "gtcids.txt";
  if(hk_gtcids_directory == NULL) 
    hk_gtcids_directory = "../../tables/hk_config_file";
  sprintf(hk_gtcids_directory_filename, "%s/%s", hk_gtcids_directory,
	  hk_gtcids_filename); 
  /*open file & read  data into GTCIDS_Version_Number link list structure*/
  file_ptr = fopen(hk_gtcids_directory_filename, "r");
  if(!file_ptr)
  {
    printkerr("Error:Couldn't open Ground To Code IDS  file %s.\n",hk_gtcids_directory_filename);
    printkerr("Set the enviroment variables HK_CONFIG_DIRECTORY"
              " to point to config directory and HK_GTCIDS_FILE"
              " environment variable to point to the correct file name\n");
    //exit (1);
    return NULL;
  }
  top_ptr_gtcids_vn = NULL;
  while( fgets(line, MAXLINE_IN_FILE, file_ptr) != NULL )
  {
    if(line[0] == '#') 
      continue; /* skip comments */
    else  
    {
      if (top_ptr_gtcids_vn == NULL)    
      {
	top_ptr_gtcids_vn = ptr_gtcids_vn = malloc(sizeof(GTCIDS_Version_Number));
	ptr_gtcids_vn->next = (GTCIDS_Version_Number*)NULL;
      } 
      else  
      {
	ptr_gtcids_vn->next = malloc(sizeof(GTCIDS_Version_Number));
	ptr_gtcids_vn = ptr_gtcids_vn->next;
        ptr_gtcids_vn->next = (GTCIDS_Version_Number*)NULL;
      }
      ptr_gtcids_vn->next= NULL;
      /*Parse key values- HMI ID and Stanford ID */
      /*Locate column for HMI_ID value and STANFORD_TLM_HMI_AIA values*/
      /*HMI_ID Column is 4th column with | as delimiter*/
      /*STANFORD_TLM_HMI_AIA values  are in 7th column*/
      /*Assume always above, otherwise more code required here*/
      sscanf( line,
	      "%s %s |%*s | %*s | %s | %s | %*s | %s | %*s",
	      ptr_gtcids_vn->change_date, 
	      ptr_gtcids_vn->change_time, 
	      ptr_gtcids_vn->hmi_id_version_number,
	      ptr_gtcids_vn->aia_id_version_number,
	      ptr_gtcids_vn->file_version_number);
    } 
  } /* END-for fgets line */
  fclose(file_ptr);

  global_gtcids_vn = top_ptr_gtcids_vn; /* set global */
  return(top_ptr_gtcids_vn);
}/* END-Module: read_gtcids_hk_file */



/***************************************************************************** 
 * Load GTCIDS(Ground_to_code_ids) Version Number Values
 * Module Name: load_gtcids_vn_values
 * Description: This function loads parameter version number into the 
 *              the APID_-Pointer and HK-Config_File structure based on
 *              the file_version_number values in the HK_Config_Files and 
 *              GTCIDS_Version_Number structures.
 * Status: load_gtcids_vn_values():Tested
 *****************************************************************************/
void load_gtcids_data( GTCIDS_Version_Number* top_ptr_gtcids_data,
                       APID_Pointer_HK_Configs *top_apid_ptr_hk_configs) 
{
  /* declarations */
  GTCIDS_Version_Number* tmp_ptr_gtcids_data;
  APID_Pointer_HK_Configs *tmp_apid_ptr_hk_configs;
  HK_Config_Files *tmp_ptr_hk_configs;

  /* Initialize data */
  tmp_ptr_gtcids_data= top_ptr_gtcids_data;
  tmp_apid_ptr_hk_configs= top_apid_ptr_hk_configs;

  /* load gtcids data in APID-PTR and HK-Config-Files structures */
  for(tmp_apid_ptr_hk_configs = top_apid_ptr_hk_configs; 
      tmp_apid_ptr_hk_configs ; 
      tmp_apid_ptr_hk_configs= tmp_apid_ptr_hk_configs->next) 
  {

    for( tmp_ptr_hk_configs = tmp_apid_ptr_hk_configs->ptr_hk_configs;
	 tmp_ptr_hk_configs ;
	 tmp_ptr_hk_configs= tmp_ptr_hk_configs->next) 
    {
      for (tmp_ptr_gtcids_data=top_ptr_gtcids_data;tmp_ptr_gtcids_data ;
	   tmp_ptr_gtcids_data=tmp_ptr_gtcids_data->next)    
      {
	if( !strcmp(tmp_ptr_gtcids_data->file_version_number,
		    tmp_ptr_hk_configs->file_version_number)) 
	{
          if(!strcmp(tmp_ptr_hk_configs->packet_id_type, HMI_ID_TYPE))
          {
	     strcpy(tmp_ptr_hk_configs->parameter_version_number,
		 tmp_ptr_gtcids_data->hmi_id_version_number);
          }
          else if(!strcmp(tmp_ptr_hk_configs->packet_id_type, AIA_ID_TYPE))
          {
	     strcpy(tmp_ptr_hk_configs->parameter_version_number,
		 tmp_ptr_gtcids_data->aia_id_version_number);
          }
          else
          {  /*default set to HMI type */
	     strcpy(tmp_ptr_hk_configs->parameter_version_number,
		 tmp_ptr_gtcids_data->hmi_id_version_number);
          }
          break;
	}
      } /* End-for thru gtcid data */
    } /*End-for loop thru hk config nodes */
  } /*End-for loop thru apid-ptr */
} /* END-module : load_gtcids_data*/ 



/***************************************************************************** 
 * Deallocate GTCIDS Version Numbers
 * Module Name: deallocate_GTCIDS_Version_Number
 * Description: This function deallocates GTCIDS_Version_Number Structure 
 *              link list.
 * Status: deallocate_GTCIDS_Version_Number(): Reviewed and Tested
 *****************************************************************************/
void deallocate_GTCIDS_Version_Number(void) 
{
  /*declarations */
  GTCIDS_Version_Number *p,*tmp;

  /* deallocate nodes in link list */
  p = global_gtcids_vn;

  while(p)
  {    
    tmp = p->next;
    free(p);
    p = tmp;
  } 
}

/***************************************************************************** 
 * DE-Allocate Memory APID HKPFD FILES NODES
 * Module Name: deallocate_apid_hkpfd_files_nodes;
 * Description: deAllocate memory for a link list of directory 
 *              and filename's.
 * Status allocate_apid_hkpfd_files_nodes(): initial coding started only
 *****************************************************************************/
void deallocate_apid_hkpfd_files_nodes(APID_HKPFD_Files *ptr) 
{
  /*declarations*/
  APID_HKPFD_Files *tmp;

  while(ptr)
  {
    tmp = ptr->next;
    free(ptr);
    ptr = tmp;
  }
  return;
}



/***************************************************************************** 
 * ReRead All APID-<#>-VERSION-<#> files
 * Module Name: 
 * Description: 
 *             
 * Status reread_all_files(): initial coding
 *****************************************************************************/
HK_Config_Files*  reread_all_files(APID_Pointer_HK_Configs *apid_ptr_configs,char version_number[]) 
{
  /*declarations*/
  int apid;
  APID_Pointer_HK_Configs *p, *prev_p;
  char file_version_number[50];
  char *ptr_fvn;
  int found_flag;
  GTCIDS_Version_Number *ptr_gtcids_data;
  APID_HKPFD_Files *top_hkpfd_files;
  APID_HKPFD_Files *hkpfd_files;

  /* init values */
  ptr_fvn=file_version_number;
  ptr_gtcids_data = global_gtcids_vn;

  /*save apid value to look up later */
  apid= apid_ptr_configs->apid;
  /* find file version number directory to read in files */
  ptr_fvn=find_file_version_number(ptr_gtcids_data, version_number);
  /* load HK_Config_Files structures for each apid */
  if ((top_hkpfd_files = read_all_hk_config_files(ptr_fvn)) == NULL)
    return (HK_Config_Files*)NULL;
  /* check which apid node to add */
  for(hkpfd_files=top_hkpfd_files; hkpfd_files;
      hkpfd_files=hkpfd_files->next)
  {
    found_flag=0;
    for(p=global_apid_configs; p ; p=p->next)
    {
      if ( p->apid == hkpfd_files->apid)
      {
        found_flag=1;
        break;
      }
      prev_p=p;
    }
    if (!found_flag) 
    {
      /* add apid node to link list */
      prev_p->next = (APID_Pointer_HK_Configs *) malloc (sizeof (APID_Pointer_HK_Configs));
      prev_p->next->next= NULL;
      prev_p->next->ptr_hk_configs= (HK_Config_Files*) NULL;
      prev_p->next->apid=hkpfd_files->apid;
    }
  }
  /*set moving pointer to top of list and load config data */
  for(p=global_apid_configs; p ; p=p->next)
  {
    load_config_data(top_hkpfd_files, p);
  }
  /* load data for packet version numbers in hk config format link list*/
  p=global_apid_configs; 
  load_gtcids_data(ptr_gtcids_data, p);
  /* deallocate  top_apid_hkpfd_files link list */
  hkpfd_files=top_hkpfd_files;
  deallocate_apid_hkpfd_files_nodes(hkpfd_files) ;
  /* locate top HK_Config_Files Node for APID_Pointer node equal to lookup apid value */
  p = global_apid_configs;
  while (p)
  {
    if( p->apid == apid )  
    {
      return p->ptr_hk_configs;
    }/* if apid equal p->apid */
    p = p->next; //added 1-19-2006 
  } 
  /* APID not found, return NULL. */
  return (HK_Config_Files*)NULL;
}


/***************************************************************************** 
 * FIND FILE VERSION NUMBER
 * Module Name: find_file_version_number()
 * Description: Gets  file version number based on packet version number.
 *             
 * Status find_file_version_number(): Coded and Tested
 *****************************************************************************/
char * find_file_version_number(GTCIDS_Version_Number *top, char p_version_number[])
{
  /*declarations*/
  GTCIDS_Version_Number  *tmp_ptr;

  for(tmp_ptr=top;tmp_ptr;tmp_ptr=tmp_ptr->next)    
  {
    if( !strcmp(tmp_ptr->hmi_id_version_number, p_version_number)) 
    {
      return tmp_ptr->file_version_number;
    }
  } /* End-for  */
  return ("");
}
