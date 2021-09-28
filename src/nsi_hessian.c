
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "nslb_hessian.h"

#define HESSIAN_BINARY                   0
#define HESSIAN_DECODE_BINARY_TO_XML     1
#define HESSIAN_ENCODE_XML_TO_BINARY     2
#define XML_VALIDATE                     3

//XML validation
#include <libxml/xmlreader.h>
#include <libxml/xmlerror.h>
#include <libxml/tree.h>

#define HESSIAN_315_JAR

char *HessianXMLTagList[] = {
"hessian","string", "base64", "long", "class", "call_2.0", "double", "envelope", "boolean", "obj_ref",
#ifndef HESSIAN_315_JAR
"untyped_map",
#endif
"date", "int", "typed_map", "null", "obj_instance", "packet", "ref", "reply_2.0", "var_typed_list", "fixed_typed_list", "var_untyped_list", "fixed_untyped_list",
#ifndef HESSIAN_315_JAR
"compact_object", "compact_fixed_typed_list", "compact_fixed_untyped_list",
#endif
"call_1.0", "number_of_args", "reply_1.0", "type", "length", "fixed_typed_list_length", "ref_typed_list_length", "compact_fixed_typed_list_length", "fixed_untyped_list_length", "compact_fixed_untyped_list_length", "method", "fault", "hessian_version", "xml", "header", "ver",
#ifdef HESSIAN_315_JAR
"obj_defn", "obj_defn_type", "obj_instance", "ref_typed_list", "Type", "type_ref", "ref_typed_list_type", "remote"
#endif
};

#define MAX_TAG2  (sizeof(HessianXMLTagList)/sizeof(HessianXMLTagList[0]))

//declarations
char *hessian_asc_ptr;
int ns_hessian_to_xml(char *, int *);
char *hessian_encode (int , void* , int , char *, int*);
void hessian_set_debug_log_fp(FILE *);
void hessian_set_debug_log_level(int );


// Usage

static void Usage(char *err)
{
  fprintf(stderr, "%s\n", err);
  fprintf(stderr, "Usage: \n");
  fprintf(stderr, "nsi_hessian -e|-d|-V -i <in_file> -o <out_file> -v <1|2>  [-D <debug level>]\n");
  fprintf(stderr, "Where: \n");
  fprintf(stderr, "  -d to convert Hessian Binary to XMl\n");
  fprintf(stderr, "  -e to convert XML to Hessian Binary\n");
  fprintf(stderr, "  -i <input file name>\n");
  fprintf(stderr, "  -o <output file name>\n");
  fprintf(stderr, "  -D <debug level (1-4)>\n");
  fprintf(stderr, "  -v <hessian version (1|2)>\n");
  fprintf(stderr, "  -V validate xml file (validate hessian tags)\n");
  
  fprintf(stderr, "  -H to spcify that input file is in ASCII Hex (To be used later ...)\n");

  exit(1);
}

static FILE *open_in_file(char *in_file)
{
  FILE *fp = fopen (in_file, "r");
  if (!fp) {
    printf ("unable to open input file %s\n", in_file);
    exit (1);
  }
  return(fp);
}

static FILE *open_out_file(char *out_file)
{

  fprintf(stdout, "Opening output file %s\n", out_file);

  FILE *fp = fopen (out_file, "w");
  if (!fp) {
    printf ("unable to open output file %s\n", out_file);
    exit (1);
  }

  return (fp);
}

//TODO: We need to make a check that file size should not be more than 2MB
#define MAX_MSG_LEN  0x200000// 2MB

static void  hessian_binary_to_xml(char *in_file, char *out_file)
{
char message[MAX_MSG_LEN];
FILE *fp, *fpout;
int len, cur = 0;

  // Open hessian data file (hex dump)
  fp = open_in_file(in_file);

  fprintf(stdout, "Reading input file %s\n", in_file);
  cur = fread(message, 1, MAX_MSG_LEN, fp);
  fprintf(stdout, "Input file size %d bytes\n", cur);

  fclose (fp);

  fpout = open_out_file(out_file);

  fprintf(stdout, "Converting Hessian binary to XML\n");

  if ((len = ns_hessian_to_xml (message, &cur)) != -1)
  {
    if(hessian_asc_ptr != NULL)
       fprintf (fpout, "%s", hessian_asc_ptr);
  }
  fclose (fpout);
}


static void  hessian_xml_to_binary(char *in_file, char *out_file)
{

char message[MAX_MSG_LEN];
char msgout[MAX_MSG_LEN];

FILE *fp, *fpout;
int cur = 0, len;


  // Open hessian data file
  fp = open_in_file(in_file);

  fprintf(stdout, "Reading input file %s\n", in_file);
  cur = fread(message, 1, MAX_MSG_LEN, fp);
  fprintf(stdout, "Input file size %d bytes\n", cur);

  fclose (fp);

  fprintf(stdout, "Converting XML to Hessian\n");

  //amfin_lineno and amf_infp must be initialized before using read_amf and skip amfr+debug
  len = MAX_MSG_LEN;

#define HESSIAN_SRC_IS_BUFFER   1
#define HESSIAN_DO_NOT_SEGMENT  0

  // This method reduced the len by the size of binary data
  if (hessian_encode (HESSIAN_SRC_IS_BUFFER, message, cur, msgout, &len) == NULL)
    return;
  //int hessian_bin_len = MAX_MSG_LEN - len;
  int hessian_bin_len =  len;
  fprintf(stdout, "XML to Hessian Conversion Successful (size=%d)\n", hessian_bin_len);

  fprintf(stdout, "Saving Hessian XML in out file (size = %d)\n", hessian_bin_len);

  fpout = open_out_file(out_file);
  fwrite(msgout, hessian_bin_len, 1, fpout);
  fclose (fpout);

#if 0

  // TODO skip_amf_debug ();
  //show_buf (msgout, 64*1024-len);
  cur = MAX_MSG_LEN - len;
  //show_buf(message, cur);
  for (i = 0; i < cur; i++) {
    if (i != 0 && i % 16 == 0)
      fprintf (fpout, "\n");
      fprintf (fpout, "%02x ", (unsigned char) msgout[i]);
    }
    fprintf (fpout, "\n");
    //cur = 64*1024-len;
    //write_amf (0, stdout, 4, msgout , &cur );
  }
  fclose (fpout);
#endif

}

void  StructErrorFunc(void * userData, xmlErrorPtr errptr)
{
  fprintf(stderr, "domain %d code %d message %s file %s line %d str1 %s str2 %s str3 %s int1 %d int2 %d cntxt %p node %p\n",  errptr->domain, errptr->code, errptr->message, errptr->file, errptr->line, errptr->str1, errptr->str2, errptr->str3, errptr->int1, errptr->int2, errptr->ctxt, errptr->node);
}

void ErrorFunc (const char * msg,xmlParserSeverities severity, xmlTextReaderLocatorPtr locator)
{
  fprintf (stderr, "msg %s severity %d locator %d\n",msg, severity, *(int*)locator);
}

// xml validation routines below
//
void
PrintError(void)
{
  xmlErrorPtr errptr;

  errptr =  xmlGetLastError();
  if (errptr)
    fprintf(stderr, "domain %d code %d message %s file %s line %d str1 %s str2 %s str3 %s int1 %d int2 %d cntxt %p node %p\n",  errptr->domain, errptr->code, errptr->message, errptr->file, errptr->line, errptr->str1, errptr->str2, errptr->str3, errptr->int1, errptr->int2, errptr->ctxt, errptr->node);
}

static int
IsNameValid(const xmlChar *name)
{
  int i;
  for (i=0; i<MAX_TAG2; i++) {
    if (!xmlStrcmp(name, (xmlChar*)HessianXMLTagList[i]) ) 
      return(0);
  }
  return(1);
}

/**
 *processNode:
 *@reader: the xmlReader
 *
 *Dump information about the current node
 */
static int
processNode(xmlTextReaderPtr reader, char *file, FILE *fp) 
{
  const xmlChar *name;
  int err =0;

  name = xmlTextReaderConstName(reader);
  if (name == NULL)
    name = BAD_CAST "--";

  if (xmlTextReaderNodeType(reader) != XML_ELEMENT_NODE)
    return(0);
  if (IsNameValid(name)) {
   xmlNodePtr node = xmlTextReaderCurrentNode(reader);
    err++;
    fprintf(fp, "%s:%ld :invalid node name \"%s\"\n",file, XML_GET_LINE(node), name);
  }
#if 0
  {
  const xmlChar *value;
  value = xmlTextReaderConstValue(reader);
  printf("depth %d type %d name %s isempty? %d hasvalue? %d\n", 
      xmlTextReaderDepth(reader),
      xmlTextReaderNodeType(reader),
      name,
      xmlTextReaderIsEmptyElement(reader),
      xmlTextReaderHasValue(reader));
  if (value == NULL)
    printf("\n");
  else {
    if (xmlStrlen(value) > 40)
      printf("value= %.40s...\n", value);
    else
      printf("value= %s\n", value);
  }
  }
#endif
  return(err);
}

/**
 *streamFile:
 *@filename: the file name to parse
 *
 *Parse, validate and print information about an XML file.
 */
static int
streamFile(char *in, char *out) {
  xmlTextReaderPtr reader;
  int ret; 
  static int err =0;
  FILE *fpout = NULL;


  if (out[0] != '\0')
    fpout = open_out_file(out);
  /*
   *Pass some special parsing options to activate DTD attribute defaulting,
   *entities substitution and DTD validation
   **/
  reader = xmlReaderForFile(in, NULL,
      XML_PARSE_DTDATTR   /* default DTD attributes */
      |XML_PARSE_PEDANTIC
      );
      // XML_PARSE_NOENT     /* substitute entities */
      //| XML_PARSE_DTDVALID); /* validate with the DTD */

  //set error handler to default
  //xmlTextReaderSetErrorHandler(reader, NULL, NULL);
  //xmlTextReaderSetErrorHandler (reader, (xmlTextReaderErrorFunc)ErrorFunc, NULL);
  //xmlTextReaderSetStructuredErrorHandler(reader, (xmlStructuredErrorFunc)StructErrorFunc, NULL);

  if (reader != NULL) {
    ret = xmlTextReaderRead(reader);
    while (ret == 1) {    // 1= success, 0 =no more nodes, -1 = error
      if (processNode(reader, in, stdout))   //to print on stdout pass stdout here
        err++;
      ret = xmlTextReaderRead(reader);
    }
#if 0
    /*
     *Once the document has been fully parsed check the validation results
     **/
    if (xmlTextReaderIsValid(reader) != 1) {
      fprintf(stderr, "Document %s does not validate\n", filename);
    }
#endif

    xmlFreeTextReader(reader);
    if (ret || err) {
      fprintf(stderr, "%s : failed to parse\n", in);
      return(1); 
    }
  } else {
    fprintf(stderr, "Unable to open %s\n", in);
    return(1); 
  }
  if (fpout)
    fclose(fpout);
  return(0);
}



#ifdef LIBXML_READER_ENABLED
static int 
validate (char* in_file, char *out_file)
{
  int ret;
  /*
   *this initialize the library and check potential ABI mismatches
   *between the version it was compiled for and the actual shared
   *library used.
   **/
  LIBXML_TEST_VERSION

    ret = streamFile(in_file, out_file);

  /*
   *Cleanup function for the XML library.
   */
  xmlCleanupParser();
  /*
   *this is to debug memory for regression tests
   *
   */
  xmlMemoryDump();
  return(ret);
}

#else
static int 
validate(void) {
    fprintf(stderr, "XInclude support not compiled in\n");
    exit(1);
}
#endif


int main (int argc, char *argv[])
{
int type  = -1;
int format = HESSIAN_BINARY;
char c;
//int hessian_ver;

char in_file[1024] = "", out_file[1024] = "";

  /* Parse args. */
  while ((c = getopt(argc, argv, "Vedi:o:D:v:")) != -1)
  {
    switch (c)
    {
      case 'v':
       // hessian_ver = atoi(optarg);
        break;

      case 'd':
        type = HESSIAN_DECODE_BINARY_TO_XML;
        break;

      case 'e':
        type = HESSIAN_ENCODE_XML_TO_BINARY;
        break;

      case 'i':
        strcpy(in_file, optarg);
        break;

      case 'o':
        strcpy(out_file, optarg);
        break;

      case 'D':
        hessian_set_debug_log_level(atoi(optarg));
        break;

      case 'V':
        type = XML_VALIDATE;
        break;

      case '?':
        Usage("Invalid arguments");
        break;
    }
  }

  if (type == -1) 
    Usage("Invalid arguments");

  if ( ( (type == HESSIAN_DECODE_BINARY_TO_XML) || (type == HESSIAN_ENCODE_XML_TO_BINARY)) &&  
      ((in_file[0] == '\0') || (out_file[0] == '\0')) )
    Usage("Invalid arguments");

  if ( (type == XML_VALIDATE) && (in_file[0] == '\0') )
    Usage("Invalid arguments");

  if(type == HESSIAN_DECODE_BINARY_TO_XML)
  {
    if(format == HESSIAN_BINARY)
      hessian_set_version(2);
      hessian_binary_to_xml(in_file, out_file);
      return 0;
  }

  if(type == HESSIAN_ENCODE_XML_TO_BINARY)
  { 
    hessian_set_version(2);
    hessian_xml_to_binary(in_file, out_file);
    return 0;
  }

  if (type == XML_VALIDATE) {
    int ret;
    ret = validate(in_file, out_file);
    if (ret) 
      return(1);
  }	
  return 0;
}
