
#include "ns_tr069_cpe_gen_data.h"

#include <stdarg.h>
#include <dirent.h>
#include <errno.h> //errno
#include <sys/wait.h>//WEXITSTATUS

#define SCHEMA_FILE    "CavIGDSimple_Schema.xml"
#define DATA_FILE    "CavIGDSimple_Data.xml" 

int root_obj_found =0;
int attr_count_schema; 
char *progname =NULL;
int appendFlag =0;
int allocedMerge  =0;
int status;
int run_forcefully =0;
#define FILE_PATH_SIZE   4096
// Location where data is to be generated
// Can be absolute path or relative path
// Default is current directory
char data_directory[FILE_PATH_SIZE] = ".";

char buf_SCHEMA_ALL_PATHS[FILE_PATH_SIZE];
char buf_SCHEMA_PARAM_PATHS[FILE_PATH_SIZE];
char buf_SCHEMA_PARAM_TYPES[FILE_PATH_SIZE];
char buf_EXPORT_PATH[FILE_PATH_SIZE];
//int PathNeeded = 0;

//Buffers for output file.
char buf_GPERF_HASH_CFILE[FILE_PATH_SIZE];
char buf_GPERF_HASH_LIB[FILE_PATH_SIZE];
char buf_GPERF_SCHEMA_PARAM_PATHS[FILE_PATH_SIZE];
char buf_NVM_DATA_FILE[FILE_PATH_SIZE];
char buf_CPE_DATA_INDEX[FILE_PATH_SIZE];

xmlDoc *docSchema, *docData = NULL;
int obj_count_schema =0;
int obj_count_data =0;

int MakeHashLib (void);
int MergeSchemaData (void);
int WriteMergeSchemaFile (const char* file, int nfile, int ncpe, int append);
int GetSoapDataType (char *s);

#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__

#define DGDL(log_level, ...)  debug_log(log_level, _FLN_, __VA_ARGS__)

static int debug_level = 0;
static void debug_log(int log_level, char *filename, int line, char *fname, char *format, ...) {
#define MAX_DEBUG_LOG_BUF_SIZE 64000

  va_list ap;
  char buffer[MAX_DEBUG_LOG_BUF_SIZE + 1] = "\0";
  int amt_written = 0, amt_written1=0;

  // fprintf(stderr, "debug_log called with log_level = %d, debug_level = %d\n", log_level, debug_level);

  if(debug_level < log_level) return;

  amt_written1 = sprintf(buffer, "\n%d|%s|", line, fname);
  
  va_start(ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_DEBUG_LOG_BUF_SIZE - amt_written1, format, ap);
  
  va_end(ap);
  
  buffer[MAX_DEBUG_LOG_BUF_SIZE] = 0;

  if(amt_written < 0) {
    amt_written = strlen(buffer) - amt_written1;
  }

  if(amt_written > (MAX_DEBUG_LOG_BUF_SIZE - amt_written1)) {
    amt_written = (MAX_DEBUG_LOG_BUF_SIZE - amt_written1);
  }

  fprintf(stderr, "%s", buffer);
}

void Usage(void)
{
  fprintf(stderr, "Usage: %s -H -c -p -D<level> [-s schema xml] [-d data xml] [-n cpes/file] [-l directory path]\n", progname);
  exit(1);
}

void Help(void)
{
  fprintf(stderr, "Usage: %s -H -c -p -D<level> [-s schema xml] [-d data xml] [-n cpes/file] [-l directory path]\n", progname);

  fprintf(stderr, "\t-h\t print this help\n");
  fprintf(stderr, "\t-s\t schema file in xml format (input) \n");  
  fprintf(stderr, "\t-p\t print schema files. outputs following files \n");
  fprintf(stderr, "\n\t\t 1.%s\n \t\t 2.%s\n \t\t 3.%s\n\n", SCHEMA_ALL_PATHS,SCHEMA_PARAM_PATHS,SCHEMA_PARAM_TYPES);
  fprintf(stderr, "\t-d\t data file in xml format (input) \n");
  //fprintf(stderr, "\t-o\t output file name (files will be x.0 x.1 etc. for filename=x)\n");
  fprintf(stderr, "\t-c\t generate CPE data (output) \n");
  fprintf(stderr, "\t-H\t make hash library \n");  
  //fprintf(stderr, "\t-N\t number of output files \n");
  fprintf(stderr, "\t-n\t number of CPEs per file \n");
  //fprintf(stderr, "\t-a\t append data to existing files (filename should be given in -o option, \n\t\t files will be x.0, x.1 .. for filename =x)\n" );
  fprintf(stderr, "\t-D\t debug level (1,2,3) level N enables 1-N \n");
  fprintf(stderr, "\t-l\t store output files at given path\n");
  exit(1);
}

static void gen_file_paths(char *data_directory)
{
  
  DGDL (1, "Method called.");
  sprintf(buf_SCHEMA_ALL_PATHS, "%s/%s", data_directory, SCHEMA_ALL_PATHS);
  sprintf(buf_SCHEMA_PARAM_PATHS, "%s/%s", data_directory, SCHEMA_PARAM_PATHS);
  sprintf(buf_SCHEMA_PARAM_TYPES, "%s/%s", data_directory, SCHEMA_PARAM_TYPES);
  sprintf(buf_GPERF_HASH_CFILE, "%s/%s", data_directory, GPERF_HASH_CFILE);
  sprintf(buf_GPERF_HASH_LIB, "%s/%s", data_directory, GPERF_HASH_LIB);
  sprintf(buf_GPERF_SCHEMA_PARAM_PATHS, "%s/%s", data_directory, GPERF_SCHEMA_PARAM_PATHS);
  sprintf(buf_NVM_DATA_FILE, "%s/%s", data_directory, NVM_DATA_FILE);
  sprintf(buf_CPE_DATA_INDEX, "%s/%s", data_directory, CPE_DATA_INDEX);
  DGDL (2, "buf_SCHEMA_ALL_PATHS = %s, buf_SCHEMA_PARAM_PATHS = %s", buf_SCHEMA_ALL_PATHS, buf_SCHEMA_PARAM_PATHS);
  DGDL (2, "buf_SCHEMA_PARAM_TYPES = %s, buf_GPERF_HASH_CFILE = %s", buf_SCHEMA_PARAM_TYPES, buf_GPERF_HASH_CFILE);
  DGDL (2, "buf_GPERF_HASH_LIB = %s, buf_GPERF_SCHEMA_PARAM_PATHS = %s", buf_GPERF_HASH_LIB, buf_GPERF_SCHEMA_PARAM_PATHS);
  DGDL (2, "buf_NVM_DATA_FILE = %s, buf_CPE_DATA_INDEX = %s", buf_NVM_DATA_FILE, buf_CPE_DATA_INDEX);
  DGDL (1, "Exiting method.");
 
}
static void create_dir(char *name, char *dir_name)
{
  char create_cmd[FILE_PATH_SIZE]="\0";
  DGDL (1, "Method called.");
  DGDL (2, "Creating %s directory named as :%s.\n", name, dir_name);
  sprintf(create_cmd, "mkdir -p -m 0777 %s", dir_name);
  status = system(create_cmd);
  if(status == -1)
  {
    fprintf(stderr, "Error in creating %s dir '%s': %s\n", name, dir_name, strerror(errno));
    exit(-1);
  }
  status = WEXITSTATUS(status); //get the return value of system()
  if(status)
  {
    fprintf(stderr, "system(create_cmd) failed : Error in creating %s dir '%s'\n", name, dir_name);
    exit(-1);
  }
  DGDL (1, "Exiting method.");
}
static void delete_dir(char *name, char *dir_name)
{
  char delete_cmd[FILE_PATH_SIZE]="\0";
  DGDL (1, "Method called.");  
  sprintf(delete_cmd, "rm -rf  %s", dir_name);
  status = system(delete_cmd);
  DGDL (2, "return status of system() is = %d", status);
  if(status == -1)
  {
    fprintf(stderr, "Error in deleting dir '%s': %s\n", dir_name, strerror(errno));
    exit(-1);
  }
  status = WEXITSTATUS(status); //get the return value of system()
  if(status)
  {
    fprintf(stderr, "system(delete_cmd) failed : Error in deleting dir '%s'\n", dir_name);
    exit(-1);
  }
  DGDL (1, "Exiting method.");
}

static void set_data_dir(char *in_data_dir)
{
  DIR *dp;
  DGDL (1, "Method called.");
  strcpy(data_directory, in_data_dir);
  DGDL (2, "Data directory =%s", data_directory);
  if((dp = opendir(data_directory)) != NULL)
  {
    closedir(dp);
    if(run_forcefully) //deleting the data directory.
      delete_dir("Data Directory", data_directory);
    else
    {
      fprintf(stderr, "Data directory %s already exists.\n", data_directory);
      exit(-1);
    }
 
  }

  create_dir("Data Directory", data_directory);
  gen_file_paths(data_directory);
  DGDL (1, "Exiting method.");

}

static char schema_file[FILE_PATH_SIZE], data_file[FILE_PATH_SIZE];
static char gperf_croutines_inp_file[FILE_PATH_SIZE];

static void set_default_args()
{
  DGDL (1, "Method called.");
  if(getenv("NS_WDIR")) {
    sprintf(schema_file, "%s/etc/tr069/schema/%s", getenv("NS_WDIR"),SCHEMA_FILE); 
    sprintf(data_file, "%s/etc/tr069/schema/%s", getenv("NS_WDIR"),DATA_FILE);
    sprintf(gperf_croutines_inp_file, "%s/etc/tr069/conf/%s", getenv("NS_WDIR"),GPERF_CROUTINES_INP);
    sprintf(data_directory, "%s", data_directory);
    DGDL (2, "Schema file =%s Data file =%s data_directory =%s", schema_file, data_file, data_directory);
  }else {
    fprintf(stderr,"Error in getting the NS_WDIR\n");
    exit(-1);
  }
  DGDL (1, "Exiting method.");
}

int
main (int argc, char *argv[])
{

  xmlNode *root_element = NULL;
  const char *empty ="";
  int cpePerFile =0, outFileNo =0, printSchemaFlag = 1, cpeFlag = 1, printDataFlag = 1; 
  int hashFlag = 1;
  int opt;
  int allocedSchema  =0, allocedData =0 ;
  int exit_val =0;
  extern int optopt;
  progname = argv[0];
  DGDL (1, "Method called.");
  set_default_args();
  
//while ((opt = getopt(argc, argv, "HcpahD:l:s:d:o:N:n:")) != -1) {
  while ((opt = getopt(argc, argv, "HcphD:l:s:d:n:")) != -1) {
  
    switch (opt) {
      case 'h':
        Help();
        break;
      case 'D':
        debug_level = atoi(optarg);
        break;
      case 'H':
        hashFlag = 1;
        break;
      case 'a':
        appendFlag = 1;
        break;
      case 'p':
        printSchemaFlag = 1;
        break;
      case 'P':
        printDataFlag = 1;
        break;
      case 'c':        
        cpeFlag = 1;
        break;
      case 's':
        strcpy(schema_file, optarg);
        break;
      case 'd':
        strcpy(data_file, optarg);
        break;
      case 'n':
        cpePerFile = atoi(optarg);
        break;
      case 'N':
        outFileNo = atoi(optarg);
        break;
      case ':':
        fprintf(stderr, "Error - Option `%c' needs a value\n\n", optopt);
        Help();
        break;
      case 'l':
        strcpy(data_directory, optarg);
        set_data_dir(data_directory);
        break;
      default: /* ? */        
        Usage();
        exit(EXIT_FAILURE);
    
    }
  }
 
  if (!(printSchemaFlag || cpeFlag || hashFlag)) {
    fprintf(stderr, "no options given\n");
    Usage();
  }

  if (appendFlag) { // only if -c is given
    if (!cpeFlag) {
      fprintf(stderr, "-a  must be specified with -c \n");
      Usage();
    }
  }
  if (cpeFlag || appendFlag ) {
/*
    if (schemaFile == NULL ||dataFile  == NULL ) {
      fprintf(stderr, "-d -s must be specified with -c \n");
      Usage();
    }
*/

    if (!cpePerFile) {
      cpePerFile =1;
    }

    if (!outFileNo) {
      outFileNo =1;
    }
  }
  printf("\nStarting CPE data generation for %d users. It may take time. Please wait ...\n", cpePerFile);

  if (printSchemaFlag || cpeFlag || hashFlag ) {
    docSchema = xmlReadFile (schema_file, NULL, 0);
    if (docSchema == NULL) {
      ERROR ("could not parse file %s, exiting..\n", schema_file);
      exit(1);
    }
    /*
     * Get the root element node
     */
    root_element = xmlDocGetRootElement (docSchema);    
    if (AllocSchema()) {
      ERROR ("error in allocating memory for schema. exiting ..\n");
      exit_val = 2;
      goto err;
    }
    allocedSchema =1;
    DGDL (2, "allocedSchema =%d", allocedSchema);
    obj_count_schema = GetElementsSchema (root_element, 0, 0, empty, empty);
    DGDL(2, "obj_count_schema =%d", obj_count_schema);    
    if (obj_count_schema < 0) {
      ERROR ("error in retrieving xml elements \n");
      exit_val = 2;
      goto err;
    }
    if (debug_level == 3)  
      PrintObjsSchema();
    /*
     * free the document
     */
    xmlFreeDoc (docSchema);;
    xmlCleanupParser ();

    if (cpeFlag || hashFlag || printSchemaFlag) {  //need hash for schema files 
      if (MakeHashLib()) {
        ERROR("error in making hash library, exiting..");
        exit_val = 3;
        goto err;
      }
    }

    //print if only if this is also set if cpeFlag is set  
    if (printSchemaFlag ) { 
      if (PrintSchemaFiles()) {
        ERROR("error in printing schema files.exiting..");
        exit_val = 3;
        goto err;
      }
    }

  }

  if (printDataFlag|| cpeFlag) {
    docData = xmlReadFile (data_file, NULL, 0);
    if (docData == NULL) {
      ERROR ("could not parse file %s, exiting ..\n", data_file);
      exit_val = 4;
      goto err;
    }
    
    root_element = xmlDocGetRootElement (docData);
    if ( (objData = AllocData()) == NULL) {
      ERROR ("AllocData failed, exiting .. ");
      exit_val = 5;
      goto err;
    }
    allocedData =1;

    obj_count_data = GetElementsData (root_element, 0, 0, empty, empty);
    DGDL(2, "obj_count_data =%d", obj_count_data);
    if (debug_level == 3)  
      PrintObjsData(objData, obj_count_data);

    xmlFreeDoc (docData);;
    /*
     * Free the global variables that may
     * have been allocated by the parser.
     */
    xmlCleanupParser ();

  }

  if (cpeFlag) {
    if ( MergeSchemaData()) {
      exit_val =6;
      ERROR ("error returned from MergeSchemaData, exiting ..\n");
      goto err;
    }  
    if (debug_level == 3)  
      PrintObjsData(mData, obj_count_schema);
    if (WriteMergeSchemaFile (buf_NVM_DATA_FILE, outFileNo, cpePerFile, appendFlag )) {
      exit_val =7;
      ERROR ("error returned from WriteMergeSchemaFile, exiting ..\n");
      goto err;
    }
  }

err:
  if (allocedSchema) 
    FreeSchema();

  if (allocedData) 
    FreeData(objData);

  if (allocedMerge) 
    FreeData(mData);
  DGDL (2, "exit_val =%d", exit_val);
  DGDL (1, "Exiting method.");
  printf("\nProcessing done.\n");  
  return (exit_val);
}


/* search for attribute name in the data array for a given object and parameter in that
 * object. we return the index in the attribute array
 */

int FindAttrName (int obj_index, int param_index, char *name, int *attr_index)
{
  int no_attrs,i;
  DGDL(1, "Method called.");
  no_attrs = PAR_ATTR_NO_DATA(obj_index,param_index);
  DGDL(2, "no_attrs =%d", no_attrs);  
  for (i=0; i<no_attrs; i++) {
    if (!strcmp(ATTR_NAME_DATA(obj_index,param_index,i), name))  {
      *attr_index =i;
      DGDL(1, "Exiting method."); 
      return(1);
    }
  }  
  DGDL(1, "Exiting method.");  
  return(0);
}

/* find array object from the data and return its index in ObjData */
int FindArrayObjData(char *name, int *obj_index)
{
  int i;
  *obj_index =0;
  DGDL(1, "Method called.");
  for (i=0; i<obj_count_data; i++) {
    if (!objData[i]->ArrayType)  continue;
    if (!strcmp(objData[i]->ObjName,name)) {
      *obj_index = i;
      DGDL(1, "Exiting method.");
      return(1);
    }
  }
  DGDL(1, "Exiting method.");
  return(0);
}
              

//find ParameterName in data for the given object
// object index is supplied use that to speed up things
int FindParamName (char *objname, char *name, int *obj_index, int *param_index)
{
  int i,j,no_params ,no_attrs;
  DGDL(1, "Method called.");
  DGDL(2, "objname =%s name =%s obj_count =%d\n", objname, name, obj_count_data);
  for (i=0; i<obj_count_data; i++) {
    if (!strcmp(objData[i]->ObjName, objname)) {
      *obj_index =i;
      no_params = objData[i]->no_of_params;
      DGDL(3, "%d ObjName =%s param_count =%d\n", i, objData[i]->ObjName, no_params);
      for (j=0; j<no_params ; j++) {
        no_attrs = PAR_ATTR_NO_DATA(i,j);
        if (!strcmp (name,PAR_NAME_DATA(i,j))) {
          DGDL (3, "Found ParamName =%s ParamVal =%s no_attrs =%d\n", PAR_NAME_DATA(i,j), PAR_VAL_DATA(i,j), no_attrs);
          *param_index =j;
          DGDL(1, "Exiting method.");         
          return(1);
        }
      }  
    }
  }
  DGDL(1, "Exiting method.");
  return(0);
}
#if 1

/* the merged data array is sized like ObjData, but like members in ObjData, its members
 * have the same dimensions as the schema. so we can copy stuff from the schema into this
 * Also we can copy from ObjData  as needed. We follow layout of the schema to copy as 
 * it is a superset of the data.
 */ 
int MergeSchemaData (void)
{
  int i,j,k,ii,jj,no_params, no_attrs  ;
  char dummy_param_val[PARAM_NAME_LEN];
 // char dummy_attr_val[ATTR_VALUE_LEN];
  char *p;
  //index of obj,param in that obj in data, attr in obj,param
  int obj_index_data, par_index_data, attr_index_data, param_found, attr_found;
  char *attr_name, *attr_value;
  int attr_type;
  DGDL(1, "Method called.");
  //allocate another array to store final data 
  if ( (mData = AllocData()) == NULL) {
    fprintf(stderr, "AllocData failed ");
    DGDL(1, "Exiting method.");
    return(1);
  }
  allocedMerge = 1;

  for (i=0; i<obj_count_schema; i++) {  //loop over all schema objects
    no_params = objSchema[i]->no_of_params;
    mData[i]->no_of_params = no_params ;
      
    strcpy (mData[i]->ObjName, objSchema[i]->ObjName);
  
    //only if the object is an array type, and was provided in the data we would have 
    //created instance for this. otherwise,
    //the instance name should be blank (even if it is an array type).
    //for former, copy the instance name of the object to mData.
    // for latter, and for the non-array case, just copy the object name
    //

    if (!strcmp(objSchema[i]->ArrayType,"true"))  {
      mData[i]->ArrayType   = 1;      //should be same as the flag value in objData
      if (FindArrayObjData(mData[i]->ObjName, &ii))  {
        strcpy (mData[i]->ObjNameInst, objData[ii]->ObjNameInst);
      } else {
        strcpy (mData[i]->ObjNameInst, objData[ii]->ObjName);
      }
    } else { //copy regular obj name
      strcpy (mData[i]->ObjNameInst, objSchema[i]->ObjName);
      mData[i]->ArrayType   = 0;      //not an array type object
    }


    for (j=0; j<no_params ; j++) {
      //copy param name from schema  to generic name
      // we could also copy the same into the instance, but we check data before that.
      strcpy (MYPAR_NAME_DATA(mData[i],j),PAR_NAME(i,j));


      //get rest from schema - length, type, min and max values for parameter
      MYPAR_VARY_DATA(mData[i],j) =   atoi(PAR_VARY(i,j));    //1 or 0 string
      MYPAR_LEN_DATA(mData[i],j) =   atoi(PAR_LEN(i,j));
      MYPAR_TYPE_DATA(mData[i],j) =   GetSoapDataType(PAR_TYPE(i,j));
      MYPAR_MINVAL_DATA(mData[i],j) =   atoi(PAR_MINVAL(i,j));
      MYPAR_MAXVAL_DATA(mData[i],j) =   atoi(PAR_MAXVAL(i,j));
      //
      //if data is available for param, copy it 

      obj_index_data = -1; 
      par_index_data = -1;      //index of obj and param in that obj in data
      param_found =0;    // if true after search, this would mean both obj and param were found.
      DGDL (3, "obj_index_data =%d par_index_data =%d param_found =%d", obj_index_data, par_index_data, param_found);
      if (MYPAR_TYPE_DATA(mData[i],j) != OBJECT) {
        if (FindParamName (objSchema[i]->ObjName, PAR_NAME(i,j), &ii, &jj)) {
          obj_index_data  = ii;
          par_index_data  =jj;  
          param_found =1;
          strcpy (MYPAR_VAL_DATA(mData[i],j),PAR_VAL_DATA(ii,jj));
          //copy param with instance -ok even if not array
          strcpy (MYPAR_NAME_INST_DATA(mData[i],j),PAR_NAME_INST_DATA(ii,jj));
        } else { 
  /* there are many params that are in schema but not in the data, these will get copied
   *  to mData, but have NULL values. so put dummy vals for these.
   *  if the param name isn't in the data, we copy the generic name from schema into its
   *  instance name, as this would be blank otherwise.
   */
          strcpy (MYPAR_NAME_INST_DATA(mData[i],j),PAR_NAME(i,j));
          p = strrchr(MYPAR_NAME_DATA(mData[i],j),'.');
          p++;
          sprintf(dummy_param_val, "%s%s", p, DUMMY_PARAM_VAL_SUFFIX );
          strcpy (MYPAR_VAL_DATA(mData[i],j),dummy_param_val);
        }
        DGDL(2, "param name %s val %s\n", MYPAR_NAME_INST_DATA(mData[i],j), MYPAR_VAL_DATA(mData[i],j));
        //
      }
      DGDL (3, "obj_index_data =%d par_index_data =%d param_found =%d", obj_index_data, par_index_data, param_found);   
      //copy no of attributes from the schema
      no_attrs = attr_count_schema;      //global
      MYPAR_ATTR_NO_DATA(mData[i],j) = no_attrs;

      // The attributes part needs work
      for (k=0; k<no_attrs ; k++) { //assume that k index is always 0 -only 1 attr
#if 1
        attr_name = ATTR_NAME(k);    //attribute name from schema struct
        attr_type = GetSoapDataType(ATTR_TYPE(k));
        attr_value = MYATTR_VAL_DATA(mData[i],j,k); //no value set yet
      
        //need to copy the attr name into mData first
        strcpy (MYATTR_NAME_DATA(mData[i],j,k), attr_name);

        if (!strcmp(ATTR_ARRAY(k),"true"))  {
          MYATTR_ARRAY_DATA(mData[i],j,k) = 1;
        } else {
          MYATTR_ARRAY_DATA(mData[i],j,k) = 0;
        }

        attr_index_data = -1;
        if (param_found) { //otherwise we ll end up passing a -ve index
          attr_found =0;
          DGDL (2, "attr_found =%d",attr_found);  
          if (FindAttrName (obj_index_data, par_index_data, attr_name, &attr_index_data)) {
              attr_found =1;
              //assuming data is also given since name was found
              strcpy (attr_value, ATTR_VAL_DATA(obj_index_data,par_index_data,attr_index_data));
#endif
                      
              //for the min and max and array attribute, only the int/uint types will be valid
              if (attr_type == INT || attr_type == UNSIGNED_INT) {
                MYATTR_MINVAL_DATA(mData[i],j,k)  = ATTR_MINVAL_DATA(obj_index_data,par_index_data,attr_index_data);
                MYATTR_MAXVAL_DATA(mData[i],j,k)  = ATTR_MAXVAL_DATA(obj_index_data,par_index_data,attr_index_data);
              } else {
                //we shouldnt be using this 
                MYATTR_MINVAL_DATA(mData[i],j,k)  = 0;
                MYATTR_MAXVAL_DATA(mData[i],j,k)  = 0;
              }
          }
        }
// we come here if param for this attr was not found in the data OR attr data was not found
        if (!param_found || !attr_found) { //fill in dummy values
#ifdef DUMMY_ATTR_VALUE
          sprintf(dummy_attr_val, "%s%s", attr_name, DUMMY_ATTR_VAL_SUFFIX );
          strcpy (MYATTR_VAL_DATA(mData[i],j,k), dummy_attr_val);
#else
          if (!strcmp(attr_name, "notification")) {
            strcpy (MYATTR_VAL_DATA(mData[i],j,k), DEFAULT_NOTIFICATION_VAL);
          } else if (!strcmp(attr_name, "accessList")) {
            strcpy (MYATTR_VAL_DATA(mData[i],j,k), DEFAULT_ACCESSLIST_VAL);
          }
#endif
        }
        //we shouldnt be using this 
        MYATTR_MINVAL_DATA(mData[i],j,k)  = 0;
        MYATTR_MAXVAL_DATA(mData[i],j,k)  = 0;
        DGDL(2, "attr name %s type %d value %s\n", attr_name, attr_type, attr_value);
      }
    }
  }
  DGDL(1, "Exiting method.");
  return(0);
}
#endif

int Strftime (char *buf, int size, const char *format)
{
  time_t t;
  struct tm *tmp;
  DGDL(1, "Method called.");
  t = time(NULL);
  tmp = localtime(&t);
  if (tmp == NULL) {
    ERROR("error getting localtime");
    DGDL(1, "Exiting method.");
    return(1);
  }
  if (strftime(buf, size, format, tmp) == 0) {
    ERROR ("strftime returned 0");
    DGDL(1, "Exiting method.");
    return(1);
  }
  DGDL(1, "Exiting method.");
  return(0);
}


/* get new value for a parameter if its the kind that needs to be varied.
 * we add a sequence no for a string at the end.
 * for an int we get a random no between the limits of its value as per schema
 */

int  GetNewParamVal (char *val,int obj_index, int param_index)
{
  static int seq_no =0 ;
  int ran, maxlen, len;
  int min, max;
  unsigned int umin, umax;
  soapDataType soap = MYPAR_TYPE_DATA(mData[obj_index],param_index);
  char tmp[32];  //used to store date in YYYYMMDD - max 8+1 chars, check size before using
  char *p;
  struct stat stbuf;
  FILE *fp = NULL;
  DGDL(1, "Method called");
  DGDL(2, "obj_index = %d, param_index = %d", obj_index, param_index);

  char *prefix = MYPAR_VAL_DATA(mData[obj_index],param_index);
  char *name = MYPAR_NAME_DATA(mData[obj_index],param_index);

  DGDL(2, "prefix = %s, name = %s", prefix, name);

/*
 Make sure in Schema file, we have parameterVary tag for Serial Number 
 <parameterName>SerialNumber</parameterName>
  <parameterType>string</parameterType>
  <parameterLength>64</parameterLength>
  <parameterVary>1</parameterVary>  <!-- To generate serial number in YYYYMMDD<seq> format -->
  </parameter>
 <parameter>

  NOTE: for the parameter name - SerialNumber (only), we generate a value of the following form
YYYYMMDD<number>
where number = 0,1,2 ...
If the sequence no has to start from a number (other than 0) , this could be input into a file
-  .param_value_temp

header file defn - #define PARAM_VALUE_TEMP ".param_value_temp"
The ending sequence no is not written back into this file. The user can supply ANY sequence no
to start from
*/
 
  //special treatment for SerialNumber
  p = strrchr(name,'.');    //extract short paramter name from full path
  p++;
  if (!strcmp(p, "SerialNumber")) {
    if (Strftime(tmp, sizeof(tmp), "%Y%m%d")) {
      ERROR ("Strftime  returned error\n");
      DGDL(1, "Exiting method.");
      return(1);
    }
    prefix = tmp;
  }


  switch (soap) {
    case STRING: 
    //check if there is a file that has the seq no already - this tells us where to start
      if ( (stat(PARAM_VALUE_TEMP, &stbuf) == 0) ) {
        if ( (fp = fopen(PARAM_VALUE_TEMP, "r")) == NULL) {
          perror("fopen");
          ERROR ("error in fopen of %s\n",PARAM_VALUE_TEMP);
          DGDL(1, "Exiting method.");
          return(1);
        }
        fscanf(fp,"%d",&seq_no);
        DGDL(3, "found seq_no = %d in %s\n", seq_no, PARAM_VALUE_TEMP);
      }
  
      sprintf(val, "%s%d",prefix,seq_no++); 
      maxlen = MYPAR_LEN_DATA(mData[obj_index],param_index);
      len = strlen(val);

      if (len > maxlen) {
        ERROR("Error.exceeded allowed size. Param %s Len %d val %s len %d ",name,maxlen,val,len);
      }
      break;  
    case INT:
      min = MYPAR_MINVAL_DATA (mData[obj_index],param_index);
      max = MYPAR_MAXVAL_DATA (mData[obj_index],param_index);
  
      //do we have min and max values in the data ? if not, get a max based on type
      //
      max = (max ==0 ? INT_MAX: max);
      //min = (min ==0 ? INT_MIN: min);
      min = (min < 0 ? 0:min);    //approx for now, as rand() handles only +ve
      
      DGDL(3, "INT : min %d max %d \n", min, max);
      ran = (float)min  + (int) ( (float)max * (rand() / (RAND_MAX + 1.0)));
      sprintf(val, "%d",ran);
      break;  
    case UNSIGNED_INT:
      umin = MYPAR_MINVAL_DATA (mData[obj_index],param_index);
      umax = MYPAR_MAXVAL_DATA (mData[obj_index],param_index);
  
      //do we have min and max values in the data ? if not, get a max based on type
      //
      umax = (umax ==0 ? UINT_MAX: umax);

      DGDL(3, "UINT : umin %u umax %u \n", umin, umax);
      ran = (float)umin  + (int) ( (float)umax * (rand() / (RAND_MAX + 1.0)));
      sprintf(val, "%u",ran);
      break;  
    case DATETIME:
    case UNKNOWN:
    case OBJECT:
    case MULTIPLE:
    case BOOLEAN:
    case BASE64:
      ERROR("type %d not handled\n",soap);
      break;  
    default:
      ERROR("invalid soap type %d\n",soap);
  }
  DGDL(2, "name =%s p =%s returning with val =%s\n", name, p, val);
  DGDL(1, "Exiting method.");  
  return(0);
}

#if 0
int GetParamNameVary (char *name) 
{
  int i;
  printf ("GetParamNameVary : name %s\n",name);

  for (i=0; i< MAX_PARAMS_TO_VARY ; i++) {
    if (!strcmp (paramVaryNames[i],name))
      return(1);
  }
  return(0);
}
#endif


/* 
 * This routine merges schema and data (collected separately by parsing 2 different files)
 * and puts it in a structure (of the data type). The schema describes all possible objects 
 * and their parameters. The data file may contain a subset of this list.
 * the resulting data structure is written out to files in various formats.
 */
int WriteMergeSchemaFile (const char* file, int nfile, int ncpe, int append)
{
  FILE *fp, *fp_off;  
  int i,j,k,n,no_params ,no_attrs ;
  //char *value, *newval;
#ifdef MULTI_FILE      //multiple files - 1 for each nvm
  int f;
  char file_cur [OUTFILE_NAME_SIZE];
#else
  const char *file_curp;
#endif
  char file_cur_off [OUTFILE_NAME_SIZE];
  char *param_name, *param_val, *param_len;
  //char *param_type, *param_attr;
  char param_new_val [PARAM_VALUE_LEN];
  //long off;
  char *param_notif, *param_access;
  int param_index;
  DGDL(1, "Method called.");
#ifdef MULTI_FILE      //multiple files - 1 for each nvm
  
  for (f=0; f< nfile; f++) {
    sprintf (file_cur, "%s.%d",file,f);
    file_curp = file_cur;
#else
    file_curp = file;
#endif

    if (append) {
      if ( (fp = fopen(file_curp,"a+")) == NULL) {
        perror("fopen");
        ERROR("error in fopen %s\n",file_curp);
        DGDL(1, "Exiting method.");
        return(1);
      }
    } else {
      if ( (fp = fopen(file_curp,"w+")) == NULL) {
        perror("fopen");
        ERROR("error in fopen %s\n",file_curp);
        DGDL(1, "Exiting method."); 
        return(1);
      }
    }
  //open a file to write offsets of nvms in the current file

  //Manish: since now we are not using offset in later processing so we have no need to make .index file 
  //But in palce of .index file we will create a .count file which contains number of records(= number of cpe user)
  //sprintf (file_cur_off, "%s.index",file_curp);
    sprintf (file_cur_off, "%s.count",file_curp);

    if ( (fp_off = fopen(file_cur_off,"w+")) == NULL) {
      perror("fopen");
      ERROR("error in fopen %s\n",file_cur_off);
      DGDL(1, "Exiting method.");
      return(1);
    }
 
    fprintf(fp_off,"%d\n",ncpe);
    for (n=0; n< ncpe; n++) {
      //get offset for this cpe
      //off = ftell(fp);
      //write the offset into a file
      //fprintf(fp_off,"%ld\n",off);

      for (i=0; i<obj_count_schema; i++) {
        no_params = objSchema[i]->no_of_params;
        for (j=0; j<no_params ; j++) {

          DGDL(3,"i =%d j =%d param =%s val =%s\n", i, j, MYPAR_NAME_DATA(mData[i],j),MYPAR_VAL_DATA(mData[i],j) );

          if (MYPAR_TYPE_DATA(mData[i],j) != OBJECT) {

            if (MYPAR_VARY_DATA(mData[i],j)) { 
              memset (param_new_val, 0, PARAM_VALUE_LEN);
              if (GetNewParamVal (param_new_val , i,j)) {
                DGDL(1, "Exiting method.");
                return(1);
              }
              param_val =  param_new_val;
              DGDL(3, "New value for %s = %s\n", MYPAR_NAME_DATA(mData[i],j), param_val  );
            } else {
              param_val = MYPAR_VAL_DATA(mData[i],j) ; 
            }

/* 
 * changed the obj and param name in the schema so that they contain an instance with the 
 * name for array types. The hash is created out of these names.
 * if the data is changed later to contain other instances (other than .1) than we start off
 * with in the schema and the data, they will be out of sync.ie., the schema will only contain
 * .1 , but the data being dynamic may contain other instances such as .2, .3 (while the .1 
 * may have been deleted too)
 * 9-aug-11
 */
#if 1
            param_name = MYPAR_NAME_INST_DATA(mData[i],j); 
#else
            param_name = PAR_NAME(i,j); 
#endif
            //param_type = PAR_TYPE(i,j);    //this is a string, use this
            param_len = PAR_LEN(i,j); 
            param_len = (param_len[0]  == '\0') ? "-":PAR_LEN(i,j);
            //param_attr = "TBD";
            // this is the index into a list of all param names (includes all objects)
            param_index = StringToIndexFunc(param_name, strlen(param_name) );
            //loop over attributes for this parameter 

            no_attrs = MYPAR_ATTR_NO_DATA(mData[i],j);
            param_notif = param_access = "TBD"; 
            for (k=0; k<no_attrs; k++) {
              DGDL (3, "attr_name %s  value %s\n", MYATTR_NAME_DATA(mData[i],j,k), MYATTR_VAL_DATA(mData[i], j, k));
              if (!strcmp(MYATTR_NAME_DATA(mData[i],j,k), "notification")) {
                param_notif  = MYATTR_VAL_DATA(mData[i],j,k); 
              } else if (!strcmp(MYATTR_NAME_DATA(mData[i],j,k), "accessList")) {
                param_access = MYATTR_VAL_DATA(mData[i],j,k);
              }
            }
            DGDL (3, "param_index =%d param_val =%s param_notify =%s param_access =%s", param_index, param_val, param_notif, param_access); 
#if 0 //old format
            fprintf (fp, "%s = %s,%s,%s,%s\n",param_name, param_type, param_len, param_attr, param_val);
#else    //new format
            fprintf (fp, "%d=%s|%s|%s\n",param_index, param_val, param_notif, param_access);

#endif
          }
        }    //parameter loop
      }    //object loop
      //insert delimiter for this CPEs data set
      fprintf(fp, "%s\n",DELIM);
    }  //cpes per file
   
    fclose(fp);

#ifdef MULTI_FILE      //multiple files - 1 for each nvm
  }    //loop over files 
#endif
  DGDL(1, "Exiting method.");
  return(0);
}


int AllocSchema()
{

  int i,j;
  DGDL(1, "Method called.");
#if 0
//1 dataModel for DeviceType
  if (( DeviceType.dataModel = (dataModelStruct*) malloc (1* sizeof(dataModelStruct))) == NULL) {
    perror("dataModel alloc");
    return(1);
  }
//array of pointers for attrSchema in dataModel

  if (( DeviceType.dataModel.attrSchema = (attributeSchemaStruct**) malloc (MAX_ATTRS *sizeof(attributeSchemaStruct*))) == NULL) {
    perror("attrSchema alloc");
    return(1);
  }

//array of attrSchema in dataModel
  for (i=0; i<MAX_ATTRS; i++) {
    if (( DeviceType.dataModel.attrSchema[i] = malloc (sizeof(attributeSchemaStruct))) == NULL) {
      perror("attrSchema alloc");
      return(1);
    }
  }

//1 dataModel for deviceType

  if (( DeviceType.dataModel.deviceType = (deviceTypeStruct*) malloc (sizeof(deviceTypeStruct))) == NULL) {
    perror("deviceType alloc");
    return(1);
  }

#endif

//array of pointers for objSchema in dataModel
  if (( objSchema = (objSchemaStruct**) malloc (MAX_OBJ *sizeof(objSchemaStruct*))) == NULL) {
    ERROR ("objSchema alloc");
    DGDL(1, "Exiting method.");
    return(1);
  }  

//array of objSchema 

  for (i=0; i<MAX_OBJ; i++) {
    if ( (objSchema[i] = (objSchemaStruct*) malloc (sizeof (objSchemaStruct))) == NULL ) {
      ERROR ("ObjSchema array alloc");
      DGDL(1, "Exiting method.");
      return(1);
    }
    memset (objSchema[i],0,  sizeof(objSchemaStruct));
  }


//array of pointers for parameterSchema in objSchema
  for (i=0; i<MAX_OBJ; i++) {
    if (( objSchema[i]->parameterSchema = (parameterSchemaStruct**) malloc (MAX_PARAMS *sizeof(parameterSchemaStruct*))) == NULL) {
      ERROR ("parameterSchema alloc");
      DGDL(1, "Exiting method.");
      return(1);
    }
  }
//array of parameterSchema in objSchema
  for (i=0; i<MAX_OBJ; i++) {
    for (j=0; j<MAX_PARAMS; j++) {
      if (( objSchema[i]->parameterSchema[j] = (parameterSchemaStruct*) malloc (sizeof(parameterSchemaStruct))) == NULL) {
        ERROR ("parameterSchema array alloc");
        DGDL(1, "Exiting method.");
        return(1);
      }
      memset(objSchema[i]->parameterSchema[j], 0, sizeof(parameterSchemaStruct)); 
    }
  }
  DGDL(1, "Exiting method.");
  return(0);
}

/* free memory in this order -
 * parameter data 
 * array of pointers for parameter structs
 * object data 
 * array of object pointers
 */

int FreeSchema(void)
{
  int i,j;
  DGDL(1, "Method called.");  
  for (i=0; i<MAX_OBJ; i++) {
    for (j=0; j<MAX_PARAMS; j++) {
      free(objSchema[i]->parameterSchema[j]);
    }
  }
  for (i=0; i<MAX_OBJ; i++) {
    free(objSchema[i]->parameterSchema);
  }

  for (i=0; i<MAX_OBJ; i++) {
    free(objSchema[i]);
  }
  free(objSchema);
  DGDL(1, "Exiting method.");
  return(0);
}

/* 
 * see description for GetElementsData 
 * This routine is similar - it parses the xml tree from the schema file.
 */

//static int GetElementsSchema (xmlNode * a_node, int obj_index, int depth, const char *obj_path, const char *obj_path_inst)
int GetElementsSchema (xmlNode * a_node, int obj_index, int depth, const char *obj_path, const char *obj_path_inst)
{
  xmlNode *cur_node = NULL;
  xmlChar *param_type, *param_name, *param_len, *array_type, *param_minval, *param_maxval ;
  xmlChar *param_vary, *param_write, *param_inform;
  param_write = param_inform = param_vary = NULL;  
  param_type = param_name = param_len = array_type = param_minval = param_maxval =NULL;
  xmlChar *attr_name, *attr_type, *attr_len, *attr_minval, *attr_maxval,  *attr_array;
  attr_name = attr_type = attr_len = attr_minval = attr_maxval = attr_array = NULL;
  char obj_full_path[OBJ_NAME_LEN], obj_full_path_inst[OBJ_NAME_LEN] ;
  char param_full_path[OBJ_NAME_LEN], param_full_path_inst[OBJ_NAME_LEN];
  const char *path, *path_inst;  
  static int attr_count =0, attr_flag=0 ;
  int param_count =0,   param_index =0;
  static int root_obj_found =0;    //indicate that root obj was found at some level.
  path = obj_path; 
  static int obj_count =0;  
  path_inst = obj_path_inst; 
	int err;
	int obj_index_parent =0;
  DGDL (1, "Method Called.");   

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {   // a child node that is an element comes here
      if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"parameterType"))) {
        param_type = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(objSchema[obj_index]->parameterSchema[param_index]->ParameterType,(const char *)param_type);

   //     printf("param name %s type %s param_index %d obj_count %d obj_index %d \n", param_name,param_type,param_index, obj_count, obj_index);

        if (!strcmp((const char *)param_type,(const char *)"object" )) { //this is an object
          //printf("found object at node 0x%x depth %d count %d\n",cur_node,depth,obj_count);
          /* NOTE: we're assuming that param_name is already known. the data currently is 
           * in this order. param_name, param_type, parm_length
           * if the order changes, this will have to be done where we check for param_name.
           * here the type would be known
           */
          if (!param_name) {
            DGDL(1, "Error. param_name is NULL for object");
            printf("Error. param_name is NULL for object");
            exit(1);
          }
          if (!root_obj_found) {    //first object is root object
            memset (obj_full_path, 0, sizeof (obj_full_path));
	    memset (obj_full_path_inst, 0, sizeof (obj_full_path_inst));
            sprintf(obj_full_path,"%s.",param_name);
            sprintf(obj_full_path_inst,"%s.",param_name);
            //printf("root object %s\n",obj_full_path);
            path = obj_full_path;
            path_inst = obj_full_path_inst;
            DGDL (2, "path =%s path_inst =%s", path, path_inst);
            root_obj_found =1;  
            //copy root object to the same index as it is the first object
            strcpy(objSchema[obj_index]->ObjName, obj_full_path);
            // root object is a parameter of type object
	    strcpy(PAR_NAME(obj_index,param_index),obj_full_path);

						//do same for instance fields of both
            strcpy(objSchema[obj_index]->ObjNameInst, obj_full_path_inst);
						strcpy(PAR_NAME_INST(obj_index,param_index),obj_full_path_inst);

            obj_count++;
            obj_index = obj_count-1;
          } else {	//non root object 
            sprintf(obj_full_path,"%s%s.",obj_path,param_name);
            //printf("Non root object %s\n",obj_full_path);

						//copy parameter name to current obj index
						strcpy(PAR_NAME(obj_index,param_index),obj_full_path);
						/*IMPORTANT if this object is an array we will need to copy the instance to this 
						 * index so save this index. we can keep this on the stack as the array check
						 * will occur at this depth! this is basically index of its parent object.
						 */
						obj_index_parent = obj_index; 
            path = obj_full_path;
            obj_count++;
            obj_index = obj_count-1;
					//copy object name to new object index
            strcpy(objSchema[obj_index]->ObjName, obj_full_path);
	//defer copy into object instance name until we see <array>
					} 
				} else {  //not object type
          sprintf(param_full_path,"%s%s",obj_path,param_name);
          strcpy(PAR_NAME(obj_index,param_index),param_full_path);
#if 1
				//parameters cannot be array type. copy the name along with the full instance
				//path passed down from the parent to the parameter instance name.	
          sprintf(param_full_path_inst,"%s%s",obj_path_inst,param_name);
          strcpy(PAR_NAME_INST(obj_index,param_index),param_full_path_inst);
#endif
        }
        
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"parameter"))) {
        // reallocate for a parameter if we exceeded limit
        ;
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"parameterName"))) {
        param_name = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        /*since a name is compulsory for a parameter, and this comes first we increment 
         * count here
         *rest is handled when we see the type
         */
          param_count = objSchema[obj_index]->no_of_params;
          param_count++;
          param_index = param_count -1;
          objSchema[obj_index]->no_of_params = param_count;
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"array"))) {
        /* usually set to true/false for an object. since the object name and 
         * type have already been discovered, and this is at the same depth as those, 
         * we can copy into the object index.
         */
        array_type = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(objSchema[obj_index]->ArrayType, (const char *)array_type);
        strcpy(objSchema[obj_index]->parameterSchema[param_index]->ParameterArray,(const char *)array_type);
#if 1		//array instance creation 

				if (param_type && !strcmp((const char *)param_type ,(const char *)"object" )) { //this is an object
					sprintf(obj_full_path_inst,"%s%s.",obj_path_inst,param_name);
					path_inst = obj_full_path_inst;

					if (!strcmp((const char*)array_type,(const char*)"true"))  {
						// shouldn't come here for root node - it cannot be an array (we presume)
						if (!obj_index) {
							ERROR ("<array> is true for root node. param_name %s obj_index %d\n",param_name, obj_index);
						   DGDL(1, "Exiting method.");	
                                                   return(-1);
						}

						strcat(obj_full_path_inst,INSTANCE);
						/* why are we copying to obj_parent here ?
						 * we found the object type earlier and deferred copying to its instance until 
						 * we looked at its array attribute. the object is also a parameter in its 
						 * parent list of parameters and we need to copy it there too. since the 
						 * obj_index was
						 * incremented when we discovered the type, we have to copy to the index of 
						 * the parent which we saved earlier.
						 */
						strcpy(PAR_NAME_INST(obj_index_parent,param_index),obj_full_path_inst);
						//copy to object instance
						strcpy(objSchema[obj_index]->ObjNameInst, obj_full_path_inst);
					} else if (obj_index != 0)  {	//not array type and not root node 
						/* copy the regular name to parameter instance name. reason for obj_index -1 above
						 * for the root node, we will have obj_index =0.  since this node has no
						 *	parent, we must skip this to avoid -ve index
						 */
						strcpy(PAR_NAME_INST(obj_index_parent,param_index),obj_full_path);
						//copy regular name to object instance too
						strcpy(objSchema[obj_index]->ObjNameInst, obj_full_path);
					}
				} else {		//not an object
					;		//<array> may appear for attributes in the xml file .ignore
				}
#endif

      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"parameterLength"))) {
        param_len = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(objSchema[obj_index]->parameterSchema[param_index]->ParameterLen,(const char *)param_len);
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"parameterWriteable"))) {
        param_write = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(objSchema[obj_index]->parameterSchema[param_index]->ParameterWriteable,(const char *)param_write);
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"parameterInform"))) {
        param_inform = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(objSchema[obj_index]->parameterSchema[param_index]->ParameterInform,(const char *)param_inform);
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"parameterVary"))) {
        param_vary = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(objSchema[obj_index]->parameterSchema[param_index]->ParameterVary,(const char *)param_vary);
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"attribute"))) {
        if (attr_flag) {
          attr_count++;
          attr_count_schema = attr_count;
        } else {
          attr_flag =1;
        }

      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"attributeName"))) {
        attr_name = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(attrSchema[attr_count].AttributeName,(const char *)attr_name);
        xmlFree(attr_name);
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"attributeType"))) {
        attr_type = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(attrSchema[attr_count].AttributeType,(const char *)attr_type);
        xmlFree(attr_type);
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"array"))) {
        attr_array = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(attrSchema[attr_count].AttributeArray,(const char *)attr_array);
        xmlFree(attr_array);
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"attributeLength"))) {
        attr_len = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
        strcpy(attrSchema[attr_count].AttributeLen,(const char *)attr_len);
        xmlFree(attr_len);
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"minValue"))) {
        if (attr_name) { //for an attribute
          attr_minval = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
          strcpy(attrSchema[attr_count].AttributeMinVal,(const char *)attr_minval);
          xmlFree(attr_minval);
        } else if (param_name) {
          param_minval = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
          strcpy(objSchema[obj_index]->parameterSchema[param_index]->ParameterMinVal,(const char *)param_minval);
        }
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"maxValue"))) {
        if (attr_name) { //for an attribute
          attr_maxval = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
          strcpy(attrSchema[attr_count].AttributeMaxVal,(const char *)attr_maxval);
          xmlFree(attr_maxval);
        } else if (param_name) {
          param_maxval = xmlNodeListGetString(docSchema, cur_node->xmlChildrenNode, 1);
          strcpy(objSchema[obj_index]->parameterSchema[param_index]->ParameterMaxVal,(const char *)param_maxval);
        }
      } 
    }
    if (cur_node->children && (cur_node->children->type != XML_TEXT_NODE ||  cur_node->children->next != NULL) ) {
      err = GetElementsSchema (cur_node->children, obj_index, depth+1, path, path_inst);
			if (err == -1) return(err);
    }
  }

  if (param_name)
    xmlFree(param_name);
  if (param_type)
    xmlFree(param_type);
  if (param_len)
    xmlFree(param_len);

//  printf("obj_index %d param_index %d param_name %s param_type %s param_len %s\n",obj_index, param_index, PAR_NAME(obj_index,param_index),PAR_TYPE(obj_index,param_index),PAR_LEN(obj_index,param_index));
  DGDL (2, "obj_count =%d", obj_count);
  DGDL (1, "Exiting method.");  
  return (obj_count);
}

objDataStruct**  AllocData(void)
{

  int i,j,k;
  objDataStruct ** Data;
  DGDL (1, "Method called.");
//array of pointers for objData 
  if (( Data = (objDataStruct**) malloc (MAX_OBJ *sizeof(objDataStruct*))) == NULL) {
    perror("objData alloc");
    DGDL(1, "Exiting method.");
    return(NULL);
  }  

//array of objSchema 

  for (i=0; i<MAX_OBJ; i++) {
    if ( (Data[i] = (objDataStruct*) malloc (sizeof (objDataStruct))) == NULL ) {
      perror("ObjData array alloc");
      DGDL(1, "Exiting method.");
      return(NULL);
    }
    memset (Data[i],0,  sizeof(objDataStruct));
  }


//array of pointers for parameterData in objData
  for (i=0; i<MAX_OBJ; i++) {
    if (( Data[i]->parameterData = (parameterDataStruct**) malloc (MAX_PARAMS *sizeof(parameterDataStruct*))) == NULL) {
      perror("parameterData alloc");
      DGDL(1, "Exiting method.");
      return(NULL);
    }
  }
//array of parameterData
  for (i=0; i<MAX_OBJ; i++) {
    for (j=0; j<MAX_PARAMS; j++) {
      if (( Data[i]->parameterData[j] = (parameterDataStruct*) malloc (sizeof(parameterDataStruct))) == NULL) {
        perror("parameterData array alloc");
        DGDL(1, "Exiting method.");
        return(NULL);
      }
      memset(Data[i]->parameterData[j], 0, sizeof(parameterDataStruct)); 
    }
  }

//array of pointers for attrData in ParameterData

  for (i=0; i<MAX_OBJ; i++) {
    for (j=0; j<MAX_PARAMS; j++) {
      if (( Data[i]->parameterData[j]->attrData = (attributeDataStruct**) malloc (MAX_ATTRS *sizeof(attributeDataStruct*))) == NULL) {
        perror("attrData pointer array alloc");
        DGDL(1, "Exiting method.");
        return(NULL);
      }
    }
  }  

//array of attrData
  for (i=0; i<MAX_OBJ; i++) {
    for (j=0; j<MAX_PARAMS; j++) {
      for (k=0; k<MAX_ATTRS; k++) {
        if (( Data[i]->parameterData[j]->attrData[k] = (attributeDataStruct*) malloc (sizeof(attributeDataStruct))) == NULL) {
          perror("attrData alloc");
          DGDL(1, "Exiting method.");
          return(NULL);
        }
      }
    }
  }
  
  DGDL (1, "Exiting method.");
  return (Data);

}

/* free memory in this order -
 * parameter data 
 * array of pointers for parameter structs
 * object data 
 * array of object pointers
 */

int FreeData (objDataStruct** Data)
{
  int i,j,k;
  DGDL (1, "Method called.");
  // free memory for attrData and the pointer that holds it
  for (i=0; i<MAX_OBJ; i++) {
    for (j=0; j<MAX_PARAMS; j++) {
      for (k=0; k<MAX_ATTRS; k++) {
        free(Data[i]->parameterData[j]->attrData[k]);
      }
      free(Data[i]->parameterData[j]->attrData);
    }
  }

  // free memory for paramaterData and the pointer that holds it
  for (i=0; i<MAX_OBJ; i++) {
    for (j=0; j<MAX_PARAMS; j++) {
      free(Data[i]->parameterData[j]);
    }
    free(Data[i]->parameterData);
  }

  for (i=0; i<MAX_OBJ; i++) {
    free(Data[i]);
  }
  free(Data);
  DGDL (1, "Exiting method.");
  return(0);
}

/* print out objects and parameters under it as input from the schema xml file
 */

int PrintObjsSchema(void)
{
  int i,j,no_params ;
  DGDL (1, "Method Called");
  DGDL (1, "SCHEMA");
  DGDL (4, "Total no of objects =%d\n", obj_count_schema);
  for (i=0; i<obj_count_schema; i++) {
    no_params = objSchema[i]->no_of_params;
    DGDL (4, "%d ObjName %s ObjNameInst %s array %s param_count %d \n", i, objSchema[i]->ObjName,objSchema[i]->ObjNameInst,  objSchema[i]->ArrayType, no_params );
    for (j=0; j<no_params ; j++) {
      DGDL (4, "i %d j %d ParamName %s ParamNameInst %s ParamType %s ParamLen %s MinVal %s MaxVal %s Array %s Vary %s Writeable %s \n", i, j, PAR_NAME(i,j), PAR_NAME_INST(i,j), PAR_TYPE(i,j), PAR_LEN(i,j), PAR_MINVAL(i,j), PAR_MAXVAL(i,j), PAR_ARR(i,j), PAR_VARY(i,j), PAR_WRITE(i,j) );
    }
  }
  DGDL (1, "Exiting method.");
  return(0); 
}

//print schema into files in a certain format
//
int PrintSchemaFiles (void)
{
  int i,j,no_params, total_no_params=0, total_all_paths=0, array,object, min; 
  unsigned int max;
  FILE *fp_all, *fp_params, *fp_types;
  char *read_or_write, *inform;
  int skip_to_offset = SKIP_TO_OFFSET+1;
  DGDL (1, "Method Called");  
   //all objects and parameters with their full path
  DGDL (2, "SCHEMA_ALL_PATHS =%s", buf_SCHEMA_ALL_PATHS);
  if ((fp_all = fopen(buf_SCHEMA_ALL_PATHS,"w+")) == NULL) 
  {
    ERROR("error in fopen  long path %s\n",buf_SCHEMA_ALL_PATHS);
    DGDL(1, "Exiting method.");
    return(1);
  }
   
  //skip a few bytes - it will be used to enter the # of parameters after the file is done.
  if ( (fseek(fp_all,skip_to_offset, SEEK_SET)) == -1) {
    perror("fseek");
    ERROR ("error in fseek");
    DGDL(1, "Exiting method.");
    return(1);
  }

  DGDL (2, "SCHEMA_PARAM_PATHS =%s", buf_SCHEMA_PARAM_PATHS);
  if ((fp_params = fopen(buf_SCHEMA_PARAM_PATHS,"w+")) == NULL)
  {
    ERROR("error in fopen  long path %s\n",buf_SCHEMA_PARAM_PATHS);
    DGDL(1, "Exiting method.");
    return(1);
  }

  //skip a few bytes - it will be used to enter the # of parameters after the file is done
  if ( (fseek(fp_params,skip_to_offset, SEEK_SET)) == -1) {
    perror("fseek");
    ERROR ("error in fseek");
    DGDL(1, "Exiting method.");
    return(1);
  }
  //parameter types (object , string) ..                                                                                                 //has one to one correspondence with SCHEMA_ALL_PATHS
  DGDL (2, "SCHEMA_PARAM_TYPES =%s", buf_SCHEMA_PARAM_TYPES);
  if ((fp_types = fopen(buf_SCHEMA_PARAM_TYPES,"w+")) == NULL)
  {
    ERROR("error in fopen  long path %s\n",buf_SCHEMA_PARAM_TYPES);
    DGDL(1, "Exiting method.");
    return(1);
  }

  //skip a few bytes - it will be used to enter the # of parameters after the file is done.
  if ( (fseek(fp_types ,skip_to_offset, SEEK_SET)) == -1) {
    perror("fseek");
    ERROR ("error in fseek for fp_types");
    DGDL(1, "Exiting method.");
    return(1);
  }


  for (i=0; i<obj_count_schema; i++) {
    no_params = objSchema[i]->no_of_params;
    for (j=0; j<no_params ; j++) {
      fprintf(fp_all, "%s \n",PAR_NAME_INST(i,j));
      total_all_paths++;
      array = (!strcmp(PAR_ARR(i,j), "true"))? 1:0;
      object = (!strcmp(PAR_TYPE(i,j), "object"))? 1:0;

      min = (!strcmp(PAR_TYPE(i,j),"string"))?0:(!strcmp(PAR_TYPE(i,j),"int"))?INT_MIN:(!strcmp(PAR_TYPE(i,j),"unsignedInt"))?0:0;
      max = (!strcmp(PAR_TYPE(i,j),"string"))?atoi(PAR_LEN(i,j)):(!strcmp(PAR_TYPE(i,j),"int"))?INT_MAX:(!strcmp(PAR_TYPE(i,j),"unsignedInt"))?UINT_MAX:(!strcmp(PAR_TYPE(i,j),"boolean"))?1:0;
      read_or_write = (!strcmp(PAR_WRITE(i,j),"true"))?"Write":"Read";
      inform = (!strcmp(PAR_INFORM(i,j),"true"))?"Yes":"No";

      /*
       * print vector/scalar, read/write,type, min value, max value, inform for objects 
       * and parameters
       * for object types, default is read. min and max and inform are NA
       */
      if (object) {
        read_or_write = "Read";
        
        fprintf (fp_types, "%s,%s,%s,%s,%s,%s\n",(array?"Vector":"Scalar"), read_or_write, PAR_TYPE(i,j), "NA", "NA", "NA");
      } else {
        fprintf (fp_types, "%s,%s,%s,%d,%u,%s\n",(array?"Vector":"Scalar"), read_or_write, PAR_TYPE(i,j), min, max, inform);
      }

      /* print only parameter paths no objects) in this format -
       * index=<parameter name with full path>
       * where index refers to the position of the parameter in the list of all objects and 
       * parameters (this list is input to gperf)
       */
      if (!object) {
//#if 0 //use this for printing out the hash 
        //fprintf(fp_params, "%d=%s\n",PublicHashFunc(PAR_NAME(i,j), strlen(PAR_NAME(i,j))) ,PAR_NAME(i,j));
//#else // use this for putting out the index  for each string
        fprintf(fp_params, "%d=%s\n",StringToIndexFunc(PAR_NAME_INST(i,j), strlen(PAR_NAME_INST(i,j))) ,PAR_NAME_INST(i,j));
        total_no_params++;
      }
    }
  }
/* add a line with the total no of parameters on the top of the file
 * the macro PFORMAT makes a format and uses that to print to the file. this is needed
 * because the # of lines is variable and so the format needs to calculate the no of spaces
 * after the stuff we print and put that out too.
 */
  DGDL (2, "total_no_params =%d", total_no_params);
  rewind(fp_params);
  PFORMAT(total_no_params,fp_params);
  fclose(fp_params);
  rewind(fp_all);
  rewind(fp_types);
  PFORMAT(total_all_paths,fp_all);
  PFORMAT(total_all_paths,fp_types);
  fclose(fp_all);
  fclose(fp_types);
  DGDL (1, "Exiting method.");
  return(0);
}

/* print out objects and parameters under it as input from the data xml file
 */
int PrintObjsData(objDataStruct **obj, int count)
{
  int i,j,k, no_params ,no_attrs;
  DGDL (1, "Method called.");
  DGDL (1, "DATA");
  //printf("obj_count %d\n",obj_count_data);
  DGDL (4, "obj_count %d\n", count);
  for (i=0; i<count; i++) {
    no_params = obj[i]->no_of_params;
    DGDL (4, "%d ObjName %s ObjNameInst %s param_count %d\n", i, obj[i]->ObjName, obj[i]->ObjNameInst, no_params);
    for (j=0; j<no_params ; j++) {
      no_attrs = MYPAR_ATTR_NO_DATA(obj[i],j);
      DGDL (4, "ParamName %s ParamNameInst %s  ParamVal %s no_attrs %d\n", MYPAR_NAME_DATA(obj[i],j), MYPAR_NAME_INST_DATA(obj[i],j), MYPAR_VAL_DATA(obj[i],j), no_attrs);
      for (k=0; k<no_attrs ; k++) {
        DGDL (4, "Attribute %s Val %s\n", MYATTR_NAME_DATA(obj[i],j,k), MYATTR_VAL_DATA(obj[i],j,k));
      }
    }
  }
 DGDL (1, "Exiting method.");
 return(0); 
}

/* check if an object name that is input is an array type based on schema definition 
 * for the object
 */
int CheckObj(char *name, short *arr)
{
    
  int i;
  *arr =0;
  DGDL (1, "Method called.");
  //printf ("CheckObj: name %s\n",name);
  for (i=0; i<obj_count_schema; i++) {
    if (!strcmp (objSchema[i]->ObjName,name)) {
      if (!strcmp(objSchema[i]->ArrayType,"true"))  {
        *arr = 1;
      } else {
        *arr =0;
      }
      DGDL(1, "Exiting method.");
      return(1);    //return 1 since its an object
    }
  }
  DGDL (1, "Exiting method."); 
  return(0);
}

/* This routine uses recursion to parse the xml tree. we start at the root and descend all 
 * nodes that have children, and their children and so on. At each node, the value of the 
 * element is extracted and stored a data structure.
 * we have - object -> parameters (some of these could be objects too) -> parameters ...
 * Each object (in our data structures) points to a list of parameter structs (this indicates
 * a leaf node). If the parameter (child) is an object, it is stored both as a parameter and 
 * as an object by itself. parameters may have attributes that must also be collected.
 * As the nodes are descended, we must remember the parent name and the count of objects 
 * and parameters
 * for the current object.
 * In order to collect the right data, we must also know what is found at the same depth and
 * what is not.
 */
int
GetElementsData (xmlNode * a_node, int obj_index, int depth, const char *obj_path, const char* obj_path_inst)
{
  xmlNode *cur_node = NULL;
  xmlChar *param_name, *param_val;
  param_name = param_val = NULL;
  xmlChar *attr_name, *attr_val;
  xmlChar *obj_instance =NULL;
  attr_name = attr_val = NULL;
  char obj_full_path[OBJ_NAME_LEN], obj_full_path_inst[OBJ_NAME_LEN] ;
  char param_full_path[OBJ_NAME_LEN],param_full_path_inst[OBJ_NAME_LEN];
  const char *path, *path_inst;  
  int attr_count =0 , attr_index =0;
  int param_count =0,   param_index =0;
  static int root_obj_found =0;    //indicate that root obj was found at some level.
  path = obj_path; 
  path_inst = obj_path_inst; 
  short array;
  static int obj_count;
  DGDL (1, "Method called.");  
#if 0
  // Open schema DB 
  if (OpenDB(DB_RDONLY)) {
    printf("DB for schema: open failed\n");
    return(1);
  }
#endif

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {   // a child node that is an element comes here
      if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"parameterName"))) {
        param_name = xmlNodeListGetString(docData, cur_node->xmlChildrenNode, 1);

        //printf("param name %s param_index %d obj_index %d \n", param_name,param_index,obj_index);
        if (root_obj_found) {  //root obj is not a param or obj under anything.
          param_count = objData[obj_index]->no_of_params;
          param_count++;
          param_index = param_count -1;
          objData[obj_index]->no_of_params = param_count;
        }
        if (!root_obj_found) {    //first object is root object
          memset (obj_full_path, 0, sizeof (obj_full_path));
          memset (obj_full_path_inst, 0, sizeof (obj_full_path_inst));
          /*both path and inst are identical for an object until an array type is found
           * somewhere in depth - then the instance is applied to all objects and params 
           *below it
           */ 
          sprintf(obj_full_path,"%s.",param_name);
          sprintf(obj_full_path_inst,"%s.",param_name);

          if (CheckObj(obj_full_path, &array)) {
            //printf("root object %s\n",obj_full_path);
            path = obj_full_path;
            path_inst = obj_full_path;
            root_obj_found =1;  
            strcpy(objData[obj_index]->ObjName, obj_full_path);
            strcpy(objData[obj_index]->ObjNameInst, obj_full_path_inst);
            obj_count++;
            obj_index = obj_count-1;
          }
        } else { //non root object or parameter
          sprintf(obj_full_path,"%s%s.",obj_path,param_name);
          sprintf(obj_full_path_inst,"%s%s.",obj_path_inst,param_name);

          if (CheckObj(obj_full_path, &array)) {
            //printf("Non root object %s\n",obj_full_path);
            //copy to list of params for current object before we change the obj index
#if 1    
            //copy the normal name to ParmaeterName
            strcpy(PAR_NAME_DATA(obj_index,param_index),obj_full_path);
          
            //put in instance # if the object is an array type
            if (array) { // copy to instance name of param (ParameterNameInst)
              objData[obj_index]->ArrayType = 1;
              //printf("object %s is an array according to schema\n",obj_full_path);
              strcat(obj_full_path_inst,INSTANCE);
              strcpy(PAR_NAME_INST_DATA(obj_index,param_index),obj_full_path_inst);
            } else {  //copy just the full object if not array. 
              strcpy(PAR_NAME_INST_DATA(obj_index,param_index),obj_full_path);
            } 
#else    //we have only 1 name and always copy name with instance to ParameterName only
            strcpy(PAR_NAME_DATA(obj_index,param_index),obj_full_path);
#endif
            path = obj_full_path;
            path_inst = obj_full_path_inst;
            obj_count++;
            obj_index = obj_count-1;
            //copy to a new object index as this is a new object
            strcpy(objData[obj_index]->ObjName, obj_full_path);
            //copy  object instance
            strcpy(objData[obj_index]->ObjNameInst, obj_full_path_inst);
          } else {  //not object type
            sprintf(param_full_path,"%s%s",obj_path,param_name);

            sprintf(param_full_path_inst,"%s%s",obj_path_inst,param_name);
          
            //printf("param_name %s inst %s\n",param_full_path, param_full_path_inst);
            strcpy(PAR_NAME_DATA(obj_index,param_index),param_full_path);

            strcpy(PAR_NAME_INST_DATA(obj_index,param_index),param_full_path_inst);
  
          }
        }  
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"instance"))) {
        //instance of an object
        obj_instance = xmlNodeListGetString(docData, cur_node->xmlChildrenNode, 1);
        DGDL (2, "object %s has instance \n", obj_full_path);
        printf("object %s has instance \n",obj_full_path);
        objData[obj_index]->instance = atoi((const char *)obj_instance);
        strcat(objData[obj_index]->ObjName,(char const *) obj_instance);
        strcat(objData[obj_index]->ObjName,".");

      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"attributeValue"))) {
        ;
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"attributeName"))) {
/* why is param_index  set here ?
 * attr name and value are at the same depth and param_index is on the stack.so we need
 * to extract the right param_index from objData to use for attr name and val
 */
        param_index = objData[obj_index]->no_of_params -1;
/* we need to get the attr count for the param that it goes with. attr and param are diff
 * depth levels, so we need to extract it 
 * we dont do this when attributeValue is found as that is at a higher level than this. and
 * then attr value cant use it.
 */
        attr_count = PAR_ATTR_NO_DATA(obj_index,param_index); 
        attr_count++;
        attr_index = attr_count -1;
        PAR_ATTR_NO_DATA(obj_index,param_index) = attr_count;

        attr_name = xmlNodeListGetString(docData, cur_node->xmlChildrenNode, 1);
        strcpy(ATTR_NAME_DATA(obj_index,param_index,attr_index),(char const *)attr_name);
      } else if ((!xmlStrcmp(cur_node->name, (const xmlChar *)"value"))) {
        if (attr_name) {
          attr_val = xmlNodeListGetString(docData, cur_node->xmlChildrenNode, 1);
          strcpy(ATTR_VAL_DATA(obj_index,param_index,attr_index),(char const *)attr_val);
        } else if (param_name) {
/*if the value belongs to a param, it is found at the same depth as the param name itself,
 * so it will use that value. we dont need to set it here
 */
          param_val = xmlNodeListGetString(docData, cur_node->xmlChildrenNode, 1);
          strcpy(PAR_VAL_DATA(obj_index,param_index),(char const *)param_val);
        }
      }
      if (cur_node->children && (cur_node->children->type != XML_TEXT_NODE ||  cur_node->children->next != NULL) ) {
        GetElementsData (cur_node->children, obj_index, depth+1, path, path_inst);
      }
    }
  }

  if (param_name)
    xmlFree(param_name);
  if (attr_name)
    xmlFree(attr_name);
  if (attr_val)
    xmlFree(attr_val);

//  printf("obj_index %d param_index %d param_name %s \n",obj_index, param_index, PAR_NAME_DATA(obj_index,param_index));
  DGDL (2, "obj_count =%d", obj_count);
  DGDL (1, "Exiting method.");
  return (obj_count);

}

/* Convert a SOAP data type into a value that is easier to check */

int GetSoapDataType (char *s)
{
  soapDataType i;
  DGDL (1, "Method called.");
  for (i=0; i<INVALID; i++) {
    if (!strcmp(s,"unknown"))
      return(UNKNOWN);
    else if (!strcmp(s,"object")) 
      return(OBJECT);
    else if (!strcmp(s,"multiple"))
      return(MULTIPLE);
    else if (!strcmp(s,"string"))
      return(STRING);
    else if (!strcmp(s,"int"))
      return(INT);
    else if (!strcmp(s,"unsignedInt"))
      return(UNSIGNED_INT);
    else if (!strcmp(s,"boolean"))
      return(BOOLEAN);
    else if (!strcmp(s,"dateTime"))
      return(DATETIME);
    else if (!strcmp(s,"base64"))
      return(BASE64);
    else  {
      printf("invalid input ");
      return(-1);
    }
  }
  DGDL (1, "Exiting method.");
  return(0);  
}

void create_index_to_hash_array_table() {
  
  int array_size = 0;
  int i,j;
  int no_params;
  int hash_code;
  FILE *fp_gperf = NULL;
  char tmp_buffer[16096] = "\0";

  DGDL(1, "Method called");

  for (i=0; i<obj_count_schema; i++) {
    no_params = objSchema[i]->no_of_params;
    for (j=0; j<no_params ; j++) {
      DGDL(4, "Getting hash code of parameter(%d, %d), name = %s", i, j, PAR_NAME_INST(i,j));
      hash_code = PublicHashFunc(PAR_NAME_INST(i,j), strlen(PAR_NAME_INST(i,j))); // TODO Manpreet
      sprintf(tmp_buffer, "%s %d,", tmp_buffer, hash_code);
      array_size++;
    }
  }

  tmp_buffer[strlen(tmp_buffer) - 1] = '\0';

  DGDL(1, "total_params = %d", array_size); 
  DGDL(2, "IndextoHashArray = %s", tmp_buffer); 

  if ( (fp_gperf = fopen(buf_GPERF_HASH_CFILE,"a")) == NULL) {
     ERROR("error in fopen %s\n", buf_GPERF_HASH_CFILE);
     exit(-1);
  }
   
  fprintf(fp_gperf, "int total_params = %d;\n", array_size);
  fprintf(fp_gperf, "int IndextoHashArray[%d] = {%s};\n", array_size, tmp_buffer);
  
  fclose(fp_gperf);
  DGDL(1, "Exiting method.");

}


/* Routine does the following 
 * 1. creates an input file read by gperf. The file has a few sections (see gperf manual)
 * sections are separated by delimiters
 * section1 - includes or declarations (these are copied to the output file by gperf)
 * section2 - list of names or structures (we use a structure that has 2 members here- name and
 * index). this part is generated by iterating over objData and printing out their parameters.
 * section3 - routines that are copied to the output of gperf 
 * a list of parameter names in this format-
 * name,i (i=0,1,2 ..)
 * 2. runs gperf on above input and generates a .c file that contains hashing routines created
 * by gperf as well as the those added in (1) by us
 * compiles and links the .c to create a shared object
 * 3. makes the functions in the library callable through function pointers
 */


int MakeHashLib (void)
{
  int i,j,no_params;
  int param_count =0;
  char buf[BUFSIZ];
  FILE *cmd_fp, *fp_gperf, *fp_gperfC ;
  void* handle;
  char* error;
  struct stat stbuf;
  DGDL (1, "Method called.");
  //if library exists, just use it - helps in testing if there is no other change
  if ( (stat(buf_GPERF_HASH_LIB , &stbuf) == 0) ) {
    DEBUG2("shared library exists, skipping creation..\n");
    goto DLOPEN;
  }

  if ( (fp_gperf = fopen(buf_GPERF_SCHEMA_PARAM_PATHS,"w+")) == NULL) {
    ERROR("error in fopen %s\n",buf_GPERF_SCHEMA_PARAM_PATHS);
    DGDL(1, "Exiting method.");
    return(1);
  }
  //write out first section that gets inserted into output directly
  fprintf (fp_gperf, "%%{\n");
  i=0;
  //gperfSection1 is an array that contains various includes and declarations that must go into
  //the file that gperf generates.
  while (strcmp(gperfSection1[i],"END")) {  
    fprintf(fp_gperf, "%s\n",gperfSection1[i++]);
  }
  fprintf(fp_gperf, "%%}\n");
  fprintf(fp_gperf, "%s\n",GPERF_INP_STRUCT_DEFN);
  fprintf(fp_gperf, "%%%%\n");

  for (i=0; i<obj_count_schema; i++) {
    no_params = objSchema[i]->no_of_params;

    for (j=0; j<no_params ; j++) {
      //hash could contain everything - objects and params
      //object = (!strcmp(PAR_TYPE(i,j), "object"))? 1:0;
      //if (!object) 
      DGDL(4, "Getting hash code of parameter(%d, %d), name = %s", i, j, PAR_NAME_INST(i,j));      
        fprintf(fp_gperf, "%s,%d\n",PAR_NAME_INST(i,j), param_count++);
    }
  }
  fprintf(fp_gperf, "%%%%\n");
 
 /*Function to store hash value in a IndexToHashCode array.*/
  //copy othe/ir C routines into the last section.
  if ( (fp_gperfC = fopen(gperf_croutines_inp_file,"r")) == NULL) {
    ERROR("error in fopen %s\n",gperf_croutines_inp_file);
    DGDL(1, "Exiting method.");
    return(1);
  }
  
  while ( fgets(buf, BUFSIZ, fp_gperfC)) {
    if (feof(fp_gperfC)) break;
    fputs (buf, fp_gperf);
  }
  fclose(fp_gperfC);
  fclose(fp_gperf);
// run gperf using input file
// -t is used for using a struct definition 
  memset(buf, 0, sizeof(buf));
   
  sprintf(buf, "gperf -cCGt --output-file=%s %s",buf_GPERF_HASH_CFILE,buf_GPERF_SCHEMA_PARAM_PATHS);
  
  DEBUG2("gperf command %s \n",buf);

  if ( (cmd_fp = popen(buf, "r")) == NULL) {
    ERROR ("error in calling popen for %s\n", buf);
    DGDL(1, "Exiting method.");
    return(1); 
  }
  pclose(cmd_fp);
  memset(buf, 0, sizeof(buf));
  
  sprintf(buf, "gcc -g -DTR069_INDEX_TO_HASH_NOT_DONE=1 -m%d -fpic -shared -o %s %s", NS_BUILD_BITS, buf_GPERF_HASH_LIB, buf_GPERF_HASH_CFILE);
  DEBUG2( "compilation command %s \n",buf);

  if ( (cmd_fp = popen(buf, "r")) == NULL) {
    ERROR ("error in calling popen for %s\n", buf);
    exit (-1);
  }
  pclose(cmd_fp);

  //make sure we did nt fail to create the lib before dlopen
  if ( (stat(buf_GPERF_HASH_LIB , &stbuf) != 0) ) {
    ERROR("shared library does not exist - create probably failed\n");
    DGDL(1, "Exiting method.");
    return(1);
  }



DLOPEN:
// open library and get the handle
  handle = dlopen (buf_GPERF_HASH_LIB, RTLD_LAZY);
  if ( (error = dlerror()) ) {
    /* If so, print the error message and exit. */
    ERROR ("dlopen of %s error =%s\n", buf_GPERF_HASH_LIB, error);
    DGDL(1, "Exiting method.");
    return (1);
  }

  //public hash function - the hash in gperf is not exported 
  *(void**) &PublicHashFunc = dlsym(handle, PUBLIC_HASH_FUNC_NAME);

  if ( (error = dlerror()) ) {
    /* If so, print the error message and exit. */
    ERROR ("%s\n", error);
    DGDL(1, "Exiting method.");
    return (1);
  }

  *(void**) &HashToStringFunc = dlsym(handle, HASH_TO_STRING_FUNC_NAME);

  if ((error = dlerror())) {
    /* If so, print the error message and exit. */
    ERROR ("%s\n", error);
    DGDL(1, "Exiting method.");
    return (1);
  }

  *(void**) &StringToIndexFunc = dlsym(handle, STRING_TO_INDEX_FUNC_NAME);

  if ((error = dlerror())) {
    /* If so, print the error message and exit. */
    ERROR ("%s\n", error);
    DGDL(1, "Exiting method.");
    return (1);
  }

  create_index_to_hash_array_table();
  DGDL (1, "Exiting method.");
  return(0);
  
}
