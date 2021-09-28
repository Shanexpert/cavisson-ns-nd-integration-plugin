#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include "ns_ldap.h"

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_msg_com_util.h"
#include "output.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "ns_http_version.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "nslb_util.h"

#include "netstorm.h"
//#include "ns_child_msg_com.h"
#include "ns_exit.h"


#define CASE_INITIAL 1
#define CASE_ANY     2
#define CASE_FINAL   3

#define DUMP_DATA(len){\
        FILE *fp = fopen("kk.bin", "w");\
        if(fp == NULL)\
          printf("Error in opening kk.bin file. Error = [%s]\n", nslb_strerror(errno));\
        int amt_written = fwrite(t_ptr, *msg_len, 1, fp);\
        if(amt_written < 0)\
         printf("error = %s\n", nslb_strerror(errno) );\
        fclose(fp);\
  }

#define COPY_BYTES(ptr, bytes, len)\
               ptr[0] = (unsigned char)(bytes);\
               *len += 1;\
               ptr++;\
               bytes = 0;
 
#define COPY_INTEGER(ptr, bytes, len)\
               char temp[4];\
               int b32 = (bytes >> 24) & 0x000000FF;\
               int b24 = (bytes >> 16) & 0x000000FF;\
               int b16 = (bytes >> 8) & 0x000000FF;\
               int b8 = bytes & 0x000000FF;\
               temp[0] = b32; temp[1] = b24; temp[2] = b16, temp[3] = b8;\
               bcopy(temp, ptr, sizeof(bytes)); \
               *len += sizeof(bytes);\
               ptr += sizeof(bytes);\
               bytes = 0;

#define COPY_STRING(ptr, str, len)\
              bcopy(str, ptr, strlen(str));\
              *len += strlen(str);\
              ptr += strlen(str);

#define FILL_ENUM(ptr, bytes, len)\
              bytes = bytes | 0x0a;\
              COPY_BYTES(ptr, bytes, len);\
              bytes = bytes | 0x01;\
              COPY_BYTES(ptr, bytes, len);

        
#define FILL_DEFAULT_FILTER(ptr, bytes, len, str)\
              bytes = bytes | 0x80;\
              bytes = bytes | 0x07;\
              COPY_BYTES(ptr, bytes, len); \
              bytes = bytes | strlen(str);\
              COPY_BYTES(ptr, bytes, len);\
              COPY_STRING(ptr, str, len);

#define FILL_SUBSTRING_FILTER(bytes)\
              bytes = bytes | 0x70;\
              bytes = bytes | 0x20;\
              bytes = bytes | 0x04; 

#define FILL_LONG_FORMAT(ptr, bytes, len, str)\
             bytes = bytes | 0x04;\
             COPY_BYTES(free_me, bytes, len);\
             bytes = bytes | 0x84;\
             COPY_BYTES(free_me, bytes, len);\
             bytes = bytes | strlen(str);\
             COPY_INTEGER(ptr, bytes, len);\
             COPY_STRING(ptr, str, len); 
            
              
#define ADD_VAL(ptr, str, len){  \
        int bytes = 0;  \
        bytes = bytes|0x04;  \
        COPY_BYTES(ptr, bytes, len);  \
        CHECK_AND_FILL_LEN(ptr, strlen(str), len)\
        COPY_STRING(ptr, str, len);  \
   }

#define ADD_TYPE(ptr, type, str, str_len, len){		\
        int check_len = 0;\
        int type_len = strlen(type);\
        bytes = bytes|0x30;				\
        COPY_BYTES(ptr, bytes, len);			\
        str_len>127?(check_len=str_len+2+4):(check_len=str_len+2);\
        type_len>127?(check_len += type_len+2+4):(check_len += type_len+2);\
        CHECK_AND_FILL_LEN(ptr, check_len, len);\
        bytes = bytes|0x04;				\
        COPY_BYTES(ptr, bytes, len);			\
        CHECK_AND_FILL_LEN(ptr, type_len, len);\
        COPY_STRING(ptr, type, len);\
        bytes = bytes|0x31;				\
        COPY_BYTES(ptr, bytes, len);			\
        CHECK_AND_FILL_LEN(ptr, str_len, len);\
        bcopy(str, ptr, str_len);\
        ptr += str_len;\
        *len += str_len;				\
   }

#define ADD_MOD_TYPE(ptr, type, str, str_len, len, operation){     \
        int bytes = 0;\
        int val_len = 0;\
        int type_len = strlen(type);\
        int tt_len = 0;\
        int upper_len = 0;\
        str_len>127?(val_len=str_len+4+2):(val_len=str_len+2);\
        type_len>127?(tt_len=val_len+type_len+4+2):(tt_len=val_len+type_len+2);\
        tt_len>127?(upper_len=tt_len+3+4+2):(upper_len=tt_len+3+2);\
        bytes = bytes|0x30;\
        COPY_BYTES(ptr, bytes, len);\
        CHECK_AND_FILL_LEN(ptr, upper_len, len);\
        bytes = bytes|0x0a;\
        COPY_BYTES(ptr, bytes, len);			\
        bytes = bytes|0x01;\
        COPY_BYTES(ptr, bytes, len);			\
        bytes = bytes|operation;\
        COPY_BYTES(ptr, bytes, len);			\
        bytes = bytes|0x30;				\
        COPY_BYTES(ptr, bytes, len);			\
        CHECK_AND_FILL_LEN(ptr, tt_len, len);\
        bytes = bytes|0x04;				\
        COPY_BYTES(ptr, bytes, len);			\
        CHECK_AND_FILL_LEN(ptr, type_len, len);\
        COPY_STRING(ptr, type, len);\
        bytes = bytes|0x31;				\
        COPY_BYTES(ptr, bytes, len);			\
        CHECK_AND_FILL_LEN(ptr, str_len, len);\
        bcopy(str, ptr, str_len);\
        ptr += str_len;\
        *len += str_len;				\
   }
#define CALCULATE_MOD_LEN(d_len, o_len, m_len, tv_len){ \
        strlen(add.dn)>127?(d_len = 2+4+strlen(add.dn)):(d_len = 2+strlen(add.dn));\
        tv_len>127?(o_len = d_len+tv_len+4+2):(o_len = d_len+tv_len+2);\
        o_len>127?(m_len = o_len + 9):(m_len = o_len + 5);\
   }
#define CALCULATE_LEN(d_len, o_len, m_len, tv_len){ \
        strlen(add.dn)>127?(d_len = 2+4+strlen(add.dn)):(d_len = 1+1+strlen(add.dn));\
        o_len = d_len+tv_len;  \
        o_len>127?(m_len = o_len + 9):(m_len = o_len + 5);\
   }

#define CHECK_AND_FILL_LEN(ptr, len, msg_len){   \
        int bytes = 0;  \
        if(len>127){  \
          bytes = bytes | 0x84; \
          COPY_BYTES(ptr, bytes, msg_len); \
          bytes = bytes | len; \
          COPY_INTEGER(ptr, bytes, msg_len); \
        }else{  \
          bytes = bytes | len; \
          COPY_BYTES(ptr, bytes, msg_len); \
        } \
     }
           
#define TOTAL_LEN 8192

#define OPERATOR 0x1
#define OPERAND 0x2


/**
                and                [0] SET OF Filter,
                or                 [1] SET OF Filter,
                not                [2] Filter,
                equalityMatch      [3] AttributeValueAssertion,
                substrings         [4] SubstringFilter,
                greaterOrEqual     [5] AttributeValueAssertion,
                lessOrEqual        [6] AttributeValueAssertion,
                present            [7] AttributeDescription,
                approxMatch        [8] AttributeValueAssertion,
                extensibleMatch    [9] MatchingRuleAssertion
 * 
 */

#define SUBSTRING_INITIAL 0x0
#define SUBSTRING_ANY 0x1
#define SUBSTRING_FINAL 0x2

void fill_bDN_ENUM(unsigned char **base_ptr, int *base_len, Search ss){
   unsigned char *ptr = *base_ptr;
   int bytes = 0;
   int scope = atoi(ss.scope);
   int deref = atoi(ss.deref);
   int sizelimit = atoi(ss.sizelimit);
   int timelimit = atoi(ss.timelimit);
   int typesonly = atoi(ss.typesonly);

   NSDL2_LDAP(NULL, NULL, "Method called");
   /* now fill baseDN     it will be application specific, primitive and octet string type*/
   bytes = bytes | 0x04;
   COPY_BYTES(ptr, bytes, base_len);

   int len = strlen(ss.base);

   // Replace below code with macro CHECK_AND_FILL_LEN
   if(len < 127){
     bytes = bytes | len;
     COPY_BYTES(ptr, bytes, base_len); 
   }else{
     bytes = bytes | 0x84;
     COPY_BYTES(ptr, bytes, base_len);
     bytes = bytes | len;
     COPY_INTEGER(ptr, bytes, base_len);
   }

   COPY_STRING(ptr, ss.base, base_len); //baseDn is copied here, next will occupy scope, derefAliases, sizelimit, timelimit, typesonly

   //fill scope here
   FILL_ENUM(ptr, bytes, base_len);
   if(scope < 0)
     bytes = bytes | 0x02;
   else 
     bytes = bytes | scope;
   COPY_BYTES(ptr, bytes, base_len); //scope is copied here, default will be 0x02(whole subtree)

   // fill derefAliases
   FILL_ENUM(ptr, bytes, base_len);
   if(deref >= 0)
     bytes = bytes | deref;
   COPY_BYTES(ptr, bytes, base_len); //derefAliases is copied here

   // fill size limit
   bytes = bytes | 0x02;
   COPY_BYTES(ptr, bytes, base_len);
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, base_len);
   if(sizelimit >= 0 )
     bytes = bytes | sizelimit;
   COPY_BYTES(ptr, bytes, base_len); //size limit is copied here

   //fill timelimit
   bytes = bytes | 0x02;
   COPY_BYTES(ptr, bytes, base_len);
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, base_len);
   if(timelimit >= 0 )
     bytes = bytes | timelimit;
   COPY_BYTES(ptr, bytes, base_len); //time limit is copied here

   //fill types only here
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, base_len);
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, base_len);
   //FILL_ENUM(ptr, bytes, base_len);
   if(typesonly >= 0 )
     bytes = bytes | typesonly;
   COPY_BYTES(ptr, bytes, base_len); //types only is copied here

}

#if 0
void fill_filter(unsigned char **fltr_ptr, int *filter_len, Search ss)
{
  unsigned char *ptr = *fltr_ptr;
  char *tmp_ptr = NULL;
  char *tmp1_ptr = NULL;
  int bytes = 0;
  char descr[1024] = "0";
  char *descr_ptr = descr;
  char assert_val[1024] = "0";
  char *assert_ptr = assert_val;

  NSDL2_LDAP(NULL, NULL, "Method called");

  if(ss.filter == NULL){//type = present;
     FILL_DEFAULT_FILTER(ptr, bytes, filter_len, "objectclass");
     return;
  }else{
    tmp_ptr = strchr(ss.filter, '='); 
    if(tmp_ptr == NULL)
    {
      NSDL2_LDAP(NULL, NULL, "ERROR: missing '=' in filter");
      fprintf(stderr, "ERROR: missing '=' in filter %s \n", ss.filter);
      END_TEST_RUN;
    }

    strncpy(descr, ss.filter, tmp_ptr-ss.filter);
    tmp_ptr++;
    strcpy(assert_val, tmp_ptr); 
   /*
     case1: If nothing is there after '=' we assume value of substring will be NULL
     case2: If only '*' is found after '=' we will assume this subfilter as of 'Present' type
     case3: If first char is '*' and after that string comes, we assume substring as of 'initial' type, i.e uid=*john
     case4: If '*' is found at the end of string, we assume substring as of 'final' type, i.e uid=john*
     case5: If '*' comes at both end, we assume it as of 'any' type, i.e uid=*john*
     case6: If only string comes after '=' we assume filter type as 'equalitymatch'
   */

    unsigned char *free_me = (unsigned char*) malloc(strlen(descr_ptr) + strlen(assert_ptr) + 16);
    unsigned char *ff_ptr  = free_me;
    int free_len = 0;
    int filter_case = 0;

    if((*tmp_ptr == '\0') || ((tmp1_ptr=strchr(assert_ptr, '*')) == NULL)){ //equality match

        if(strlen(descr_ptr) > 127){ //use long format, 4 bytes for length
           FILL_LONG_FORMAT(free_me, bytes, &free_len, descr_ptr);
        }else{

           bytes = bytes | 0x04;
           COPY_BYTES(free_me, bytes, &free_len);
           bytes = bytes | strlen(descr_ptr);
           COPY_BYTES(free_me, bytes, &free_len);
           COPY_STRING(free_me, descr_ptr, &free_len);
        }

        if(strlen(assert_ptr) > 127){ //use long format, 4 bytes for length
           FILL_LONG_FORMAT(free_me, bytes, &free_len, assert_ptr);
        }else{
           if(*tmp_ptr != '\0'){
             bytes = bytes | 0x04;
             COPY_BYTES(free_me, bytes, &free_len);
             bytes = bytes | strlen(assert_ptr); //BUG : fill with length of assert_val
             COPY_BYTES(free_me, bytes, &free_len);
             COPY_STRING(free_me, assert_ptr, &free_len);
           }else{
             bytes = bytes | 0x04;
             COPY_BYTES(free_me, bytes, &free_len);
             bytes = bytes | 0x00;
             COPY_BYTES(free_me, bytes, &free_len);
           }
        }

        bytes = bytes | 0xa3;
        COPY_BYTES(ptr, bytes, filter_len);
        CHECK_AND_FILL_LEN(ptr, free_len, filter_len); 
        bcopy(ff_ptr, ptr, free_len);
        *filter_len += free_len;
      }else if(((*tmp_ptr == '*') && (*(tmp_ptr+1) == '\0'))) //present
      {
         //just fill len and value
        /* if(strlen(descr_ptr) > 127){
            fill_long_format(free_me, bytes, &free_len, descr_ptr);
         }else{
            bytes = bytes | 0x04;
            copy_bytes(free_me, bytes, &free_len);
            bytes = bytes | strlen(descr_ptr);
            copy_bytes(free_me, bytes, &free_len);
            copy_string(free_me, descr_ptr, &free_len); 
         }
       */
         bytes = bytes | 0x87;
         COPY_BYTES(ptr, bytes, filter_len);

         CHECK_AND_FILL_LEN(ptr, strlen(descr_ptr), filter_len);
         COPY_STRING(ptr, descr_ptr, filter_len);          

      }else{ //substring case
         if(strlen(descr_ptr) > 127){
            FILL_LONG_FORMAT(free_me, bytes, &free_len, descr_ptr);
         }else{
            bytes = bytes | 0x04;
            COPY_BYTES(free_me, bytes, &free_len);
            bytes = bytes | strlen(descr_ptr);
            COPY_BYTES(free_me, bytes, &free_len);
            COPY_STRING(free_me, descr_ptr, &free_len); 
         }

         bytes = bytes | 0x30;
         COPY_BYTES(free_me, bytes, &free_len); 

         if((*assert_ptr == '*') && (assert_ptr[strlen(assert_ptr)-1] == '*')){
            filter_case = CASE_ANY;
            *assert_ptr = '\0';
            assert_ptr++;
            assert_ptr[strlen(assert_ptr)-1] = '\0';
         }else if(*assert_ptr == '*'){ //initial case
            assert_ptr++;
            filter_case = CASE_INITIAL;
         }else{
            filter_case = CASE_FINAL;
            assert_ptr[strlen(assert_ptr) - 1] = '\0';
         }

         int ll = 0; 
         if(strlen(assert_ptr) > 127){
            ll = strlen(assert_ptr) + 6; 
            bytes = bytes | 0x84;
            COPY_BYTES(free_me, bytes, &free_len);
            bytes = bytes | ll;
            COPY_INTEGER(free_me, bytes, &free_len); 
          }else{
            ll = strlen(assert_ptr) + 2;
            bytes = bytes | ll;
            COPY_BYTES(free_me, bytes, &free_len);
          }       

        //be careful about each length, handle long format carefully 
         if(filter_case == CASE_ANY){ //any case
            bytes = bytes | 0x81;
            COPY_BYTES(free_me, bytes, &free_len);
         }else if(filter_case == CASE_INITIAL){ //initial case
            bytes = bytes | 0x80;
            COPY_BYTES(free_me, bytes, &free_len);
         }else{ //final case
            bytes = bytes | 0x82;
            COPY_BYTES(free_me, bytes, &free_len);
         }

       if(strlen(assert_ptr) > 127){
          bytes = bytes | 0x84;
          COPY_BYTES(free_me, bytes, &free_len);
          bytes = bytes | strlen(assert_ptr);
          COPY_INTEGER(free_me, bytes, &free_len);
       }else{
          bytes = bytes | strlen(assert_ptr);
          COPY_BYTES(free_me, bytes, &free_len);
       }

       COPY_STRING(free_me, assert_ptr, &free_len);
       bytes = bytes | 0xa4;
       COPY_BYTES(ptr, bytes, filter_len);

       CHECK_AND_FILL_LEN(ptr, free_len, filter_len);
       bcopy(ff_ptr, ptr, free_len);
       *filter_len += free_len;
    }
    free(ff_ptr); 
  }
} 
#endif

void fill_attribute(unsigned char **attribute_ptr, int* attr_len, Search ss){
 
  unsigned char *ptr = *attribute_ptr;
  int bytes = 0;
  unsigned char *tmp_ptr = NULL;
  char *fields[100];
  int total_fields = 0;
  int tmp_len = 0;
  int i;

  NSDL2_LDAP(NULL, NULL, "Method called");

  tmp_ptr = (unsigned char*)malloc(2048 + 1);
  unsigned char *tt_ptr = tmp_ptr;
  bytes = bytes | 0x30;
  COPY_BYTES(ptr, bytes, attr_len);

  if(ss.attribute == NULL){
    bytes = bytes | 0x00;
    COPY_BYTES(ptr, bytes, attr_len); 
    if(tmp_ptr != NULL)free(tmp_ptr);
  }else{
     total_fields = get_tokens(ss.attribute, fields, "|", 100);
     for(i  = 0; i< total_fields; i++){
        bytes = bytes | 0x04;
        COPY_BYTES(tmp_ptr, bytes, &tmp_len); 
        if(strlen(fields[i]) >127){
          bytes = bytes | 0x84;
          COPY_BYTES(tmp_ptr, bytes, &tmp_len);
          bytes = bytes | strlen(fields[i]);
          COPY_INTEGER(tmp_ptr, bytes, &tmp_len); 
          COPY_STRING(tmp_ptr, fields[i], &tmp_len);
        }else{
          bytes = bytes | strlen(fields[i]);
          COPY_BYTES(tmp_ptr, bytes, &tmp_len);
          COPY_STRING(tmp_ptr, fields[i], &tmp_len);
        }
     }
    if(tmp_len > 127){
      bytes = bytes | 0x84;
      COPY_BYTES(ptr, bytes, attr_len);
      bytes = bytes | tmp_len;
      COPY_INTEGER(ptr, bytes, attr_len);
    }else{
      bytes = bytes | tmp_len;
      COPY_BYTES(ptr, bytes, attr_len);
    }

    bcopy(tt_ptr, ptr, tmp_len);
    *attr_len += tmp_len;
  }
  free(tt_ptr); 
}


void free_LdapOperand(LdapOperand_t *obj) {
  if (obj->operator == LDAP_SEARCH_OR || obj->operator == LDAP_SEARCH_AND || obj->operator == LDAP_SEARCH_NOT) {
    // free all operands.
    LdapOperand_t *node = obj->children;
    LdapOperand_t *next;
    while (node) {
      next = node->next; 
      free_LdapOperand(node);
      node = next;
    }
  }

  // else free as it is.
  FREE_AND_MAKE_NULL(obj, "LdapOperand_t obj", -1);
}

int fillOctetString(unsigned char *dst, char *str, int len, int fill_type) {
  int consumed = 0;

  NSDL2_LDAP(NULL, NULL, "Method called");
  if (len == 0)
    len = strlen(str);

  uint32_t bytes = 0;

  if (fill_type) {
    bytes = BER_STRING_TYPE;
    COPY_BYTES(dst, bytes, &consumed);
  }

  CHECK_AND_FILL_LEN(dst, len, &consumed);

  COPY_STRING(dst, str, &consumed);

  return consumed;
}

int fillSubstringSecondArg(unsigned char *dst, LdapOperand_t *obj) {
  unsigned char *ptr = dst;
  uint32_t bytes = 0;
  int consumed = 0;
  int filled_len = 0;
  int ret;

  unsigned char *length_ptr;

  NSDL2_LDAP(NULL, NULL, "Method called");
  // fill type for SEQUENCE. 
  bytes = BER_SEQUENCE_TYPE;

  COPY_BYTES(ptr, bytes, &consumed);

  length_ptr = ptr;

  ptr++;

  // it may have three fields. 
  // initial - * given in end
  // any    - * given in both end
  // final  - * given in start
  char initial[256] = "";
  char any[256] = "";
  char final[256] = "";

  if (obj->value[0] == '*' || obj->value[strlen(obj->value) - 1] == '*') {
  if (obj->value[0] == '*' && obj->value[strlen(obj->value) - 1] == '*') {
    // fill value.
    strcpy(any, obj->value+1);
    any[strlen(obj->value) - 2] = 0;
  } else if (obj->value[0] == '*') {
    strcpy(final, obj->value + 1);
    final[strlen(obj->value) - 1] = 0;
  } else {
    strcpy(initial, obj->value);
    initial[strlen(obj->value) - 1] = 0;
  }
  }
  else {
    // * is in center 
    char *tmp = strchr(obj->value, '*');
    if (tmp) {
      strncpy(initial, obj->value, (tmp - obj->value));
      initial[(tmp - obj->value)] = 0;

      // copy final.
      strcpy(final, tmp + 1);
    } else {
      // It should not happen. But sending as initial.
      strcpy(initial, obj->value);
    }
  }

  if (initial[0]) {
    bytes = 0x80;
    COPY_BYTES(ptr, bytes, &filled_len);

    ret = fillOctetString(ptr, initial, 0, 0);
    filled_len += ret;
    ptr += ret;
  } 
  if (any[0]) {
    bytes = 0x81;
    COPY_BYTES(ptr, bytes, &filled_len);

    ret = fillOctetString(ptr, any, 0, 0);
    filled_len += ret;
    ptr += ret;
  } 
  if (final[0]) {
    bytes = 0x82;
    COPY_BYTES(ptr, bytes, &filled_len);

    ret = fillOctetString(ptr, final, 0, 0);
    filled_len += ret;
    ptr += ret;
  } 

  if (filled_len > 127) {
    // need to memmove.
    memmove(length_ptr + 5, length_ptr + 1, filled_len);
  }

  CHECK_AND_FILL_LEN(length_ptr, filled_len, &consumed);

  consumed += filled_len;

  return consumed;
}

int fillSubstringFilter(unsigned char *dst, LdapOperand_t *obj) {
  unsigned char *ptr = dst;
  uint32_t bytes = 0;
  int consumed = 0;
  int filled_len = 0;

  unsigned char *length_ptr;

  NSDL2_LDAP(NULL, NULL, "Method called");
  // fill type for sequence. 
  bytes = BER_SEQUENCE_TYPE;
  COPY_BYTES(ptr, bytes, &consumed);

  //fill attribute value description. 
  length_ptr = ptr;
  ptr ++;

  // fill type/attribute description.
  filled_len += fillOctetString(ptr, obj->tag, 0, 1);

  // fill second arg. 
  filled_len += fillSubstringSecondArg(ptr, obj);

  if (filled_len > 127) {
    // need to move. 
    memmove(length_ptr + 5, length_ptr + 1, filled_len);
  }

  CHECK_AND_FILL_LEN(length_ptr, filled_len, &consumed);

  consumed += filled_len;

  return consumed;

}

int fillAttributeValueAssertion(unsigned char *dst, LdapOperand_t *obj) {
  unsigned char *ptr = dst;
  uint32_t bytes = 0;
  int consumed = 0;
  int filled_len = 0;
  int ret = 0;
  unsigned char *length_ptr;

  NSDL2_LDAP(NULL, NULL, "Method called, tag - %s, value - %s", obj->tag, obj->value);
  // fill type for sequence. 
  bytes = BER_SEQUENCE_TYPE;
  COPY_BYTES(ptr, bytes, &consumed);

  //fill attribute value description. 
  length_ptr = ptr;

  ptr ++;

  //copy description 
  ret = fillOctetString(ptr, obj->tag, 0, 1);
  filled_len += ret;
  ptr += ret;

  // fill assert value.
  ret = fillOctetString(ptr, obj->value, 0, 1);
  filled_len += ret;
  ptr += ret;

  if (filled_len > 127) {
    // need to move. 
    memmove(length_ptr + 5, length_ptr + 1, filled_len);
  }

  CHECK_AND_FILL_LEN(length_ptr, filled_len, &consumed);

  consumed += filled_len;

  return consumed;
}

LdapOperand_t *parseFilter(char *filterString, int *length_consumed) {
  char *ptr = filterString;
  int consumed = 0;
  LdapOperand_t *lastChild = NULL;
  char *tmp;

  NSDL2_LDAP(NULL, NULL, "Method called, filterString - %s", filterString);
  //skip all closing braces.
  while(*ptr == ')')
  { 
    ptr++;
  }

  //if nothing left. 
  //if (*ptr == '') return NULL;
  if (*ptr == '\0') return NULL;

  LdapOperand_t *obj = (LdapOperand_t *)malloc(sizeof(LdapOperand_t));
  memset(obj, 0, sizeof(LdapOperand_t));

  LdapOperand_t *operand = NULL;

  //skip first parenthisis.
  ptr ++;

  // check for composite operator.
  obj->operator = *ptr;

  if (obj->operator == '&' || obj->operator == '|' || obj->operator == '!') {
    ptr++;
    // fill operator. 
    obj->operator = obj->operator == '&' ? LDAP_SEARCH_AND : (
      obj->operator == '|' ? LDAP_SEARCH_OR : LDAP_SEARCH_NOT
    );

    // fill operand.
    while(1) {
      operand = parseFilter(ptr, &consumed);
      // update operand.
      ptr += consumed;


      if (lastChild == NULL) {
        obj->children = operand;
      } else {
         lastChild->next = operand;
      }
      lastChild = operand;

      if (operand == NULL) break;

      // Check if we got closing braces then break.
      if (*ptr == ')') {
        ptr ++;
        break;
      }

      if (*ptr == 0)
      {
        break;
      }
    }
  } else {
    //parse normal (a=b)

    //skip (
    //ptr++;

    // it can have >=,<=, ~=,

    tmp = strchr(ptr, '=');

    // check for other format.

    // TODO: Put validations. 
    // if (tmp == NULL) {
      // Put error and return

    // }

    if (tmp[-1] == '>')
      obj->operator = LDAP_SEARCH_GREATEREQUAL;
    else if (tmp[-1] == '<')
      obj->operator = LDAP_SEARCH_LESSEQUAL;
    else if (tmp[-1] == '~')
      obj->operator = LDAP_SEARCH_APPROX_MATCH;
    else
      obj->operator = LDAP_SEARCH_EQUALITY;

    if (obj->operator == LDAP_SEARCH_EQUALITY) {
      strncpy(obj->tag, ptr, (tmp - ptr));
      obj->tag[tmp - ptr] = 0;
    } else  {
      // skip first char in operator.
      strncpy(obj->tag, ptr, (tmp - ptr -1));
      obj->tag[tmp - ptr - 1] = 0;
      
    }


    // skip =.
    ptr = tmp +1;

    // TODO: validation
    tmp = strchr(ptr, ')');
    strncpy(obj->value, ptr, (tmp - ptr));
    obj->value[tmp - ptr] = 0;

    // find till closing braces
    ptr = tmp +1;

    //identify operator here. 
    // check for present and substring.
    if (obj->operator == LDAP_SEARCH_EQUALITY) {
      // if value is given as * 
      if (obj->value[0] == '*' && obj->value[1] == 0) {
        obj->operator = LDAP_SEARCH_PRESENT;
      } else if (strchr(obj->value, '*') != NULL) /*value contains * */ {
        obj->operator = LDAP_SEARCH_SUBSTRING;
      }
    }
  }

  //update consumed.
  *length_consumed = (ptr - filterString);
  NSDL2_LDAP(NULL, NULL, "Method Exiting., length_consumed - %d", *length_consumed);
  return obj;
}

int fillFilter(unsigned char *dst, LdapOperand_t *obj) {
  uint8_t byte;
  int consumed = 0;
  unsigned char *ptr = dst;
  unsigned char *length_ptr;
  int filled_len = 0;
  LdapOperand_t *operand;
  int ret = 0;

  NSDL2_LDAP(NULL, NULL, "Method called. obj->operator = %d", obj->operator);
  // fill type on basis of operator.
  switch(obj->operator) {
    case LDAP_SEARCH_AND:
      byte = 0xA0;
      break;
    case LDAP_SEARCH_OR:
      byte = 0xA1;
      break;
    case LDAP_SEARCH_NOT:
      byte = 0xA2;
      break;
    case LDAP_SEARCH_EQUALITY:
      byte = 0xA3; 
      break;
    case LDAP_SEARCH_GREATEREQUAL:
      byte = 0xA5;
      break;
    case LDAP_SEARCH_LESSEQUAL:
      byte = 0xA6;
      break;
    case LDAP_SEARCH_APPROX_MATCH:
      byte = 0xA8;
      break;
    case LDAP_SEARCH_SUBSTRING:
      byte = 0xA4;
      break;
    case LDAP_SEARCH_EXTENSIBLE:
      byte = 0xA9;
      break;  
    case LDAP_SEARCH_PRESENT:
      byte = 0x87;
      break;
/*
    default: 
       // TODO: do error handling.
*/
  }

  COPY_BYTES(ptr, byte, &consumed);
  
  length_ptr = ptr;

  ptr++;
  
  //TODO: fill length later. 
  switch(obj->operator) {
    case LDAP_SEARCH_EQUALITY:
    case LDAP_SEARCH_GREATEREQUAL:
    case LDAP_SEARCH_APPROX_MATCH:
    case LDAP_SEARCH_LESSEQUAL:
      // need to fill both the strings.
      ret = fillOctetString(ptr, obj->tag, 0, 1); 
      ptr += ret;
      filled_len += ret;

      ret = fillOctetString(ptr, obj->value, 0, 1);
      ptr += ret;
      filled_len += ret;
/*
      ret = fillAttributeValueAssertion(ptr, obj);
      filled_len += ret;
      ptr += ret;
*/
      break;

    case LDAP_SEARCH_PRESENT:
      // Note: it is a primite/string so don't leave byte for length. 
      ptr --;
      //fill attribute description.
      // filling after 1 byte so we can get space of filling length. 
      // Need not to set string type. 
      filled_len += fillOctetString(ptr, obj->tag, 0, 0);

      // Note: it is a primitive and we already have filled length in fillOctetString. So we are done here. 
      consumed += filled_len;

      return consumed;

    case LDAP_SEARCH_SUBSTRING:
       // fill type/attribute description.
       ret = fillOctetString(ptr, obj->tag, 0, 1);
       filled_len += ret;
       ptr += ret;

       // fill second arg. 
       ret = fillSubstringSecondArg(ptr, obj);
       filled_len += ret;
       ptr += ret;

      // filled_len += fillSubstringFilter(ptr, obj);
      break;

    case LDAP_SEARCH_OR:
    case LDAP_SEARCH_AND:
    case LDAP_SEARCH_NOT:
      // these are composite one. 
      // fill all operands.
      operand = obj->children;
      while(operand) {
        ret = fillFilter(ptr, operand);
        filled_len += ret;
        ptr += ret;
        operand = operand->next;
      }
      break;

    default:
      NSDL2_LDAP(NULL, NULL, "Control should not come in this lag.");
      break; 
  }  

  // If it is more than 127 then length will occupy 5 byte but we had given space for 1 byte only so have to move. 
  if (filled_len > 127) 
  {
    memmove(length_ptr + 5, length_ptr + 1, filled_len);
  }

  CHECK_AND_FILL_LEN(length_ptr, filled_len, &consumed);

  consumed += filled_len;

  return consumed;
} 

//bind request
/*Currently we are supporting simple authentication only: TODO: support for sasl and md5 authentication also
   case1: Anonymous(no username(DN), no password)
   case2: Unauthenticated (username(dn), no password)
   case3: Authenticated (username(dn), password)

*/
void fill_user_pass(unsigned char **msg, int* len, Authentication auth){
   unsigned char *ptr = *msg;
   int bytes = 0;
   int dn_len = 0;
   int mech_len = 0;
   int dm_len = 0;
   int aa;
   
   NSDL2_LDAP(NULL, NULL, "Method called");
   //validate authentication, 0:simple 1,2:reserved 3:sasl  
   (auth.type != NULL)?(aa = atoi(auth.type)):(aa = 0); 

   if(aa != 0){ //only supporting simple authentication type, TODO: support for sasl also
   //log error 
   //exit
   }
   bytes = bytes | 0x60;
   COPY_BYTES(ptr, bytes, len);

   if(auth.dn_name != NULL)
      dn_len = strlen(auth.dn_name);
   if(auth.auth_name != NULL)
      mech_len = strlen(auth.auth_name);

   NSDL2_LDAP(NULL, NULL, "dn_name length = [%d]  auth_len = [%d]", dn_len, mech_len);

  // Calculate len 
  // user name len + 3 bytes of LDAP version + 2 bytes for tag +len
   dn_len>127?(dm_len=dn_len+3+2+4):(dm_len=dn_len+3+2);
   mech_len>127?(dm_len += mech_len+4+2):(dm_len += mech_len+2);

   CHECK_AND_FILL_LEN(ptr, dm_len, len);
  // COPY_BYTES(ptr, bytes, len);
   //fill version

   bytes = bytes | 0x02;
   COPY_BYTES(ptr, bytes, len);
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, len);
   bytes = bytes | 0x03;    //in our case we are filling default to version 3
   COPY_BYTES(ptr, bytes, len);

   //fill dn_name
   if(auth.dn_name){
     bytes = bytes | 0x04; 
     COPY_BYTES(ptr, bytes, len);
     CHECK_AND_FILL_LEN(ptr, dn_len, len);
     COPY_STRING(ptr, auth.dn_name, len);
   }else{
     bytes = bytes | 0x04;
     COPY_BYTES(ptr, bytes, len);
     bytes = bytes | 0x00;
     COPY_BYTES(ptr, bytes, len);
   }
     
  //we are only supporting simple type
     bytes = bytes | 0x80 | aa;   //TODO: support for sasl also
    
   COPY_BYTES(ptr, bytes, len); 
  //fill authentication mechanism 
   if(auth.auth_name){
     CHECK_AND_FILL_LEN(ptr, mech_len, len);
     COPY_STRING(ptr, auth.auth_name, len);
   }else{
     bytes = bytes | 0x00;
     COPY_BYTES(ptr, bytes, len);
   }
}

void fill_modify_type_value(unsigned char **tv_ptr, int *tv_len, Add add){
  unsigned char *ptr = *tv_ptr;
  char *tmp_ptr = NULL;
  char *tmp_ptr2 = NULL;
  char *fields[100];
  int operation;

  NSDL2_LDAP(NULL, NULL, "Method called");

  if(add.msg_buf == NULL){//if add message is null we are treating as an error and exitting,TODO:handle it in api parsing
     NS_EXIT(1, "Add/Modify operation/atrribute/value can't be null.");
  }

  unsigned char *free_me = (unsigned char*)malloc(2048 + 1);  
  unsigned char *mm = free_me;
  int i;
  int num_tokens = 0;
  int free_len = 0;

  num_tokens = get_tokens(add.msg_buf, fields, "|", 100); 

  for(i=0; i<num_tokens; i++){
    //get operation here
    tmp_ptr = strchr(fields[i], ',');
    *tmp_ptr = '\0';
    tmp_ptr++;
    tmp_ptr2 = tmp_ptr;

    //get type and value here
    tmp_ptr = strchr(tmp_ptr2, '=');
    *tmp_ptr = '\0';
    tmp_ptr++;

    if(!strcasecmp(fields[i], "ADD")){ operation = 0;}
    else if(!strcasecmp(fields[i], "DELETE")){ operation = 1;}
    else if(!strcasecmp(fields[i], "MODIFY")){ operation = 2;}
    else{ NS_EXIT(1, "Unrecognized modify operation in ldap.");}

    //adding value
    ADD_VAL(free_me, tmp_ptr, &free_len);
    //copy type
    ADD_MOD_TYPE(ptr, tmp_ptr2, mm, free_len, tv_len, operation);
    free_len = 0;
    free_me = mm;
  }
 
  free(mm);
  //DUMP_DATA(tv_len);
}


void fill_type_value(unsigned char **tv_ptr, int *tv_len, Add add){
  unsigned char *ptr = *tv_ptr;
  char *tmp_ptr = NULL;
  int bytes = 0;
  char *fields[100];
  char flags[100] = {0};

  NSDL2_LDAP(NULL, NULL, "Method called");

  if(add.msg_buf == NULL){//if add message is null we are treating as an error and exitting,TODO:handle it in api parsing
     //print error message and exit
     NS_EXIT(1, "Add/Modify operation/atrribute/value can't be null.");
  }

  unsigned char *free_me = (unsigned char*)malloc(2048 + 1);  
  unsigned char *mm = free_me;
  int i;
  int j;
  int num_tokens = 0;
  int free_len = 0;

  num_tokens = get_tokens(add.msg_buf, fields, "|", 100); 

  for(i=0; i<num_tokens; i++){
    if((tmp_ptr = strchr(fields[i], '=')) != NULL){
      *tmp_ptr = '\0';
      tmp_ptr++;
    }else{
      NSDL2_LDAP(NULL, NULL, " = is missing in name=value pair");
      fprintf(stderr, " = is missing in name=value pair, %s\n", fields[i]);
      END_TEST_RUN
    }
   //first entry for val
    if(flags[i] == 0){
      ADD_VAL(free_me, tmp_ptr, &free_len);
    }

   //check if all entries for this type
    for(j=i+1; j<num_tokens; j++){
      tmp_ptr = strchr(fields[j], '=');
      if(flags[j] == 0 && !strncmp(fields[j], fields[i], strlen(fields[i])>tmp_ptr-fields[j]?strlen(fields[i]):tmp_ptr-fields[j])){
         if(tmp_ptr != NULL){
           tmp_ptr++; 
           ADD_VAL(free_me, tmp_ptr, &free_len);
           flags[j] = 1;
         }else{
           NSDL2_LDAP(NULL, NULL, " = is missing in name=value pair");
           fprintf(stderr, " = is missing in name=value pair, %s\n", fields[j]);
           END_TEST_RUN
         }
      }
    }
  //copy type
    if(flags[i] == 0){
      //  ADD_MOD_TYPE(ptr, fields[i], mm, free_len, tv_len, operation);   //in case of modify operation call this macro and in case of add call ADD_TYPE
       ADD_TYPE(ptr, fields[i], mm, free_len, tv_len);
       
       flags[i] = 1;
       free_len = 0;
       free_me = mm;
    }
  } 
  free(mm);
  //DUMP_DATA(tv_len);
}

void make_delete_request(connection *cptr, unsigned char **req_buf,int* msg_len, Delete del){
   int bytes = 0;
   unsigned char *ptr = *req_buf;
   int mm_len = 0;
   int msgID = 1;
   int dn_len = 0;

   NSDL2_LDAP(NULL, cptr, "Exitting method, request message length = [%d]", *msg_len);

   if(del.dn[0]){
     dn_len = strlen(del.dn);
   }else{
     fprintf(stderr, "Exitting method, dn cant be null in LDAP delete request\n"); 
     END_TEST_RUN
   }
 
   if(dn_len > 127)
     mm_len = dn_len + 6 + 3; 
   else
     mm_len = dn_len + 2 + 3;

   bytes = bytes | 0x30;
   COPY_BYTES(ptr, bytes, msg_len);
   CHECK_AND_FILL_LEN(ptr, mm_len, msg_len);
 
   bytes = bytes | 0x02;
   COPY_BYTES(ptr, bytes, msg_len);
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, msg_len);
   bytes = bytes | msgID;
   COPY_BYTES(ptr, bytes, msg_len);
   msgID++;


   bytes = bytes | 0x4a;
   COPY_BYTES(ptr, bytes, msg_len);
  
   CHECK_AND_FILL_LEN(ptr, dn_len, msg_len);
   COPY_STRING(ptr, del.dn, msg_len);

   NSDL2_LDAP(NULL, cptr, "Exitting method, request message length = [%d]", *msg_len);
   //DUMP_DATA(msg_len); 
}
 
void make_rename_request(connection *cptr, unsigned char **req_buf, int* msg_len, Rename ren){
   int bytes = 0;
   unsigned char *ptr = *req_buf;
   int mm_len = 0;
   int dn_len = 0;
   int rdn_len = 0;
   int total_len = 0;
   int del_old = 1;
   int msgID = 1;

   NSDL2_LDAP(NULL, cptr, "Exitting method, request message length = [%d]", *msg_len);

 
   if(!(ren.dn[0]) || !(ren.new_dn[0]))
   {
     fprintf(stderr, "Exitting method, dn or rdn cant be null in LDAP rename request"); 
     END_TEST_RUN
   }

  if(ren.del_old[0]){
     del_old = atoi(ren.del_old);
     if(del_old < 0 || del_old > 1){
       NS_EXIT(1, "Exiting method, delete old dn is boolean it can be only 0 or 1 in LDAP rename request");
     }
   }

   dn_len = strlen(ren.dn);
   rdn_len = strlen(ren.new_dn);

   (dn_len>127)?(mm_len=dn_len+6+3):(mm_len=dn_len+2+3); //extra 3 bytes for del_old flag
   (rdn_len>127)?(mm_len += rdn_len+6):(mm_len += rdn_len+2);

   (mm_len>127)?(total_len=mm_len+3+6):(total_len=mm_len+3+2);

   bytes = bytes | 0x30;
   COPY_BYTES(ptr, bytes, msg_len);
   CHECK_AND_FILL_LEN(ptr, total_len, msg_len);
 
   bytes = bytes | 0x02;
   COPY_BYTES(ptr, bytes, msg_len);
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, msg_len);
   bytes = bytes | msgID;
   COPY_BYTES(ptr, bytes, msg_len);
   msgID++;


   bytes = bytes | 0x6c;
   COPY_BYTES(ptr, bytes, msg_len);
   CHECK_AND_FILL_LEN(ptr, mm_len, msg_len);

   bytes = bytes | 0x04;
   COPY_BYTES(ptr, bytes, msg_len);
   CHECK_AND_FILL_LEN(ptr, dn_len, msg_len);
   COPY_STRING(ptr, ren.dn, msg_len);
   
   bytes = bytes | 0x04;
   COPY_BYTES(ptr, bytes, msg_len);
   CHECK_AND_FILL_LEN(ptr, rdn_len, msg_len);
   COPY_STRING(ptr, ren.new_dn, msg_len);

   bytes = bytes | 0x02;
   COPY_BYTES(ptr, bytes, msg_len);
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, msg_len);
   bytes = bytes | del_old;
   COPY_BYTES(ptr, bytes, msg_len);

   NSDL2_LDAP(NULL, cptr, "Exitting method, request message length = [%d]", *msg_len);
   //DUMP_DATA(msg_len); 
}

void make_add_request(connection *cptr, unsigned char **req_buf, int* msg_len, Add add, int operation){
   int bytes = 0;
   unsigned char *ptr = *req_buf;
   int msgID = 1;
   int tv_len = 0;
   int DN_l = 0;
   int OP_l = 0;
   int MSG_l = 0;

   //start_filling message
   NSDL2_LDAP(NULL, cptr, "Method called");

   if(!add.dn[0]){
     fprintf(stderr, "Exitting method, dn cant be null in LDAP add request");
     END_TEST_RUN
   }

   unsigned char *val_ptr = (unsigned char*)malloc(2048 + 1);

   if(operation == LDAP_ADD)
     fill_type_value(&val_ptr, &tv_len, add);
   else
     fill_modify_type_value(&val_ptr, &tv_len, add);

   //CALCULATE_LEN(DN_l, OP_l, MSG_l, tv_len);
   CALCULATE_MOD_LEN(DN_l, OP_l, MSG_l, tv_len);
   
   bytes = bytes | 0x30;
   COPY_BYTES(ptr, bytes, msg_len);

   CHECK_AND_FILL_LEN(ptr, MSG_l, msg_len);

  //fill message id here
   bytes = bytes | 0x02;
   COPY_BYTES(ptr, bytes, msg_len);
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, msg_len);
   bytes = bytes | msgID;
   COPY_BYTES(ptr, bytes, msg_len);
     
   //fill operation here
   operation==LDAP_ADD?(bytes = bytes | 0x68):(bytes = bytes | 0x66);
   COPY_BYTES(ptr, bytes, msg_len);

   CHECK_AND_FILL_LEN(ptr, OP_l, msg_len);

   //fill Dn here
   bytes = bytes | 0x04;
   COPY_BYTES(ptr, bytes, msg_len);

   CHECK_AND_FILL_LEN(ptr, strlen(add.dn), msg_len);
   COPY_STRING(ptr, add.dn, msg_len); 


   /*changes verify it*/
   bytes = bytes | 0x30;
   COPY_BYTES(ptr, bytes, msg_len);    
   CHECK_AND_FILL_LEN(ptr, tv_len, msg_len);


   bcopy(val_ptr, ptr, tv_len); 
   *msg_len += tv_len; 

   NSDL2_LDAP(NULL, cptr, "Exitting method, request message length = [%d]", *msg_len);
  // DUMP_DATA(msg_len);
   free(val_ptr);

}

void make_unbind_request(connection *cptr, unsigned char **req_buf, int* msg_len){
   int bytes = 0;
   unsigned char *ptr = *req_buf;
   int msgID = 1;

   NSDL2_LDAP(NULL, cptr, "Method called");

   bytes = bytes | 0x30;
   COPY_BYTES(ptr, bytes, msg_len);

   bytes = bytes | 0x05;            //fixed length as there will be nothing in this request
   COPY_BYTES(ptr, bytes, msg_len);

   // Set byte for integer
   bytes = bytes | 0x02;
   COPY_BYTES(ptr, bytes, msg_len);

   // Set byte for message id len
   bytes = bytes | 0x01;
   COPY_BYTES(ptr, bytes, msg_len);

   // Set byte for message id 
   bytes = bytes | msgID;
   COPY_BYTES(ptr, bytes, msg_len);
   msgID++;   //right now taking it as local,  TODO: need to check its significanse

   bytes = bytes | 0x42;
   COPY_BYTES(ptr, bytes, msg_len);
 
   bytes = bytes | 0x00;
   COPY_BYTES(ptr, bytes, msg_len);
}

void make_bind_request(connection *cptr, unsigned char **req_buf, int* msg_len, Authentication auth){

   int bytes = 0;
   unsigned char *ptr = *req_buf;
   int msgID = 1;
   int mm_len = 0;

   unsigned char *user_msg = (unsigned char *)malloc(2048);

   NSDL2_LDAP(NULL, cptr, "Method called");
   fill_user_pass(&user_msg, &mm_len, auth); 

   // Now we have operation message in user_msg, we will create a SEQUENCE for bind message
   // This SEQUENCE will contain message id and operation OPERATION   

   // Set byte for sequence 
   bytes = bytes | 0x30;
   COPY_BYTES(ptr, bytes, msg_len);
   CHECK_AND_FILL_LEN(ptr, (mm_len+3), msg_len);


   // Set byte for integer
   bytes = bytes | 0x02; 
   COPY_BYTES(ptr, bytes, msg_len);

   // Set byte for message id len
   bytes = bytes | 0x01; 
   COPY_BYTES(ptr, bytes, msg_len);

   // Set byte for message id 
   bytes = bytes | msgID; 
   COPY_BYTES(ptr, bytes, msg_len);
   msgID++;
  
   // copy operation msg 
   bcopy(user_msg, ptr, mm_len);
    
   *msg_len += mm_len;

   NSDL2_LDAP(NULL, cptr, "Exitting method, mm_length = [%d] request message length = [%d]", mm_len, *msg_len);
   //DUMP_DATA(MSG_LEN); 
   free(user_msg);
}

//search request
void make_search_request(connection *cptr, unsigned char **req_buf, int* msg_len, Search ss)
{
   int bytes = 0;
   unsigned char *ptr = *req_buf;
   int msgID = 1;
   int base_len= 0;
   int filter_len= 0;
   int attr_len= 0;
   int total_len = 0;
   int tmp_len =0;

   NSDL2_LDAP(NULL, cptr, "Method called");

   if(!ss.base[0]){
     fprintf(stderr, "Exitting method, base dn cant be null in LDAP search request");
     END_TEST_RUN
   }

   unsigned char *base_ptr = (unsigned char*)malloc(2048 + 1);
   unsigned char *filter_ptr = (unsigned char*)malloc(2048 + 1);
   unsigned char *attr_ptr = (unsigned char*)malloc(2048 + 1);

   //make a seperate function for messageID to typesOnly bytes
   fill_bDN_ENUM(&base_ptr, &base_len, ss);

   //fill_filter
   //fill_filter(&filter_ptr, &filter_len, ss);
   //TODO: Discuss with Devendar sir. Parameters
   NSDL2_LDAP(NULL, cptr, "ss.filter = %s", ss.filter);
   LdapOperand_t *ldap_op = parseFilter(ss.filter, &tmp_len);
   filter_len = fillFilter(filter_ptr, ldap_op);
   NSDL2_LDAP(NULL, cptr, "filter_len = %d, filter_ptr = %p", filter_len, filter_ptr);

   free_LdapOperand(ldap_op);

   //free recursive
   //free(ldap_op);

   fill_attribute(&attr_ptr, &attr_len, ss);

   bytes = bytes | 0x30;
   COPY_BYTES(ptr, bytes, msg_len);

   int bfa_len = base_len + filter_len + attr_len;

   // add some extra bytes for long or short format of length
   // if length is less then equal 127 then operation will take 5 bytes( one for operation tag, one for operation length, 3 for message id )
   bfa_len>127?(total_len = bfa_len + 9):(total_len = bfa_len + 5);

   // replace below code with macro Check_AND_FILL_LEN
   if(total_len > 127){
     bytes = bytes | 0x84;
     COPY_BYTES(ptr, bytes, msg_len);
     bytes = bytes | total_len;
     COPY_INTEGER(ptr, bytes, msg_len);
   }else{
     bytes = bytes | total_len;
     COPY_BYTES(ptr, bytes, msg_len);
   }

   //fill message ID
   bytes = bytes | 0x02;
   COPY_BYTES(ptr, bytes, msg_len);
   
   //fill its value, TODO: handle for long format also
   bytes = bytes | 0x01;//right now we are assuming it will be in one byte
   COPY_BYTES(ptr, bytes, msg_len);
    
   bytes = bytes | msgID;
   COPY_BYTES(ptr, bytes, msg_len);
   msgID++;

   // fill protocol type here
   bytes = bytes | 0x63; //for search operation
   COPY_BYTES(ptr, bytes, msg_len);

   //fill length of rest of the message here , TODO: Replace it with Macro CHECK_AND_FILL_LEN
   if(bfa_len > 127){
     bytes = bytes | 0x84;
     COPY_BYTES(ptr, bytes, msg_len);
     bytes = bytes | bfa_len;
     COPY_INTEGER(ptr, bytes, msg_len);
   }else{
     bytes = bytes | bfa_len;
     COPY_BYTES(ptr, bytes, msg_len);
   }

   //concatenate baseDN, filter and attribute here
   bcopy(base_ptr, ptr, base_len);
   ptr += base_len;
   *msg_len += base_len;
   bcopy(filter_ptr, ptr, filter_len);
   ptr += filter_len;
   *msg_len += filter_len;
   bcopy(attr_ptr, ptr, attr_len);
   *msg_len += attr_len;

   free(base_ptr);
   free(filter_ptr);
   free(attr_ptr);
  
   NSDL2_LDAP(NULL, cptr, "Exitting method, request message length = [%d]", *msg_len);
   //DUMP_DATA(msg_len); 
}
/*
int main(){
    unsigned char *buffer = NULL;
    int remaining_len = 8192;
    int used = 0;

    buffer = (unsigned char*)malloc(8192);
    memset(buffer, 0, 8192);

    make_bind_request(&buffer, &remaining_len, &used, auth);
    make_add_request(&buffer, &remaining_len, &used, add);
    make_search_request(&buffer, &remaining_len, &used, ss); 
    make_delete_request(&buffer, &remaining_len, &used, del); 
    make_unbind_request(&buffer, &remaining_len, &used, auth);
    make_modify_request(&buffer, &remaining_len, &used, add);

}*/
