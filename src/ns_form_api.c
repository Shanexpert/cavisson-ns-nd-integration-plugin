/*-----------------------------------------------------------------------------
Name         :  ns_form_api.c
Description  :  This file contains API and methods for processing html forms
Flow details :
Modification History:
 for standalone compilation
 gcc -g -DTEST ns_form_api.c -o  ns_form_api
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdarg.h>
#include "ns_string.h"
#include "nslb_encode.h"
#include "ns_exit.h"

extern char* ns_eval_string_flag_internal(char* string, int encode_flag, long *size, VUser *vptr);
extern int ns_save_string_flag_internal(const char* param_value, int value_length, const char* param_name, int encode_flag, VUser *my_vptr, int not_binary_flag); 

#define LEFT_SEARCH_STRING_1 "<input "
#define LEFT_SEARCH_STRING_2 "<select "
#define LEFT_SEARCH_STRING_3 "<textarea "
#define LEFT_SEARCH_STRING_1_LEN 7
#define LEFT_SEARCH_STRING_2_LEN 8
#define LEFT_SEARCH_STRING_3_LEN 10
#define RIGHT_SEARCH_STRING_1 "\">"
#define RIGHT_SEARCH_STRING_2 "\"/>"
#define RIGHT_SEARCH_STRING_3 "\' />"
#define RIGHT_SEARCH_STRING_4 "\" />"
#define RIGHT_SEARCH_STRING_5 "\'/>"
#define RIGHT_SEARCH_STRING_6 "\"  />"
#define RIGHT_SEARCH_STRING_7 "\"   />"
#define RIGHT_SEARCH_STRING_8 "/>"
#define RIGHT_SEARCH_STRING_9 ">"

#define MAX_ATTRIBUTE_LEN 512


#define INIT_NUM_ALLOCATED_ATTRIBUTES 50
#define DELTA_NUM_ALLOCATED_ATTRIBUTES 50

//#define MAX_NUM_ATTRIBUTES 50

#define MAX_BUFFER_LEN 1024

#define MAX_NUM_RADIO_NAMES_SAVED 10

#define MAX_FORM_BUF_SIZE 1024*1024
#define MAX_BODY_BUF_SIZE 128*1024

#define MAX_HTML_DECODED_STRLEN 1024


//Narendra: This datatype will be used in place of charcter buffer. 
typedef struct nsString{
  char *ptr;
  int len;
}nsString;

struct replace_attribute{
  nsString name;
  nsString value;
  int cur_occurance;
  char addattr_flag; /* If set, this is a new attribute to be appended at the end */
};


struct saved_tag {
  nsString name;
  nsString id;
};

//asumming max_length will be greater than in_length
static char *my_decode_html_escaping(char *string, int in_length, char *outstr, int max_length, VUser *vptr)
{
  NSDL1_API(vptr, NULL, "Method Called, string: '%s'", string?string:"NULL");

  int i;
  char *temp_ptr;
  char temp_buf[(in_length>max_length?in_length:max_length)+1];
  temp_buf[0] = '\0';


  struct html_encoding_character_map{
    char *html_encoded_string;
    char *character;
  };

  const struct html_encoding_character_map html_encoding_table[] = 
  { 
    {"&lt;", "<"}, 
    {"&gt;", ">"}, 
    {"&amp;", "&"},
    {"__LAST__", ""}
  };

  if(!string || string[0] == '\0'){
    NSDL4_API(vptr, NULL, "input string is null, returning from method");
    return NULL;
  }

  strncpy(outstr, string, in_length);
  outstr[in_length] = '\0';

  NSDL4_API(vptr, NULL, "Copied orig string to outstr='%s',", outstr);

  for(i=0;;i++)
  {
    NSDL4_API(vptr, NULL, "Inside 'for' loop, i=%d, html_encoding_table[i].html_encoded_string='%s'", 
                               i, html_encoding_table[i].html_encoded_string);
 
    if (!strcmp(html_encoding_table[i].html_encoded_string, "__LAST__")){
      NSDL4_API(vptr, NULL, "done searching for all the html encode patterns");
      break;
    }

    NSDL4_API(vptr, NULL, "searching for '%s' in the string '%s'", 
                              html_encoding_table[i].html_encoded_string, outstr);

    //while(temp_ptr = strstr(outstr, html_encoding_table[i].html_encoded_string))
    while(1)
    {
      temp_ptr = strstr(outstr, html_encoding_table[i].html_encoded_string);

      if(!temp_ptr)
        break;

      NSDL4_API(vptr, NULL, "found '%s' in the string '%s'", 
                                html_encoding_table[i].html_encoded_string, outstr);

      /* copy till before the encoded string */
      strncpy(temp_buf, outstr, temp_ptr - outstr);
      temp_buf[temp_ptr - outstr] = '\0';
      NSDL4_API(vptr, NULL, "making temp_buf ... temp_buf='%s'", temp_buf);

  
      /* append the decoded character */
      strcat(temp_buf, html_encoding_table[i].character);
      NSDL4_API(vptr, NULL, "making temp_buf ... temp_buf='%s'", temp_buf);
  
      /* append the remaining string */
      strcat(temp_buf, temp_ptr + strlen(html_encoding_table[i].html_encoded_string));
      NSDL4_API(vptr, NULL, "making temp_buf ... temp_buf='%s'", temp_buf);
  
      /* replace the outstr  with decoded character before looping */
      strncpy(outstr, temp_buf, max_length);
      outstr[max_length - 1] = '\0';

      NSDL4_API(vptr, NULL, "created temp_buf='%s', copied to outstr='%s'", temp_buf, outstr);

      temp_buf[0] = '\0';
      NSDL4_API(vptr, NULL, "Making temp_buf[0]=0, looping back");
    }
  }

  NSDL4_API(vptr, NULL, "returning outstr='%s'", outstr);
  return outstr;
}


static char replacement_chars[128];
inline void init_form_api_escape_replacement_char_array(void)
{
  memset(replacement_chars, 0, 128);

  replacement_chars['<'] = 1;
  replacement_chars['>'] = 1;
  replacement_chars['/'] = 1;
  replacement_chars[','] = 1;
  replacement_chars['+'] = 1;
  replacement_chars['~'] = 1;
  replacement_chars[' '] = 1;
  replacement_chars[':'] = 1;
  replacement_chars['?'] = 1;
  replacement_chars['#'] = 1;
  replacement_chars['%'] = 1;
  replacement_chars['='] = 1;
  replacement_chars['&'] = 1;
  replacement_chars['@'] = 1;
}

/* This method calls modified version of ns_escape() which is ns_escape_ex()
 * that does not do any malloc if the out_buffer is supplied.
 * Assuminng that the string length after encoding will not exceed MAX_BUFFER_LEN */

/*Change: Now out_buffer will be passed by caller function 
  assumtion: passsed buffer will be sufficient to be used */
static char *my_escape(char *string, char *out_buffer, VUser *vptr)
{
  NSDL1_API(vptr, NULL, "Method Called, string: '%s'", string?string:"NULL");

  char *encodedURL;
  //char replacement_chars[128];
  int out_len;

  if(!string || string[0] == '\0')
    return string;

#if 0
  memset(replacement_chars, 0, 128);

  replacement_chars['<'] = 1;
  replacement_chars['>'] = 1;
  replacement_chars['/'] = 1;
  replacement_chars[','] = 1;
  replacement_chars['+'] = 1;
  replacement_chars['~'] = 1;
  replacement_chars[' '] = 1;
  replacement_chars[':'] = 1;
  replacement_chars['?'] = 1;
  replacement_chars['#'] = 1;
  replacement_chars['%'] = 1;
  replacement_chars['='] = 1;
  replacement_chars['&'] = 1;
  replacement_chars['@'] = 1;
#endif 

  out_buffer[0] = '\0';
  encodedURL = ns_escape_ex(string, strlen(string), &out_len, replacement_chars, "+", out_buffer, 0);

  return encodedURL;
}

/* These are some comman routines for nsString */

#define INC_STR(str, amt) (str)->ptr += amt; (str)->len -= amt;
#define DEC_STR(str, amt) (str)->ptr -= amt; (str)->len += amt;

#define COPY_STR(in, out) (out)->ptr = (in)->ptr; (out)->len = (in)->len;

/* This will search for given character in string within it's length 
   if it is not found within it's length then it will return null nsString */
#define STRNCHR(in_str, pchar, out) {		\
  char *ptr = (in_str)->ptr;						\
  int len = (in_str)->len;							\
  (out)->ptr = NULL;										\
  (out)->len = 0;												\
  while(*ptr && len) {									\
    if(*ptr == pchar) {									\
      (out)->ptr = ptr;									\
      (out)->len = len;									\
      break;														\
    }																		\
    ptr++;															\
    len--;															\
  }																			\
}

/* This will search for given pattern in string within it's length,
   it pattern not matched within it's length then return null nsString  */

#define STRNSTR(in_str, pattern, out){									\
  char *tmp;																						\
  tmp = strcasestr((in_str)->ptr, pattern);									\
  if(tmp && (tmp < ((in_str)->ptr + (in_str)->len))){		\
    (out)->ptr = tmp;																		\
    (out)->len = ((in_str)->ptr+(in_str)->len - tmp);		\
  }																											\
  else	{																								\
    (out)->ptr = NULL;																	\
    (out)->len = 0;																			\
  }																											\
}

static inline void form_api_read_attribute(nsString *tmp_ptr, nsString *attribute, VUser *vptr)
{
  nsString loc_str, end_str1, end_str2;
  
  NSDL1_API(vptr, NULL, "Method Called, input string = %*.*s", tmp_ptr->len, tmp_ptr->len, tmp_ptr->ptr);

  //copy to loc_str
  COPY_STR(tmp_ptr, &loc_str);
  
  if(*(loc_str.ptr) == '"'|| *(loc_str.ptr) == '\'')
    INC_STR(&loc_str, 1);

  STRNCHR(&loc_str, '"', &end_str1);
  STRNCHR(&loc_str, '\'', &end_str2);
  //if(!(end_str1.ptr)) STRNCHR(&loc_str, ' ', &end_str1); 
  //if(!(end_str2.ptr)) STRNCHR(&loc_str, ' ', &end_str2);
  //Now check for nearest end point(end_str with large length)
  //loc_str.len -= ((end_str1.len > end_str2.len)?end_str1.len:end_str2.len;
  char *endptr = NULL, *ep1 = NULL, *ep2 = NULL;
  if(end_str1.ptr && end_str2.ptr) endptr = end_str1.ptr<end_str2.ptr?end_str1.ptr:end_str2.ptr;
  else if (end_str1.ptr) endptr = end_str1.ptr;
  else if (end_str2.ptr) endptr = end_str2.ptr;
  else {
    ep1 = strchr(loc_str.ptr, '"');;
    ep2 = strchr(loc_str.ptr, '\'');;
    if(ep1 && ep2) endptr = ep1<ep2?ep1:ep2;
    else if(ep1) endptr = ep2;
    else if(ep2) endptr = ep2;
  }
  
  if(endptr)
    loc_str.len = endptr - loc_str.ptr;

  COPY_STR(&loc_str, attribute);
  NSDL4_API(vptr, NULL, "attribute.len = %d", attribute->len);

  if(attribute->len)
    NSDL4_API(vptr, NULL, "returning, attribute = %*.*s", attribute->len, attribute->len, attribute->ptr);
  else
    NSDL4_API(vptr, NULL, "returning, attribute = NULL");
}


/*********************************************************
 * read_tag_attributes()
 * takes input_buf as input and extracts the name and value
 * ********************************************************/
static inline void read_tag_attributes(nsString *input_buf, nsString *name, nsString *value, nsString *type, nsString *id, VUser *vptr)
{
  nsString tmp_ptr;

  NSDL1_API(vptr, NULL, "Method Called, input_buf: '%*.*s'", input_buf->len, input_buf->len, input_buf->ptr);

  name->ptr = value->ptr = type->ptr = id->ptr = NULL;
  name->len = value->len = type->len = id->len = 0;

  STRNSTR(input_buf, "name=", &tmp_ptr);
  //tmp_ptr = strstr(input_buf, "name=");
  if (tmp_ptr.ptr){
     INC_STR(&tmp_ptr, 5);
     form_api_read_attribute(&tmp_ptr, name, vptr);
  }

  STRNSTR(input_buf, "value=", &tmp_ptr);
  if (tmp_ptr.ptr) {
     INC_STR(&tmp_ptr, 6);
     form_api_read_attribute(&tmp_ptr, value, vptr);
  }

  STRNSTR(input_buf, "type=", &tmp_ptr);
  if (tmp_ptr.ptr) {
     INC_STR(&tmp_ptr, 5);
     form_api_read_attribute(&tmp_ptr, type, vptr);
  }

  STRNSTR(input_buf, "id=", &tmp_ptr);
  if (tmp_ptr.ptr) {
     INC_STR(&tmp_ptr, 3); 
     form_api_read_attribute(&tmp_ptr, id, vptr);
  }
   

  //Currently null will come as empty string 
  NSDL2_API(vptr, NULL, "returning  name='%*.*s', value='%*.*s', type='%*.*s', id='%*.*s'",
                            name->len, name->len, name->ptr, value->len, value->len, value->ptr,
                            type->len, type->len, type->ptr, id->len, id->len, id->ptr); 
}

char *nearest_rb(char *p1, char *p2, char *p3, char *p4, char *p5, char *p6, char *p7, char *p8, char *p9)
{
  char *small = (p1)? p1: (p2)? p2: (p3)? p3: (p4)? p4: (p5)? p5: (p6)? p6: (p7) ? p7: p8; 

  if(small && p2 && p2 < small)
    small = p2;

  if(small && p3 && p3 < small)
    small = p3;

  if(small && p4 && p4 < small)
    small = p4;

  if(small && p5 && p5 < small)
    small = p5;

  if(small && p6 && p6 < small)
    small = p6;

  if(small && p7 && p7 < small)
    small = p7;

  if(small && p8 && p8 < small)
    small = p8;

  if(small && p9 && p9 < small)
    small = p9;

  if(!small)
    return NULL;

  return small;
}

char *smallest(char *p1, char *p2, char *p3)
{
  char *small = (p1)? p1: p2;

  if(small && p2 && p2 < small)
    small = p2;

  if(small && p3 && p3 < small)
    small = p3;

  if(!small)
    return NULL;


  if(small == p1)
    return (p1 + LEFT_SEARCH_STRING_1_LEN);
  else if(small == p2)
    return (p2 + LEFT_SEARCH_STRING_2_LEN);
  else if(small == p3)
    return (p3 + LEFT_SEARCH_STRING_3_LEN);
  else 
    return NULL;
}

/*  Narendra: In place of char buffer we will use nsString type variables(it will hold len and pointer of the value).
    and this will point to the user memory, so no need to take any buffer. */

static inline void process_attributes(char *form_buf, char *body_buf, 
                                      struct replace_attribute *attributes, 
                                      int num_args, int ordinal, VUser *vptr)
{
  char *form_ptr, *cur_ptr, *lb, *rb, *tmp_ptr1=NULL, *tmp_ptr2=NULL, *tmp_ptr3=NULL, *tmp_ptr4=NULL, *tmp_ptr5=NULL, *tmp_ptr6=NULL;
  char *tmp_ptr7=NULL, *tmp_ptr8=NULL, *tmp_ptr9=NULL;

  //replace buffer by nsString
  nsString att_name;
  nsString att_value;
  nsString att_type;
  nsString att_id;

  IW_UNUSED(char *encoded_string = 0);
  //This buffer will be used for html decoding of attribute value
  char *att_value_decode_buf = NULL; 
  int att_value_decode_buf_max_len = 0; 

  //This will be used for decoding attribute name(assumption that attribute name will not be longer than 1k) 
  char html_decoded_string[MAX_HTML_DECODED_STRLEN] = "\0";  

  //Change buf max length 
  nsString buf;
  int i, attr_length = 0;
  int num_radio_attributes_saved = 0;
  struct saved_tag radio_saved[MAX_NUM_RADIO_NAMES_SAVED];
  //This buffer will be used for handling image type tag
  char img_attr_name[MAX_ATTRIBUTE_LEN] = "";  
  char img_attr_value[MAX_ATTRIBUTE_LEN] = "";
  char  encode_value_flag; // Flag whether the att value has to be encoded or not. In case of image, imgname.x=5&imgname.y=5, this shd not be encoded.

 
  /* Init stuff */
  form_ptr = cur_ptr = lb = rb = NULL;
  memset(radio_saved, 0, MAX_NUM_RADIO_NAMES_SAVED * sizeof(struct saved_tag));

  /* Init stuff */
  body_buf[0] = 0;
  buf.ptr = NULL; 
  buf.len = 0;
  form_ptr = form_buf;

  while(1)
  {
    encode_value_flag = 1;
    buf.ptr = NULL;
    buf.len = 0;
    //Manish: we need to initialise all local declared array as we are using this in loop 
    att_name.ptr = att_value.ptr = att_type.ptr = att_id.ptr = NULL;
    att_name.len = att_value.len = att_type.len = att_id.len = 0;
    //att_name[0] = att_value[0] = att_type[0] = att_id[0] = html_decoded_string[0] = 0;

    /* Search the left search string in the input form buffer */
    tmp_ptr1 = strcasestr(form_ptr, LEFT_SEARCH_STRING_1);  
    tmp_ptr2 = strcasestr(form_ptr, LEFT_SEARCH_STRING_2);  
    tmp_ptr3 = strcasestr(form_ptr, LEFT_SEARCH_STRING_3);  

    if(tmp_ptr1 == NULL && tmp_ptr2 == NULL && tmp_ptr3 == NULL)
    {
      if(body_buf[0] == '\0')
      {
        NSDL1_API(vptr, NULL, "Error: Left search string [%s] or [%s] or [%s] not found", 
                                  LEFT_SEARCH_STRING_1, LEFT_SEARCH_STRING_2, LEFT_SEARCH_STRING_3);
        break;
      }
      else
      {
        /* Done writing the output body buffer */
        NSDL1_API(vptr, NULL, "Done writing the output body buffer");
        break;
      }
    }

    if(!tmp_ptr1 && !tmp_ptr3)
    {
      cur_ptr = tmp_ptr2 + LEFT_SEARCH_STRING_2_LEN;
      NSDL1_API(vptr, NULL, "Found left search string [%s]", LEFT_SEARCH_STRING_2);
    }
    else if(!tmp_ptr2 && !tmp_ptr3)
    { 
      cur_ptr = tmp_ptr1 + LEFT_SEARCH_STRING_1_LEN;
      NSDL1_API(vptr, NULL, "Found left search string [%s]", LEFT_SEARCH_STRING_1);
    }
    else if(!tmp_ptr1 && !tmp_ptr2)
    { 
      cur_ptr = tmp_ptr3 + LEFT_SEARCH_STRING_3_LEN;
      NSDL1_API(vptr, NULL, "Found left search string [%s]", LEFT_SEARCH_STRING_3);
    }
    else 
    {
//      cur_ptr = (tmp_ptr1 < tmp_ptr2)?(tmp_ptr1 + LEFT_SEARCH_STRING_1_LEN):(tmp_ptr2 + LEFT_SEARCH_STRING_2_LEN); 
      cur_ptr = smallest(tmp_ptr1, tmp_ptr2, tmp_ptr3); 
      //rb --> "> or "/>
      NSDL1_API(vptr, NULL, "Found more than one search strings [%s], [%s] and [%s].",
                                LEFT_SEARCH_STRING_1,LEFT_SEARCH_STRING_2,LEFT_SEARCH_STRING_3);
    }

    /*lb --> "hidden" name="bmForm" value="endeca_search">  */
    lb = cur_ptr; 

    /* Now search RB, can be either of the two "> or "/> */
    /*Example:
           1) <input type="hidden" name="iref1" value="SKUPG">
           2) <input type="hidden" name="trackingCategory" value="519956"/>
           3) <input type="hidden" name="from" value='/catalog/catalogSku.do?id=396241&pr=' /> 
           4) <input type="hidden" name="comparisonCount" value="0" id="comparisonCount" /> 
           5) <input type="text" name="username" value="cavisson" required /> */

    NSDL1_API(vptr, NULL, "Searching for right string [%s]", RIGHT_SEARCH_STRING_1);
    tmp_ptr1 = strstr(cur_ptr, RIGHT_SEARCH_STRING_1); 

    NSDL1_API(vptr, NULL, "Searching for right string [%s]", RIGHT_SEARCH_STRING_2);
    tmp_ptr2 = strstr(cur_ptr, RIGHT_SEARCH_STRING_2);

    NSDL1_API(vptr, NULL, "Searching for right string [%s]", RIGHT_SEARCH_STRING_3);
    tmp_ptr3 = strstr(cur_ptr, RIGHT_SEARCH_STRING_3);

    NSDL1_API(vptr, NULL, "Searching for right string [%s]", RIGHT_SEARCH_STRING_4);
    tmp_ptr4 = strstr(cur_ptr, RIGHT_SEARCH_STRING_4);

    NSDL1_API(vptr, NULL, "Searching for right string [%s]", RIGHT_SEARCH_STRING_5);
    tmp_ptr5 = strstr(cur_ptr, RIGHT_SEARCH_STRING_5);

    NSDL1_API(vptr, NULL, "Searching for right string [%s]", RIGHT_SEARCH_STRING_6);
    tmp_ptr6 = strstr(cur_ptr, RIGHT_SEARCH_STRING_6);

    NSDL1_API(vptr, NULL, "Searching for right string [%s]", RIGHT_SEARCH_STRING_7);
    tmp_ptr7 = strstr(cur_ptr, RIGHT_SEARCH_STRING_7);

    //Bug 13510 - ns_set_form_body API is not scanning tags if we use required attribute in html's input tag.
    NSDL1_API(vptr, NULL, "Searching for right string [%s]", RIGHT_SEARCH_STRING_8);
    tmp_ptr8 = strstr(cur_ptr, RIGHT_SEARCH_STRING_8);

    NSDL1_API(vptr, NULL, "Searching for right string [%s]", RIGHT_SEARCH_STRING_9);
    tmp_ptr9 = strstr(cur_ptr, RIGHT_SEARCH_STRING_9);

    if (!tmp_ptr1 && !tmp_ptr2 && !tmp_ptr3 && !tmp_ptr4 && !tmp_ptr5 && !tmp_ptr6 && !tmp_ptr7 && !tmp_ptr8 && !tmp_ptr9)
    {
      NSDL1_API(vptr, NULL, "Error: none of the following right search strings [%s], [%s], [%s], [%s], [%s], [%s], [%s], [%s] or [%s] found."
                                " Returning from method.",
                                RIGHT_SEARCH_STRING_1, RIGHT_SEARCH_STRING_2, RIGHT_SEARCH_STRING_3, RIGHT_SEARCH_STRING_4,
                                RIGHT_SEARCH_STRING_5, RIGHT_SEARCH_STRING_6, RIGHT_SEARCH_STRING_7, RIGHT_SEARCH_STRING_8,
                                RIGHT_SEARCH_STRING_9);
      break;
    }

    //if(!tmp_ptr1)
    if(!tmp_ptr1 && !tmp_ptr3 && !tmp_ptr4 && !tmp_ptr5 && !tmp_ptr6 && !tmp_ptr7 && !tmp_ptr8 && !tmp_ptr9)
    {
      rb = tmp_ptr2;
      NSDL1_API(vptr, NULL, "Found right search string [%s]", RIGHT_SEARCH_STRING_2);
    }
    //else if(!tmp_ptr2)
    else if(!tmp_ptr2 && !tmp_ptr3 && !tmp_ptr4 && !tmp_ptr5 && !tmp_ptr6 && !tmp_ptr7 && !tmp_ptr8 && !tmp_ptr9)
    { 
      rb = tmp_ptr1;
      NSDL1_API(vptr, NULL, "Found right search string [%s]", RIGHT_SEARCH_STRING_1);
    }
    //else if(!tmp_ptr3)
    else if(!tmp_ptr1 && !tmp_ptr2 && !tmp_ptr4 && !tmp_ptr5 && !tmp_ptr6 && !tmp_ptr7 && !tmp_ptr8 && !tmp_ptr9)
    { 
      rb = tmp_ptr3;
      NSDL1_API(vptr, NULL, "Found right search string [%s]", RIGHT_SEARCH_STRING_3);
    }
    else if(!tmp_ptr1 && !tmp_ptr2 && !tmp_ptr3 && !tmp_ptr5 && !tmp_ptr6 && !tmp_ptr7 && !tmp_ptr8 && !tmp_ptr9)
    { 
      rb = tmp_ptr4;
      NSDL1_API(vptr, NULL, "Found right search string [%s]", RIGHT_SEARCH_STRING_4);
    }
    else if(!tmp_ptr1 && !tmp_ptr2 && !tmp_ptr3 && !tmp_ptr4 && !tmp_ptr6 && !tmp_ptr7 && !tmp_ptr8 && !tmp_ptr9)
    { 
      rb = tmp_ptr5;
      NSDL1_API(vptr, NULL, "Found right search string [%s]", RIGHT_SEARCH_STRING_5);
    }
    else if(!tmp_ptr1 && !tmp_ptr2 && !tmp_ptr3 && !tmp_ptr4 && !tmp_ptr5 && !tmp_ptr7 && !tmp_ptr8 && !tmp_ptr9)
    { 
      rb = tmp_ptr6;
      NSDL1_API(vptr, NULL, "Found right search string [%s]", RIGHT_SEARCH_STRING_6);
    }
    else if(!tmp_ptr1 && !tmp_ptr2 && !tmp_ptr3 && !tmp_ptr4 && !tmp_ptr5 && !tmp_ptr6 && !tmp_ptr8 && !tmp_ptr9)
    { 
      rb = tmp_ptr7;
      NSDL1_API(vptr, NULL, "Found right search string [%s]", RIGHT_SEARCH_STRING_7);
    }
    else if(!tmp_ptr1 && !tmp_ptr2 && !tmp_ptr3 && !tmp_ptr4 && !tmp_ptr5 && !tmp_ptr6 && !tmp_ptr7 && !tmp_ptr9)
    { 
      rb = tmp_ptr8;
      NSDL1_API(vptr, NULL, "Found right search string [%s]", RIGHT_SEARCH_STRING_8);
    }
    else if(!tmp_ptr1 && !tmp_ptr2 && !tmp_ptr3 && !tmp_ptr4 && !tmp_ptr5 && !tmp_ptr6 && !tmp_ptr7 && !tmp_ptr8)
    { 
      rb = tmp_ptr9;
      NSDL1_API(vptr, NULL, "Found right search string [%s]", RIGHT_SEARCH_STRING_9);
    }
    else 
    {
      //rb = (tmp_ptr1 < tmp_ptr2)?tmp_ptr1:tmp_ptr2; //rb --> "> or "/>
      rb = nearest_rb(tmp_ptr1, tmp_ptr2, tmp_ptr3, tmp_ptr4, tmp_ptr5, tmp_ptr6, tmp_ptr7, tmp_ptr8, tmp_ptr9);
      NSDL1_API(vptr, NULL, "Found multiple right search strings. First occuring is considered.");
    }

    /* save rb in form_ptr */
    form_ptr = rb;

    buf.ptr = lb;
    buf.len = (rb - lb);
    //strncpy(buf, lb, input_block_len);
    //buf[input_block_len] = '\0';      //buf --> "hidden" name="bmForm" value="endeca_search 
    NSDL2_API(vptr, NULL, "Raw form buffer block containing name value pair [%*.*s]", buf.len, buf.len, buf.ptr);

    read_tag_attributes(&buf, &att_name, &att_value, &att_type, &att_id, vptr);
    /* Manish: Thu Jun 13 20:33:52 IST 2013
     * In Kohls we are finding some blocks in which name attribute is not present generally it not happend 
     * Eg:-- 
     * 1)  <input type="hidden" class="childProductImage" value="http://media.kohls.com.edgesuite.net/is/image/kohls/190878_Raisin?wid=73&amp;hei=73&amp;op_sharpen=1"/>
     * 2)  <input class="preSelectedskuId" type="hidden" value="T190878_"/>
     * 3)  <input type="hidden" id="variantsCount" value="2"/>
     * Due to this body is made wrongly to avoid this we continue from here and in output body consider only that block which contain name
     */
    if(att_name.len == 0)
      continue;
/*
    if (!strcmp(att_name, "bmImage") && !strcmp(att_value, "add"))
    {
      NSDL2_API(vptr, NULL, "found bmImage=add in the form, att_name='%s', att_value='%s'", 
                                att_name, att_value);

      strcat(att_value, "&add.x=48&add.y=17");
      NSDL2_API(vptr, NULL, "updated att_value='%s'", att_value);

    }
*/
    if (((att_type.len == 5 /*strlen("image")*/) && !strncmp(att_type.ptr, "image", att_type.len)) && att_name.len != '\0' && att_value.len == '\0')
    {
      NSDL2_API(vptr, NULL, "found image in the form, att_name='%*.*s', "
                                "att_value='%*.*s', appending %*.*s.x=47&%*.*s.y=18",
                                att_name.len, att_name.len, att_name.ptr, att_value.len, att_value.len, att_value.ptr,
                                att_name.len, att_name.len, att_name.ptr, att_name.len, att_name.len, att_name.ptr); 

      //save changed attribute name and value in temporary buffer.
      img_attr_name[0]  = 0;
      img_attr_value[0] = 0;
      snprintf(img_attr_value, MAX_ATTRIBUTE_LEN, "5&%*.*s.y=5", att_name.len, att_name.len, att_name.ptr);
      snprintf(img_attr_name, MAX_ATTRIBUTE_LEN, "%*.*s.x", att_name.len, att_name.len, att_name.ptr);
      att_name.ptr = img_attr_name;
      att_name.len = strlen(img_attr_name);
      att_value.ptr = img_attr_value;
      att_value.len = strlen(img_attr_value);       
      encode_value_flag = 0;
    }

    /* Special Case for Kohls */
    if (((att_type.len == 6/*strlen(submit)*/) && !strncmp(att_type.ptr, "submit", att_type.len))&& 
        ((att_name.len == 16/*strlen("ship_select_addr")*/) && !strncmp(att_name.ptr, "ship_select_addr", att_name.len)) && 
        ((att_value.len == 16/*strlen("ship_select_addr")*/) && !strncmp(att_value.ptr, "ship_select_addr", att_value.len)))
    {
      NSDL2_API(vptr, NULL, "found \"submit\" type input tag with name and value as \"ship_select_addr\", "
                                "skipping and continuing ..."); 

      continue;
    }
   
    NSDL2_API(vptr, NULL, "num_radio_attributes_saved = %d", num_radio_attributes_saved);

    if(att_type.len >= 5 && !strncasecmp(att_type.ptr, "radio", 5))
    {
      for (i = 0; i<num_radio_attributes_saved; i++)
      {
        if((att_name.len == radio_saved[i].name.len) && !(strncmp(att_name.ptr, radio_saved[i].name.ptr, att_name.len))) 
        {
          NSDL2_API(vptr, NULL, "found same name radio type input in the form, att_name='%*.*s', "
                                    "att_value='%*.*s', att_id='%*.*s', skipping and continuing ...",
                                    att_name.len, att_name.len, att_name.ptr, att_value.len, att_value.len,
                                    att_value.ptr, att_id.len, att_id.len, att_id.ptr);
  
          break;
  
        }
      }

      if(i<num_radio_attributes_saved && (att_name.len == radio_saved[i].name.len && !strncmp(att_name.ptr, radio_saved[i].name.ptr, att_name.len))) 
      {
        NSDL2_API(vptr, NULL, "found same name radio type input in the form, att_name='%*.*s', "
                                  "att_value='%*.*s', att_id='%*.*s', skipping and continuing ...", 
                                  att_name.len, att_name.len, att_name.ptr, att_value.len, att_value.len, att_value.ptr,
                                  att_id.len, att_id.len, att_id.ptr);
        continue;
      }
 
      if(num_radio_attributes_saved < MAX_NUM_RADIO_NAMES_SAVED)
      {
        //save current attribute to radio_saved
        COPY_STR(&att_name, &(radio_saved[num_radio_attributes_saved].name));
        COPY_STR(&att_id, &(radio_saved[num_radio_attributes_saved].id));
 
        num_radio_attributes_saved++;
 
        if(num_radio_attributes_saved >= MAX_NUM_RADIO_NAMES_SAVED){
            NSDL1_API(vptr, NULL, "num_radio_attributes_saved (%d) reached its limit %d", 
                                      num_radio_attributes_saved, 
                                      MAX_NUM_RADIO_NAMES_SAVED);
        }
      }
    }
    /* Before writing the att_name, first decode any html encodings such as &lt; */
    //currently we are using the different buffer but we can use same buffer for this purpose
    my_decode_html_escaping(att_name.ptr, att_name.len, html_decoded_string, MAX_HTML_DECODED_STRLEN, vptr);  
    NSDL1_API(vptr, NULL, "Html decoded string: '%s'", html_decoded_string);
   
    /* Now url encode special characters */
    //we are directly copying to body_buf
    char *buf_start = body_buf + strlen(body_buf);
    IW_NDEBUG_UNUSED(encoded_string, my_escape(html_decoded_string, buf_start, vptr));
    NSDL2_API(vptr, NULL, "After encoding, encoded_string='%s'", buf_start);


    strcat(body_buf, "=");
    
    /* Save the length of the attribute name and '=' character, so that if the attribute is not to be
     * included in the body buf, we should be able to remove the name from body buf 
     * The way to exclude the attribute is to use att_name=__EXCLUDE__ in the api 
     */
     //attr_length = strlen(encoded_string) + 1; /* 1 added for '=' */ 
     attr_length = strlen(buf_start);  

    /* If the att_name retrieved from the form buffer matches with any 
     * name in the attributes array poulated from input arguments,
     * increment the current occurance in and if ordinality matches 
     * then replace the value with the value passed to this API's arguments
     */
    for (i=0; i<num_args; i++)
    {
      if(ordinal == 0 && attributes[i].cur_occurance > 0) {
        NSDL2_API(vptr, NULL, "attribute #%d, ordinal = 0 and cur_occurence is greater than 0 so continuing", i);
        continue;
      }
      
      if(attributes[i].addattr_flag) {
        NSDL4_API(vptr, NULL, "attribute #%d, __ADDATTR__ found, so skipping", i);
        continue;
      }
      

      NSDL2_API(vptr, NULL, "att_name.len = %d, attributes[%d].name.len = %d, att_name.ptr = %*.*s, attributes[%d].name.ptr = %*.*s",
          att_name.len, i, attributes[i].name.len, att_name.len, att_name.len, att_name.ptr,i, 
          attributes[i].name.len, attributes[i].name.len, attributes[i].name.ptr);

      if((attributes[i].name.len == att_name.len) && !strncmp(attributes[i].name.ptr, att_name.ptr, att_name.len))
      {
        attributes[i].cur_occurance++;
        NSDL2_API(vptr, NULL, "Checking attr name '%*.*s' in the form "
                                  "with attr name in the argument '%*.*s' "
                                  "with ord=%d", 
                                  att_name.len, att_name.len, att_name.ptr, 
                                  attributes[i].name.len, attributes[i].name.len, attributes[i].name.ptr, 
                                  attributes[i].cur_occurance);

        if(ordinal == 0 || attributes[i].cur_occurance == ordinal)
        {
          //copy to att_value
          COPY_STR(&(attributes[i].value), &att_value);
          NSDL2_API(vptr, NULL, "Matched attr name '%*.*s' "
                                    "in the form with attr name in the argument '%*.*s' "
                                    "with ord=%d. Using argument value '%*.*s'", 
                                    att_name.len, att_name.len, att_name.ptr, 
                                    attributes[i].name.len, attributes[i].name.len, attributes[i].name.ptr, 
                                    attributes[i].cur_occurance, 
                                    att_value.len, att_value.len, att_value.ptr);
          break;
        }
      }
      else
      {
        NSDL2_API(vptr, NULL, "Value of attr name '%*.*s' "
                                  "in the form is not to be replaced. "
                                  "Value from form is '%*.*s'", 
                                  att_name.len, att_name.len, att_name.ptr, att_value.len, att_value.len, att_value.ptr);
      }
    }

    if(att_value.len != 0)
    {
      if((att_value.len != 19/* strlen("%7F%7FEXCLUDE%7F%7F") */ || strncmp(att_value.ptr, "%7F%7FEXCLUDE%7F%7F", 19)) &&
         ((att_value.len != 11) || strncmp(att_value.ptr, "__EXCLUDE__", 11)))
      {
				/* Before writing the att_value, first decode any html encodings such as &lt; */
        //check if att_value_decode_buf is sufficient for html decoding 
        if(att_value.len+1 > att_value_decode_buf_max_len) {
          att_value_decode_buf = realloc(att_value_decode_buf, att_value.len+1);
          if(!att_value_decode_buf) {
            NSDL1_API(vptr, NULL, "Failed to realloc att_value_decode_buf");
            return;
          }
          att_value_decode_buf_max_len = att_value.len+1; 
        }
        //we are passing max length equal to att_value's length because after html decoding resulted buffer 
        // can never be of more size than att_value's length.
        my_decode_html_escaping(att_value.ptr, att_value.len, att_value_decode_buf, att_value_decode_buf_max_len,vptr);  
        NSDL1_API(vptr, NULL, "Html decoded string: '%s'", att_value_decode_buf);
		
        /* Now url encode special characters */
	if(encode_value_flag)
	{
          IW_NDEBUG_UNUSED(encoded_string, my_escape(att_value_decode_buf, body_buf+strlen(body_buf), vptr));
          //passing body_buf because my_escape will directly write in this buffer.
          NSDL2_API(vptr, NULL, "After encoding, att_value='%s'", encoded_string);
	}
        else
          strcat(body_buf, att_value_decode_buf);
          

      }
      else
      {
        /* This is the case where this attribute is to be excluded from the body */
        int tmplen = strlen(body_buf) - attr_length;
        body_buf[tmplen] = '\0';
        if (body_buf[tmplen - 1] == '&') 
          body_buf[tmplen - 1] = '\0'; /* Otherwise there will be two '&' characters */
        attr_length = 0;
      }
    }
 
    strcat(body_buf, "&");

    NSDL4_API(vptr, NULL, "body_buf='%s'", body_buf);
  }
  //printf("\n\n=============> sleeping for 1 minute\n");
  //usleep(60000000);  
  /* Append the attributes with __ADDATTR__ string in the name */
  for (i=0; i<num_args; i++)
  {
    if(!attributes[i].addattr_flag) {
      NSDL4_API(vptr, NULL, "attribute #%d, __ADDATTR__ not found, so skipping", i);
      continue;
    }
    /* Append */
    NSDL4_API(vptr, NULL, "Going to append added attribute '%*.*s=%s'", attributes[i].name.len, attributes[i].name.len, attributes[i].name.ptr, attributes[i].value.ptr);

    my_decode_html_escaping(attributes[i].name.ptr, attributes[i].name.len, html_decoded_string, MAX_HTML_DECODED_STRLEN, vptr);  
    NSDL4_API(vptr, NULL, "Html decoded Name string: '%s'", html_decoded_string);
   
    /* Now url encode special characters */
    IW_NDEBUG_UNUSED(encoded_string, my_escape(html_decoded_string, body_buf + strlen(body_buf), vptr));
    NSDL4_API(vptr, NULL, "After encoding name, encoded_string='%s'", encoded_string);

    strcat(body_buf, "=");

    if(attributes[i].value.len && attributes[i].value.ptr && attributes[i].value.ptr[0]){
      my_decode_html_escaping(attributes[i].value.ptr, attributes[i].value.len, html_decoded_string, MAX_HTML_DECODED_STRLEN, vptr);  
      NSDL4_API(vptr, NULL, "Html decoded Value string: '%s'", html_decoded_string);
     
      /* Now url encode special characters */
      IW_NDEBUG_UNUSED(encoded_string, my_escape(html_decoded_string, body_buf + strlen(body_buf), vptr));
      NSDL4_API(vptr, NULL, "After encoding value, encoded_string='%s'", encoded_string);
    }

    strcat(body_buf, "&");
  }

  int buf_len =  strlen(body_buf) - 1; /* 1 less for truncating the last & */
  body_buf[buf_len] = '\0';
//  strcat(body_buf, "\r\n");

  NSDL4_API(vptr, NULL, "Freeing the radio_saved[] array, "
                            "num_radio_attributes_saved=%d", 
                            num_radio_attributes_saved);

  NSDL2_API(vptr, NULL, "body_buf='%s'", body_buf);

  if(att_value_decode_buf)
    free(att_value_decode_buf);
}


static inline void __ns_set_form_body(char *form_buf, char *body_buf, int ordinal, int num_args, va_list ap, VUser *vptr)
{
  int i;

  char *temp_attr_str = 0;
  char *ptr_separator = NULL;

  //struct replace_attribute attributes[MAX_NUM_ATTRIBUTES];
  struct replace_attribute attributes[num_args];

  /* Zero the array */
  memset(attributes, 0, sizeof(attributes));
 
  /* read all the variable arguments in the attributes array
   * These will be replaced in the output body buffer 
   */
  for (i=0; i<num_args; i++)
  {
    temp_attr_str = va_arg(ap, char *);
    if(!temp_attr_str){
      fprintf(stderr, "ERROR: ns_set_form_body(), Failed to get Argument #%d value\n", i);
      return;
    }
    NSDL3_API(vptr, NULL, "Input variable Argument #%d: \"%s\"", i, temp_attr_str);
    
    ptr_separator = strchr(temp_attr_str, '=');

    if(ptr_separator)
    {
      if(!strncmp(temp_attr_str, "__ADDATTR__", 11))
      {
        attributes[i].addattr_flag = 1;
        temp_attr_str += 11; /* strlen("__ADDATTR__") */
      }
      attributes[i].name.ptr = temp_attr_str;
      attributes[i].name.len = (ptr_separator - temp_attr_str); 
    
      attributes[i].value.ptr = ptr_separator + 1;
      attributes[i].value.len = strlen(ptr_separator + 1);  
    }
    else
    {
      NSDL3_API(vptr, NULL, "Warning: replacement attribute not in format \"name=value\"; char "
                                "'=' is missing, considering whole string as name and blank value");
      if(!strncmp(temp_attr_str, "__ADDATTR__", 11))
      {
        attributes[i].addattr_flag = 1;
        temp_attr_str += 11; /* strlen("__ADDATTR__") */
      }

      attributes[i].name.ptr = temp_attr_str;
      attributes[i].name.len = strlen(temp_attr_str);
      
      attributes[i].value.ptr = NULL;
      attributes[i].value.len = 0;
    }

    NSDL3_API(vptr, NULL, "replacement attribute[%d].name='%*.*s'", i, attributes[i].name.len, attributes[i].name.len, attributes[i].name.ptr);
    NSDL3_API(vptr, NULL, "replacement attribute[%d].value='%*.*s'", i, attributes[i].value.len, attributes[i].value.len, attributes[i].value.ptr);
    
  }

  process_attributes(form_buf, body_buf, attributes, num_args, ordinal, vptr);
}

int ns_set_form_body_internal(VUser *vptr, char *form_buf_param_name, char *form_body_param_name, int ordinal, int num_args, va_list ap);

/********************************************************************************** 
 * API name: ns_set_form_body()
 * 
 * Synopsis:
 *   This API takes
 *
 * Input arguments
 *   -> form_buf_param_name     - (char *) name of the NS variable  where 'form raw buffer' is saved
 *
 *   -> form_body_param_name    - (char *) name of the NS variable where the API fills the body buffer
 *   -> ordinal - 1 based index - determines the sequence number of the set of form_variable values
 *                                to be replaced in form post body.
 *   -> num_args -                (int) Number of variable arguments following this argument. This contains
 *                                the number of html form variables whose values have to be replaced
 *   -> variable number of        (char *) type arguments in the format 
 *
 *                                "form_variable_name=form_variable_replacement_value". 
 *                                 --------------------------------------------------
 *                                From the raw form buffer, the api searches for the form_variable_name 
 *                                which is provided in the API, and replaces its original value in the 
 *                                form with the value supplied in this API.
 *      
 *
 *
 * Output argumnents
 *   -> body_buf - HTTP POST BODY
 *
 * returns (int) 
 *   ->  0       - for success
 *      -1       - for failure
 *
 * NOTES: html encodings (&lt; &gt; &amp;) in the raw form buffer are first decoded and then 
 *        searched and replaced and then url encoded.
 *
 * EXAMPLE FLOW FILE:
 * =================
 *flow()
 *{
 * char form_buf[] = "<form name=\"endeca_search\" method=\"post\" action=\"http://www.kohls.com/upgrade/webstore/home.jspjsessionid=YqxnTslCLpYfh4f2nhSRLdzppFwSghT42ZJMl54zsJ0B62xgR12J!-1746483480!896610992\"><input type=\"hidden\" name=\"bm&lt;Form\" value=\"endeca_search\"><input type=\"hidden\" name=\"bmFormID\" value=\"1324115245947\" method=\"post\" action=\"http://www.kohls.com/upgrade/webstore/home.jspjsessionid=YqxnTslCLpYfh4f2nhSRLdzppFwSghT42ZJMl54zsJ0B62xgR12J!-1746483480!896610992\"><input type=\"hidden\" name=\"bm&lt;Form\" value=\"endeca_search\"><input type=\"hidden\" name=\"bmFormID\" value=\"1324115245947\"/></form>";
 *
 *  ns_save_string(form_buf, "form_buf_parameter");
 *  printf("\n\nform buf saved:\n---------------\n%s\n\n", ns_eval_string("{form_buf_parameter}"));
 *
 *  ns_set_form_body("form_buf_parameter",
 *                   "form_body_parameter",
 *                   2,//ordinal
 *                     2,//num_args
 *                    "bm<Form=Manmeet_Value1",
 *                    "bmFormID=Manmeet_Value2");
 *
 *   printf("\n\nform body populated:\n-------------------\n%s\n\n", ns_eval_string("{form_body_parameter}"));
 *
 *   ns_web_url ("form_post",
 *       "URL=http://192.168.1.35:8000/netapplication/clickScriptResult.html",
 *       "METHOD=POST",
 *       "HEADER=Accept-Language: en-US",
 *       "HEADER=Content-Type: application/x-www-form-urlencoded",
 *       "HEADER=Content-Length: 61",
 *       "HEADER=Cache-Control: no-cache",
 *       "BODY={form_body_parameter}");
 *}
 *
 **************************************************************************************************/ 

int ns_set_form_body(char *form_buf_param_name, char *form_body_param_name, int ordinal, int num_args, ...)
{
  va_list ap;
  VUser *vptr = TLS_GET_VPTR();
  int status = 0;
  
    NSDL2_API(vptr, NULL, "Method Called");
  va_start (ap, num_args);
  status = ns_set_form_body_internal(vptr, form_buf_param_name, form_body_param_name,ordinal,num_args, ap);
  va_end(ap);
  return status;
}

int ns_set_form_body_internal(VUser *vptr, char *form_buf_param_name, char *form_body_param_name, int ordinal, int num_args, va_list ap)
{

  char temp_name_buf[1024] = "\0";
  char *form_buf = NULL;
  char *body_buf = NULL;

  NSDL2_API(vptr, NULL, "Method Called; form_buf_param_name = %s, "
                            "form_body_param_name = %s, "
                            "num_args=%d", 
                            form_buf_param_name, 
                            form_body_param_name,
                            num_args);

/*
  if (num_args > MAX_NUM_ATTRIBUTES)
  {
    printf("ERROR: ns_set_form_body():number of arguments can not be greater than %d", MAX_NUM_ATTRIBUTES);
    return -1;
  }
*/
  form_buf = malloc(MAX_FORM_BUF_SIZE);
  if(!form_buf) {
    NSDL1_API(vptr, NULL, "Failed to malloc form_buf");
    return -1;
  }

  body_buf = malloc(MAX_BODY_BUF_SIZE);
  if(!body_buf) {
    NSDL1_API(vptr, NULL, "Failed to malloc form_buf");
    return -1;
  }

  form_buf[0] = '\0';
  body_buf[0] = '\0';

  /* Sandwich the parameter */
  snprintf(temp_name_buf, 1024, "{%s}", form_buf_param_name);
  long len;
  strncpy(form_buf, ns_eval_string_flag_internal(temp_name_buf, 0 , &len, vptr), MAX_FORM_BUF_SIZE);
  form_buf[MAX_FORM_BUF_SIZE - 1] = '\0';

  NSDL2_API(vptr, NULL, "form_buf: '%s'", form_buf);

  __ns_set_form_body(form_buf, body_buf, ordinal, num_args, ap, vptr);

  ns_save_string_flag_internal(body_buf, -1, form_body_param_name, 0, vptr, 1);
  NSDL2_API(vptr, NULL, "Returning final body buffer: '%s'", body_buf);
  free(form_buf);
  free(body_buf);
  return 0;
}

char *get_next_attr_str(char *ptr, int *fwd)
{
  char *p_amp; /* Pointer to separator ampersand */
  if(fwd) 
    *fwd = 0;

  if(!ptr || !(*ptr))
    return NULL;

  p_amp = strchr(ptr, '&');
  if (!p_amp)
  {
    if(fwd)
      *fwd = strlen(ptr);
    return ptr;
  }

  *p_amp = '\0';

  if(fwd) 
    *fwd = p_amp - ptr + 1;

  return (ptr);
}

static inline void __ns_set_form_body_ex(char *form_buf, char *body_buf, char *in_str, VUser *vptr)
{
  if(!in_str || !(*in_str))
    return;

  int num_args = 0;
  int cur_allocated_num_attributes = 0;

  char *temp_attr_str = 0;
  char *ptr_separator = NULL;
 
  //struct replace_attribute *attributes[MAX_NUM_ATTRIBUTES];
  struct replace_attribute *attributes = NULL;

  attributes = malloc(sizeof(struct replace_attribute) * INIT_NUM_ALLOCATED_ATTRIBUTES);

  if(!attributes)
  {
    NSDL2_API(vptr, NULL, "ERROR: ns_form_body_ex(): "
                              "Error in allocating memory for attributes array.!");
    return;
  }

  cur_allocated_num_attributes = INIT_NUM_ALLOCATED_ATTRIBUTES;

  /* Zero the array */
  memset(attributes, 0, sizeof(struct replace_attribute) * INIT_NUM_ALLOCATED_ATTRIBUTES);
 
  /* read all the variable arguments in the attributes array
   * These will be replaced in the output body buffer 
   */
  char *l_buffer = NULL;

  l_buffer = malloc(strlen(in_str) + 1);
  strcpy(l_buffer, in_str);

  char *ptr = l_buffer;

  int fwd = 0;
  while (1)
  {
    temp_attr_str = get_next_attr_str(ptr, &fwd);
    if(!temp_attr_str)
      break;

    num_args++;

    if(num_args > cur_allocated_num_attributes)
    {
      attributes = realloc(attributes, sizeof(struct replace_attribute) * 
                                         (cur_allocated_num_attributes + 
                                           DELTA_NUM_ALLOCATED_ATTRIBUTES));

      if(!attributes)
      {
        NSDL2_API(vptr, NULL, "ERROR: ns_form_body_ex(): "
                                  "Error in allocating memory for attributes array.!");
        return;
      }

      /* Zero the array */
      memset(attributes + cur_allocated_num_attributes, 0, 
             sizeof(struct replace_attribute) * DELTA_NUM_ALLOCATED_ATTRIBUTES);

      cur_allocated_num_attributes += DELTA_NUM_ALLOCATED_ATTRIBUTES;
    }

    if(fwd)
      ptr += fwd;

    else
      break;

    NSDL3_API(vptr, NULL, "Input variable Argument #%d: \"%s\"", num_args - 1, temp_attr_str);
    
    ptr_separator = strchr(temp_attr_str, '=');

    if(ptr_separator)
    {
      if(!strcmp(temp_attr_str, "__ADDATTR__"))
      {
        attributes[num_args - 1].addattr_flag = 1;
        temp_attr_str += 11; /* strlen("__ADDATTR__") */
      }

      attributes[num_args - 1].name.ptr = temp_attr_str;
      attributes[num_args - 1].name.len = (ptr_separator - temp_attr_str);
    
      attributes[num_args - 1].value.ptr = ptr_separator+1;
      attributes[num_args - 1].value.len = strlen(ptr_separator+1); 
    }
    else
    {
      NSDL3_API(vptr, NULL, "Warning: replacement attribute not in format \"name=value\"; char "
                                "'=' is missing, considering whole string as name and blank value");
      if(!strcmp(temp_attr_str, "__ADDATTR__"))
      {
        attributes[num_args - 1].addattr_flag = 1;
        temp_attr_str += 11; /* strlen("__ADDATTR__") */
      }

      attributes[num_args - 1].name.ptr = temp_attr_str;
      attributes[num_args - 1].name.len = strlen(temp_attr_str);

      attributes[num_args - 1].value.ptr = NULL;
      attributes[num_args - 1].value.len = 0;
    }

    NSDL3_API(vptr, NULL, "replacement attribute[%d].name='%*.*s'", num_args - 1, attributes[num_args - 1].name.len, 
                attributes[num_args - 1].name.len, attributes[num_args - 1].name.ptr);
    NSDL3_API(vptr, NULL, "replacement attribute[%d].value='%*.*s'", num_args - 1, attributes[num_args - 1].value.len,
                attributes[num_args - 1].value.len, attributes[num_args - 1].value.ptr);
    
  }

  process_attributes(form_buf, body_buf, attributes, num_args, 0/*ordinal*/, vptr);

  free(l_buffer);
  l_buffer = NULL;

  free(attributes);
  attributes = NULL;
}

/********************************************************************************** 
 * API name: ns_set_form_body_ex()
 * 
 * Synopsis:
 *   This API takes
 *
 * Input arguments
 *   -> form_buf_param_name     - (char *) name of the NS variable  where 'form raw buffer' is saved
 *
 *   -> form_body_param_name    - (char *) name of the NS variable where the API fills the body buffer
 *                                the number of html form variables whose values have to be replaced
 *   -> string containing variable names to be replaces in the form
 *      body along with the values in following format:
 *      "Quantity=1&Color=Blue&Size=12&Quantity=1&Size=4xx&Quantity=1&Color=Black"
 *
 * Output argumnents
 *   -> body_buf - HTTP POST BODY
 *
 * returns (int) 
 *   ->  0       - for success
 *      -1       - for failure
 *
 **************************************************************************************************/ 

int ns_set_form_body_ex_internal(char *form_buf_param_name, char *form_body_param_name, char *in_str, VUser *vptr)
{

  if(!in_str || *in_str == '\0')
  {
    fprintf(stderr, "ERROR: ns_set_form_body_ex(), Input string is empty");
    return -1;
  }

  char temp_name_buf[1024] = "\0";
  char *form_buf = NULL;
  char *body_buf = NULL;

  form_buf = malloc(MAX_FORM_BUF_SIZE);
  if(!form_buf) {
    NSDL1_API(vptr, NULL, "Failed to malloc form_buf");
    return -1;
  }

  body_buf = malloc(MAX_BODY_BUF_SIZE);
  if(!body_buf) {
    NSDL1_API(vptr, NULL, "Failed to malloc form_buf");
    return -1;
  }

  form_buf[0] = 0;
  body_buf[0] = 0;

  NSDL2_API(vptr, NULL, "Method Called; form_buf_param_name = %s, "
                            "form_body_param_name = %s, "
                            "in_str=%s", 
                            form_buf_param_name, 
                            form_body_param_name,
                            in_str);
  long len;
  /* Sandwich the parameter */
  snprintf(temp_name_buf, 1024, "{%s}", form_buf_param_name);
  strncpy(form_buf, ns_eval_string_flag_internal(temp_name_buf, 0 , &len, vptr), MAX_FORM_BUF_SIZE);
  form_buf[MAX_FORM_BUF_SIZE - 1] = '\0';

  NSDL2_API(vptr, NULL, "form_buf: '%s'", form_buf);

  __ns_set_form_body_ex(form_buf, body_buf, in_str, vptr);

  ns_save_string_flag_internal(body_buf, -1, form_body_param_name, 0, vptr, 1);
  NSDL2_API(vptr, NULL, "Returning final body buffer: '%s'", body_buf);
  free(form_buf);
  free(body_buf);
  return 0;
}


int ns_set_form_body_ex(char *form_buf_param_name, char *form_body_param_name, char *in_str)
{
  VUser *vptr = TLS_GET_VPTR();
  

  return ns_set_form_body_ex_internal(form_buf_param_name, form_body_param_name, in_str, vptr);
}

#ifdef TEST
int main(int argc, char **argv)
{
  int num_bytes_read=0;

  char form_buf[MAX_BUFFER_LEN];
  char *body_buf = NULL;

  strcpy(form_buf,  "<form name=\"endeca_search\" method=\"post\" action=\"http://www.kohls.com/upgrade/webstore/home.jspjsessionid=YqxnTslCLpYfh4f2nhSRLdzppFwSghT42ZJMl54zsJ0B62xgR12J!-1746483480!896610992\"><input type=\"hidden\" name=\"bmForm\" value=\"endeca_search\"><input type=\"hidden\" name=\"bmFormID\" value=\"1324115245947\"/></form>");

  if(argc > 1)
  {
    int fd = open(argv[1], O_RDONLY|O_CLOEXEC, 0644);
 
    if (fd < 0) {
        perror("open");
        NS_EXIT(1, "error in opening file");
    }
    num_bytes_read = read(fd, form_buf, MAX_BUFFER_LEN);  
  }

  printf("num_bytes_read = %d", num_bytes_read);

  nsl_decl_var(form_buf_param);
  nsl_decl_var(form_body_param);

  ns_save_string(form_buf, "form_buf_param");
  printf("Input raw buffer containing html form='%s'\n", form_buf);

  init_form_api_escape_replacement_char_array();
  ns_set_form_body("form_buf_param", "form_body_param", 
          1,/*ordinal*/
          3,/*num_args*/
          "bmFormID=Manmeet_Value",
          "ADD_CART_ITEM<>prm_id=Manmeet_Value2",
          "__ADDATTR__NewAttr=NewValue");

  printf("After calling the api, body='%s'\n", body_buf);
  return 0;
}
#endif

