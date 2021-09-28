#ifndef _CPE_GEN_DATA_H

#include <stdio.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <getopt.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>



#define OBJ_NAME_LEN      256

#define PARAM_NAME_LEN    256
#define PARAM_VALUE_LEN   256      //max length of the value of param in bytes, depends on schema

#define MAX_PARAMS 64
#define MAX_OBJ 96

#define MAX_ATTRS 24
#define ATTR_NAME_LEN 64
#define ATTR_VALUE_LEN 64    //size of attribute value -depends on the size as per schema

#define OUTFILE_NAME_SIZE 64 // output file to which we write schema
#define DUMMY_PARAM_VAL_SUFFIX  "_val"    //prefix added to dummy values for params
#define DUMMY_ATTR_VAL_SUFFIX  "_val"
#define DEFAULT_NOTIFICATION_VAL "0"
#define DEFAULT_ACCESSLIST_VAL  "NA"
//
//defns from schema here

#define PARAM_TYPE_LEN 16    //string, dateTime, int etc.
#define PARAM_LEN 4    //#of bytes required to hold the length value as string (eg. 64)
#define ATTR_LEN 8      // #of bytes required to hold the length as a string.
#define TRUE_FALSE_LEN  8  //length of true or false  = 5+1 max
#define ATTR_TYPE_LEN 12    //string, unsigned Int etc.
#define INSTANCE "1."
#define DELIM "+"        //delimiter between sets of CPE data in the same file
#define NVM_DATA_FILE        "tr069_cpe_data.dat"    //default nvm data file
#define CPE_DATA_INDEX      "tr069_cpe_data.index" //index where data for each cpe starts
//contains all object and parameters with their full paths
#define SCHEMA_ALL_PATHS    "tr069_schema_path.dat"
//corresponds to above file, and has other info about each parameter/object
#define SCHEMA_PARAM_TYPES  "tr069_schema_path_type.dat"
// path of all parameter names
#define SCHEMA_PARAM_PATHS  "tr069_full_paths.dat"
#define SKIP_TO_OFFSET 20      //skip bytes to leave space for #Total=xxx
#define PARAM_VALUE_TEMP ".param_value_temp"    //get start sequence no here


//macro to create a format based on a given max length of string and print it
//x = value to print (decimal), y = file ptr
#define PFORMAT(x,y) { \
char format[12];\
char buf[SKIP_TO_OFFSET];  \
sprintf(format,"%%-%ds\n",SKIP_TO_OFFSET);\
sprintf(buf,"#Total=%d",x); \
fprintf(y,format,buf); \
}



#define DEBUG1(x, args ...)  \
do {  \
if (debug_level >= 1) \
  fprintf(stderr, x, ## args);  \
} while(0)

#define DEBUG2(x, args ...)  \
do {  \
if (debug_level >= 2) \
  fprintf(stderr, x, ## args);  \
} while(0)

#define DEBUG3(x, args ...)  \
do {  \
if (debug_level >= 3) \
  fprintf(stderr, x, ## args);  \
  fprintf(stderr, " [%s, %s():%u]\n" x, __FILE__, __FUNCTION__ ,__LINE__, ## args);  \
} while(0)


// debug is always enabled
//#define DEBUGP(x, args ...)

#define ERROR(x, args ...)  \
do {  \
  fprintf(stderr, " [%s, %s():%u]\n" x, __FILE__, __FUNCTION__ ,__LINE__, ## args);  \
} while(0)

//char *paramVaryNames[] = {"InternetGatewayDevice.DeviceInfo.SerialNumber"};

//#define MAX_PARAMS_TO_VARY (sizeof(paramVaryNames)/sizeof(paramVaryNames[0]))

typedef enum {
  UNKNOWN,
  OBJECT,
  MULTIPLE,
  STRING,
  INT,
  UNSIGNED_INT,
  BOOLEAN,
  DATETIME,
  BASE64,
  INVALID
} soapDataType;


//for the attribute, min, max are known from the schema and their values can be
//generated from these
typedef struct {
  char AttributeName [ATTR_NAME_LEN] ;  //data and schema
  soapDataType AttributeType;  //taken from schema and converted to int here
  char AttributeVal [ATTR_VALUE_LEN] ;  //data
  unsigned int AttributeLen; //taken from schema and converted to int here
  int AttributeMinVal;  //taken from schema and converted to int here
  int AttributeMaxVal;  //taken from schema and converted to int here
  int  AttributeArray;  //0 or 1 for true/false will be set based on schema
}attributeDataStruct;

typedef struct {
  char ParameterName [PARAM_NAME_LEN] ; //name - data  as well as schema
  soapDataType ParameterType ; // taken from schema 
  char ParameterNameInst [PARAM_NAME_LEN] ; //name with instance
  char ParameterVal [PARAM_VALUE_LEN] ; //value of param
  int ParameterLen ;  //length of value 
  int ParameterMinVal ;  //min value in case of int
  int ParameterMaxVal ;  //max value
  int ParameterVary ;  // 1 or 0 - whether to vary ,taken from schema
  int no_of_attrs;
  attributeDataStruct **attrData;
}parameterDataStruct;



typedef struct {
  char ObjName[OBJ_NAME_LEN]; // sizes are known from xml data -- this is the key for object
  char ObjNameInst[OBJ_NAME_LEN]; // name with instance
  int ArrayType;      // 1 or 0  -taken from the schema if object data is given
  int no_of_params;    //no of params for this object
  int instance;  //instance of object
  parameterDataStruct **parameterData;  //array
}objDataStruct;

objDataStruct**objData ;
objDataStruct** mData;

static int  GetElementsData (xmlNode * , int , int, const char*, const char*);
objDataStruct **AllocData(void);
static int FreeData(objDataStruct **);

static int PrintObjsData(objDataStruct**, int);
xmlDoc *docData ;

static int  GetElementsSchema (xmlNode * , int , int, const char*, const char*);
static int AllocSchema(void);
static int FreeSchema(void);

static int PrintObjsSchema(void);
int PrintSchemaFiles (void);
xmlDoc *docSchema ;


#define PAR_NAME_DATA(x,y)      (objData[x]->parameterData[y]->ParameterName)
#define PAR_NAME_INST_DATA(x,y) (objData[x]->parameterData[y]->ParameterNameInst)
#define PAR_VAL_DATA(x,y)        (objData[x]->parameterData[y]->ParameterVal)
#define PAR_LEN_DATA(x,y)        (objData[x]->parameterData[y]->ParameterLen)
#define PAR_MINVAL_DATA(x,y)    (objData[x]->parameterData[y]->ParameterMinVal)
#define PAR_MAXVAL_DATA(x,y)    (objData[x]->parameterData[y]->ParameterMaxVal)
#define PAR_TYPE_DATA(x,y)      (objData[x]->parameterData[y]->ParameterType)
#define PAR_ATTR_NO_DATA(x,y)    (objData[x]->parameterData[y]->no_of_attrs)

#define MYPAR_VARY_DATA(x,y)      ((x)->parameterData[y]->ParameterVary)
#define MYPAR_NAME_DATA(x,y)      ((x)->parameterData[y]->ParameterName)
#define MYPAR_NAME_INST_DATA(x,y) ((x)->parameterData[y]->ParameterNameInst)
#define MYPAR_VAL_DATA(x,y)        ((x)->parameterData[y]->ParameterVal)
#define MYPAR_ATTR_NO_DATA(x,y)    ((x)->parameterData[y]->no_of_attrs)
#define MYPAR_LEN_DATA(x,y)        ((x)->parameterData[y]->ParameterLen)
#define MYPAR_TYPE_DATA(x,y)      ((x)->parameterData[y]->ParameterType)
#define MYPAR_MINVAL_DATA(x,y)    ((x)->parameterData[y]->ParameterMinVal)
#define MYPAR_MAXVAL_DATA(x,y)    ((x)->parameterData[y]->ParameterMaxVal)

#define ATTR_NAME_DATA(x,y,z)      (objData[x]->parameterData[y]->attrData[z]->AttributeName)
#define ATTR_VAL_DATA(x,y,z)      (objData[x]->parameterData[y]->attrData[z]->AttributeVal)
#define ATTR_MINVAL_DATA(x,y,z)    (objData[x]->parameterData[y]->attrData[z]->AttributeMinVal)
#define ATTR_MAXVAL_DATA(x,y,z)    (objData[x]->parameterData[y]->attrData[z]->AttributeMaxVal)

#define MYATTR_MINVAL_DATA(x,y,z) ((x)->parameterData[y]->attrData[z]->AttributeMinVal)
#define MYATTR_MAXVAL_DATA(x,y,z) ((x)->parameterData[y]->attrData[z]->AttributeMaxVal)
#define MYATTR_NAME_DATA(x,y,z)    ((x)->parameterData[y]->attrData[z]->AttributeName)
#define MYATTR_VAL_DATA(x,y,z)    ((x)->parameterData[y]->attrData[z]->AttributeVal)
#define MYATTR_ARRAY_DATA(x,y,z)  ((x)->parameterData[y]->attrData[z]->AttributeArray)

//
typedef struct {
  char ParameterName [PARAM_NAME_LEN] ;    // actual parameter name as in schema
  char ParameterNameInst [PARAM_NAME_LEN] ; //name with instance
  char ParameterType [PARAM_TYPE_LEN];  // int, string etc.
  char ParameterLen [PARAM_LEN];  //string containing the length - eg., 256
  char ParameterArray [TRUE_FALSE_LEN  ];    //true or false, if it is an object too
  char ParameterWriteable[TRUE_FALSE_LEN];    //writable - true or false
  char ParameterInform[TRUE_FALSE_LEN];      //true or false
  char ParameterMinVal [ATTR_VALUE_LEN] ;  // min value for case where param is a int
  char ParameterMaxVal [ATTR_VALUE_LEN] ;  // max value for case where param is a int
  char ParameterVary[PARAM_LEN]  ;  // 1 or 0 - whether to vary ,taken from schema
}parameterSchemaStruct;


typedef struct {
  char AttributeName [ATTR_NAME_LEN] ; //known from XML data model
  char AttributeType [ATTR_TYPE_LEN];  // known from XML
  char AttributeLen [ATTR_LEN];  // known from XML
  char AttributeMinVal [ATTR_VALUE_LEN] ; //known from XML data model
  char AttributeMaxVal [ATTR_VALUE_LEN] ; //known from XML data model
  char AttributeArray [TRUE_FALSE_LEN] ; //true or false
}attributeSchemaStruct;


typedef struct {
  char ObjName[OBJ_NAME_LEN]; // sizes are known from xml data -- this is the key for object
  char ObjNameInst[OBJ_NAME_LEN]; //instance name for arrays (contains instance in this case)
  int no_of_params;    //no of params for this object
  char ArrayType [TRUE_FALSE_LEN] ; //true or false
  parameterSchemaStruct **parameterSchema;  //array
}objSchemaStruct;

attributeSchemaStruct attrSchema[MAX_ATTRS];
objSchemaStruct **objSchema ;


#define PAR_TYPE(x,y)        (objSchema[x]->parameterSchema[y]->ParameterType)
#define PAR_LEN(x,y)        (objSchema[x]->parameterSchema[y]->ParameterLen)
#define PAR_MINVAL(x,y)      (objSchema[x]->parameterSchema[y]->ParameterMinVal)
#define PAR_MAXVAL(x,y)      (objSchema[x]->parameterSchema[y]->ParameterMaxVal)
#define PAR_NAME(x,y)        (objSchema[x]->parameterSchema[y]->ParameterName)
#define PAR_NAME_INST(x,y)   (objSchema[x]->parameterSchema[y]->ParameterNameInst)
#define PAR_VARY(x,y)        (objSchema[x]->parameterSchema[y]->ParameterVary)
#define PAR_ARR(x,y)        (objSchema[x]->parameterSchema[y]->ParameterArray)
#define PAR_WRITE(x,y)      (objSchema[x]->parameterSchema[y]->ParameterWriteable)
#define PAR_INFORM(x,y)      (objSchema[x]->parameterSchema[y]->ParameterInform)

#define MYPAR_TYPE(x,y)      ((x)->parameterSchema[y]->ParameterType)
#define MYPAR_LEN(x,y)      ((x)->parameterSchema[y]->ParameterLen)
#define MYPAR_NAME(x,y)      ((x)->parameterSchema[y]->ParameterName)

enum {
  ATTR_NOTIFICATION, 
  ATTR_ACESSLIST,
  ATTR_VISIBILITY ,
};

#define ATTR_NAME(x)      (attrSchema[x].AttributeName)
#define ATTR_TYPE(x)      (attrSchema[x].AttributeType)
#define ATTR_LENGTH(x)    (attrSchema[x].AttributeLen)
#define ATTR_MINVAL(x)    (attrSchema[x].AttributeMinVal)
#define ATTR_MAXVAL(x)    (attrSchema[x].AttributeMaxVal)
#define ATTR_ARRAY(x)      (attrSchema[x].AttributeArray)


//hash code stuff
//
#if ( (Fedora && RELEASE >= 14) || (Ubuntu && RELEASE >= 1204) )
  #define NS_BUILD_BITS 64
#else
  #define NS_BUILD_BITS 32
#endif
// first part of input file to gperf is copied directly to the C file
#define GPERF_INP_SECTION1 { printf("#include <string.h>\n"); }
char *gperfSection1[] = {"#include <stdio.h>", "#include <string.h>", "END"};
#define GPERF_INP_STRUCT_DEFN "struct paramNames {char *name; int index;};"

//input file into gperf with paths of all parameters
#define GPERF_SCHEMA_PARAM_PATHS "gperf_schema_paths.inp"
//C routines that will also go into the hash lib - this will be concated with the schema paths
//and (GPERF_SCHEMA_PARAM_PATHS) and input to gperf
#define GPERF_CROUTINES_INP        "gperf_croutines.inp"
//output of gperf - contains hash routines 
#define GPERF_HASH_CFILE          "tr069_hash_code.c"
// shareable lib formed from the above
#define GPERF_HASH_LIB            "./gperf_hash_lib.so" 
//gperf makes a hash function - this is static, so we have a public version of the same
#define PUBLIC_HASH_FUNC_NAME      "PublicHash"
//function to convert hash to string 
#define STRING_TO_HASH_FUNC_NAME  "StringToHash"
#define HASH_TO_STRING_FUNC_NAME  "HashToString"
#define STRING_TO_INDEX_FUNC_NAME  "StringToIndex"

//Hash Function declarations 

//1st arg = string to hash
//2nd arg = strlen
int (*PublicHashFunc) (const char*, unsigned int);

//arg = hash code
char* (*HashToStringFunc)(unsigned int);

//arg = string , len of string
int (*StringToIndexFunc) (char *s, unsigned int len);


//extern int generate_hash_table(char *file_name, char *hash_fun_name, Str_to_hash_code *str_to_hash_code_fun, Hash_code_to_str *hash_to_str_fun, int debug, char *base_dir);


#if 0

typedef struct  {
  int no_of_attrs;    //no of params for this object
  attributeSchemaStruct **attrSchema;
  objSchemaStruct **objSchema;
} dataModelStruct;

typedef struct {
  dataModelStruct *dataModel;
} deviceTypeStruct;

deviceTypeStruct DeviceType;

#endif

#endif   // _CPE_GEN_DATA_H
