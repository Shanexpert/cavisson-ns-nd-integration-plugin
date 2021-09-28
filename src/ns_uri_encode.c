#include <stdio.h>
#include <assert.h>
#include "url.h"
#include "util.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_parse_scen_conf.h"
#include "ns_runtime_changes.h"
#include "ns_common.h"
#include "nslb_util.h"
#include "ns_exit.h"
#include "ns_uri_encode.h"
#include "ns_global_settings.h"
#include "nslb_encode.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"


/*******************************************************************************************
  Purpose: This function encode the special characters present in url if these characters 
           are mentioned in URI_ENCODING keyword.
  Input:- num_entries:- It specify the number of segments present in url.
          url:- Url which need to be encoded
          new_len:- Length after encoding.
  Return Value:- It returns encoded url.
********************************************************************************************/
int ns_encode_char_in_url(char *url, int len, char *output, int first_segment)
{
  static int input_type;
  int i;
  int k = 0;
  URIEncoding *encode_ptr; 

  NSDL1_HTTP(NULL, NULL, "Method Called url = %s", url);

  // Check for first STR segment. If it is First STR segment then set input type to HTTP_URI
  if(first_segment == 1)
  {
    NSDL1_HTTP(NULL, NULL, "This is first STR segment");
    input_type = HTTP_URI;
  }

  if(input_type == HTTP_URI)
  {
    encode_ptr = global_settings->encode_uri;
  }
  else
  {
    encode_ptr = global_settings->encode_query;
  }

  /* Check each character of URL if that character is need to be encoded in url then copy the encoded value to the
     output buffer */
  for(i = 0; i < len; i++)
  {
    NSDL1_HTTP(NULL, NULL, " input_type = %d url[i] = %c global_settings->encode_uri[124]", input_type, url[i], 
               global_settings->encode_uri[124].encode);

    if(input_type == HTTP_URI && url[i] == '?')
    {
      output[k++] = url[i];
      encode_ptr = global_settings->encode_query;
      input_type = HTTP_QUERY;
      continue;
    }
    NSDL1_HTTP(NULL, NULL, "encode_ptr[url[i]].encode = %s", encode_ptr[(int)url[i]].encode);
    if(!encode_ptr[(int)url[i]].encode[0])
    {
      NSDL1_HTTP(NULL, NULL, "copying data to output");
      output[k++] = url[i];
    }
    else
    {
      /*In case if 'space' is encoded by '+' then only one character(+) is copied to the output buffer */
      if(encode_ptr[(int)url[i]].encode[0] != '+') {
        output[k++] = encode_ptr[(int)url[i]].encode[0];
        output[k++] = encode_ptr[(int)url[i]].encode[1];
        output[k++] = encode_ptr[(int)url[i]].encode[2];
      }
      else
        output[k++] = encode_ptr[(int)url[i]].encode[0];
    }
  }
  output[k] = '\0';
  NSDL1_HTTP(NULL, NULL, "output = %s k = %d", output, k);
  return k;
}

/****************************************************************************************
  Purpose:- This function is used for parsing of keyword URI_ENCODING
  Input:- It takes input a buffer which contains the keyword and its argument.
  Return Value:- It fills the value in uri_encoding structure. 
*****************************************************************************************/
int kw_set_uri_encoding(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[1024] = "\0";
  char encode_space_by[4] = "\0";
  char encode_char_in_uri[256] = "\0";
  char encode_char_in_query[256] = "\0";
  char encode_pipe[10] = "\0";
  int num = 0;
  int pipe = 0;
  int i;
  char temp[256] = "";

  NSDL3_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s %s %s", keyword, encode_space_by, encode_pipe, encode_char_in_uri, encode_char_in_query, temp);

  if (num < 3 || num > 5) {
    NSDL3_PARSING(NULL, NULL, "num = %d", num);
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, URI_ENCODING_USAGE, CAV_ERR_1011115, CAV_ERR_MSG_1);
  }
 
  if (num == 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, URI_ENCODING_USAGE, CAV_ERR_1011115, CAV_ERR_MSG_1);
  }

  if(encode_space_by[0] != '+') {
    strcpy(global_settings->encode_uri[32].encode, encode_space_by);
    strcpy(global_settings->encode_query[32].encode, encode_space_by);
  }
  else {
    global_settings->encode_uri[32].encode[0] = '+';
    global_settings->encode_query[32].encode[0] = '+';
  }

  if(!strcasecmp(encode_char_in_uri, "SAME"))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, URI_ENCODING_USAGE, CAV_ERR_1011116, "");
  }

  if(strcasecmp(encode_char_in_uri, "NONE")) {
    for(i = 0; encode_char_in_uri[i] != '\0'; i++) {
      if(isalnum(encode_char_in_uri[i]))
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, URI_ENCODING_USAGE, CAV_ERR_1011222, "");

      sprintf(global_settings->encode_uri[(int)encode_char_in_uri[i]].encode, "%%%02x", encode_char_in_uri[i]);
      NSDL3_PARSING(NULL, NULL, "encoded value = %s", global_settings->encode_uri[(int)encode_char_in_uri[i]].encode);
    }
  }

  if((strcasecmp(encode_char_in_query, "NONE") && strcasecmp(encode_char_in_query, "SAME"))){
    for(i = 0; encode_char_in_query[i] != '\0'; i++) {
      if(isalnum(encode_char_in_query[i]))
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, URI_ENCODING_USAGE, CAV_ERR_1011222, "");

      sprintf(global_settings->encode_query[(int)encode_char_in_query[i]].encode, "%%%02x", encode_char_in_query[i]);
      NSDL3_PARSING(NULL, NULL, "char = %c encoded value is = %s", encode_char_in_query[i], global_settings->encode_query[(int)encode_char_in_query[i]].encode);

      
    }
  }
  else if(!strcasecmp(encode_char_in_query, "SAME")) {
    if(!strcasecmp(encode_char_in_uri, "NONE")) {
      NSDL3_PARSING(NULL, NULL, "encode_uri is none hence");
      memset(encode_char_in_uri, 0, 256);
    }

    for(i = 0; encode_char_in_uri[i] != '\0'; i++) {
      sprintf(global_settings->encode_query[(int)encode_char_in_uri[i]].encode, "%%%02x", encode_char_in_uri[i]);
      NSDL3_PARSING(NULL, NULL, "encode_char in query= %s", global_settings->encode_query[(int)encode_char_in_uri[i]].encode);
    }
  }

  pipe = atoi(encode_pipe);

  if(pipe < 0 || pipe > 3)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, URI_ENCODING_USAGE, CAV_ERR_1011115, CAV_ERR_MSG_3);
  
  if(pipe == 1 || pipe == 3) {
    strncpy(global_settings->encode_uri[124].encode, "%7c", 3);
  }
    
  if(pipe == 2 || pipe == 3) {
    strncpy(global_settings->encode_query[124].encode, "%7c", 3);
  }
 
  return 0;
}
