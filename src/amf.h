
#ifndef  __NS_AMF__
#define __NS_AMF__ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <ctype.h>


#define AMF_VERSION0 0
#define AMF_VERSION3 3
//unsigned short version; // 0/3
#define MAX_INTEGER_VALUE              0x0FFFFFFF       //Max value that can be stored in 28 bits
#define MIN_INTEGER_VALUE              -0x1FFFFFFF      //Max value that can be stored in 28 bits
#define MAX_STRING_LEN                 0x0FFFFFFF       //Max value for string
#define EMPTY_STRING                   0x01     //Empty String denoting end of dynamic members
#define MAX_VAL                        8192
#define STR_VAL                        8192 * 4         //Max buffer 32 KB for AMF3 String 
#define MAX_TRAITS_REF_SIZE_27_BITS    0x07FFFFFF
#define MAX_SIZE_28_BITS               0x0FFFFFFF
#define AMF_NAN_DOUBLE                 "9223372036317904896"
#define AMF3_ERROR                     -1
#define AMF3_SUCCESS                   0
#define AMF3_DO_NOT_COPY_MARKER        0
#define AMF3_COPY_MARKER               1
#define AMF3_ARRAY_KEY                 1
#define AMF3_NOT_ARRAY_KEY             0
#define AMF3_DYNAMIC_MEMBER            1
#define AMF3_NOT_DYNAMIC_MEMBER        0
#define AMF3_TYPE_OBJECT               1
#define AMF3_NOT_TYPE_OBJECT           0
#define AMF_DO_NOT_TRIM_STRING         0
#define AMF_TRIM_STRING                1
#define AMF3_EXTERNAZIABLE             1
#define AMF3_NOT_EXTERNAZIABLE         0


extern char amf_data_version;
extern char amf_pkt_version;

extern char *read_data_amf0 (char *out, int *len);
extern char *amf3_read_data (char *out, int *len,
                             short is_dyn_member, short is_array_member, short is_externalized);
extern char * amf_decode (int out_type, void *out, int offset, char *in, int *len);

extern int convert_amf_hextoasc (char *in, int *len);
extern void amf_encode_value (struct iovec *vector, int idx, char * value, int vlen, int version);
extern char *write_data_amf3 (int indent, char *in, int *len, char *attr);
extern void
append_to_xml (char *format, ...);
extern char * leading_blanks (int num);
extern char 
*readExternalData(int indent, int *len, char *type, char* in, int body_len, int body_left, int available);

char *
amf_encode (int src_type, void *src_ptr, int src_parameter, char *out, int *len,
          int out_mode, int no_param_flag, int *version);

extern void put_amf_debug (FILE * fp, char *buf, int len);
extern void show_buf (char *buf, int len);

extern char line[], copy_to[], copy_from[];
extern FILE *amf_infp;
extern int amfin_lineno;
extern char *amfin_ptr;
extern int amfin_left;
extern int amf_out_mode;	//0: standard AMF bytes.
extern int amf_no_param_flag;	//0: standard AMF bytes.
extern unsigned short amf_seg_count;
extern char *last_seg_start;	//ptr in out for last seg start
extern unsigned short seg_len;

extern FILE *outfp;
extern char *out_buf;
extern int max_mlen;
extern int mlen;
extern char *amf_asc_ptr;

extern char *strptime (const char *s, const char *format, struct tm *tm);

extern void amf_set_debug(int debug_level);
extern void amf_debug_log(int level, int log_always, char *filename, int line, char *fname, char *format, ...);

extern int ns_amf_binary_to_xml (char *in, int *len);
//#Shilpa 7Mar11 - Defined for AMF3
//--------------------------------------------------------------
#define AMF0_NUMBER                    0x00
#define AMF0_BOOLEAN                   0x01
#define AMF0_STRING                    0x02
#define AMF0_OBJECT                    0x03
#define AMF0_MOVIECLIP                 0x04
#define AMF0_NULL                      0x05
#define AMF0_UNDEFINED                 0x06
#define AMF0_REFERENCE                 0x07
#define AMF0_ECMA_ARRAY                0x08
#define AMF0_OBJECT_END_MARKER         0x09
#define AMF0_STRICT_ARRAY              0x0A
#define AMF0_DATE                      0x0B
#define AMF0_LONG_STRING               0x0C
#define AMF0_UNSUPPORTED               0x0D
#define AMF0_RECORDSET                 0x0E
#define AMF0_XML_DOCUMENT              0x0F
#define AMF0_TYPED_OBJECT              0x10
#define AMF0_TO_AMF3                   0x11


#define AMF3_UNDEFINED                 0x00
#define AMF3_NULL                      0x01
#define AMF3_FALSE                     0x02
#define AMF3_TRUE                      0x03
#define AMF3_INTEGER                   0x04
#define AMF3_DOUBLE                    0x05
#define AMF3_STRING                    0x06
#define AMF3_XML_DOC                   0x07
#define AMF3_DATE                      0x08
#define AMF3_ARRAY                     0x09
#define AMF3_OBJECT                    0x0A
#define AMF3_XML                       0x0B
#define AMF3_BYTE_ARRAY                0x0C

//---------------------------------------------------------------


typedef struct 
{
  char externalizable;
  char dynamic;
  int  object_ref;
  int  traits_ref;
  int  classname_ref;
  int  sealed_members;
  int  dyn_member_ref;
  int  key_ref;
  char dyn_member_name[MAX_VAL];
  char key[MAX_VAL];
  char classname[MAX_VAL];
} amf3_object;

typedef struct 
{
  char is_parameterized;
  int  str_ref;
  int  dyn_member_ref;
  int  key_ref;
  char dyn_member_name[MAX_VAL];
  char key[MAX_VAL];
  char str_val[MAX_VAL * 4 + 1]; // TODO - This may not hold large strings
} amf3_string;

typedef struct 
{
  char is_parameterized;
  int  date_ref;
  int  dyn_member_ref;
  int  key_ref;
  char dyn_member_name[MAX_VAL];
  char key[MAX_VAL];
  char date_val[MAX_VAL];
} amf3_date;

typedef struct 
{  
  int  dyn_member_ref;
  int  key_ref;
  char dyn_member_name[MAX_VAL];
  char key[MAX_VAL];
} amf3_simple_datatypes;

typedef struct 
{  
  int  byte_array_ref;
  int  dyn_member_ref;
  int  key_ref;
  char dyn_member_name[MAX_VAL];
  char key[MAX_VAL];
} amf3_byte_array;


typedef struct 
{ 
  char is_parameterized;
  int  dyn_member_ref;
  int  key_ref;
  char dyn_member_name[MAX_VAL];
  char key[MAX_VAL];
  char int_val[MAX_VAL];
} amf3_integer;

typedef struct 
{
  int  size;
  int  arr_ref;
  int  dyn_member_ref;
  int  key_ref;
  char dyn_member_name[MAX_VAL];
  char key[MAX_VAL];
} amf3_array;

typedef struct 
{
  int  str_ref;
  char member_val[MAX_VAL];
} amf3_member;

#define XML_INDENT 2

#ifdef USE_WITH_NS
#define AMFDL1(...) NSDL1_HTTP(NULL, NULL, __VA_ARGS__)
#define AMFDL2(...) NSDL2_HTTP(NULL, NULL, __VA_ARGS__)
#define AMFDL3(...) NSDL3_HTTP(NULL, NULL, __VA_ARGS__)
#define AMFDL4(...) NSDL4_HTTP(NULL, NULL, __VA_ARGS__)

#define AMFEL(...) NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING, __FILE__, (char*)__FUNCTION__, __VA_ARGS__)

#else
#define AMFDL1(...) amf_debug_log(1, 0, _FLN_, __VA_ARGS__)
#define AMFDL2(...) amf_debug_log(2, 0, _FLN_, __VA_ARGS__)
#define AMFDL3(...) amf_debug_log(3, 0, _FLN_, __VA_ARGS__)
#define AMFDL4(...) amf_debug_log(4, 0, _FLN_, __VA_ARGS__)

#define AMFDL_ALWAYS(...) amf_debug_log(1, 1, _FLN_, __VA_ARGS__)

// Events are logged always
#define AMFEL(...) amf_debug_log(1, 1, _FLN_, __VA_ARGS__)

#endif

#define AMF3_EXTERNALIZABLE_FLAG 0x01
#define AMF3_DYNAMIC_FLAG        0x02

#define AMF_END_TEST_RUN exit(-1);

#define AMF_MY_REALLOC(buf, size, msg, index)  \
{ \
    if (size <= 0) {  \
      fprintf(stderr, "Trying to realloc a negative or 0 size (%d) for index  %d\n", (int)size, index); \
      AMF_END_TEST_RUN  \
    } else {  \
      buf = (void*)realloc(buf, size); \
      if ( buf == (void*) 0 )  \
      {  \
        fprintf(stderr, "Out of Memory: %s for index %d\n", msg, index); \
        AMF_END_TEST_RUN;  \
      }  \
    } \
  }

//GET num bytes from 'in' buffer to 'copy_to' buffer
#define GET_BYTES(num)  if (num >= STR_VAL) {\
                            AMFEL("Getting more than 32 KB\n");\
                            return NULL;\
                         }\
                         if (*len < num) {\
                            AMFEL("Not enough data in input data buffer to read  message\n");\
                            return NULL;\
                           }\
                           bcopy (in, copy_to, num);\
                           in += num;\
                           *len -= num;

// AMF3
#define GET_OBJECT_AMF3(data, indent) if ((in = get_object_amf3(data, in, len, indent)) == NULL) {\
    AMFEL("ERROR: failed to get amf3 object. still left %d bytes\n", *len);\
    append_to_xml ("%s</object>\n", leading_blanks (indent)); \
    return NULL;\
  }

extern char * get_object_amf3(char *buf, char *in, int *len, int indent);

#define WRITE_DATA_AMF3(indent, in, len, attr) if ((in = write_data_amf3 (indent, in, len, attr)) == NULL) {\
    AMFEL("ERROR: failed to write data amf3. still left %d bytes\n", *len);\
    return NULL;\
  }

#endif
