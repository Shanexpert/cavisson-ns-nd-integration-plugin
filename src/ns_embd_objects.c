/******************************************************************
 * Name    :    ns_embd_objects.c
 * Purpose :    This file contains methods to extract all Embedded 
                URL from buffer.
 * Author  :    Archana
 * Intial version date:    20/08/08
 * Last modification date: 25/08/08
 * Note    :    
*******************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <sys/stat.h>
#include <time.h>
#include "ns_common.h"
#include "ns_data_types.h"
#include "url.h"
#include "ns_global_settings.h"
#include "nslb_time_stamp.h"
#include "ns_log.h"
#include "ns_embd_objects.h"

#define KEEP_URL 0 
#define REMOVE_URL 1 

//To free extracted embedded URLs
#ifndef CAV_MAIN
PatternTable *pattern_table = NULL;
#else
__thread PatternTable *pattern_table = NULL;
#endif

void free_embd_array(EmbdUrlsProp *extracted_embd_url, int num_urls)
{
  //NSDL2_HTTP(vptr, cptr, "Method called, num_urls = %d", num_urls);
  int i;
  for (i = 0; i < num_urls; i++) 
  {
    if(extracted_embd_url[i].embd_url) // We are freeing embd_url and making it NULL in case of filter, so check for null is added here
      free(extracted_embd_url[i].embd_url);
  }
  //Dynamic Host:Added check for embd url if ignored(error case) while extracting from dyn host 
  if(extracted_embd_url != NULL)
    free(extracted_embd_url);
}

//To extract urls and realloc with num_urls
static EmbdUrlsProp* extract_url(int *num_urls, EmbdUrlsProp *embd_url, xmlNode *cur_node, xmlChar *embd_obj)
{
  //NSDL2_HTTP(vptr, cptr, "Method called, num_urls = %d", *num_urls);
  xmlChar *type_obj;
  int i = 0;

  for (i = 0 ; i < *num_urls; i++) {
    if (strcmp(embd_url[i].embd_url, (char *)embd_obj) == 0) {
      free(embd_obj);
      return (embd_url);
    }
  }

  *num_urls = *num_urls + 1;
  embd_url = (EmbdUrlsProp*)realloc(embd_url, sizeof(EmbdUrlsProp) * *num_urls);
  //embd_obj is malloced object by xmlGetProp 
  embd_url[*num_urls - 1].embd_url  = (char*)embd_obj; 
  embd_url[*num_urls - 1].embd_type = 0;

  if((type_obj = xmlGetProp(cur_node, (xmlChar *)"type")) != NULL) {
     if (!xmlStrcmp(type_obj, (const xmlChar *)"text/javascript")) {
       embd_url[*num_urls - 1].embd_type |= XML_TEXT_JAVASCRIPT;
     }
    free(type_obj);
  } else if((type_obj = xmlGetProp(cur_node, (xmlChar *)"language")) != NULL) {
     if (!xmlStrcmp(type_obj, (const xmlChar *)"JavaScript")) {
       embd_url[*num_urls - 1].embd_type |= XML_TEXT_JAVASCRIPT;
     }
     free(type_obj);
  } 

  return (embd_url);
}

//This method used xmlGetProp() API to search and get the value of an attribute associated to a node.
//This API returns the attribute value or NULL if attribute not found.
//This method to get all extracted objects names.
static EmbdUrlsProp* extract_element_names(EmbdUrlsProp *embd_url, xmlDocPtr doc, xmlNode * a_node, int *num_urls)
{
  xmlNode *cur_node = NULL;
  xmlChar *embd_obj, *tmp_obj, *tmp;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next)
  {
    if (cur_node->type == XML_ELEMENT_NODE)
    {
    /* BugFix: Bug 2545 - Why link with href is treated as embedded URL
       This is not a embedded URL:
         <link rel="canonical" href="http://www1.d2c2macys.fds.com/shop/mens?id=1" />
       This is embedded URL:
         <link rel="stylesheet" href="../css/Netstorm.css" type="text/css" media="all" />
       The rel attribute specifies the relationship between the current document 
       and the linked document.  
       Only the "stylesheet" value of the rel attribute is fully supported in all major browsers. 
       The other values are only partially supported.
    */
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"link"))
      {
      /* BugFix: Bug 5169 - While testing Walgreens, 
       * following embedded URL was fetched:
       *   <link itemprop="availability" href="http://schema.org/InStock"> 
       *   To create an item, the item attribute is used.
       *   To add a property to an item, the itemprop attribute is used on one of the item's descendants.
       * Is "itemprop" treated as embedded URL, do we need to handle it in code??
       * In current code added check to verify whether embd_obj is NULL or not */  
        if((embd_obj = xmlGetProp(cur_node, (xmlChar *)"rel")) != NULL)  // Using embd_obj to check 
        {
          if( (strcmp((char *)embd_obj, "stylesheet") == 0))
          {
            if((embd_obj = xmlGetProp(cur_node, (xmlChar *)"href")) != NULL)
            {
              embd_url = extract_url(num_urls, embd_url, cur_node, embd_obj);
            }
          }  
        }
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"embed")  ||
          !xmlStrcmp(cur_node->name, (const xmlChar *)"script") ||
          !xmlStrcmp(cur_node->name, (const xmlChar *)"img")    ||
          !xmlStrcmp(cur_node->name, (const xmlChar *)"layer")  ||
          !xmlStrcmp(cur_node->name, (const xmlChar *)"iframe") ||
          !xmlStrcmp(cur_node->name, (const xmlChar *)"input")  ||
          !xmlStrcmp(cur_node->name, (const xmlChar *)"link"))
      {
        if((embd_obj = xmlGetProp(cur_node, (xmlChar *)"src")) != NULL)
        {
          /*bug id 90377: skip inline data url*/
          if(xmlStrncmp(embd_obj,(const xmlChar *)"data:", 5))
            embd_url = extract_url(num_urls, embd_url, cur_node, embd_obj); 
        }
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"meta")) /* this code handles Meta refresh and redirect */
      {
        if((tmp_obj = xmlGetProp(cur_node, (xmlChar *)"http-equiv")) != NULL)
        {
          if (xmlStrcasecmp(tmp_obj, (xmlChar *)"Refresh") == 0) {
            if((embd_obj = xmlGetProp(cur_node, (xmlChar *)"content")) != NULL) {
              tmp = (xmlChar *)xmlStrstr(embd_obj, (xmlChar *)"url=");
              if(tmp) {
                tmp = tmp + strlen("url=");
                if(tmp) {
                  xmlChar *new = (xmlChar *)strdup((char *)tmp);
                  embd_url = extract_url(num_urls, embd_url, cur_node, new);
                }
              }
              free(embd_obj); // content's object
            }
          }
          free(tmp_obj);  // http-equiv's object
        }
      }
    }
    //Call itself till it get ChildrenNode in doc 
    embd_url = extract_element_names(embd_url, doc, cur_node->xmlChildrenNode, num_urls);
  }
  return (embd_url);
}

//Free doc and parser ctxt
void close_parser(xmlDocPtr doc)
{
  xmlCleanupParser();  //To cleanup function for the XML library. 
  xmlMemoryDump();     //Dump in-extenso the memory blocks allocated to the file .memorylist
  xmlFreeDoc(doc);     //To free up all the structures used by a document, tree included
}

/*
Purpose:
  This method used to fetch or extract embedded URLs from buffer(input argument)
Arguments:
  buffer is data of main page html
  len is buffer size
  num_embd_urls is number of embedded urls (output argument)
  errMsg is if any error found  (output argument)
Return: 
  If return NULL then copy all error in errMsg that is output argument
  otherwise return all fetched embedded URLs as malloced 2D array
*/
EmbdUrlsProp* get_embd_objects(char *buffer, unsigned int len, int *num_embd_urls, char *errMsg)
{
  //char **extracted_embd_url = NULL;
  EmbdUrlsProp* extracted_embd_url = NULL;
  xmlDocPtr doc;
  xmlNodePtr cur;
  htmlParserCtxtPtr ctxt;  //HTML parser context
  *num_embd_urls = 0;
  errMsg[0]='\0';
  //Create a parser context for an HTML in-memory document.
  ctxt = htmlCreateMemoryParserCtxt(buffer, len);
  if (ctxt == NULL)
  {
    sprintf(errMsg, "Error: %s() - Failed to create parser context!",
                     (char*)__FUNCTION__);
    return NULL;
  }

  //Parse a Chunk of memory
  int ret;  
  if((ret = htmlParseChunk(ctxt, buffer, len, 1)) != 0)
  {
    sprintf(errMsg, "Error: %s() - Failed to parse chunk. Error = %d", 
                    (char*)__FUNCTION__, ret);
    //return NULL; 
  }
  
  doc = ctxt->myDoc;
  //Free all the memory used by a parser context. However the parsed document in ctxt->myDoc is not freed.
  htmlFreeParserCtxt(ctxt);
  if (doc == NULL )
  {
    sprintf(errMsg, "Error: %s() - Buffer not parsed successfully.",
                     (char*)__FUNCTION__);
    return NULL;
  }

  //Get the root element of the document
  if ((cur = xmlDocGetRootElement(doc)) == NULL)
  {
    sprintf(errMsg, "Error: %s() - There are not root elements in the document.",
                    (char*)__FUNCTION__);
    close_parser(doc);
    return NULL;
  }
  extracted_embd_url = extract_element_names(extracted_embd_url, doc, cur, num_embd_urls);
  close_parser(doc);

  return extracted_embd_url;
}

#ifdef TEST
/*Compile for standalone TEST:
gcc -o ns_embd_objects `xml2-config --cflags` `xml2-config --libs` ns_embd_objects.c  -D TEST -I../ -ldl -ggdb

Manmeet: 13 Sept 2013 - Above command does not work, use following command instead

gcc -I./thirdparty/libxml2-2.7.8/include/  -I./libnscore -g -o ns_embd_objects ns_embd_objects.c nslb_time_stamp.c -DTEST -L./thirdparty/libxml2-2.7.8/.libs/ -lxml2_cav

*/

int main(int argc, char **argv) 
{
  EmbdUrlsProp *extracted_embd_url;
  char *docname;
  FILE *fp;
  char *buffer, err_msg[1024];
  struct stat st;
  unsigned int s;
  int num_embd_urls, count, i;
  int num_counts = 1;
  int flag = 1;

  //  printf("main() - Method called\n");
  //if ( argc <=1 ) 
  if (argc < 1 || argc > 4) 
  {
    printf("Usage: %s <filename> <flag> <count> \n", argv[0]);
    printf("Where flag 0 - Not to print Extracted Embedded URL\n");
    printf("And flag 1 - To print Extracted Embedded URL\n");
    return(0);
  }
  docname = argv[1];
  if(argc >= 3){
    flag = atoi(argv[2]);
  }
  if(argc == 4){
    num_counts = atoi(argv[3]);
  }

  init_ms_stamp();

/*
const char * * __xmlParserVersion(void) 
*/
  printf("Libxml include file version used for compilation is %s\n", LIBXML_VERSION_STRING);
  printf("Libxml shared library version used in running is %s\n", *__xmlParserVersion());

  fp = fopen(docname, "r");
  if (fp != NULL) 
  {
    stat(docname, &st);
    /*if(st.st_size == 0)  //Check if file is empty
    {
      fprintf(stderr,"Error: File %s is empty.\n", docname);
      exit(-1);
    }*/

    buffer = malloc(st.st_size + 1);
    if ((s = fread(buffer, 1, st.st_size, fp)) != st.st_size) 
    {
      fprintf(stderr, "unable to read fully, read only %d of %lu\n", s, st.st_size);
      exit(-1);
    }
    u_ns_ts_t t1, t2, total_time = 0;
    set_cpu_freq();
    //run depend on num_count that will pass as third argument by user
    for(count = 0; count < num_counts; count++)
    {
      t1 = get_ms_stamp();
      extracted_embd_url = get_embd_objects(buffer, st.st_size, &num_embd_urls, err_msg);
      t2 = get_ms_stamp();
      total_time += (t2 - t1);

      if(flag)
        printf("Time taken to extract %d embedded URLs from file \"%s\" is %llu msec\n", 
               num_embd_urls, docname, t2 - t1);

      if(extracted_embd_url == NULL)
      {
        printf("%s\n", err_msg);
        exit(-1);
      }
      if(flag)
      {
        for (i = 0; i < num_embd_urls; i++) 
          printf("URL[%d] = %s\n", i, extracted_embd_url[i].embd_url);
      }
      free_embd_array(extracted_embd_url, num_embd_urls);
    }
    printf("Avg Time taken to extract %d embedded URLs from file \"%s\" is %llu msec\n", num_embd_urls, docname, (total_time/num_counts));
    free(buffer);
  }
  if (fp == NULL )  //Check if file does not exist
  {
    fprintf(stderr,"Error: File \"%s\" not found \n", docname);
    exit(-1);
  }
  return (1);
}
#endif
