
#define _GNU_SOURCE

#include <string.h>
#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_log.h"
#include "ns_global_settings.h"
#include "amf.h"

#define MAX_XML_LINE_LEN 8192 * 5  //XML Line len should be more than the string len supported

/* Todo

1. How to handle NAN in double

*/

//Copies num bytes from 'copy_from' buffer to 'out' buffer
#define COPY_BYTES(num)  if (*len < num) {\
               AMFDL1("Not enough space *len = %d, num =%d", *len, num); \
	    		    AMFEL("Not enough space in output data buffer to write message\n");\
	    		    AMFDL1("Not enough space in output data buffer to write message len=%d, num=%d\n", *len, num);\
	    		    return NULL;\
			   }\
                           bcopy (copy_from, out, num);\
			   out += num;\
                           AMFDL1("Before copy *len = %d", *len); \
			   *len -= num;\
                           AMFDL1("After copy *len = %d", *len);

#define COPY_SHORT(data)  if ((out = copy_short ( data, out, len)) == NULL) {\
				AMFEL("ERROR: failed to write short. still left %d bytes\n", *len);\
				return NULL;\
			   }

#define COPY_STRING(data)  if ((out = copy_string ( data, out, len)) == NULL) {\
				AMFEL("ERROR:copy_string() failed to write string (%s). still left %d bytes\n",data, *len);\
				return NULL;\
			   }

#define COPY_STRING_TYPE(data, copy_marker)  if ((out = copy_string_type ( data, out, len, copy_marker)) == NULL) {\
				AMFEL("ERROR: copy_string_type() failed to write string (%s). still left %d bytes\n",data, *len);\
				return NULL;\
			   }

#define COPY_OCTET_TYPE(data)  if ((out = copy_octet_type ( data, out, len)) == NULL) {\
				AMFEL("ERROR: copy_octet_type() failed to write string (%s). still left %d bytes\n",data, *len);\
				return NULL;\
			   }
#define COPY_NULL_TYPE  if ((out = copy_null_type ( out, len)) == NULL) {\
				AMFEL("ERROR: copy_null_type() failed to null type. still left %d bytes \n", *len);\
				return NULL;\
			   }

#define COPY_UNSUPPORTED_TYPE  if ((out = copy_unsupported_type ( out, len)) == NULL) {\
				AMFEL("ERROR: copy_unsupporte_type() failed. still left %d bytes \n", *len);\
				return NULL;\
			   }

#define COPY_UNDEFINED_TYPE  if ((out = copy_undefined_type ( out, len)) == NULL) {\
				AMFEL("ERROR: copy_undefined_type() failed. still left %d bytes \n", *len);\
				return NULL;\
			   }

#define COPY_TRUE_TYPE_AMF3  if ((out = copy_true_type_amf3 ( out, len)) == NULL) {\
				AMFEL("ERROR: copy_undefined_type() failed. still left %d bytes \n", *len);\
				return NULL;\
			   }

#define COPY_FALSE_TYPE_AMF3  if ((out = copy_false_type_amf3 ( out, len)) == NULL) {\
				AMFEL("ERROR: copy_undefined_type() failed. still left %d bytes \n", *len);\
				return NULL;\
			   }

#define COPY_NUMBER_TYPE(data, copy_marker)  if ((out = copy_number_type ( data, out, len, copy_marker)) == NULL) {\
				AMFEL("ERROR: copy_number_type() failed to write number (%s). still left %d bytes\n",data, *len);\
				return NULL;\
			   }

#define COPY_INTEGER_TYPE_AMF3(data)  if ((out = amf3_copy_integer_type ( data, out, len)) == NULL) {\
				AMFEL("ERROR: amf3_copy_integer_type() failed to write number (%s). still left %d bytes\n",data, *len);\
				return NULL;\
			   }

#define COPY_SHORT_TYPE(data)  if ((out = copy_short_type ( data, out, len)) == NULL) {\
				AMFEL("ERROR: copy_short_type() failed to write number (%s). still left %d bytes\n",data, *len);\
				return NULL;\
			   }

#define COPY_BOOL_TYPE(data)  if ((out = copy_bool_type ( data, out, len)) == NULL) {\
				AMFEL("ERROR: copy_bool_type() failed to write bool (%s). still left %d bytes\n",data, *len);\
				return NULL;\
			   }

#define COPY_DATE_TYPE(data, copy_marker)  if ((out = copy_date_type ( data, out, len, copy_marker)) == NULL) {\
				AMFEL("ERROR: copy_date_type() failed to write date (%s). still left %d bytes\n",data, *len);\
				return NULL;\
			   }

#define GET_LINE if (get_line(AMF_TRIM_STRING) == NULL) return out;

//placeholder for seg_length
#define START_SEGMENT	COPY_SHORT (0x0000);\
	    		last_seg_start = out;

//copy including NULL char but without {}
#define MARK_SEGMENT(data_code, var_name) copy_from[0] = data_code;\
    			COPY_BYTES(1);\
			END_SEGMENT();\
			START_SEGMENT;\
			bcopy(var_name, copy_from, strlen(var_name)+1);\
			COPY_BYTES (strlen(var_name)+1);\
			END_SEGMENT();\
			START_SEGMENT;

#define END_SEGMENT() seg_len = out - last_seg_start;\
		bcopy ((char *)&seg_len, last_seg_start-2, 2);\
		amf_seg_count++;

//TODO: check return values everywhere
static char *amf3_set_reference(char *out, int *len, int ref_index);

static char *amf3_set_U29S_value(char *out, int *len, int string_size);

static char *
copy_short (unsigned short num, char *out, int *len)
{
  short s;
  s = htons (num);
  bcopy ((char *) &s, copy_from, 2);
  COPY_BYTES (2);
  return out;
}

//#Shilpa 7Mar11 - For AMF3
/*
 1. 8-bits    0      -  256
 2. 15-bits   256    -  16383
 3. 22-bits   16384  -  4194303
 4. 29-bits   4194304 - 536870911
*/
/*
[shilpa@netocean3 src]$ ./test_int
With htonl():
Enter number: 16909060
Intger = 67305985 (0x4030201)
byte 0, 1 = 0x1
byte 1, 2 = 0x2
byte 2, 3 = 0x3
byte 3, 4 = 0x4

Without htonl():
Enter number: 16909060
Intger = 16909060 (0x1020304)
byte 0, 4 = 0x4
byte 1, 3 = 0x3
byte 2, 2 = 0x2
byte 3, 1 = 0x1
*/

static char *
amf3_copy_integer_type (char *buf, char *out, int *len)
{
  unsigned int integer;
  unsigned char *ptr;
  unsigned int byte;
  //unsigned int max_int_val;

  integer = atoi (buf);
  AMFDL1("Method called. Encoded value = %d (0x%X)", integer, integer);

/*
  //Since negetive values are accepted till 1 more than positive values
  if(integer < 0)
    max_int_val++;
*/

  if((abs(integer) > MAX_INTEGER_VALUE)) {
    AMFEL("ERROR: Out of Range Integer Received %d Acceptable Range(%d, %d)", integer,
                         -MAX_INTEGER_VALUE , MAX_INTEGER_VALUE);
    return NULL;
  }

  
  if(integer < 0 || integer >= 0x200000)
  {
     byte = (((integer >> 22) & 0x7F) | 0x80);
     ptr = (unsigned char *) &byte;
     copy_from[0] = ptr[0];
     AMFDL1("integer byte[3]=[%02X]\n", byte);
     COPY_BYTES(1);
     
     byte = (((integer >> 15) & 0x7F) | 0x80);
     ptr = (unsigned char *) &byte;
     copy_from[0] = ptr[0];
     AMFDL1("integer byte[2]=[%02X]\n", byte);
     COPY_BYTES(1);
 
     byte = (((integer >> 8) & 0x7F) | 0x80);
     ptr = (unsigned char *) &byte;
     copy_from[0] = ptr[0];
     AMFDL1("integer byte[1]=[%02X]\n", byte);
     COPY_BYTES(1);
     
     byte = (integer & 0xFF);
     ptr = (unsigned char *) &byte;
     copy_from[0] = ptr[0];
     AMFDL1("integer byte[0]=[%02X]\n", byte);
     COPY_BYTES(1);
  }
  else
  {
     if(integer >= 0x4000)
     {
       byte = (((integer >> 14) & 0x7F) | 0x80);
       ptr = (unsigned char *) &byte;
       copy_from[0] = ptr[0];
       AMFDL1("integer byte[2]=[%02X]\n", byte);
       COPY_BYTES(1);
     }
     if(integer >= 0x80)
     {
       byte = (((integer >> 7) & 0x7F) | 0x80);
       ptr = (unsigned char *) &byte;
       copy_from[0] = ptr[0];
       AMFDL1("integer byte[1]=[%02X]\n", byte);
       COPY_BYTES(1);
     }
     {
       byte = (integer  & 0x7F);
       ptr = (unsigned char *) &byte;
       copy_from[0] = ptr[0];
       AMFDL1("integer byte[0]=[%02X]\n", byte);
       COPY_BYTES(1);
     }
  }
  return out;
}

static char *
copy_string (char *buf, char *out, int *len)
{
  short slen = strlen (buf);

  AMFDL1("Method called. str_len = %hd, str_val = %s", slen, buf);

  //out = copy_short (slen, out, len);
  COPY_SHORT (slen);
  if (slen >= MAX_XML_LINE_LEN) {
    AMFDL1("string size >MAX_XML_LINE_LEN (%s)\n", buf);
    return NULL;
  }
  bcopy (buf, copy_from, slen);
  COPY_BYTES (slen);
  return out;
}


static char *
copy_string_amf3 (char *buf, char *out, int *len)
{
  int slen = strlen (buf);

  AMFDL1("Method called. str_len = %d, str_val = %s, len = %d", slen, buf, *len);

  if((out = amf3_set_U29S_value(out, len, slen)) == NULL)
     return NULL;
 
  if (slen >= MAX_SIZE_28_BITS) {
    AMFDL1("string length %d > (%s)\n", slen, buf);
    return NULL;
  }
  bcopy (buf, copy_from, slen);
  COPY_BYTES (slen);
  return out;
}

static char *
copy_string_type (char *buf, char *out, int *len, int copy_marker)
{
  AMFDL1("amf_data_version=%d copy_marker=[%d]", amf_data_version, copy_marker);
  if (amf_data_version == AMF_VERSION3)
  {
      if(copy_marker)
      {
         copy_from[0] = AMF3_STRING;
         COPY_BYTES (1);
      }
      return (copy_string_amf3 (buf, out, len));
  }
  else
  {
    if(copy_marker)
    {
      copy_from[0] = AMF0_STRING;
      COPY_BYTES (1);
    }
    return (copy_string (buf, out, len));
  }
}

static char *
copy_octet_type (char *buf, char *out, int *len)
{
  short slen = 0;
  int buf_len = strlen (buf);
  //unsigned char buf1[MAX_XML_LINE_LEN];
  unsigned char byte_buf[3];;
  int i;

  //copy type byte
  copy_from[0] = 0x02;
  COPY_BYTES (1);

  byte_buf[2] = '\0';
  for (i = 0; i < buf_len; slen++) {
    bcopy (buf + i, byte_buf, 2);
    i += 2;
    //buf1[slen] = (unsigned char) strtol ((const char *) byte_buf, NULL, 16);
  }
  //buf1[slen] = '\0';
  //out = copy_short (slen, out, len);
  COPY_SHORT (slen);

  if (slen >= MAX_XML_LINE_LEN) {
    AMFDL1("string size >MAX_XML_LINE_LEN (%s)\n", buf);
    return NULL;
  }
  bcopy (buf, copy_from, slen);
  COPY_BYTES (slen);
  return out;
}


static char *
copy_null_type (char *out, int *len)
{
  //#Shilpa 7Mar11 - For AMF3
  if (amf_data_version == AMF_VERSION3)
    copy_from[0] = AMF3_NULL;
  else
    copy_from[0] = AMF0_NULL;

  COPY_BYTES (1);
  return out;
}

	//amfin_lineno and amf_infp must be initialized before using read_amf and skip amfr+debued_line


static char *
copy_undefined_type (char *out, int *len)
{
  //#Shilpa 7Mar11 - For AMF3
  if (amf_data_version == AMF_VERSION3)
    copy_from[0] = AMF3_UNDEFINED;
  else
    copy_from[0] = AMF0_UNDEFINED;

  COPY_BYTES (1);
  return out;
}

static char *
copy_unsupported_type (char *out, int *len)
{
  copy_from[0] = AMF0_UNSUPPORTED;
  COPY_BYTES (1);
  return out;
}

static char *
copy_true_type_amf3 (char *out, int *len)
{
  copy_from[0] = AMF3_TRUE;
  COPY_BYTES (1);
  return out;
}

static char *
copy_false_type_amf3 (char *out, int *len)
{
  copy_from[0] = AMF3_FALSE;
  COPY_BYTES (1);
  return out;
}
static char *
copy_number_type (char *buf, char *out, int *len, int copy_marker)
{
  double num;
  int i;
  char *ptr;

  AMFDL1("amf_data_version=%d copy_marker=[%d]", amf_data_version, copy_marker);
  //#Shilpa 7Mar11 - For AMF3
  if(copy_marker)
  {
    if (amf_data_version == AMF_VERSION3)
      copy_from[0] = AMF3_DOUBLE;
    else
      copy_from[0] = AMF0_NUMBER;

    COPY_BYTES (1);
  }

  num = atof (buf);
  ptr = (char *) &num;

  for (i = 0; i < 8; i++) {
    //For our machine host byte order is little endian
    copy_from[i] = ptr[7 - i];
  }

  COPY_BYTES (8);
  return out;
}

static char *
copy_short_type (char *buf, char *out, int *len)
{
  short num;
  char *ptr;

  copy_from[0] = AMF0_REFERENCE;
  COPY_BYTES (1);

  num = (short) atoi (buf);
  ptr = (char *) &num;

  //For our machine host byte order is little endian
  copy_from[0] = ptr[1];
  copy_from[1] = ptr[0];

  COPY_BYTES (2);
  return out;
}

static char *
copy_bool_type (char *buf, char *out, int *len)
{
  AMFDL1("Method called. value = %s", buf);

  copy_from[0] = AMF0_BOOLEAN;
  if (atoi (buf) == 0)
    copy_from[1] = 0x00;
  else
    copy_from[1] = 0x01;
  COPY_BYTES (2);

  return out;
}

static char *
copy_date_type (char *buf, char *out, int *len, int copy_marker)
{
  struct tm tm;
  double dt;
  int i;
  char *ptr, *ms_ptr, *tz_ptr, *pt, date_value[255];
  short tz_value = 0, nw_tzvalue = 0;
  int ms_value = 0;

  if(copy_marker) {
    if (amf_data_version == AMF_VERSION3) {
      copy_from[0] = AMF3_DATE;
      copy_from[1] = 1; // Set 1 to indicate that date is a value (Bit 0 is 1)
      COPY_BYTES (2);
    } else {
      copy_from[0] = AMF0_DATE;
      COPY_BYTES (1);
    }
  }

  //Date format fixed for now yyyy-mm-dd-HH:MM:SS.ms+TZ
  ms_ptr = index (buf, '.');
  AMFDL4("ms_ptr=%s", ms_ptr);
  if (ms_ptr) {
    *ms_ptr = '\0';
    ms_ptr++;
    tz_ptr = index (ms_ptr, '+');
    AMFDL4("tz_ptr=%s", tz_ptr);
    if (tz_ptr) {
      *tz_ptr = '\0';
      tz_ptr++;
      tz_value = (short) atoi (tz_ptr);
      nw_tzvalue = htons (tz_value);
    }
    ms_value = atoi (ms_ptr);
  }

  ptr = strptime (buf, "%F-%T", &tm); 

/*
  //For checking invalid Date value
  if(ptr == NULL)
  {
      AMFEL("Unexpected format/Invalid Date received. Date received=[%s]",buf);
      AMFDL1("Unexpected format/Invalid Date received. Date received=[%s]",buf);
      
  }
  if(ptr[0] != '\0')
  {
      AMFEL("Unexpected format/Invalid Date received. Date received=[%s]",buf);
      AMFDL1("Unexpected format/Invalid Date received. Date received=[%s]",buf);
  }
*/
  AMFDL4("tm_sec=%d , tm_min=%d, tm_hour=%d ,  tm_mon=%d , tm_year=%d , tm_mday=%d", 
                    tm.tm_sec, tm.tm_min, tm.tm_hour, tm.tm_mon, tm.tm_year, tm.tm_mday);

  strftime(date_value, sizeof(date_value), "%s", &tm);
  dt = strtoll(date_value, &pt, 10);
  dt = dt * 1000;                //make ms since 1/1/1970
  dt += ms_value;

  AMFDL3("Date Value = %lf", dt);
  ptr = (char *) &dt;

  for (i = 0; i < 8; i++) {
    //For our machine host byte order is little endian
    copy_from[i] = ptr[7 - i];
  }

  // Amf3 does not have time zone
  if (amf_data_version == AMF_VERSION3) {
    COPY_BYTES (8);
  } else {
    bcopy ((char *) &nw_tzvalue, &copy_from[9], 2);
    COPY_BYTES (10); // 2 extra bytes for timezone
  }

  return out;
}


static char *
get_line (short trim_line)
{
  char *ptr;
  int len, i;

get_next_line:
  if (amf_infp) {
    if (fgets (line, MAX_XML_LINE_LEN, amf_infp) == NULL) {
      AMFEL("Unexpected end of file while reading AMF at line %d\n",
	      amfin_lineno);
      return NULL;
    }
  } else {
    //Read a line frm input buffer
    if (amfin_left <= 0) {
      AMFEL("Unexpected end of buffer while reading AMF at line %d\n",
	      amfin_lineno);
      return NULL;
    }
    for (i = 0; i < (MAX_XML_LINE_LEN - 1) && amfin_left > 0; i++) {
      line[i] = *amfin_ptr;
      amfin_ptr++;
      amfin_left--;
      if (line[i] == '\n')
	break;
    }
    line[i] = '\0';
  }

  amfin_lineno++;

  // For reading string, we cannot trim as string can be multi line
  if(trim_line)
  {
    //Remove trailing new line character
    len = strlen (line);
    if (line[len - 1] == '\n')
      line[len - 1] = '\0';

    //Remove trailing blanks
    while (isspace (line[len])) {
      line[len] = '\0';
      len--;
    }

    //Remove leading blanks
    ptr = line;
    while (isspace (*ptr)) {
      ptr++;
    }

   //strcpy (line, ptr); //causing buffer overlapping, instead use bcopy or memmove
    memmove(line, ptr, strlen(ptr) + 1); // one extra lenght for \0 byte

    // Skip commented and empty lines
     if((strncmp(line, "//", 2) == 0) || (strcmp(line, "") == 0))
      goto get_next_line;
  }

  AMFDL1("Line: %s\n", line);

  return line;

}

static int get_attribute_value(char *input_buffer, char *attribute_name,
                               char *attribute_value) {

  attribute_value[0] = '\0';
  char *ptr = NULL;
  char *first_quote_idx = NULL;
  //char *last_quote_idx   = NULL;
  char *value_start_idx = NULL;
  char *value_end_idx = NULL;

  if(input_buffer != NULL) {
    ptr = strstr(input_buffer, attribute_name);

    if(ptr != NULL)  {
      ptr += strlen(attribute_name);

      if(ptr) {
         first_quote_idx = index(ptr, '"');
         if(first_quote_idx) {
            value_start_idx = first_quote_idx + 1;
            if(value_start_idx) {
              value_end_idx = index(value_start_idx, '"');
            }
         }
      }
    }

    if(value_start_idx && value_end_idx) {
      int len = value_end_idx - value_start_idx;
      if(len > 0) {
        strncpy(attribute_value, value_start_idx, len);
        attribute_value[len] = '\0';
        if(strcmp(attribute_value, "NA") == 0) attribute_value[0] = '\0';
      }
      return AMF3_SUCCESS;
    }
  }
  return AMF3_ERROR;
}

//Used to extract and encode 
static char* amf3_set_datatype_ref_names(char *input_buffer, char *out, int *len, char *member_ref,
                          char *member_name, int *member_ref_val, char *member_name_val)
{
  char reference_string[1024];
  *member_ref_val = -1;
  AMFDL1("Method Called input_buffer=[%s], member_ref_val=[%d], member_name_val=[%s]",     \
                   input_buffer, *member_ref_val, member_name_val);

    //Extracting member's reference id
    if(get_attribute_value(input_buffer, member_ref, reference_string) == AMF3_SUCCESS) 
    {
       AMFDL1("reference_string=[%s]", reference_string);
       if(reference_string[0] != '\0')
          *member_ref_val = atoi(reference_string);
    }

    //Extracting member's name/key name
    get_attribute_value(input_buffer, member_name, member_name_val);
    AMFDL1("Extracting =[%s]",member_name_val);

    //Encoding member's reference id
    if(*member_ref_val != -1)
    {
       AMFDL1("Encoding %s reference value=[%d]",member_name, member_ref_val);
       if((out = amf3_set_reference(out, len, *member_ref_val)) == NULL)
         return NULL;
    }
    //Encoding member's name/key name
    else if(member_name_val[0] != '\0')
    {
       AMFDL1("Encoding %s name=[%s]",member_name, member_name_val);
       COPY_STRING_TYPE(member_name_val, AMF3_DO_NOT_COPY_MARKER);
    }
    //Neither reference id nor name is found
    else
    {
       AMFEL("Error: Dynamic member name is empty\n");
       return NULL;
    }

    return out;
}

#if 0
static char* amf3_set_dyn_member(char *input_buffer, char *out, int *len,
                                int *dyn_member_ref, char *dyn_member_name, int is_dyn_member) {

  char reference_string[1024];
  *dyn_member_ref = -1;
  AMFDL1("Method Called input_buffer=[%s], dyn_member_ref=[%d], dyn_member_name=[%s], is_dyn_membe=[%d]",
                               input_buffer, *dyn_member_ref, dyn_member_name, is_dyn_member);

  if(is_dyn_member)
  {
    if(get_attribute_value(input_buffer, "dyn_member_ref", reference_string) == AMF3_SUCCESS) {
       AMFDL1("reference_string=[%s]",reference_string);
       if(reference_string[0] != '\0')
          *dyn_member_ref = atoi(reference_string);
    }

    get_attribute_value(input_buffer, "dyn_member", dyn_member_name);
    AMFDL1("dyn_member_name=[%s]",dyn_member_name);

    if(*dyn_member_ref != -1) 
    {
       AMFDL1("setting dynamic reference=[%d]",dyn_member_ref);
       if((out = amf3_set_reference(out, len, *dyn_member_ref)) == NULL)
         return NULL;
    } 
    else if(dyn_member_name[0] != '\0') 
    {
       AMFDL1("setting dynamic member name=[%s]",dyn_member_name);
       COPY_STRING_TYPE(dyn_member_name, AMF3_DO_NOT_COPY_MARKER);
    } 
    else 
    {
       AMFDL1("Error: Dynamic member name is empty\n");
       return NULL;
    }
  }
  return out;
}

static char* amf3_set_key(char *input_buffer, char *out, int *len,
                         int *key_ref, char *key, short is_array_key) {

  char reference_string[1024];
  *key_ref = -1;

  AMFDL1("Method Called input_buffer=[%s], key_ref=[%d], key=[%s], is_array_key=[%d]",
                               input_buffer, *key_ref, key, is_array_key);

    if(get_attribute_value(input_buffer, "key_ref", reference_string) == 0) {
       if(reference_string[0] != '\0')
           *key_ref = atoi(reference_string);
    }
    get_attribute_value(input_buffer, "key", key);

    if(*key_ref != -1) {
      if((out = amf3_set_reference(out, len, *key_ref)) == NULL)
        return NULL;
    } else if(key != '\0') {
        COPY_STRING_TYPE(key, AMF3_DO_NOT_COPY_MARKER)
    } else {
      AMFDL1("Error: Key is empty\n");
      return NULL;
    }
  return out;
}
#endif

static void set_array_attr_default_value(amf3_array *attr) {

  attr->size                = 0;
  attr->arr_ref             = -1;
  attr->dyn_member_ref      = -1;
  attr->key_ref             = -1;
  attr->dyn_member_name[0]  = '\0';
  attr->key[0]              = '\0';
}

// <object classname="flex.messaging.messages.RemotingMessage" sealed_members="9" dynamic="true" externalizable="false">
//
static char *amf3_set_array_attr(char *out, int *len, amf3_array *attr, short is_dyn_member, short is_array_key, short is_externalized)
{

  char value[1024];

  set_array_attr_default_value(attr);

  if(!get_attribute_value(line, "size", value)) {
    attr->size= atoi(value);
  }

  if(!get_attribute_value(line, "array_ref", value)) {
    attr->arr_ref = atoi(value);
  }


  if(is_dyn_member)
  {
    if((out = amf3_set_datatype_ref_names(line, out, len, "dyn_member_ref", "dyn_member", 
                                  &attr->dyn_member_ref, attr->dyn_member_name )) == NULL)
     {
        AMFEL("ERROR: Reading Dynamic Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }

  if(is_array_key)
  {
    if((out = amf3_set_datatype_ref_names(line, out, len, "key_ref", "key", &attr->key_ref,
                   attr->key)) == NULL)
     {
        AMFEL("ERROR: Reading Array Key/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
/*
  if(is_externalized)
  {
     if((out = amf3_set_datatype_ref_names(line, out, len, "ext_member_ref", "ext_member",
                      &attr->ext_member_ref, attr->ext_member_name)) == NULL)
     {
        AMFDL1("ERROR: Reading Externalized Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
*/

 return out;
}
static void set_obj_attr_default_value(amf3_object *attr) {

  attr->externalizable      = 0;   //SS:18/4/13 Initializing externalizable, was giving error in obj_ref
  attr->dynamic             = 0;
  attr->object_ref          = -1;
  attr->traits_ref          = -1;
  attr->classname_ref       = -1;
  attr->sealed_members      = 0;
  attr->dyn_member_ref      = -1;
  attr->key_ref             = -1;
  attr->dyn_member_name[0]  = '\0';
  attr->classname[0]        = '\0';
  attr->key[0]              = '\0';
}

// <object classname="flex.messaging.messages.RemotingMessage" sealed_members="9" dynamic="true" externalizable="false">
//
static char *amf3_set_object_attr(char *out, int *len, amf3_object *attr, short is_dyn_member, short is_array_key, short is_externalized )
{

  char value[1024];

  AMFDL1("Method Called is_dyn_member=[%d], is_array_key=[%d]",is_dyn_member, is_array_key);
  set_obj_attr_default_value(attr);

  if(!get_attribute_value(line, "externalizable", value)) {
    AMFDL1("Got Extrnalizable=[%s]", value);
    if(strcmp(value, "true") == 0 ) {
      attr->externalizable = 1;
    } else {
      attr->externalizable = 0;
    }
  }

  if(!get_attribute_value(line, "dynamic", value)) {
    AMFDL1("Got Dynamic=[%s]", value);
    if(strcmp(value, "true") == 0 ) {
      attr->dynamic = 1;
    } else {
      attr->dynamic = 0;
    }
  }

  if(!get_attribute_value(line, "object_ref", value)) {
    AMFDL1("Got Object Reference=[%s]", value);
    attr->object_ref = atoi(value);
  }

  if(!get_attribute_value(line, "traits_ref", value)) {
    AMFDL1("Got Traits Reference=[%s]", value);
    attr->traits_ref = atoi(value);
  }

  if(!get_attribute_value(line, "classname_ref", value)) {
    AMFDL1("Got Classname Reference=[%s]", value);
    attr->classname_ref = atoi(value);
  }

  if(!get_attribute_value(line, "classname", value)) {
    AMFDL1("Got Classname=[%s]", value);
    strcpy(attr->classname, value);
  }

  if(!get_attribute_value(line, "sealed_members", value)) {
    AMFDL1("Got Sealed Members=[%s]", value);
    attr->sealed_members = atoi(value);
  }

  if(is_dyn_member)
  {
    if((out = amf3_set_datatype_ref_names(line, out, len, "dyn_member_ref", "dyn_member",
                                  &attr->dyn_member_ref, attr->dyn_member_name )) == NULL)
     {
        AMFEL("ERROR: Reading Dynamic Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }

  if(is_array_key)
  {
    if((out = amf3_set_datatype_ref_names(line, out, len, "key_ref", "key", &attr->key_ref,
                   attr->key)) == NULL)
     {
        AMFEL("ERROR: Reading Array Key/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
/*
  if(is_externalized)
  {
     if((out = amf3_set_datatype_ref_names(line, out, len, "ext_member_ref", "ext_member",
                      &attr->ext_member_ref, attr->ext_member_name)) == NULL)
     {
        AMFDL1("ERROR: Reading Externalized Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
*/

return out;
}

// return 0 on success
static char* amf3_extract_byte_array(char *out, int *len,
                                     char *input_buffer, char *type_str,
                                     amf3_byte_array *ptr,
                                     short is_dyn_member, short is_array_key, short is_externalized, int amf3_type) {

  char value[2048];
  char *closing_tag_ptr = NULL;
  char closing_tag[512] ;
  value[0] = '\0';
  char *tmp, *byte_ptr;
  char *attributes_ptr;
  char *value_ptr;
  int  i_decimal_val;
  char c_ascii_val, byte[3];
  byte[2] = '\0';

  AMFDL1("Method Called, input_buffer = [%s]"
                     "is_dyn_member = %d, is_array_key = %d, is_externalied ",
                      input_buffer, is_dyn_member, is_array_key, is_externalized);

  if(input_buffer == NULL)  {
     AMFEL("ERROR: Error extracting byte_array at line=%d", amfin_lineno);
     return NULL;
  }

  memset(ptr, 0, sizeof(amf3_byte_array));
  sprintf(closing_tag, "</%s>", type_str);

  attributes_ptr = input_buffer;
  tmp = index(attributes_ptr, '>');
  if(tmp == NULL) {
    AMFEL("ERROR: '>' not found at line=%d", amfin_lineno);
    return NULL;
  } else {
    value_ptr = tmp + 1;
    *tmp = 0;
  }

  AMFDL1("value_ptr (%s)", value_ptr);
  if(value_ptr == NULL)
  {
    AMFEL("ERROR: Value for %s found empty line=%d", type_str, amfin_lineno);
    return NULL;
  }
  else
  {
    closing_tag_ptr = strstr(value_ptr, closing_tag);
    if(closing_tag_ptr == NULL)
    {
      AMFEL("ERROR: closing tag not found at line=%d", amfin_lineno);
      return NULL;
    }
    else
    {
      *closing_tag_ptr = 0;
    }
  }

  if(is_dyn_member)
  {
     if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "dyn_member_ref", "dyn_member",
                                 &ptr->dyn_member_ref, ptr->dyn_member_name)) == NULL)
     {
        AMFEL("ERROR: Reading Dynamic Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }

  if(is_array_key)
  {
    if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "key_ref", "key", &ptr->key_ref,
                   ptr->key)) == NULL)
     {
        AMFEL("ERROR: Reading Array Key/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }

/*
  if(is_externalized)
  {
     if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "ext_member_ref", "ext_member",
                      &ptr->ext_member_ref, ptr->ext_member_name)) == NULL)
     {
        AMFDL1("ERROR: Reading Externalized Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
*/

  copy_from[0] = AMF3_BYTE_ARRAY;
  COPY_BYTES (1);
 
  if(!get_attribute_value(input_buffer, "byte_array_ref", value)) {
       ptr->byte_array_ref = atoi(value);

       //copy_from[0] = AMF3_BYTE_ARRAY;
       //COPY_BYTES (1);
       if((out = amf3_set_reference(out, len, ptr->byte_array_ref)) == NULL)
           return NULL;
       return out;
  }

  int val_len = strlen(value_ptr);

  if((out = amf3_set_U29S_value(out, len, val_len/2)) == NULL)
     return NULL;
 
  byte_ptr = value_ptr;
  //Converting Hex to Decimal
  while(byte_ptr[0] != '\0')
  {
    bcopy(byte_ptr, byte, 2);
    i_decimal_val = strtol(byte, &tmp, 16);
    AMFDL1("Byte Array Decimal Value for byte %d\n", i_decimal_val);
    c_ascii_val = (char)i_decimal_val;
    copy_from[0] = c_ascii_val;
    COPY_BYTES(1);
    byte_ptr += 2;
  }
 
  return out;
}



// Sequence of hex is
//   <null dyn_member_ref="1" dyn_member="name"/>
//   if(dyn_member_ref) then ref_id
//   <null dyn_member="name"/>
//   else if(dyn_member) then dyn_member string

//   Note: In case of dyn_member, key will not come
//   <null key_ref="1" key="name"/>
//   if(key_ref) then ref_id
//   <null key="name"/>
//   else if(key) then key string

//   Then simple_datatype marker

static char* amf3_extract_simple_datatypes (char *out, int *len,
                                     char *input_buffer, char *type_str,
                                     amf3_simple_datatypes *ptr,
                                     short is_dyn_member, short is_array_key, short is_externalized, int amf3_type) {

  //char value[2048];
  //value[0] = '\0';
  char *tmp;
  char *attributes_ptr;

  AMFDL1("Method Called, input_buffer = [%s], type_str = [%s],"
                     "amf3_type = 0x%x, is_dyn_member = %d, is_array_key = %d ",
                      input_buffer, type_str, amf3_type, is_dyn_member, is_array_key);

  if(input_buffer == NULL) return NULL;

  memset(ptr, 0, sizeof(amf3_simple_datatypes));

  attributes_ptr = input_buffer;
  tmp = strstr(attributes_ptr, "/>");
  if(tmp == NULL) {
     return NULL;
  } else {
    *tmp = 0;
  }

  if(is_dyn_member)
  {
     if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "dyn_member_ref", "dyn_member",
                                 &ptr->dyn_member_ref, ptr->dyn_member_name)) == NULL)
     {
        AMFEL("ERROR: Reading Dynamic Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
 
  if(is_array_key)
  {
    if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "key_ref", "key", &ptr->key_ref,
                   ptr->key)) == NULL)
     {
        AMFEL("ERROR: Reading Array Key/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }

/*
  if(is_externalized)
  {
     if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "ext_member_ref", "ext_member",
                      &ptr->ext_member_ref, ptr->ext_member_name)) == NULL)
     {
        AMFDL1("ERROR: Reading Externalized Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
*/
  switch(amf3_type) {
    case AMF3_NULL:
      COPY_NULL_TYPE;
      break;
    case AMF3_UNDEFINED:
      COPY_UNDEFINED_TYPE;
      break;
    case AMF3_TRUE:
      COPY_TRUE_TYPE_AMF3;
      break;
    case AMF3_FALSE:
      COPY_FALSE_TYPE_AMF3;
      break;
    default:
      AMFEL("Error: Invalid marker type %x\n", amf3_type);
      return NULL;
  }
  return out;
}

// return 0 on success
static char* amf3_extract_integer_double(char *out, int *len,
                               char *input_buffer, char *type_str,
                               amf3_integer *ptr,
                               short is_dyn_member, short is_array_key, short is_externalized, int amf3_type) {

  //char value[2048];
  char *closing_tag_ptr = NULL;
  char closing_tag[512] ;
  //value[0] = '\0';
  char *tmp;
  char *attributes_ptr;
  char *value_ptr;

  AMFDL1("Method Called, input_buffer = [%s], type_str = [%s],"
                     "amf3_type = 0x%x, is_dyn_member = %d, is_array_key = %d ",
                      input_buffer, type_str, amf3_type, is_dyn_member, is_array_key);

  if(input_buffer == NULL)  {
     AMFEL("ERROR: Error extracting value for %s at line=%d", type_str, amfin_lineno);
     return NULL;
  }

  memset(ptr, 0, sizeof(amf3_integer));
  sprintf(closing_tag, "</%s>", type_str);

  attributes_ptr = input_buffer;
  tmp = index(attributes_ptr, '>');
  if(tmp == NULL) {
    AMFEL("ERROR: '>' not found at line=%d", amfin_lineno);
    return NULL;
  } else {
    value_ptr = tmp + 1;
    *tmp = 0;
  }

  AMFDL1("value_ptr (%s)", value_ptr);
/*
  if(value_ptr == NULL)
  {
    AMFDL1("ERROR: Value for %s found empty line=%d", type_str, amfin_lineno);
    return NULL;
  }
  else
*/
  {
    closing_tag_ptr = strstr(value_ptr, closing_tag);
    if(closing_tag_ptr == NULL)
    {
      AMFEL("ERROR: closing tag not found at line=%d", amfin_lineno);
      return NULL;
    }
    else
    {
      *closing_tag_ptr = 0;
    }
  }

  if(is_dyn_member)
  {
     if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "dyn_member_ref", "dyn_member",
                                 &ptr->dyn_member_ref, ptr->dyn_member_name)) == NULL)
     {
        AMFEL("ERROR: Reading Dynamic Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }

  if(is_array_key)
  {
    if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "key_ref", "key", &ptr->key_ref,
                   ptr->key)) == NULL)
     {
        AMFEL("ERROR: Reading Array Key/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
/*
  if(is_externalized)
  {
     if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "ext_member_ref", "ext_member",
                      &ptr->ext_member_ref, ptr->ext_member_name)) == NULL)
     {
        AMFEL("ERROR: Reading Externalized Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
*/

  strcpy(ptr->int_val, value_ptr);
  int int_val_len = strlen(value_ptr);

  if(int_val_len <= 0)
  {
     AMFEL("ERROR: Invalid %s value received=[%s]", type_str, ptr->int_val);
     return NULL;
  }

  switch(amf3_type) {
    case AMF3_INTEGER:
      if(!amf_no_param_flag && ptr->int_val[0] == '{' && ptr->int_val[int_val_len - 1] == '}') {
        value_ptr[int_val_len - 1] = '\0';
        MARK_SEGMENT (AMF3_INTEGER, value_ptr + 1);
      } else {
        copy_from[0] = AMF3_INTEGER;
        COPY_BYTES (1);
        COPY_INTEGER_TYPE_AMF3(value_ptr);
      }
      break;
    case AMF3_DOUBLE:
      if(!amf_no_param_flag && ptr->int_val[0] == '{' && ptr->int_val[strlen(ptr->int_val) - 1] == '}') {
        value_ptr[strlen(value_ptr) - 1] = '\0';
        MARK_SEGMENT (AMF3_DOUBLE, value_ptr + 1);
      } else {
        //If NAN/nan appering putting (0x7F FF FF FF E0 00 00 00) in its place
        if(strcasestr(value_ptr, "nan") != NULL)
        {
          copy_from[0] = AMF3_DOUBLE;
          copy_from[1] = 0x7F;
          copy_from[2] = 0xFF;
          copy_from[3] = 0xFF;
          copy_from[4] = 0xFF;
          copy_from[5] = 0xE0;
          copy_from[6] = 0x00;
          copy_from[7] = 0x00;
          copy_from[8] = 0x00;
          COPY_BYTES (9);
        }
        else
        {
           COPY_NUMBER_TYPE (value_ptr, AMF3_COPY_MARKER);
        }
      }
      break;
    default:
      AMFEL("Error: Invalid marker type %x at line=%d\n", amf3_type, amfin_lineno);
      return NULL;
  }

  return out;
}

static char * amf3_extract_date(char *out, int *len,
                               char *input_buffer, char *type_str,
                               amf3_date *ptr,
                               short is_dyn_member, short is_array_key, short is_externalized, int amf3_type) {

  char value[2048];
  char *closing_tag_ptr = NULL;
  char closing_tag[512] ;
  value[0] = '\0';
  char *tmp;
  char *attributes_ptr;
  char *value_ptr;

  AMFDL1("Method Called, input_buffer = [%s], type_str = [%s],"
                     "is_dyn_member = %d, is_array_key = %d ",
                      input_buffer, type_str, is_dyn_member, is_array_key);
  if(input_buffer == NULL) return NULL;

  memset(ptr, 0, sizeof(amf3_date));
  sprintf(closing_tag, "</%s>", type_str);

  attributes_ptr = input_buffer;
  tmp = index(attributes_ptr, '>');
  if(tmp == NULL) {
     return NULL;
  } else {
    value_ptr = tmp + 1;
    *tmp = 0;
  }

  if(value_ptr == NULL) {
    return NULL;
  } else {
    closing_tag_ptr = strstr(value_ptr, closing_tag);
    if(closing_tag_ptr == NULL) {
      return NULL;
    } else {
      *closing_tag_ptr = 0;
    }
  }

  if(is_dyn_member)
  {
     if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "dyn_member_ref", "dyn_member",
                                 &ptr->dyn_member_ref, ptr->dyn_member_name)) == NULL)
     {
        AMFEL("ERROR: Reading Dynamic Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }

  if(is_array_key)
  {
    if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "key_ref", "key", &ptr->key_ref,
                   ptr->key)) == NULL)
     {
        AMFEL("ERROR: Reading Array Key/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
/*
  if(is_externalized)
  {
     if((out = amf3_set_datatype_ref_names(attributes_ptr, out, len, "ext_member_ref", "ext_member",
                      &ptr->ext_member_ref, ptr->ext_member_name)) == NULL)
     {
        AMFDL1("ERROR: Reading Externalized Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
*/

  // If date is a reference, then we need to set reference id and ignore value
  if(!get_attribute_value(input_buffer, "date_ref", value)) {
      ptr->date_ref = atoi(value);
      copy_from[0] = AMF3_DATE;
      COPY_BYTES (1);
      if((out = amf3_set_reference(out, len, ptr->date_ref)) == NULL)
        return NULL;
    return out;
  }

  strcpy(ptr->date_val, value_ptr);
  int date_val_len = strlen(ptr->date_val);
  if(!amf_no_param_flag && ptr->date_val[0] == '{' && ptr->date_val[date_val_len - 1] == '}') {
     value_ptr[date_val_len - 1] = '\0';
     MARK_SEGMENT (AMF3_DATE, value_ptr + 1);
   } else {
     COPY_DATE_TYPE (value_ptr, AMF3_COPY_MARKER);
   }

  return out;
}
/*
  <member>operation</member>
  <member str_ref="1">operation</member>

*/
static char * amf3_extract_member(char *out, int *len,
                               char *input_buffer, char *type_str,
                               amf3_member *ptr,
                               int is_dyn_member, int is_array_key, int amf3_type) {
  char value[2048];
  char *closing_tag_ptr = NULL;
  char closing_tag[512] ;
  value[0] = '\0';
  char *tmp;
  char *attributes_ptr;
  char *value_ptr;

  AMFDL1("Method Called, input_buffer = [%s], type_str = [%s],"
                     "is_dyn_member = %d, is_array_key = %d ",
                      input_buffer, type_str, is_dyn_member, is_array_key);

  if(input_buffer == NULL) {
     AMFEL("ERROR: Error extracting value for member at line=%d", amfin_lineno);
     return NULL;
  }

  memset(ptr, 0, sizeof(amf3_member));
  sprintf(closing_tag, "</%s>", type_str);

  attributes_ptr = input_buffer;
  tmp = index(attributes_ptr, '>');
  if(tmp == NULL) {
     AMFEL("ERROR: '>' not found at line=%d", amfin_lineno);
     return NULL;
  } else {
    value_ptr = tmp + 1;
    *tmp = 0;
  }

  AMFDL1("value_ptr = [%s]", value_ptr);
  if(value_ptr == NULL) {
    AMFEL("ERROR: Value for %s found empty line=%d", type_str, amfin_lineno);
    return NULL;
  } else {
    closing_tag_ptr = strstr(value_ptr, closing_tag);
    if(closing_tag_ptr == NULL) {
      AMFEL("ERROR: closing tag not found at line=%d", amfin_lineno);
      return NULL;
    } else {
      *closing_tag_ptr = 0;
    }
  }

  AMFDL1("value_ptr = [%s]", value_ptr);
  if(!get_attribute_value(attributes_ptr, "str_ref", value)) {
     ptr->str_ref = atoi(value);
     AMFDL1("Member is a reference. Ref_id = %d", ptr->str_ref);
     if((out = amf3_set_reference(out, len, ptr->str_ref)) == NULL)
        return NULL;
  }
  else
  {
    strcpy(ptr->member_val, value_ptr);
    AMFDL1("Member is a value. value = %d", ptr->member_val);
    COPY_STRING_TYPE(ptr->member_val, AMF3_DO_NOT_COPY_MARKER);
  }

  return out;
}


static int extract_string(char *start_line, char *str_val, int max_len)
{
char *cur_ptr = start_line;
char *start_ptr;
int value_len, tot_str_len;
char closing_tag[] = "</string>";

  AMFDL1("Method called. start_line = %s", start_line);

  // Strings can be in multiple lines. So search for </string> in current line and then next lines
  // Following cases are possible:
  // 1. Empty string
  //    <sting></string>
  // 2. String with only newline -> Make sure new line is not stripped off in the binary
  //    <sting>
  //    </string>
  // 3. Single line string
  //    <sting>neeraj</string>
  // 4. Multi line string -> Make sure new line is not stripped off in the binary
  //    <sting>neeraj,
  //    shilpa,
  //    arun</string>

  start_ptr = index(cur_ptr, '>') + 1; // Point to next location after >

  str_val[0] = '\0'; // Must start with empty as we are using strncat 
  tot_str_len = 0;

  do 
  {
    AMFDL1("Checking %s in line number %d, line = %s", closing_tag, amfin_lineno, cur_ptr);

    char *closing_tag_ptr = strstr(cur_ptr, closing_tag);
    if(closing_tag_ptr)
    {
      if(start_ptr) // Start and closing tags on same line
      {
           AMFDL1("Closing tag found on first line");
           value_len = closing_tag_ptr - start_ptr;
           if(value_len > 0) // String can be empty
           {
             strncat(str_val, start_ptr, value_len);
             tot_str_len += value_len;
           }
           str_val[tot_str_len] = '\0';
      }
      else // Closing tag on different line
      {
           AMFDL1("Closing tag found on differnt line");
           value_len = closing_tag_ptr - cur_ptr;
           if(value_len > 0) // String can be empty
           {
             strncat(str_val, cur_ptr, value_len);
             tot_str_len += value_len;
           }
           str_val[tot_str_len] = '\0';
      }
      break;
    }
    else // Closing tag not found. Concat current line string value and then read next line
    {
         if(start_ptr) // First line
         {
           cur_ptr = start_ptr;
           value_len = strlen(cur_ptr);
           if(value_len > 0) // String can be empty
           {
             strncat(str_val, cur_ptr, value_len);
             tot_str_len += value_len;
             str_val[tot_str_len] = '\0';
           }
           strncat(str_val, "\n", 1); // Concat new line which was stripped by get_line method
           tot_str_len += 1;
           str_val[tot_str_len] = '\0';
         }
         else // Other lines
         {
           value_len = strlen(cur_ptr);
           if(value_len > 0) // String can be empty
           {
             strncat(str_val, cur_ptr, value_len);
             tot_str_len += value_len;
             str_val[tot_str_len] = '\0';
           }
           strncat(str_val, "\n", 1); // Concat new line which was stripped by get_line method
           tot_str_len += 1;
           str_val[tot_str_len] = '\0';
         }
     }
     if(get_line(AMF_DO_NOT_TRIM_STRING) == NULL)
     {
       // Error
       AMFDL1("Error: bad format. End of string tag not found\n");
       return -1;
     }
     cur_ptr = line;
     start_ptr = NULL; // String is from the start of line
  } while(1);
  return 0;
}

static char * amf3_extract_string(char *out, int *len,
                               char *input_buffer, char *type_str,
                               amf3_string *ptr,
                               short is_dyn_member, short is_array_key, short is_externalized, int amf3_type) {
  char closing_tag[512] ;
  //char *cur_ptr;
  int tot_str_len = 0;
  char value[4096];
  value[0] = '\0';

  AMFDL1("Method Called, input_buffer = [%s], type_str = [%s],"
                     " is_dyn_member = %d, is_array_key = %d ",
                      input_buffer, type_str, is_dyn_member, is_array_key);

  memset(ptr, 0, sizeof(amf3_string));

  sprintf(closing_tag, "</%s>", type_str);

  if(input_buffer == NULL) return NULL; // Safety check

  if(is_dyn_member)
  {
     if((out = amf3_set_datatype_ref_names(input_buffer, out, len, "dyn_member_ref", "dyn_member",
                                 &ptr->dyn_member_ref, ptr->dyn_member_name)) == NULL)
     {
        AMFEL("ERROR: Reading Dynamic Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }

  if(is_array_key)
  {
    if((out = amf3_set_datatype_ref_names(input_buffer, out, len, "key_ref", "key", &ptr->key_ref,
                   ptr->key)) == NULL)
     {
        AMFEL("ERROR: Reading Array Key/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }

/*
  if(is_externalized)
  {
     if((out = amf3_set_datatype_ref_names(input_buffer, out, len, "ext_member_ref", "ext_member",
                      &ptr->ext_member_ref, ptr->ext_member_name)) == NULL)
     {
        AMFDL1("ERROR: Reading Externalized Member Name/Ref at line=%d", amfin_lineno);
        return NULL;
     }
  }
*/
 if(!get_attribute_value(input_buffer, "str_ref", value)) {
       ptr->str_ref = atoi(value);

       copy_from[0] = AMF3_STRING;
       COPY_BYTES (1);
       if((out = amf3_set_reference(out, len, ptr->str_ref)) == NULL)
           return NULL;
       return out;
  }

  //cur_ptr = input_buffer;

  if(extract_string(input_buffer, ptr->str_val, -1) < 0) // TODO - pass correct len
  {
    AMFDL1("Returning str_val = [%s]", ptr->str_val);
    return NULL;
  }


  tot_str_len = strlen(ptr->str_val); // TODO Optimize 
  AMFDL1("str_val = [%s]", ptr->str_val);

  if(!amf_no_param_flag && ptr->str_val[0] == '{' && ptr->str_val[tot_str_len - 1] == '}') {
    ptr->str_val[tot_str_len - 1] = '\0';
    MARK_SEGMENT (AMF3_STRING, ptr->str_val + 1)
  } else {
    if (strncmp (ptr->str_val, "CAVHEX:", 7)) {
      COPY_STRING_TYPE (ptr->str_val, AMF3_COPY_MARKER);
    } else {
      COPY_OCTET_TYPE (ptr->str_val + 7);
    }
  }
  return out;
}
//Reads AMF message from infp and writes binary into out

//For all complex , header line is already read

static char *
amf0_read_object_element (char *out, int *len)
{
  int in_object = 0;
  char buf1[MAX_XML_LINE_LEN];

  while (1) {
    GET_LINE;
    if (in_object) {
      if (strcmp (line, "</objelement>") == 0) {
        in_object = 0; // Set to 0
      } else {
        AMFEL("Bad format: expecting </objelement> at line %d\n",
                amfin_lineno);
        return NULL;
      }
    } else {
      if ((strcmp (line, "</object>") == 0)
          || (strcmp (line, "</ecma_array>") == 0)
          || (strcmp (line, "</typed_object>") == 0)) {
        copy_from[0] = 0x00;
        copy_from[1] = 0x00;
        copy_from[2] = AMF0_OBJECT_END_MARKER;
        COPY_BYTES (3);
        return out;
      } else if (sscanf (line, "<objelement name=\"%s\">", buf1)) {
        if ((buf1[strlen (buf1) - 1] != '>')
            && (buf1[strlen (buf1) - 2] != '"')) {
          AMFEL("Bad format: expecting <objelement name=xxx> at line %d\n",
                  amfin_lineno);
          return NULL;
        }
        buf1[strlen (buf1) - 2] = '\0';
        COPY_STRING (buf1);
        if ((out = read_data_amf0 (out, len)) == NULL) {
          AMFEL("Bad data read at line=%d\n", amfin_lineno);
          return NULL;
        }
        in_object = 1;
      } else {
        AMFEL("Bad data read at line=%d expecting objelement or /object \n",
                amfin_lineno);
        return NULL;
      }
    }
  }
  return out;
}



static char *amf3_set_U29S_value(char *out, int *len, int string_size)
{
  int flags=0;
  char buf1[MAX_VAL];

  AMFDL1("String Size [%d]", string_size);
  if(string_size > MAX_SIZE_28_BITS)
  {
     AMFEL("Bad format: String Size [%d] exceeded its Maximum permitted[%d] at line=%d\n",
                                                                    string_size, MAX_SIZE_28_BITS, amfin_lineno);
     return NULL;
  }

  flags = string_size;
  flags <<= 1;
  flags |= 0x01;
  AMFDL1("U29S Flags=%x", flags);
  sprintf(buf1, "%d", flags);
  COPY_INTEGER_TYPE_AMF3(buf1);

  return out;
}


static char *amf3_set_reference(char *out, int *len, int ref_index)
{
  int flags=0;
  char buf1[MAX_VAL];

  AMFDL1("Reference Index[%d]", ref_index);
  if(ref_index > MAX_SIZE_28_BITS)
  {
     AMFEL("Bad format: reference size[%d] exceeded its Maximum permitted[%d] at line=%d\n",
                                                                    ref_index, MAX_SIZE_28_BITS, amfin_lineno);
     return NULL;
  }

  flags = ref_index;
  flags <<= 1;
  flags &= 0x1FFFFFFE;
  AMFDL1("Reference Flags=%x\n", flags);
  sprintf(buf1, "%d", flags);
  COPY_INTEGER_TYPE_AMF3(buf1);

  return out;
}

// return 0 on success
static char* amf3_extract_flags(char *out, int *len,
                               char *input_buffer, char *type_str, char *end_of_flags)
{
  //char value[2048];
  char *closing_tag_ptr = NULL;
  char closing_tag[512] ;
  //value[0] = '\0';
  char *tmp;
  char *attributes_ptr;
  char *value_ptr;
  int  i_decimal_val, i;
  char c_ascii_val, byte[3];
  byte[2] = '\0';

  AMFDL2("Method Called, input_buffer = [%s], type_str = [%s]",
                      input_buffer, type_str);

  if(input_buffer == NULL)  {
     AMFEL("ERROR: Error extracting value for %s at line=%d", type_str, amfin_lineno);
     return NULL;
  }

  sprintf(closing_tag, "</%s>", type_str);

  attributes_ptr = input_buffer;
  tmp = index(attributes_ptr, '>');
  if(tmp == NULL) {
    AMFDL1("ERROR: '>' not found at line=%d", amfin_lineno);
    return NULL;
  } else {
    value_ptr = tmp + 1;
    *tmp = 0;
  }

  if(value_ptr == NULL)
  {
    AMFEL("ERROR: Value for %s found empty line=%d", type_str, amfin_lineno);
    return NULL;
  }
  else
  {
    closing_tag_ptr = strstr(value_ptr, closing_tag);
    if(closing_tag_ptr == NULL)
    {
      AMFEL("ERROR: closing tag not found at line=%d", amfin_lineno);
      return NULL;
    }
    else
    {
      *closing_tag_ptr = 0;
    }
  }
  
  AMFDL1("value_ptr (%s)", value_ptr);
  //strncpy(flags, value_ptr, value_ptr - closing_tag_ptr);
  //AMFDL1("flags (%s)", flags);

  if((strcmp(value_ptr,"00") == 0) || (strcmp(value_ptr,"0") == 0))
  {
     AMFDL1("Externalizable Flags=%d received denotes end of externalizable data line=%d", 
                                                value_ptr, amfin_lineno);
     *end_of_flags = '1';
     return out;
  }

  //Converting Hex to Decimal
  for(i = 0; i < 2; i++)
  {
    bcopy((value_ptr + i*2), byte, 2);
    i_decimal_val = strtol(byte, &tmp, 16);
    AMFDL1("Flags Decimal Value for byte %d\n", i_decimal_val);
    if(i_decimal_val == 0)   //As A13 is send as A103 through Java, hence when we receive 0, we just ignore it 
       continue;
    c_ascii_val = (char)i_decimal_val;
    copy_from[0] = c_ascii_val;
    COPY_BYTES(1);
  }

/*
  AMFDL1("value_ptr (%c)", value_ptr + i*2);
  if((value_ptr + i*2) != NULL)
  {
     AMFDL1("Unacceptable Externalizable Flags=%d received at line=%d", 
                                                value_ptr, amfin_lineno);
     return NULL;
  }
*/
  
  *end_of_flags = '0';
  return out;
}


//Extract Externalizable Data
char *amf3_extract_ext_data(char *out, int *len)
{
  char end_of_flags='0' ;
  int flag_found=0, obj_read=0;
  AMFDL1("Reading Externalized Members at line=%d\n", amfin_lineno);

  while(1)
  {

    AMFDL3("obj_read=%d, flag_found=%d", obj_read, flag_found);
    //If flags not found, we are assuming we have to read one element in the external object 
    //(as we do not have the trait information of the external class)
    //We are reading only one data element and returning after that
    if(obj_read == 1 && !flag_found) 
    {
      AMFDL3("External class data read complete");
      return out;
    }

   AMFDL3("Reading Externalized Members at line=%d\n", amfin_lineno);
   GET_LINE;

    if (strncmp(line, "<flags", 6) == 0)
    {
       flag_found = 1;
       out = amf3_extract_flags(out, len, line, "flags", &end_of_flags);
       AMFDL1("end_of_flags %c at line=%d\n", end_of_flags);
       if (end_of_flags == '1')
       {
          //Write 0
          copy_from[0] = 0;
          COPY_BYTES(1);
          AMFDL1("End of Externalized Members at line=%d\n", amfin_lineno);
          return out;
       }
       GET_LINE;
    }

    obj_read++;
    if ((out = amf3_read_data (out, len, AMF3_NOT_DYNAMIC_MEMBER, AMF3_NOT_ARRAY_KEY, AMF3_EXTERNAZIABLE)) == NULL)
    {
      AMFEL("Bad data read at line=%d\n", amfin_lineno);
      return NULL;
    }
  }
  AMFDL3("Externalize data read complete\n");
  return out;
}

static char *
amf3_read_object (char *out, int *len, short is_dyn_member, short is_array_key, short is_externalized)
{
  char buf1[MAX_XML_LINE_LEN];
  int index, flags = 0;
  amf3_object obj;
  amf3_member member;

  AMFDL1("Method Called out=[%s], len=[%d]  is_dyn_member=[%d], is_array_key=[%d]",
	                                                     out, *len, is_dyn_member, is_array_key);
  //  handle dyn/key in function
  if((out = amf3_set_object_attr(out, len, &obj, is_dyn_member, is_array_key, is_externalized)) == NULL)
  {
    AMFEL("Error Reading Object Attributes at line=%d", amfin_lineno);
    return NULL;
  }
  
  copy_from[0] = AMF3_OBJECT;
  COPY_BYTES(1);

  // Case1 - Object is reference
  // <object object_ref="1">
  // Hex Dump
  // 0A 02 (No more data for object as it is reference)
  if(obj.object_ref != -1)
  {
    AMFDL1("Got Object Reference=%d at line=%d obj.traits_ref=%d, obj.externalizable=%d, obj.dynamic=%d, obj.sealed_members=%d\n", obj.object_ref, amfin_lineno, obj.traits_ref, obj.externalizable, obj.dynamic, obj.sealed_members);
    if((obj.traits_ref != -1) || obj.externalizable || obj.dynamic || obj.sealed_members)
    {
      AMFEL("Bad format: Undefined attribute combination in object at line=%d\n", amfin_lineno);
      return NULL;
    }
    if((out = amf3_set_reference(out, len, obj.object_ref)) == NULL)
      return NULL;

    GET_LINE;
    if (strcmp (line, "</object>") != 0)
    {
      AMFEL("Bad Format: Expecting </object> at line=%d\n", amfin_lineno);
      return NULL;
    }

    return out; // Return as nothing to be done
  }

/*
  if(obj.classname_ref != -1)
  {
     AMFDL1("Got Class Reference=%d at line=%d\n", obj.classname_ref, amfin_lineno);
     if((out = amf3_set_reference(out, len, obj.classname_ref)) == NULL)
        return NULL;
  }
*/
  //Case 2 - Object is an instance but Traits is a reference
  //  Notes:
  //  1. dynamic can be true or false based the traits which are refereced from here.
  //  2. externalizable can be true or false based the traits which are refereced from here.
  //  3. Can have sealed members but only value will come
  if(obj.traits_ref != -1)
  {
    AMFDL1("Got Object Trait Reference=%d at line=%d\n", obj.traits_ref, amfin_lineno);
    if(obj.traits_ref > MAX_TRAITS_REF_SIZE_27_BITS)
    {
       AMFEL("Bad format: Object traits reference size[%d] exceeded    \
                    Maximum permitted[%d] in object at line=%d\n", 
                    obj.traits_ref, MAX_TRAITS_REF_SIZE_27_BITS, amfin_lineno);
       return NULL;
    }

    flags = obj.traits_ref;
    flags <<= 2;
    flags |= 0x00000001;
    AMFDL1("Object Trait Reference Flags=%x\n", flags);
    sprintf(buf1, "%d", flags);
    COPY_INTEGER_TYPE_AMF3(buf1);
  }

  //Case 3 - Object Traits are Externalizable, Traits member count=0
  //  Notes:
  //  1. dynamic can be true or false based the traits which are refereced from here.
  //  2. externalizable can be true or false based the traits which are refereced from here.
  if(obj.externalizable)
  {
    flags = 0;
    AMFDL1("Object Externalizable");
    if(obj.dynamic || obj.sealed_members)
    {
       AMFEL("Bad format: Undefined attribute combination in object at line=%d\n", amfin_lineno);
       return NULL;
    }

    if(obj.traits_ref == -1)
    {
      flags |= 0x07;
      AMFDL1("Object Externalized Flags=%x", flags);
      sprintf(buf1, "%d", flags);
      COPY_INTEGER_TYPE_AMF3(buf1);
    }  

    //SS:18/4/13 Adding check for not to copy classname in case classname is empty
    //           Was copying 01 for empty string earlier.
    if(obj.classname_ref == -1 && obj.classname != NULL && (strlen(obj.classname) != 0))      
    {
      AMFDL1("Got Object Classname=%s at line=%d\n", obj.classname, amfin_lineno);
      //Cannot have both classname & object reference in an object
      COPY_STRING_TYPE(obj.classname, AMF3_DO_NOT_COPY_MARKER);
    }
    else if(obj.classname_ref != -1)
    {
       AMFDL1("Got Class Reference=%d at line=%d\n", obj.classname_ref, amfin_lineno);
       if((out = amf3_set_reference(out, len, obj.classname_ref)) == NULL)
         return NULL;
    }

    out = amf3_extract_ext_data(out, len);
    GET_LINE;
    if (strcmp (line, "</object>") != 0)
    {
      AMFEL("Bad Format: Expecting </object> at line=%d\n", amfin_lineno);
      return NULL;
    }
    return out;
  }

  //Case 4: Object traits are received - Sealed and/or Dynamic
  //  Notes:
  //  1. dynamic can be true or false based the traits which are refereced from here.
  //  2. externalizable can be true or false based the traits which are refereced from here.
  if(obj.traits_ref == -1)
  {
   flags = obj.sealed_members;
   flags <<= 4;
   if(obj.dynamic)
     flags |= 0x0000000B;
   else
     //flags &= 0xFFFFFFF7;
     flags |= 0x0000003;
     AMFDL1("Object Flags=%x", flags);
     sprintf(buf1, "%d", flags);
     COPY_INTEGER_TYPE_AMF3(buf1);
  
    if(obj.classname_ref == -1 && obj.classname != NULL)
    {
      AMFDL1("Got Object Classname=%s at line=%d\n", obj.classname, amfin_lineno);
      //Cannot have both classname & object reference in an object
      COPY_STRING_TYPE(obj.classname, AMF3_DO_NOT_COPY_MARKER);
    }
    else if(obj.classname_ref != -1)
    {
       AMFDL1("Got Class Reference=%d at line=%d\n", obj.classname_ref, amfin_lineno);
       if((out = amf3_set_reference(out, len, obj.classname_ref)) == NULL)
         return NULL;
    }
  }
 
  // Traits are referenced, then sealed member names will not come in xml
  if(obj.traits_ref == -1)
  {
    AMFDL1("Total sealed members=%d at line %d\n", obj.sealed_members, amfin_lineno);
    for (index = 0; index < obj.sealed_members; index++)
    {
      AMFDL1("Sealed Member index= %d", index);
      GET_LINE;
      if (strncmp (line, "<member", 7) == 0)
      {
        if((out = amf3_extract_member(out, len, line, "member", &member, -1, -1, -1)) == NULL)  
        {
          AMFEL("Error: Member name cound not be extracted at line=%d", amfin_lineno);
          return NULL;
        }
      }
    }

    if(obj.sealed_members)
    {
      GET_LINE;
      AMFDL1("Reading end_of_sealed_member_names");
      if(strcmp(line, "<end_of_sealed_member_names/>") != 0)
      {
        AMFEL("Bad format: did not find <end_of_sealed_member_names/> at line %d\n", amfin_lineno);
        return NULL;
      }
    }
  }

  if(!obj.externalizable)
  {
    // Sealed members can be a reference, can be an array key
    AMFDL1("Sealed Member=[%d] line=%d\n", obj.sealed_members, amfin_lineno);
    for (index = 0; index < obj.sealed_members; index++)
    {
      AMFDL1("Reading Sealed Member at index=[%d] line=%d\n", index, amfin_lineno);
      if ((out = amf3_read_data (out, len, AMF3_NOT_DYNAMIC_MEMBER, AMF3_NOT_ARRAY_KEY, AMF3_NOT_EXTERNAZIABLE)) == NULL)
      {
        AMFEL("Bad data read at line=%d\n", amfin_lineno);
        return NULL;
      }
    }
    if(obj.sealed_members)
    {
      GET_LINE;
      if(strcmp(line,"<end_of_sealed_member_values/>") != 0)
      {
        AMFEL("Bad format: did not find <end_of_sealed_member_values/> at line %d\n", amfin_lineno);
        return NULL;
      }
    }

    AMFDL1("Dynamic=[%d] at line=%d\n", obj.dynamic, amfin_lineno);
    if (obj.dynamic)
    {
      AMFDL1("Reading Dynamic Members at line=%d\n", index, amfin_lineno);
      GET_LINE;
      while (strcmp(line, "<end_of_dyn_members/>") != 0)
      {
        if ((out = amf3_read_data (out, len, AMF3_DYNAMIC_MEMBER, AMF3_NOT_ARRAY_KEY, AMF3_NOT_EXTERNAZIABLE)) == NULL)
        {
   	  AMFEL("Bad data read at line=%d\n", amfin_lineno);
          return NULL;
        }
        GET_LINE;
      }
      copy_from[0] = EMPTY_STRING;	//Empty String denoting end of dynamic members
      COPY_BYTES(1);
    }
  }
  GET_LINE;

  if (strcmp (line, "</object>") != 0)
  {
      AMFEL("Bad Format: Expecting </object> at line=%d\n", amfin_lineno);
      return NULL;
  }
  return out;
}


static char *
amf3_read_array (char *out, int *len, short is_dyn_member, short is_array_key, short is_externalized)
{
  int index=0;
  int flags = 0;
  amf3_array  arr;
  char buf1[1024];

  AMFDL1("Method Called out=[%s], len=[%d]  is_dyn_member=[%d], is_array_key=[%d]",
                                                             out, *len, is_dyn_member, is_array_key);

  if((out = amf3_set_array_attr(out, len, &arr, is_dyn_member, is_array_key, is_externalized)) == NULL)
  {
    AMFEL("Error Reading Object Attributes at line=%d", amfin_lineno);
    return NULL;
  }

  //amf3_extract_array(out, len, line, "array", &arr, is_dyn_member, is_array_key, AMF3_ARRAY);
  copy_from[0] = AMF3_ARRAY;
  COPY_BYTES(1);

  //If array reference, other attributes will not be there
  if(arr.arr_ref != -1)
  {
    AMFDL1("Got Array Reference=%d at line=%d\n", arr.arr_ref, amfin_lineno);
    if(arr.size)
    {
      AMFEL("Bad format: Undefined attribute combination in array at line=%d\n", amfin_lineno);
      return NULL;
    }
    if((out = amf3_set_reference(out, len, arr.arr_ref)) == NULL)
      return NULL;

    GET_LINE;
    AMFDL1("Read </array> at line=%d\n", amfin_lineno);
    if (strcmp (line, "</array>")) {
      AMFEL("Bad Format: Expecting </array> at line=%d\n", amfin_lineno);
        return NULL;
    }
    return out;
  }

  AMFDL1("Got Array Size=%d at line=%d\n", arr.size, amfin_lineno);
  //Array Size can be 0
  flags = arr.size;
  flags <<= 1;
  flags |= 0x01;
  sprintf(buf1, "%d", flags);
  COPY_INTEGER_TYPE_AMF3(buf1);

  GET_LINE;
  while (strcmp(line, "<end_of_associative_array/>") != 0)
  {
     AMFDL1("Read Associative Member=%d at line=%d\n", index++, amfin_lineno);
     if ((out = amf3_read_data (out, len, AMF3_NOT_DYNAMIC_MEMBER, AMF3_ARRAY_KEY, AMF3_NOT_EXTERNAZIABLE)) == NULL)
     {
	AMFEL("Bad data read at line=%d\n", amfin_lineno);
	return NULL;
     }
      GET_LINE;
  }

  // Terminate associative array with empty string
  copy_from[0] = EMPTY_STRING;
  COPY_BYTES(1);

  // Indexed elements can be a reference, can be an array key
  for (index = 0; index < arr.size; index++)
  {
    AMFDL1("Read Indexed Elements=%d at line=%d\n", index, amfin_lineno);
    if ((out = amf3_read_data (out, len, AMF3_NOT_DYNAMIC_MEMBER, AMF3_NOT_ARRAY_KEY, AMF3_NOT_EXTERNAZIABLE)) == NULL)
    {
      AMFEL("Bad data read at line=%d\n", amfin_lineno);
      return NULL;
    }
  }

  GET_LINE;
  AMFDL1("Read </array> at line=%d\n", amfin_lineno);
  if (strcmp (line, "</array>")) {
    AMFEL("Bad Format: Expecting </array> at line=%d\n", amfin_lineno);
    return NULL;
  }
  return out;
}


static char *
amf0_read_array (char *out, int *len, char *size)
{
  int sz = atoi (size);
  int nwsz, i;

  copy_from[0] = AMF0_STRICT_ARRAY;
  nwsz = htonl (sz);
  bcopy ((char *) &nwsz, copy_from + 1, 4);
  COPY_BYTES (5);

  for (i = 0; i < sz; i++) {
    if ((out = read_data_amf0 (out, len)) == NULL) {
      AMFDL1("amf0_read_array:Bad data read at line=%d\n", amfin_lineno);
      return NULL;
    }

  }
  GET_LINE;
  if (strcmp (line, "</array>")) {
    AMFDL1("Expecting </array> at line=%d\n", amfin_lineno);
    return NULL;
  }
  return out;
}


// AMF0 Only
static char *
amf0_read_object (char *out, int *len)
{
  copy_from[0] = AMF0_OBJECT;
  COPY_BYTES (1);
  return (amf0_read_object_element (out, len));
}

static char *
amf0_read_ecma_array(char *out, int *len, char *size)
{
  int sz = atoi (size);
  int nwsz;

  copy_from[0] = AMF0_ECMA_ARRAY;
  nwsz = htonl (sz);
  bcopy ((char *) &nwsz, copy_from + 1, 4);
  COPY_BYTES (5);

  return (amf0_read_object_element (out, len));
}

static char *
amf0_read_typed_object (char *out, int *len, char *classname)
{
  copy_from[0] = AMF0_TYPED_OBJECT;
  COPY_BYTES (1);
  COPY_STRING (classname);
  return (amf0_read_object_element (out, len));
}

char *
read_data_amf0 (char *out, int *len)
{
  char *ptr, buf1[MAX_XML_LINE_LEN];

  GET_LINE;

  if (strcmp (line, "<switch_to_amf3/>") == 0) {
    copy_from[0] = AMF0_TO_AMF3;
    COPY_BYTES (1);
    amf_data_version = AMF_VERSION3;
    out = amf3_read_data (out, len, AMF3_NOT_DYNAMIC_MEMBER, AMF3_NOT_ARRAY_KEY, AMF3_NOT_EXTERNAZIABLE);
    return out;
  }

  //Expect any data type - complete reading simple data type such as <string, <null/>, <bool, cur_state dont chnage
  //Read more for complex data data type - <*objects, <array,  cur state depends upon object
  //<string>xxx</string>, get xxx in buf1
  if (strncmp (line, "<string>", 8) == 0) {
    if(extract_string(line, buf1, -1) < 0) {
      AMFDL1("Returning str_val = [%s]", buf1);
      return NULL;
    }

    int val_len = strlen(buf1);
    if (!amf_no_param_flag && amf_out_mode && (buf1[0] == '{') && (buf1[val_len - 1] == '}')) {
      buf1[val_len - 1] = '\0'; // replace } by null
      MARK_SEGMENT (AMF0_STRING, buf1 + 1);
    } else {
      if (strncmp (buf1, "CAVHEX:", 7)) {
	COPY_STRING_TYPE (buf1, AMF3_COPY_MARKER);
      } else {
	COPY_OCTET_TYPE (buf1 + 7);
      }
    }
  // TODO - large string and xml doc
  } else if (strcmp (line, "<null/>") == 0) {
    COPY_NULL_TYPE;
  } else if (strcmp (line, "<undefined/>") == 0) {
    COPY_UNDEFINED_TYPE;
  } else if (strcmp (line, "<unsupported/>") == 0) {
    COPY_UNSUPPORTED_TYPE;
  } else if (sscanf (line, "<number>%s", buf1)) {
    if ((ptr = strstr (buf1, "</number>")) == NULL) {
      AMFEL("Bad format: did not find terminating </number> at line %d\n",
	      amfin_lineno);
      return NULL;
    }
    *ptr = '\0';
    if (!amf_no_param_flag && amf_out_mode && (buf1[0] == '{') && (*(ptr - 1) == '}')) {
      *(ptr - 1) = '\0';
      MARK_SEGMENT (AMF0_NUMBER, buf1 + 1);
    } else {
      //out = copy_number_type (buf1, char *out, int *outlen);
      COPY_NUMBER_TYPE (buf1, AMF3_COPY_MARKER);
    }
  } else if (sscanf (line, "<reference>%s", buf1)) {
    if ((ptr = strstr (buf1, "</reference>")) == NULL) {
      AMFEL("Bad format: did not find terminating </reference> at line %d\n",
	      amfin_lineno);
      return NULL;
    }
    *ptr = '\0';
    if (!amf_no_param_flag && amf_out_mode && (buf1[0] == '{') && (*(ptr - 1) == '}')) {
      *(ptr - 1) = '\0';
      MARK_SEGMENT (AMF0_REFERENCE, buf1 + 1);
    } else {
      COPY_SHORT_TYPE (buf1);
    }
  } else if (sscanf (line, "<bool>%s", buf1)) {
    if ((ptr = strstr (buf1, "</bool>")) == NULL) {
      AMFEL("Bad format: did not find terminating </bool> at line %d\n",
	      amfin_lineno);
      return NULL;
    }
    *ptr = '\0';
    if (!amf_no_param_flag && amf_out_mode && (buf1[0] == '{') && (*(ptr - 1) == '}')) {
      *(ptr - 1) = '\0';
      MARK_SEGMENT (AMF0_BOOLEAN, buf1 + 1);
    } else {
      COPY_BOOL_TYPE (buf1);
    }
  } else if (strncmp (line, "<date>", 6) == 0) {
    if ((ptr = strstr (line + 6, "</date>")) == NULL) {
      AMFEL("Bad format: did not find terminating </date> at line %d\n",
	      amfin_lineno);
      return NULL;
    }
    *ptr = '\0';
    if (!amf_no_param_flag && amf_out_mode && (line[6] == '{') && (*(ptr - 1) == '}')) {
      *(ptr - 1) = '\0';
      strcpy (buf1, line + 6);
      MARK_SEGMENT (AMF0_DATE, buf1 + 1);
    } else {
      strcpy (buf1, line + 6);
      COPY_DATE_TYPE (buf1, AMF3_COPY_MARKER);
    }
  } else if (strcmp (line, "<object>") == 0) {
    return (amf0_read_object (out, len));
  } else if (strncmp (line, "<ecma_array", 11) == 0) {
    if(get_attribute_value(line, "size", buf1) == AMF3_ERROR) {
      AMFEL("Bad format: expecting <ecma_array size=xxx> at line %d\n",
	      amfin_lineno);
      return NULL;
    }
    return (amf0_read_ecma_array (out, len, buf1));
  } else if (sscanf (line, "<typed_object classname=\"%s\">", buf1)) {
    if ((buf1[strlen (buf1) - 1] != '>') && (buf1[strlen (buf1) - 2] != '"')) {
      AMFEL("Bad format: expecting <cobj name=xxx> at line %d\n",
	      amfin_lineno);
      return NULL;
    }
    buf1[strlen (buf1) - 2] = '\0';
    return (amf0_read_typed_object (out, len, buf1));
  } else if (strncmp (line, "<array", 6) == 0) {
    if(get_attribute_value(line, "size", buf1) == AMF3_ERROR) {
      AMFEL("Bad format: expecting <array size=xxx> at line %d\n",
	      amfin_lineno);
      return NULL;
    }
    return (amf0_read_array (out, len, buf1));
  } else {
    AMFEL("Bad format: unexpected input(%s) at line %d\n", line,
	    amfin_lineno);
    return NULL;
  }

  return out;
}

#if 0
static int extract_amf_data_value(buf1, "</integer>", AMF3_INTEGER, amf_out_mode, )
{

    if ((ptr = strstr (buf1, "</integer>")) == NULL) {
      fprintf (stderr, "Bad format: did not find terminating </integer> at line %d\n",
	      amfin_lineno);
      return -1;
    }
    *ptr = '\0';
    if (amf_out_mode && (buf1[0] == '{') && (*(ptr - 1) == '}')) {
      *(ptr - 1) = '\0';
      MARK_SEGMENT (AMF3_INTEGER, buf1 + 1);
    } else {
      copy_from[0] = AMF3_INTEGER;
      COPY_BYTES (1);

      switch(type)
      {
        case AMF3_INTEGER:
          COPY_INTEGER_TYPE_AMF3 (buf1);
          break;
      }
    }

  return 0;
}
#endif

//Used to pack 2 hex char in one byte

unsigned char htob (const char *ptr)
{
  unsigned char value = 0;
  char ch = *ptr;
  int i;

  AMFDL1("Method Called.");
  //for reading 2 characters from hex string
  for (i = 0; i < 2; i++)
  {
    if (ch >= '0' && ch <= '9')
      value = (value << 4) | (ch - '0');
    else if (ch >= 'A' && ch <= 'F')
      value = (value << 4) | (ch - 'A' + 10);
    else if (ch >= 'a' && ch <= 'f')
      value = (value << 4) | (ch - 'a' + 10);
    else
    {
      AMFDL4("ch=(0x%x) value(0x%02x)", ch, value);
      return value;
    }

    AMFDL4("ch=(0x%x) value(0x%02x)", ch, value);
    ch = *(++ptr);
  }
  AMFDL2("value=(0x%02x)", value);
  return value;
}

//Used to convert hex stream in binary
//Input: hstr will contain buffer in hex
//Output: out will get contain binary stream

char* convert_hex_to_bin(char *hstr, char *out, int *len)
{
  AMFDL1("Method Called.");  
  int cavhex_len=0;
  while ((strncmp("</cavhex>", hstr, 9)) && (*hstr != '\0'))
  {
    copy_from[0] = htob(hstr);
    AMFDL3("value=(0x%02x)", copy_from[0]);
    hstr = hstr + 2;
    COPY_BYTES(1);
    cavhex_len++;
  }
  AMFDL1("cavhex bytes=%d", cavhex_len);
  return out;
}

char *
amf3_read_data (char *out, int *len, short is_dyn_member, short is_array_key, short is_externalized)
{
  amf3_simple_datatypes  simple_dt;
  amf3_integer integer;
  amf3_integer dbl;
  amf3_string str;
  amf3_date dt;
  amf3_byte_array byte_arr;
  char *ptr;

  AMFDL1("Method Called out=[%s], len=[%d]  is_dyn_member=[%d], is_array_key=[%d]",
	                                                     out, *len, is_dyn_member, is_array_key, is_externalized);
  // In case of dynamic member or associative array member, line is already read
  if(!(is_dyn_member || is_array_key || is_externalized))
     GET_LINE;

  //Expect any data type - complete reading simple data type such as <string, <null/>, <bool, cur_state dont change
  //Read more for complex data data type - <*objects, <array,  cur state depends upon object
  if (strncmp (line, "<null", 5) == 0)
  {
    AMFDL1("Null data type received");
    if((out = amf3_extract_simple_datatypes(out, len, line, "null", 
                   &simple_dt, is_dyn_member, is_array_key, is_externalized, AMF3_NULL)) == NULL)
    {
      AMFDL3("Error reading Null Undefined (line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 Null successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<undefined", 10) == 0)
  {
   AMFDL1("Undefined data type received");
   if((out = amf3_extract_simple_datatypes(out, len, line, "undefined", 
                   &simple_dt, is_dyn_member, is_array_key,is_externalized, AMF3_UNDEFINED)) == NULL)
    {
      AMFDL3("Error reading AMF3 Undefined (line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 Undefined successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<true", 5) == 0)
  {
   AMFDL1("True data type received");
   if((out = amf3_extract_simple_datatypes(out, len, line, "true", 
                   &simple_dt, is_dyn_member, is_array_key, is_externalized, AMF3_TRUE)) == NULL)
    {
      AMFDL3("Error reading AMF3 True (line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 True successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<false", 6) == 0)
  {
   AMFDL1("False data type received");
   if((out = amf3_extract_simple_datatypes(out, len, line, "false", 
                    &simple_dt, is_dyn_member, is_array_key, is_externalized, AMF3_FALSE)) == NULL)
    {
      AMFDL1("Error reading AMF3 False (line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 False successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<integer", 8) == 0)
  {
   AMFDL1("Integer data type received");
   if((out = amf3_extract_integer_double(out, len, line, "integer", &integer,
                                 is_dyn_member, is_array_key, is_externalized, AMF3_INTEGER)) == NULL)
    {
      AMFDL3("Error reading AMF3 integer (line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 integer successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<double", 7) == 0)
  {
    AMFDL1("Double data type received");
    if((out = amf3_extract_integer_double(out, len, line, "double", &dbl,
                                  is_dyn_member, is_array_key, is_externalized, AMF3_DOUBLE)) == NULL)
    {
      AMFDL3("Error reading AMF3 double(line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 double successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<string", 7) == 0)
  {
    AMFDL1("String data type received");
    if((out = amf3_extract_string(out, len, line, "string", &str, 
                                  is_dyn_member, is_array_key, is_externalized, AMF3_STRING)) == NULL)
    {
      AMFDL3("Error reading AMF3 string(line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 string successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<date", 5) == 0)
  {
    AMFDL1("Date data type received");
    if((out = amf3_extract_date(out, len, line, "date", &dt, 
                                  is_dyn_member, is_array_key, is_externalized, AMF3_DATE)) == NULL)
    {
      AMFDL3("Error reading AMF3 date(line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 date successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<byte_array", 11) == 0)
  {
     AMFDL1("Byte Array received");
     if((out = amf3_extract_byte_array(out, len, line, "byte_array", &byte_arr, 
                                  is_dyn_member, is_array_key, is_externalized, AMF3_BYTE_ARRAY)) == NULL)
     {
      AMFDL3("Error reading AMF3 byte_array(line=%s)", line);
      return NULL;
     }
     AMFDL3("AMF3 byte_array successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<array", 6) == 0)
  {
    AMFDL1("Array data type received");
    if((out = amf3_read_array (out, len, is_dyn_member, is_array_key, is_externalized)) == NULL)
    {
      AMFDL3("Error reading AMF3 array type(line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 Array successfully encoded(line=%s)", line);
  }
  else if (strncmp (line, "<object", 7) == 0)
  {
    AMFDL1("Object data type received");
    if((out = amf3_read_object (out, len, is_dyn_member, is_array_key, is_externalized)) == NULL)
    {
      AMFDL3("Error reading AMF3 object type(line=%s)", line);
      return NULL;
    }
    AMFDL3("AMF3 Object successfully encoded(line=%s)", line);
  }
  //Test with <cavhex></cavhex>
  //Test if closing tag of </cavhex> not found
  else if (strncmp (line, "<cavhex>", 8) == 0) 
  {
     AMFDL1("cavhex found at line=%s", line);
     ptr = line + 8;
     if((out = convert_hex_to_bin(ptr, out, len)) == NULL)
     {
       AMFDL3("Error reading cavhex (line=%s)", line);
       return NULL;
     }
 
     //Reading & ignoring remaining closing tags after cavhex in xml file
     while (1)
        GET_LINE;
  }
  else
  {
    AMFEL("Bad format: unexpected input(%s) at line %d\n", line, amfin_lineno);
    return NULL;
  }
  return out;
}


//Reads AMF message from infp and writes binary into out
static char *
read_body (char *out, int *len)
{
  char attr_value[MAX_VAL] ;
  // For Multiple bodies Making, initializing amf_data_version = 0 as all bodies starts with AMF 0 array
  amf_data_version = AMF_VERSION0;   
  GET_LINE;

  // Expect <body targetmethod=xxx response=yyy>
  //ptr = strtok (line, "\"");
  if (strncmp (line, "<body ", 6)) {
    AMFEL("Bad format: expecting <body at line %d\n", amfin_lineno);
    return NULL;
  }

  if(get_attribute_value(line, "target_method", attr_value) == AMF3_ERROR) {
     AMFEL("Bad format: expecting <body target_method=... at line %d\n",
       amfin_lineno);
     return NULL;
  }
  COPY_STRING_TYPE (attr_value, AMF3_DO_NOT_COPY_MARKER);

  if(get_attribute_value(line , "response", attr_value) == AMF3_ERROR) {
     AMFEL("Bad format: expecting <body target_method=... response=... at line %d\n",
       amfin_lineno);
     return NULL;
  }
  COPY_STRING_TYPE (attr_value, AMF3_DO_NOT_COPY_MARKER);

  // Fill len with -1 as we cannot determine length due to paramterization
  copy_from[0] = 0xFF; copy_from[1] = 0xFF; copy_from[2] = 0xFF; copy_from[3] = 0xFF;
  COPY_BYTES (4);

  AMFDL1("Starting data version=%d\n", amf_data_version);

  if (amf_data_version == AMF_VERSION3) {
    out = amf3_read_data (out, len, AMF3_NOT_DYNAMIC_MEMBER, AMF3_NOT_ARRAY_KEY, AMF3_NOT_EXTERNAZIABLE);
  } else {
    out = read_data_amf0 (out, len);
  }

  if (out == NULL) {
    AMFDL1("Bad format: data read failed at line %d\n", amfin_lineno);
    return NULL;
  }
  GET_LINE;
  if (strcmp (line, "</body>")) {
    AMFEL("Bad format: expecting </body> at line %d\n", amfin_lineno);
    return NULL;
  }
  return out;
}

//Reads AMF xml message from infp and writes binary into out
//src_type = 0 for input from file and 1 from memory buffer
//For File input: src_ptr is FILE *
//src_parametr is the starting linenumber of the open file
//For memory input, src_ptr is the char * and src_parametre is the taotal input buffer bytes.
//out_mode = 0 for standard AMF out and 1 is for segmented out.
//Segmented output with parametrization can be used with 
//  for all data values only
//noparam_flag  - 1 -> Do not paramterize data values 
//
//Segmented format:  2 byte number of segments, than 2 byte segment length before every segment
//Seg len and num segs are in host byte order

char *
amf_encode (int src_type, void *src_ptr, int src_parameter, char *out, int *len,
	  int out_mode, int noparam_flag, int *version)
{
  unsigned short num_bodies = 0, num_headers=0;
  int i;
  char *num_segs_ptr;		//ptr in out for num_segs
  char attr_value[MAX_VAL];

  amf_seg_count = 0;
  if (src_type == 0) {		//File input
    amf_infp = (FILE *) src_ptr;
    amfin_lineno = src_parameter;
  } else {			//Memory input
    amf_infp = (FILE *) NULL;
    amfin_ptr = (char *) src_ptr;
    amfin_left = src_parameter;
    amfin_lineno = 0;
  }

  amf_out_mode = out_mode;
  amf_no_param_flag = noparam_flag;

  GET_LINE;

  if (strncmp (line, "<msg ", 5) != 0) {
    AMFEL("Bad format:  expecting <msg version=.. at line %d\n",
	    amfin_lineno);
    return NULL;
  }

  if (get_attribute_value(line, "version", attr_value) == AMF3_ERROR)
  {
    AMFEL("Bad format: expecting <msg version=.. at line %d\n",
	    amfin_lineno);
    return NULL;
  }

  // Set the version so that NetStorm knows what version is to be used
  // Note - AMF packet is always in version 0 which can switch to AMF3
  *version = atoi (attr_value);
  amf_pkt_version = *version; // Save in global variable
  //amf_data_version = AMF_VERSION0; // Start with 0

  if (get_attribute_value(line, "headers", attr_value) == AMF3_ERROR)
  {
    AMFEL("Bad format: expecting </msg version=..  headers=.. at line %d\n",
	    amfin_lineno);
    return NULL;
  }

  GET_LINE;
  if (strncmp (line, "<body_info", 10) != 0) {
    AMFEL("Bad format: expecting <body_info bodies=.. at line %d\n",
	    amfin_lineno);
    return NULL;
  }

  if(get_attribute_value(line, "bodies", attr_value) < 0) {
     AMFEL("Bad format: 2 expecting <body_info bodies=...> at line %d\n",
       amfin_lineno);
     return NULL;
  }
  num_bodies = atoi (attr_value);

  // For non amf parameterization we dont need to mark segment, as it is called after joining the segments
  if (amf_out_mode && (amf_no_param_flag != 2)) {
    num_segs_ptr = out;
    // Initialize num segs to 0
    COPY_SHORT ( 0x0000); 
    // placeholder for seg_length
    START_SEGMENT;
  }

  //Copy in output stream - copy_short (unsgined short, char *out, int *outlen);
  COPY_SHORT (amf_pkt_version);
  //Copy placeholder num headers
  COPY_SHORT (num_headers);
  //Copy num_bodies num headers
  COPY_SHORT (num_bodies);

  for (i = 0; i < num_bodies; i++) {
    if ((out = read_body (out, len)) == NULL) {
      AMFEL("Bad format: while reading body at line %d\n", amfin_lineno);
      return NULL;
    }
  }

  GET_LINE;
  if (strcmp (line, "</msg>")) {
    AMFEL("Bad format: expecting </msg> at line %d\n", amfin_lineno);
    return NULL;
  }

  if (amf_out_mode && (amf_no_param_flag != 2)) {
    if (last_seg_start == out) {
      //just seg_length is added no bytes in seg
      //remove last segs seg length
      *len += 2;
      out -= 2;
    } else {
      END_SEGMENT();
    }
    bcopy ((char *) &amf_seg_count, num_segs_ptr, 2);
  }
  
  return out;
}


//Used to validate the value passed as a parameter, and set default value
//in case value is invalid or NULL
//Handling values of all basic datatypes - Integer, Dowuble, String, Date, Boolean(AMF0)
static int  amf_val_param_set_default(char *dest, char *src, int *vlen, char type)
{
  AMFDL1("Method called. src = %s, len = %d, dest = [%s]", src, *vlen, dest);

  switch(type)
  {
    default:
    if(src == NULL)
    {
      AMFDL1("Parameter value pointer is NULL");
      //strcpy(dest, src);
      *vlen = 0;
      return -1; //To denote parameter value is empty
    }
    else
    {
      AMFDL1("Copying paramter_value to value");
      // Since paramter value passed is not NOT terminated, we need to
      // local buf and then NULL terminated as methods called from here
      // assume that value is NULL terminated
      memcpy(dest, src, *vlen);
      dest[*vlen] = '\0';
    }
  }
  AMFDL1("src = %s, len = %d, dest = [%s]", src, *vlen, dest);
  return 0;
}


// TODO: Change this as per String max value later. See amf.h MAX_VAL * 4
#define MAX_PARAMETER_VAL  81920

char *
amf3_encode_value (struct iovec *vector, int idx, char *param_value, int vlen)
{

  char type;
  char *strt_ptr = NULL; // Initlally strt_ptr is same as out (Because on every copy we move out with size)
  int strt_len;          // Initially strt_len is same as len (because on every copy we reduce *len)
  char *ptr, *out = NULL;
  int tmp_len = 0;
  int *len = &tmp_len; // Since we use macros, we need to use *len
  char value[MAX_PARAMETER_VAL + 1]; // This is  used to copy value as value is not NULL terminated
  value[0] = '\0';

  ptr = NULL;
  vector[idx].iov_base = NULL;
  vector[idx].iov_len = 0;

  AMFDL1("Method called. Parameter value=[%s], len = %d", param_value, vlen);

  if((vlen > MAX_PARAMETER_VAL) || (idx < 1))
  {
    if(vlen > MAX_PARAMETER_VAL) 
    {
      AMFEL("Error: Length (%d) of variable is more than max (%d) allowed", vlen, MAX_PARAMETER_VAL);
    }
    else if (idx < 1)
    {
      AMFEL("Bad segment number %d\n", idx);
    }

    strt_ptr = malloc (1);
    if (strt_ptr) 
    {
      *strt_ptr = AMF3_NULL;
      vector[idx].iov_base = strt_ptr;
      vector[idx].iov_len = 1;
    } 
    else 
    {
      AMFEL("%s(): Unable to allocate space for AMF3 at line %d\n",
	       (char *) __FUNCTION__, __LINE__);
    }
    return NULL;
  }

  ptr = (char *) vector[idx - 1].iov_base;
  type = ptr[vector[idx - 1].iov_len - 1];
  //fprintf ("type is 0x%02x\n", (unsigned char)type);
  //type = vector[i-1].iov_base[vector[i-1].iov_len-1];

  AMFDL1("Parameter AMF3 Type=[0x%X]", type);
  
  short param_exists = amf_val_param_set_default(value, param_value, &vlen, type);

  /* Notes:
    strt_len should be length of serialized data value in AMF format
    out is malloced buffer
    strt_ptr should point to buffer where data is serialized
    *len should have total size of buffer which gets reduced as we copy data
  */
  switch (type) {
   case AMF3_INTEGER:
     // strt_len = *len = amf3_get_integer_len(atoi(value));
     // AMFDL1("Integer Parameter value=[%s], number of bytes needed for encoding = [%d]", value, *len);
     // if (*len == -1) 
     // {
       // AMFEL("Size of integer is not correct. Length is coming to %d", *len);
       // return NULL;
     // }

     *len = 4; // Always allocate for 4 bytes
     strt_ptr = out = malloc(*len);
     if(out == NULL) 
     {
       AMFEL("%s(): Unable to allocate space for AMF3_INTEGER at line %d\n",
	       (char *) __FUNCTION__, __LINE__);
       return NULL;
     }
    COPY_INTEGER_TYPE_AMF3(value);
    strt_len = 4 - *len; // Number of byets used for encoding will be 4 - bytes left
    break;
  case AMF3_DOUBLE:
    strt_len = *len = 8;
    AMFDL1("Double Parameter value=[%s], number of bytes needed for encoding = [%d]", value, *len);
    strt_ptr = out = malloc(*len);
    if(out == NULL) {
      AMFEL("%s(): Unable to allocate space for AMF3_DOUBLE at line %d\n",
	       (char *) __FUNCTION__, __LINE__);
      return NULL;
    }
    COPY_NUMBER_TYPE (value, AMF3_DO_NOT_COPY_MARKER);
    break;
  case AMF3_STRING:
    *len = 4; // Always allocate for 4 bytes for string len
    *len += vlen;
    strt_ptr = out = malloc(*len);
    // First get how many bytes will be requires for keeping string lenght in AMF3 data
    /*len = amf3_get_integer_len(vlen); 
    AMFDL1("String Parameter value=[%s], len = %d, number of bytes needed for encoding length of string = [%d]", value, vlen, *len);
    if (*len == -1) 
    {
       AMFEL("Size of integer is not correct. Length is coming to %d", *len);
       return NULL;
    } */

    //strt_len = *len; // Save in strt_len
    AMFDL1("String Parameter value=[%s], number of bytes needed for encoding length and value of string=[%d]", value, *len);
    //strt_ptr = out = malloc(*len);

    if(out == NULL) {
      AMFEL("%s(): Unable to allocate space for AMF3_STRING at line %d\n",
	       (char *) __FUNCTION__, __LINE__);
      return NULL;
    }
    strt_len = *len; // Save in strt_len
    //Safety check in case parameter value is blank
    if(param_exists)
    {
      copy_from[0] = EMPTY_STRING;
      COPY_BYTES(1);
      strt_len = 1; // Number of byets used for encoding will be 1 for empty string
    }
    else
    {
      COPY_STRING_TYPE (value, AMF3_DO_NOT_COPY_MARKER); // Marker is already filled in MARK_SEGMENT
      strt_len = (4 + vlen)- *len; // Number of byets used for encoding will be 4 - bytes left
    }
 
    break;
 case AMF3_DATE:
    strt_len = *len = 8 + 1;  // +1 for value indicator 
    AMFDL1("Date Parameter value=[%s], number of bytes needed for encoding = [%d]", value, *len);
    strt_ptr = out = malloc(*len);
    if(out == NULL) {
      AMFEL("%s(): Unable to allocate space for AMF3_DATE at line %d\n",
	       (char *) __FUNCTION__, __LINE__);
      return NULL;
    }
    // Value indicator
    copy_from[0] = 1;
    COPY_BYTES(1);

    COPY_DATE_TYPE (value, AMF3_DO_NOT_COPY_MARKER);
    break;

  default:
    AMFEL("Unsupported AMF3 type 0x%02X\n", (unsigned char) type);
    // This code will not work as we already have marker. TODO
    out = malloc (1);
    if (out) {
      *out = AMF3_NULL;
      vector[idx].iov_base = out;
      vector[idx].iov_len = 1;
    } else {
      AMFEL("%s(): Unable to allocate space for AMF3 at line %d\n",
	       (char *) __FUNCTION__, __LINE__);
      return NULL;
    }
  }
  
  //DL *p and len;
  vector[idx].iov_base = strt_ptr;
  vector[idx].iov_len = strt_len;

  return out;
}
void
amf0_encode_value (struct iovec *vector, int idx, char *param_value, int vlen)
{
  char type;
  char *ptr, *vptr;
  int size, slen, i;
  short nwshort, snum;
  double num;
  char str[128];
  struct tm tm;
  time_t t;
  double dt;
  char value[MAX_PARAMETER_VAL + 1]; // This is  used to copy value as value is not NULL terminated

  //fprintf ("Encoding %s\n", value);
  if (idx < 1) {
    AMFEL("bad segment number %d\n", idx);
    ptr = malloc (1);
    *ptr = AMF0_NULL;		//NULL
    vector[idx].iov_base = ptr;
    vector[idx].iov_len = 1;
    return;
  }

  ptr = (char *) vector[idx - 1].iov_base;
  type = ptr[vector[idx - 1].iov_len - 1];
  //fprintf ("type is 0x%02x\n", (unsigned char)type);
  //type = vector[i-1].iov_base[vector[i-1].iov_len-1];

  amf_val_param_set_default(value, param_value, &vlen, type);

  if (type == AMF0_NUMBER) {	//Number
    size = 8;
    vptr = malloc (size);
    slen = (vlen < 128) ? vlen : 127;
    bcopy (value, str, slen);
    str[slen] = '\0';
    //fprintf ("number var val is %s\n", str);
    num = atof (str);
    ptr = (char *) &num;

    for (i = 0; i < 8; i++) {
      //For our machine host byte order is little endian
      vptr[i] = ptr[7 - i];
    }
  } else if (type == AMF0_REFERENCE) {	//Number
    size = 2;
    vptr = malloc (size);
    slen = (vlen < 128) ? vlen : 127;
    bcopy (value, str, slen);
    str[slen] = '\0';
    //fprintf ("number var val is %s\n", str);
    snum = (short) atoi (str);
    ptr = (char *) &snum;

    for (i = 0; i < 2; i++) {
      //For our machine host byte order is little endian
      vptr[i] = ptr[1 - i];
    }
  } else if (type == AMF0_BOOLEAN) {	//Bool
    size = 1;
    vptr = malloc (size);
    slen = (vlen < 128) ? vlen : 127;
    bcopy (value, str, vlen);
    str[slen] = '\0';
    if (atoi (str) == 0)
      vptr[0] = 0x00;
    else
      vptr[0] = 0x01;
  } else if (type == AMF0_STRING) {	//UTF string
    if (strncmp (value, "cavhex:", 7)) {
      size = 2 + vlen;
      vptr = malloc (size);
      nwshort = htons (vlen);
      bcopy ((char *) &nwshort, vptr, 2);
      bcopy (value, vptr + 2, vlen);
    } else {
      int i, slen;
      char byte_buf[3], *val_ptr;

      slen = (vlen - 7) / 2;
      size = 2 + slen;
      vptr = malloc (size);
      nwshort = htons (slen);
      bcopy ((char *) &nwshort, vptr, 2);
      byte_buf[2] = '\0';
      val_ptr = value + 7;
      for (i = 0; i < slen; i++) {
	bcopy (val_ptr, byte_buf, 2);
	val_ptr += 2;
	*(vptr + 2 + i) = (unsigned char) strtol (byte_buf, NULL, 16);
      }
    }
  } else if (type == AMF0_DATE) {	//Date
    char *ptr, *ms_ptr, *tz_ptr;
    short tz_value = 0, nw_tzvalue = 0;
    int ms_value = 0;
    u_ns_8B_t tot_ms;

    size = 10;
    vptr = malloc (size);
    slen = (vlen < 128) ? vlen : 127;
    bcopy (value, str, vlen);
    str[slen] = '\0';

    //Date format fixed for now yyyy-mm-dd-HH:MM:SS.ms+TZ
    ms_ptr = index (str, '.');
    if (ms_ptr) {
      *ms_ptr = '\0';
      ms_ptr++;
      tz_ptr = index (ms_ptr, '+');
      if (tz_ptr) {
	*tz_ptr = '\0';
	tz_ptr++;
	tz_value = (short) atoi (tz_ptr);
	nw_tzvalue = htons (tz_value);
      }
      ms_value = atoi (ms_ptr);
    }
    //fprintf ("ms =%d str=%s\n", ms_value, str);
    //Date format fixed for now yyyy-mm-dd-HH:MM-SS.ms+TZ
    strptime (str, "%F-%T", &tm);
    //fprintf ("y=%d m=%d d=%d h=%d m=%d s=%d\n", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    //tm.tm_sec =0; tm.tm_min=0; tm.tm_hour=0;
    t = mktime (&tm);
    tot_ms = t;
    tot_ms *= 1000;		//make ms since 1/1/1970
    tot_ms += ms_value;
    dt = (double) tot_ms;	//make ms since 1/1/1970

    ptr = (char *) &dt;

    for (i = 0; i < 8; i++) {
      //For our machine host byte order is little endian
      vptr[i] = ptr[7 - i];
    }

    bcopy ((char *) &nw_tzvalue, &vptr[8], 2);
    //fprintf ("Sent Date is: ");
    //for (i =0; i < 10; i++) {
    //    fprintf ("%02x ", (unsigned char) vptr[i]);
    //}
    //fprintf ("\n");
    //vptr[8] = 0x00; vptr[9] = 0x00;
  } else {
    AMFEL("Unsupported AMF type %02X\n", (unsigned char) type);
    size = 1;
    vptr = malloc (size);
    *vptr = AMF0_NULL;		//NULL
  }
  vector[idx].iov_base = vptr;
  vector[idx].iov_len = size;
  //fprintf ("iov_len is %d idx=%d vector=0x%x iov_len_add=0x%x\n", vector[idx].iov_len, idx, vector, &(vector[idx].iov_len));
}

void
amf_encode_value (struct iovec *vector, int idx, char *value, int vlen,
		  int version)
{

  amf_pkt_version = version;
  amf_data_version = version; // Must set data version also to same version

  if (version == AMF_VERSION0) {
    amf0_encode_value (vector, idx, value, vlen);
  } else if (version == AMF_VERSION3) {
    amf3_encode_value (vector, idx, value, vlen);
  } else {
    AMFEL("AMF version (%d) not supported.\n", version);
  }
}

