#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#include "ns_log.h"
#include "nslb_alloc.h"
#include "nslb_log.h"
//#include "ns_page_dump.h"
#include "ns_ldap.h"
#include "ns_exit.h"

#define WRITE_TAG(ptr, id, str, len, close, new_line){\
	      int amt_wrt = 0;\
	      if(close && new_line)\
		amt_wrt = sprintf(ptr, " %s=\"%s\">\n", id, str);\
	      else if(close && !new_line)\
		amt_wrt = sprintf(ptr, " %s=\"%s\">", id, str);\
              else if(!close && new_line)\
		amt_wrt = sprintf(ptr, " %s=\"%s\"\n", id, str);\
	      else\
		amt_wrt = sprintf(ptr, " %s=\"%s\"", id, str);\
	      ptr += amt_wrt;\
	      total_wrt += amt_wrt;\
}

#define WRITE_INITIAL_TAG(ptr, str, len){\
	      int amt_wrt = 0;\
	      amt_wrt = sprintf(ptr, "%s", str);\
	      ptr += amt_wrt;\
	      total_wrt += amt_wrt;\
}
#define CONSUME_BYTE(in, out){\
          out = *in;\
          in++;\
          consumed_bytes++;\
}
//fill the closing tag structure
#define PUSH_TO_STACK(tag, str, len, len_len){\
          strcpy(tag.buf, str);\
          tag.len = len;\
          tag.updated_len = len;\
          tag.val_len = len_len;\
          push(tag);\
}
#define CONSUME_VALUE(in, len, tmp_buf){\
         bcopy(in, tmp_buf,len);\
         tmp_buf[len] = '\0';\
         in += len;\
         consumed_bytes += len;\
}

//operation
#define BINDREQUEST 		0
#define BINDRESPONSE 		1
#define UNBINDREQUEST 		2
#define SEARCHREQUEST		3
#define SEARCHRESULTENTRY 	4
#define SEARCHRESULTDONE 	5
#define SEARCHRESULTREFERENCE 	19
#define MODIFYREQUEST 		6
#define MODIFYRESPONSE 		7
#define ADDREQUEST 		8      //TODO:need to veryfy operation codes , have some confusion
#define ADDRESPONSE 		9
#define DELREQUEST		10
#define DELRESPONSE 		11
#define MODIFYDNREQUEST 	12
#define MODIFYDNRESPONSE 	13
#define COMPAREREQUEST 		14  //dont know
#define COMPARERESPONSE 	15
#define ABANDONREQUEST 		16
#define EXTENDEDREQUEST 	23
#define EXTENDEDRESPONSE 	24
#define INTERMEDIATERESPONSE 	25

#define MAX_STACK 32
#define MAX_MLEN 64*1024
  
typedef struct{
   char buf[1024];
   int len;
   int updated_len;
   int val_len;
}CloseStructTag;

FILE *out_fp = NULL;
char *in;
int in_len;

CloseStructTag *stack = NULL;
static int max_stack_size;
static int top = 0;

char *output_msg;
//char output_msg[8*1024];
int consumed_bytes = 0;
int msg_len = 0;
int ldap_indent = 0;

#define MAX_LDAP_TYPE 100
enum {UNI, FILTER, SUBFILTER,AUTH};
const char ldap_type_tag[][MAX_LDAP_TYPE][MAX_LDAP_TYPE] = {{"EOC", 
                             "Boolean",
                             "Integer",
                             "BitString",
                             "OctetString",
                             "Null",
                             "ObjectIdentifier",
                             "ObjecttDescriptor",
                             "External",
                             "Real",
                             "Enumerated",
                             "EmbeddedPDV",
                             "UTF8String",
                             "RelativeOID",
                             "Reserved",
                             "Reserved",
                             "Sequence",
                             "Set",
                             "NumericString",
                             "PrintableString",
                             "T61String",
                             "VideoTextString",
                             "IA5String",
                             "UTCTime",
                             "GeneralizedTime",
                             "GraphicString",
                             "VisibleString",
                             "GeneralString",
                             "UniversalString",
                             "CharacterString",
                             "BMPString",
                             "UseLongForm"},
                             {"And","Or", "Not","EqualityMatch","Substrings", "GOE","LOE","Present", "ApproxMatch", "ExtensibleMatch"},//filter
                             {"Initial", "Any", "Final"}, //substring
                             {"Simple", "Reserved", "Reserved","Sasl"}//Authentication type
                             
};

static void init_stack(void)
{
  NSDL2_LDAP(NULL, NULL, "Method called");
  NSLB_MALLOC(stack, (MAX_STACK * sizeof(CloseStructTag)), "stack", -1, NULL);
 // stack = (CloseStructTag*)malloc(MAX_STACK * sizeof(CloseStructTag));
  max_stack_size = MAX_STACK;
}

#define UPDATE_STACK_VAL(dec_len, val){\
        CloseStructTag tag;\
        stack[top].updated_len -=  dec_len;\
        NSDL2_LDAP(NULL, NULL, "updated_len = [%d].....val=[%s]....tag_len=[%d]  val_len=[%d]\n", stack[top].updated_len, stack[top].buf, stack[top].len,stack[top].val_len);\
        while(stack[top].updated_len == 0){\
           tag = pop();\
           stack[top].updated_len = stack[top].updated_len - 2 - tag.val_len - tag.len;\
           ldap_append_to_xml("%s", tag.buf);\
           ldap_indent -= 4;\
        }\
}

static void push(CloseStructTag tag_st)
{
  NSDL2_LDAP(NULL, NULL, "Method called.");

  if (stack == NULL) { //not inited
    init_stack();
  }

  if (top == max_stack_size) {
    max_stack_size += MAX_STACK;
    NSLB_REALLOC(stack, (max_stack_size*sizeof(CloseStructTag)), "stack", -1, NULL);
   // stack = realloc(stack, max_stack_size*sizeof(CloseStructTag)); 
  }
  top++;
  strcpy(stack[top].buf, tag_st.buf);
  stack[top].len = tag_st.len;
  stack[top].updated_len = tag_st.updated_len;
  stack[top].val_len = tag_st.val_len;
}

void
ldap_append_to_xml (char *format, ...)
{
#define MAX_LDAP_DATA_VAL 8192*5 // This must be bigger than max string lenght supported

// TODO - Strings can be multi-line and can be more than 4096 length

  va_list ap;
  int amt_written = 0;
  char buffer[MAX_LDAP_DATA_VAL + 1];

  NSDL2_LDAP(NULL, NULL, "Method called");

  va_start (ap, format);
  amt_written = vsnprintf (buffer, MAX_LDAP_DATA_VAL, format, ap);
  va_end (ap);

  NSDL2_LDAP(NULL, NULL, "buffer contents %s",buffer);

  // In some cases, vsnprintf return -1 but data is copied in buffer
  // This is a quick fix to handle this. need to find the root cause
  if(amt_written < 0)
  {
    NSDL2_LDAP(NULL, NULL, "Warning: vsnprintf returned error (%d). errno = %d\n", amt_written, errno);
    amt_written = strlen(buffer);
  }

  if(amt_written > MAX_LDAP_DATA_VAL)
  {
    NSDL2_LDAP(NULL, NULL, "Warning: XML line length (%d) is more than maximum buffer length (%d)\n", amt_written, MAX_LDAP_DATA_VAL);
    amt_written = MAX_LDAP_DATA_VAL;
  }


  buffer[MAX_LDAP_DATA_VAL] = 0;

  //out_fp, max_mlen amd out_buf are global vars
  if (out_fp) {
    fprintf (out_fp, "%s", buffer);
  } else {
    if (amt_written > (MAX_MLEN - 1 - msg_len))
    {
      //NSDL2_LDAP(NULL, NULL, "Error: No space left in XML buffer. XML buffer length = %d, space left = %d,  current line length = %d\n", max_mlen, amt_written, (max_mlen - 1 - mlen));
      amt_written = MAX_MLEN - 1 - msg_len;
    }
    bcopy (buffer, output_msg + msg_len, amt_written);
    msg_len += amt_written;
    output_msg[msg_len] = '\0';
  }
}

int ldap_read_int(const char *buf, int len)
{
  NSDL2_LDAP(NULL, NULL, "Method called");

  int i =0, val, b32=0, b24=0, b16=0,b8=0;
  if(len == 1){ b8 = buf[i++] & 0xFF;}
  if(len == 2){ b16 = buf[i++] & 0xFF; b8 = buf[i++] & 0xFF;}
  if(len == 3){ b24 = buf[i++] & 0xFF; b16 = buf[i++] & 0xFF; b8 = buf[i++] & 0xFF;}
  if(len == 4){ b32 = buf[i++] & 0xFF; b24 = buf[i++] & 0xFF; b16 = buf[i++] & 0xFF; b8 = buf[i++] & 0xFF;}
  
  val = ((b32 << 24) + (b24 << 16) + (b16 << 8) + b8) & 0x00000000FFFFFFFF;
  return (val);
}

static CloseStructTag pop(void)
{
  NSDL2_LDAP(NULL, NULL, "Method called");

  if (top < 0) {
    NS_EXIT(1, "Nothing to pop.");
  }
  return (stack[top--]);
}

void process_identifier_len(char *tag, int int_str, int *handled_len, int tag_index)
{

   int total_wrt = 0;
   unsigned char c;
   int val;
   int len;
   int dec_len;
   int constructive = 0;
   char tmp_buf[1024];
   char val_buf[1024];
   char *ptr = val_buf;

   NSDL2_LDAP(NULL, NULL, "Method called. opcode = [%.2x]", *in);

   CloseStructTag tag_structure;
   
   CONSUME_BYTE(in, c);

   sprintf(tmp_buf, "%*s<%s", ldap_indent, " ", tag);
   WRITE_INITIAL_TAG(ptr, tmp_buf, total_wrt); 

   if((c & 0x80) && (c & 0x40)) { WRITE_TAG(ptr, " Identifier", "P", total_wrt, 0, 0);}
   else if(!(c & 0x80) && (c & 0x40)) { WRITE_TAG(ptr, " Identifier", "A", total_wrt, 0, 0);}
   else if((c & 0x80) && !(c & 0x40)) { WRITE_TAG(ptr, " Identifier", "C", total_wrt, 0, 0);}
   else WRITE_TAG(ptr, " Identifier", "U", total_wrt, 0, 0);

   if(c & 0x20) { WRITE_TAG(ptr, "Encoding", "C", total_wrt, 0, 0); constructive = 1;}
   else { WRITE_TAG(ptr, "Encoding", "P", total_wrt, 0, 0);}

   val = (c & 0x1F);
   WRITE_TAG(ptr, "Type", ldap_type_tag[tag_index][val], total_wrt, 0, 0);

   CONSUME_BYTE(in, c); //consume for length

   val = 0; //reset it as we are using it in PUSH
   if(c & 0x80){//long format, consume 
      val = (c & 0x7f);
      char kk[127] = {0};
      CONSUME_VALUE(in, val, kk);
      len = ldap_read_int(kk, val); //convert buf(val_buf) of length(val) to integer
      sprintf(tmp_buf, "%d", len);  
      WRITE_TAG(ptr, "Len", tmp_buf, total_wrt, 1, 1); 
   }else{ //short format
      len = c; 
      sprintf(tmp_buf, "%d", len);
      if(constructive){ WRITE_TAG(ptr, "Len", tmp_buf, total_wrt, 1, 1);}
      else{ WRITE_TAG(ptr, "Len", tmp_buf, total_wrt, 1, 0);}
         
   }

   len<127?(dec_len= len+2):(dec_len=len+val+2); 

   if(handled_len != NULL)
     *handled_len = dec_len;

   ldap_append_to_xml("%s", val_buf);

  
   if(constructive){
     sprintf(tmp_buf, "%*s</%s>\n", ldap_indent, " ", tag);
     ldap_indent += 4;
   }
   else{ 
     sprintf(tmp_buf, "</%s>\n", tag);
   }

   //if constructive push closing tag into stack
   if(constructive){
      PUSH_TO_STACK(tag_structure, tmp_buf, len, val);
   }else{//start_primitive

     //copy value to xml upto length
     char b[2048];
     CONSUME_VALUE(in, len, b);


     if(int_str){//if int value need to write
        int m = ldap_read_int(b, len);
        NSDL2_LDAP(NULL, NULL, "KK = [%d]", m); 
        ldap_append_to_xml("%d", m); 
     }else{ //string will be written
        ldap_append_to_xml("%s", b);
     }

     ldap_append_to_xml("%s", tmp_buf);
     UPDATE_STACK_VAL(dec_len, val); 
   }
}

void handle_bind_req()
{
  NSDL2_LDAP(NULL, NULL, "Method called");

  CloseStructTag close_tag;

  process_identifier_len("bindRequest", 0, NULL, UNI);
  process_identifier_len("version", 1, NULL, UNI);
  process_identifier_len("name", 0, NULL, UNI);
  process_identifier_len("authentication", 0, NULL, AUTH);

  while(top){
     close_tag = pop(); 
     ldap_append_to_xml("%s", close_tag.buf); 
  }
}

void  handle_del_req()
{

  NSDL2_LDAP(NULL, NULL, "Method called");

  CloseStructTag close_tag;

  process_identifier_len("deleteRequest", 0, NULL, UNI);

  while(top){
     close_tag = pop(); 
     ldap_append_to_xml("%s", close_tag.buf); 
  }
}

void  handle_rename_req()
{

  NSDL2_LDAP(NULL, NULL, "Method called");

  CloseStructTag close_tag;

  process_identifier_len("renameRequest", 0, NULL, UNI);
  process_identifier_len("dn", 0, NULL, UNI);
  process_identifier_len("newDN", 0, NULL, UNI);
  process_identifier_len("delOld", 1, NULL, UNI);

  while(top){
     close_tag = pop(); 
     ldap_append_to_xml("%s", close_tag.buf); 
  }
}

void handle_modify_req()
{
  int handled_len = 0;
  int mod_len = 0;

  NSDL2_LDAP(NULL, NULL, "Method called");

  CloseStructTag close_tag;

  process_identifier_len("modifyRequest", 0, NULL, UNI);
  process_identifier_len("dnObject", 0, &handled_len, UNI);
  process_identifier_len("modifications", 0, &handled_len, UNI);

  mod_len = stack[top].len;

  while(mod_len){

     process_identifier_len("modificationItem", 0, &handled_len, UNI);

     mod_len -= handled_len;
     process_identifier_len("operation", 1, NULL, UNI);
     process_identifier_len("modificationType", 0, NULL, UNI);
     process_identifier_len("type", 0, NULL, UNI);
     process_identifier_len("values", 0, NULL, UNI);
     process_identifier_len("value", 0, NULL, UNI);
  } 
  while(top){
     close_tag = pop(); 
     ldap_append_to_xml("%s", close_tag.buf); 
  }
}

#define SUBSTRING       4 
#define PRESENT         7
#define EQALITYMATCH    3 
#define INITIAL         0
#define ANY             1
#define FINAL           2

void handle_add_req()
{
  int handled_len = 0;
  int val_len = 0;
  int attr_len = 0;

  NSDL2_LDAP(NULL, NULL, "Method called");

  CloseStructTag close_tag;

  process_identifier_len("addRequest", 0, NULL, UNI);
  process_identifier_len("entry", 0, &handled_len, UNI);
  process_identifier_len("attributes", 0, &handled_len, UNI);

  attr_len = stack[top].len; 

  while(attr_len){       
    process_identifier_len("attributeList", 0, &handled_len, UNI);
    attr_len -= handled_len;

    process_identifier_len("item", 0, NULL, UNI);
    process_identifier_len("itemValues", 0, NULL, UNI);
    
    val_len = stack[top].len;
    while(val_len){
      process_identifier_len("value", 0, &handled_len, UNI);
      val_len -= handled_len; 
    }
  }

  while(top){
    close_tag = pop(); 
    ldap_append_to_xml("%s", close_tag.buf); 
  }
}

inline void handle_search_filter_fields(int *consumed)
{
  // char *tag;
   int val;
   int subtree_len = 0;
   CloseStructTag close_tag;
   int handle_len = 0;
   int local_consumed;
   
   if (consumed == NULL)
     consumed = &local_consumed;

   *consumed = 0;
  

   //get filter type
   val = (*in & 0x1F);

   NSDL4_LDAP(NULL, NULL, "Method called, filter type - %s", ldap_type_tag[FILTER][val]);

   process_identifier_len("filter", 0, &handle_len, FILTER);
   *consumed += handle_len;

   if (val == LDAP_SEARCH_OR || val == LDAP_SEARCH_AND || val == LDAP_SEARCH_NOT) {
     //tag = (val == LDAP_SEARCH_OR ? "or": (
     //      val == LDAP_SEARCH_AND ? "and": "not")); 

     *consumed += handle_len;

     subtree_len = stack[top].len;

     // do handling recursively.
     while (subtree_len > 0) {
       //NSDL3_LDAP(NULL, NULL, "%s items, subtree_len - %d", tag, subtree_len); 

       handle_search_filter_fields(&handle_len);
       subtree_len -= handle_len;
       *consumed += handle_len;
     }

     close_tag = pop(); 
     ldap_append_to_xml("%s", close_tag.buf); 
   } 
   else if(val == PRESENT){
     //process_identifier_len("PRESENT", 0, &handle_len, UNI);
     //*consumed += handle_len;
   }
   else if(val == SUBSTRING){
     process_identifier_len("type", 0, &handle_len, UNI);
     *consumed += handle_len;
     process_identifier_len("substring", 0, &handle_len, UNI);
     *consumed += handle_len;

     subtree_len = stack[top].len; 
     while (subtree_len > 0) {     
       process_identifier_len("item", 0, &handle_len, SUBFILTER);
       *consumed += handle_len;
       subtree_len -= handle_len;
     }
   }else if(val == EQALITYMATCH){
      process_identifier_len("description", 0, &handle_len, UNI);
      *consumed += handle_len;
      process_identifier_len("value", 0, &handle_len, UNI);
      *consumed += handle_len;
   } else {
     NSDL2_LDAP(NULL, NULL, "Currently we are not supporting these  types");
     NS_EXIT(1, "Currently we are not supporting these  types, val = %d ", val);
   }
}

void handle_search_req()
{
   int handled_len = 0;
   int attr_len;

   NSDL2_LDAP(NULL, NULL, "Method called");

   CloseStructTag close_tag;

   process_identifier_len("searchRequest", 0, NULL, UNI);
   process_identifier_len("baseObject", 0, NULL, UNI);
   process_identifier_len("scope", 1, NULL, UNI);
   process_identifier_len("derefAlias", 1, NULL, UNI);
   process_identifier_len("sizeLimit", 1, NULL, UNI);
   process_identifier_len("timeLimit", 1, NULL, UNI);
   process_identifier_len("typesOnly", 1, NULL, UNI);

   handle_search_filter_fields(NULL); 
/*
   if(val == PRESENT){
    // process_identifier_len("PRESENT", 0, NULL);
   }
   else if(val == SUBSTRING){
     process_identifier_len("type", 0, NULL, UNI);
     process_identifier_len("substring", 0, NULL, UNI);
     process_identifier_len("item", 0, NULL, SUBFILTER);
   }else if(val == EQALITYMATCH){
      process_identifier_len("description", 0, NULL, UNI);
      process_identifier_len("value", 0, NULL, UNI);
   }else{
     NSDL2_LDAP(NULL, NULL, "Currently we are not supporting these  types");
     NS_EXIT(1, "Currently we are not supporting these  types, val = %d ", val);
   }
*/

   process_identifier_len("attributes", 0, NULL, UNI); 
   attr_len = stack[top].len;

   while(attr_len > 0){
      process_identifier_len("attributesDescription", 0, &handled_len, UNI);
      attr_len -= handled_len;
   }
   
   while(top){
      close_tag = pop(); 
      ldap_append_to_xml("%s", close_tag.buf); 
   }
}

void handle_search_resp(connection *cptr)
{
   int handled_len = 0;
   int attr_len;
   int p_attr_len, v_attr_len;

   NSDL2_LDAP(NULL, NULL, "Method called");

   CloseStructTag close_tag;

   process_identifier_len("searchResEntry", 0, NULL, UNI);
   process_identifier_len("objectName", 0, NULL, UNI);


   process_identifier_len("attributes", 0, NULL, UNI); 
   attr_len = stack[top].len;

   while(attr_len > 0){
      process_identifier_len("partialAttributes", 0, &handled_len, UNI);
      attr_len -= handled_len;
     
      p_attr_len = stack[top].len;
      while(p_attr_len > 0 )
      {
        process_identifier_len("type", 0, &handled_len, UNI);
        p_attr_len -= handled_len;
        process_identifier_len("vals", 0, &handled_len, UNI);
        p_attr_len -= handled_len;
        
        v_attr_len = stack[top].len;
        while(v_attr_len > 0) 
        {
          process_identifier_len("attributeValue", 0, &handled_len, UNI);
          v_attr_len -= handled_len; 
        }
      }
   }

   while(top){
      close_tag = pop();
      ldap_append_to_xml("%s", close_tag.buf);
   }

   //fulshing the LDAP response if response size is >= MAX_MLEN - 1024
   if(msg_len >= (MAX_MLEN - 1024))
   {
     ldap_buffer_len = msg_len;
#ifdef NS_DEBUG_ON
  debug_log_ldap_res(cptr, NULL);  
#else
  LOG_LDAP_RES(cptr, NULL);
#endif
     msg_len = 0;
   }
}


void handle_operation_resp(char *name)
{

  NSDL2_LDAP(NULL, NULL, "Method called");
  CloseStructTag close_tag;

  process_identifier_len(name, 0, NULL, UNI);
  process_identifier_len("resultCode", 1, NULL, UNI);
  process_identifier_len("dn", 0, NULL, UNI);
  process_identifier_len("message", 0, NULL, UNI);

  while(top){
     close_tag = pop();
     ldap_append_to_xml("%s", close_tag.buf);
  }
}

static void free_stack(void *stk)
{
  NSDL2_LDAP(NULL, NULL, "Method called");
  NSLB_FREE_AND_MAKE_NULL(stk, "stk", -1, NULL);
  max_stack_size =0;
  top =0;
}

int ldap_decode(connection *cptr, char *buf, char *out, int in_len, int fd)
{
  int ldap_operation;
  int ret;
  msg_len = 0; 

  int req_len = 0;
  char handle = 0;
  int val = 0;
  int bytes_remaining = in_len;
  int msg_opcode = -1;
  char *next_buf;

  NSDL2_LDAP(NULL, NULL, "Method called");

  if(in_len == 0)  //in case of unbind...we will not get any response
    return 0;
  output_msg = out;

  next_buf = buf;
  init_stack(); // Allocated stack will be needed   

  //start_processing messages  
  while(bytes_remaining > 0)
  {
    in = next_buf;
    ldap_indent = 0; 
    req_len = 0; 
    handle = *(in+1);

    if(handle & 0x80){
      req_len = (handle & 0x7f);
      NSDL2_LDAP(NULL, NULL, "req_len=[%d]", req_len);
      val = ldap_read_int(in + 2, req_len);
    } else {
      val = handle;  // save length 
    }

    NSDL2_LDAP(NULL, NULL, "bytes_remaining = %d", bytes_remaining);
    bytes_remaining -=  2 + req_len + val;
    next_buf = in + 2 + req_len + val;
    NSDL2_LDAP(NULL, NULL, "After calc: bytes_remaining = %d", bytes_remaining);
    get_ldap_msg_opcode(in, &msg_opcode);

    if(msg_opcode > MODIFYDNRESPONSE)
    {
      //TODO: logs
      continue;
    }

    process_identifier_len("LDAP", 0, NULL, UNI);
    process_identifier_len("msgId", 1, NULL, UNI);
    ldap_operation = *(in) & 0x1f;

  //switch to operation processing

  switch(ldap_operation){

    case BINDREQUEST:
         handle_bind_req();
         break;

    case BINDRESPONSE:
         handle_operation_resp("bindResponse");
         break;

    case UNBINDREQUEST:
         process_identifier_len("unbindRequest", 1, NULL, UNI);
         break;
    
     case SEARCHREQUEST:
          handle_search_req();
          break;

     case SEARCHRESULTENTRY:
          handle_search_resp(cptr);
          break;

     case SEARCHRESULTDONE:
          handle_operation_resp("searchResultDone");
          break;

     /*case SEARCHRESULTREFERENCE:
          {
              fprintf(stderr, "This operation is not implemented\n");
              //not implemented
          }
          break;
     */
     case MODIFYREQUEST:
          handle_modify_req();
          break;

     case MODIFYRESPONSE:
          handle_operation_resp("modifyResponse");
          break;

     case ADDREQUEST:
          handle_add_req();
          break;

     case ADDRESPONSE:
          handle_operation_resp("addResponse");
          break;

     case DELREQUEST:
          handle_del_req();
          break;

     case DELRESPONSE:
          handle_operation_resp("deleteResponse");
          break;

     case MODIFYDNRESPONSE:
          handle_operation_resp("renameResponse");
          break;

     case MODIFYDNREQUEST:
          handle_rename_req();
          break;

     case COMPAREREQUEST:
     case COMPARERESPONSE:
     case ABANDONREQUEST:
     case EXTENDEDREQUEST:
     case EXTENDEDRESPONSE:
     case INTERMEDIATERESPONSE:
          {
              //not implemented
              NSDL2_LDAP(NULL, NULL, "LDAP Operation[%d]: This operation is not implemented", ldap_operation);
              //fprintf(stderr, "This operation is not implemented\n");
              CloseStructTag close_tag;
              while(top){
                 close_tag = pop(); 
                 ldap_append_to_xml("%s", close_tag.buf); 
              }
          }
          break;
   }
 /* 
    req_len = 0; 
    handle = *(in+1);

    if(handle && 0x80){
      req_len = (handle & 0x7f);
      NSDL2_LDAP(NULL, NULL, "req_len=[%d]", req_len);
      val = ldap_read_int(in + 2, req_len);
    } else {
      val = handle;  // save length 
    }
 
    in += 2 + req_len + val;
    if(in && *in) 
      ldap_operation = *(in) & 0x1f;
    else
      ldap_operation = -1;
*/

  } 
  // Write decoded buffer into given fd of the file
  // TODO :handle partial write
  //memset(output_msg, 0, msg_len);

  if(fd != -1){
    ret = write(fd, output_msg, msg_len);
    if(ret != msg_len){
      if(ret == -1){
        fprintf(stderr, "Error in logging decoding response for ldap");
      } else {
        fprintf(stderr, "Partial write while writing decoded response for ldap");
      }
    }
  }

  
  ldap_indent = 0; 

  free_stack(stack);
  stack = NULL;
  return msg_len;
  // TODO: call free stack here
}

/*
int main()
{
  int ret;
  int amt_read;
  struct stat st ;
 
  FILE* in_file_fp = fopen("kk.bin", "r"); 
  ret = stat("kk.bin", &st);

  init_stack(); //TODO: free it in the end

  amt_read = fread(in_buf, st.st_size, 1, in_file_fp);

  in_len = st.st_size;
 
  ldap_decode();

  printf("msggg_len = [%d]", msg_len); 
  output_msg[msg_len] = '\0';
  FILE *out = fopen("x.xml", "w");
  int n  = fwrite(output_msg, msg_len, 1, out);
  printf("******msg = [%s]\n", output_msg);
  return 0;
}
*/
