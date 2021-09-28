/*-----------------------------------------------------------------------------
  Name: ns_amf_decode.c
  Auther: Neeraj Jain
  Purpose: To decode AMF binary data and create XML file
           Supports both AMF0 and AMF3 versions

  Note: Any change in this code may also need change in Java Code
-------------------------------------------------------------------------------*/


#include <errno.h>
#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_log.h"
#include "amf.h"
#include "util.h"


// Append atrributes to XML or not
#define APPEND_TO_XML_NO       0
#define APPEND_TO_XML_YES      1

static unsigned int data_len, is_string;
// xmlBuf is used to store object attributes
static char xmlBuf[1024] = "";

static char *write_data_amf0 (int indent, char *in, int *len);


// Stream method


static int body_len, body_left;
static int available(int left)
{
  AMFDL1("Method called. Byte left is %d", left);
  return left;
}



// For all AMF Versions
#if 0
//GET num bytes from 'in' buffer to 'copy_to' buffer
#define GET_BYTES(num)  if (num >= 8192) {\
	    		    AMFEL("Getting more than 8192 bytes\n");\
	    		    return NULL;\
			 }\
			 if (*len < num) {\
	    		    AMFEL("Not enough data in input data buffer to read  message, len = [%d], num = [%d]\n", *len, num);\
	    		    return NULL;\
			   }\
			   bcopy (in, copy_to, num);\
			   in += num;\
			   *len -= num;
#endif

#define GET_SHORT(data) if ((in = get_short(data, in, len)) == NULL) {\
				AMFEL("ERROR: failed to get short. still left %d bytes\n", *len);\
				return NULL;\
			}

#define GET_INT(data) if ((in = get_int(data, in, len)) == NULL) {\
				AMFEL("ERROR: failed to get int. still left %d bytes\n", *len);\
				return NULL;\
			}

// AMF0 (same as AMF3 double)
#define GET_NUMBER(data) if ((in = get_number(data, in, len)) == NULL) {\
				AMFEL("ERROR: failed to get number. still left %d bytes\n", *len);\
				return NULL;\
			}

// AMF3
#define GET_INTEGER_AMF3(data) if ((in = get_integer_amf3(data, in, len)) == NULL) {\
        AMFEL("ERROR: failed to get number. still left %d bytes\n", *len);\
        return NULL;\
      }

// AMF3 (double)
#define GET_DOUBLE(data) if ((in = get_number(data, in, len)) == NULL) {\
        AMFEL("ERROR: failed to get number. still left %d bytes\n", *len);\
        return NULL;\
      }


// AMF0
#define GET_DATE(data) if ((in = get_date(data, in, len)) == NULL) {\
        AMFEL("ERROR: failed to get date. still left %d bytes\n", *len);\
        return NULL;\
      }

// AMF3
#define GET_DATE_AMF3(data) if ((in = get_date_amf3(data, in, len)) == NULL) {\
        AMFEL("ERROR: failed to get date amf3. still left %d bytes\n", *len);\
        append_to_xml (">%s</date>\n", buf);\
        return NULL;\
      }


// AMF0
#define GET_BOOL(data) if ((in = get_bool(data, in, len)) == NULL) {\
				AMFEL("ERROR: failed to get bool. still left %d bytes\n", *len);\
				return NULL;\
			}

// AMF0
#define GET_STRING(data) if ((in = get_string(data, in, len)) == NULL) {\
  AMFEL("ERROR: failed to get string. still left %d bytes\n", *len);\
	return NULL;\
}

#define GET_LONG_STRING(data) if ((in = get_long_string(data, in, len)) == NULL) {\
  AMFEL("ERROR: failed to get Long string. still left %d bytes\n", *len);\
	return NULL;\
}

// AMF3
#define GET_STRING_AMF3(data, indent, appendXml, attrRef, attrName) if ((in = get_string_amf3(data, in, len, indent, appendXml, attrRef, attrName)) == NULL) {\
    AMFEL("ERROR: failed to get string. still left %d bytes\n", *len);\
    return NULL;\
  }


// AMF3

#define GET_ARRAY_AMF3(data, indent) if ((in = get_array_amf3(data, in, len, indent)) == NULL) {\
    AMFEL("ERROR: failed to get amf3 array. still left %d bytes\n", *len);\
    append_to_xml ("%s</array>\n", leading_blanks (indent)); \
    return NULL;\
  }


/* Utility methods */

char *
leading_blanks (int num)
{
static char blanks[4096];
int i;

  if (num > 256)
    num = 255;

  for (i = 0; i < num; i++)
    blanks[i] = ' ';

  blanks[num] = '\0';

  return blanks;
}


void
append_to_xml (char *format, ...)
{
#define MAX_AMF_DATA_VAL STR_VAL + 8192 // This must be bigger than max string lenght supported

// TODO - Strings can be multi-line and can be more than 4096 length

  va_list ap;
  int amt_written = 0;
  char buffer[MAX_AMF_DATA_VAL + 1];

  va_start (ap, format);
  amt_written = vsnprintf (buffer, MAX_AMF_DATA_VAL, format, ap);
  va_end (ap);


  // In some cases, vsnprintf return -1 but data is copied in buffer
  // This is a quick fix to handle this. need to find the root cause
  //  see ns_log.c for similar code
  if(amt_written < 0)
  {
    AMFEL("Warning: vsnprintf returned error (%d). errno = %d\n", amt_written, errno);
    amt_written = strlen(buffer);
  }

  if(amt_written > MAX_AMF_DATA_VAL)
  {
    AMFEL("Warning: XML line length (%d) is more than maximum buffer length (%d)\n", amt_written, MAX_AMF_DATA_VAL);
    amt_written = MAX_AMF_DATA_VAL;
  }


  buffer[MAX_AMF_DATA_VAL] = 0;

  //outfp, max_mlen amd out_buf are global vars
  if (outfp) {
    fprintf (outfp, "%s", buffer);
  } else {
    if (amt_written > (max_mlen - 1 - mlen))
    {
      AMFEL("Error: No space left in XML buffer. XML buffer length = %d, space left = %d,  current line length = %d\n", max_mlen, amt_written, (max_mlen - 1 - mlen));
      amt_written = max_mlen - 1 - mlen;
    }
    bcopy (buffer, out_buf + mlen, amt_written);
    mlen += amt_written;
    out_buf[mlen] = '\0';
  }
}


// AMF3 Reference Tables

#define MAX_CLASSNAME_LEN 4096


//MAX_STRING_LEN
typedef struct
{
  char type; // AMF Data type
  char data_val[STR_VAL + 1]; // To identify for what key this reference is
  int  data_len;
  //Shilpa: For Storing String data_val should be MAX_STRING_LEN + 1
} StoredObjTbl;


typedef struct
{
  char classname[MAX_CLASSNAME_LEN + 1];
  char dynamic;
  char externalizable;
  char encoding;
  int propertiesCount;
  // char *traits;
} StoredClassDescriptors;

#define DELTA_REF_TBL_ENTRIES 5



//This method to create table entry
//On success row num contains the newly created row-index of table
static int amf3_create_table_entry(int *row_num, int *total, int *max, char **ptr, int size, char *name)
{
  if (*total == *max)
  {
    AMF_MY_REALLOC(*ptr, (*max + DELTA_REF_TBL_ENTRIES) * size, name, -1);
    *max += DELTA_REF_TBL_ENTRIES;
  }
  *row_num = (*total)++;
     //NSDL(NULL, NULL, DM_EXECUTION, MM_MISC, "row_num = %d, total = %d, max = %d, ptr = %x, size = %d, name = %s", *row_num, *total, *max, (int)&ptr, size, name);
  return 0;
}

static void init_stored_obj_tbl(char type, StoredObjTbl *obj)
{
  memset(obj, 0, sizeof(StoredObjTbl));
  obj->type = type;
}

static int set_data_val_in_obj_tbl(StoredObjTbl *obj, char *val, int len)
{
  AMFDL1("Method Called. len=%d", len);
  // TODO - Check len for overflow
  memcpy(obj->data_val, val, len);
  obj->data_val[len] = '\0';
  obj->data_len = len;
  AMFDL1("Exitting Method.");
  return 0;
}

static int get_data_val_from_obj_tbl(StoredObjTbl *obj, char *val, int len)
{
  // TODO - Check len for overflow
  int data_len;
  AMFDL1("Method Called. data_len=%d", obj->data_len);
  data_len = obj->data_len;
  memcpy(val, obj->data_val, data_len);
  val[data_len] = '\0';
  AMFDL1("Method Called. ");

  return data_len;
}



// Table 1 - Strings Reference Tables

static int totalStoredStrings = 0;  // total storedStrings entries
static int maxStoredStrings = 0;    // max storedStrings entries
static StoredObjTbl *storedStrings = NULL;

// Add in reference table and return reference id
static int addToStoredStrings(StoredObjTbl *obj)
{
int row_num;


  AMFDL1("Method called. Type = 0x%x. obj len=%d", obj->type, obj->data_len); 
  if(amf3_create_table_entry(&row_num, &totalStoredStrings, &maxStoredStrings, (char **)&storedStrings, sizeof(StoredObjTbl), "StoredStrings table") == -1)
  {
    AMFEL("Could not create table entry for StoredStrings Table\n");
    exit(-1);
  }

  memcpy((storedStrings + row_num), obj, sizeof(StoredObjTbl));

//  AMFDL1("Type = 0x%x. data_val = %s, row_num = %d", obj->type, obj->data_val, row_num);
  AMFDL1("Type = 0x%x. row_num = %d", obj->type, row_num);

  return row_num;
}

static StoredObjTbl *getFromStoredStrings(int ref_id)
{
StoredObjTbl *obj;

  if(ref_id >= totalStoredStrings || ref_id < 0)
  {
    AMFEL("Error: Reference Id (%d) of stored strings is invalid. Total stored strings = %d\n", ref_id, totalStoredStrings);
    return NULL;
  }
  obj = storedStrings + ref_id;

  AMFDL1("Method called for type = 0x%x. data_val = %s, ref_id = %d", obj->type, obj->data_val, ref_id);

  return obj;
}


// Table 2 - Complex Objects (Objects, types Objects, Arrays, Dates, Xml, XmlDoc, ByteArrays)

static int totalStoredObjects = 0;  // total storedObjects entries
static int maxStoredObjects = 0;    // max storedObjects entries
static StoredObjTbl *storedObjects = NULL;


// Add in reference table and return reference id
static int addToStoredObjects(StoredObjTbl *obj)
{
int row_num;

  if(amf3_create_table_entry(&row_num, &totalStoredObjects, &maxStoredObjects, (char **)&storedObjects, sizeof(StoredObjTbl), "StoredStrings table") == -1)
  {
    AMFEL("Could not create table entry for StoredStrings Table\n");
    exit(-1);
  }
  memcpy((storedObjects + row_num), obj, sizeof(StoredObjTbl));

  AMFDL1("Method called for type = 0x%x. data_val = %s, row_num = %d", obj->type, obj->data_val, row_num);

  return row_num;
}

static StoredObjTbl *getFromStoredObjects(int ref_id)
{
StoredObjTbl *obj;

 if(ref_id >= totalStoredObjects || ref_id < 0)
  {
    AMFEL("Error: Reference Id (%d) of stored object is invalid. Total stored objects = %d\n", ref_id, totalStoredObjects);
    return NULL;
  }

  obj = storedObjects + ref_id;

  AMFDL1("Method called for type = 0x%x. data_val = %s, ref_id = %d", obj->type, obj->data_val, ref_id);

  return obj;

}


// Table 3 - Object traits

static int totalStoredClassDescriptors = 0;  // total storedClassDescriptors entries
static int maxStoredClassDescriptors = 0;    // max storedClassDescriptors entries
static StoredClassDescriptors *storedClassDescriptors = NULL;

// Add in reference table and return reference id
static int addToStoredClassDescriptors(StoredClassDescriptors *obj)
{
int row_num;

  if(amf3_create_table_entry(&row_num, &totalStoredClassDescriptors, &maxStoredClassDescriptors, (char **)&storedClassDescriptors, sizeof(StoredClassDescriptors), "StoredStrings table") == -1)
  {
    AMFEL("Could not create table entry for StoredClassDescriptors Table\n");
    exit(-1);
  }
  if(obj->encoding & AMF3_DYNAMIC_FLAG)
    obj->dynamic = 1;

  if(obj->encoding & AMF3_EXTERNALIZABLE_FLAG)
    obj->externalizable = 1;

  memcpy((storedClassDescriptors + row_num), obj, sizeof(StoredClassDescriptors));

  AMFDL1("Method called. classname = %s, row_num = %d", obj->classname, row_num);

  return row_num;
}

static StoredClassDescriptors *getFromStoredClassDescriptors(int ref_id)
{
StoredClassDescriptors *obj;


  if(ref_id >= totalStoredClassDescriptors || ref_id < 0)
  {
    AMFEL("Error: Reference Id (%d) of stored class decscriptor (traits) is invalid. Total stored objects = %d\n", ref_id, totalStoredClassDescriptors);
    return NULL;
  }

  obj = storedClassDescriptors + ref_id;

  AMFDL1("Method called. classname = %s, ref_id = %d", obj->classname, ref_id);

  return obj;

}

// End - AMF3 Reference tables


static char *
get_short (unsigned short *num, char *in, int *len)
{
  unsigned short s;
  GET_BYTES (2);
  bcopy (copy_to, (char *) &s, 2);

  *num = ntohs (s);

  AMFDL1("Value of short = %hd (network order = %hd)", *num, s);

  return in;
}

static char *
get_int (int *num, char *in, int *len)
{
  int s;
  GET_BYTES (4);

  bcopy (copy_to, (char *) &s, 4);

  *num = ntohl (s);

  AMFDL1("Value of int = %d (network order = %d)", *num, s);

  return in;
}

// AMF0 -> number
// AMF3 -> double
static char *
get_number (char *buf, char *in, int *len)
{
  double num;
  int i;
  char *ptr;

  GET_BYTES (8);

  ptr = (char *) &num;

  for (i = 0; i < 8; i++) {
    //For our machine host byte order is little endian
    ptr[i] = copy_to[7 - i];
  }

  sprintf (buf, "%f", num);
  if(strcmp(buf,"nan") == 0)
     strcpy(buf, "NAN"); 
  AMFDL1("Value of number (double) = %s", buf);

  return in;
}


// Only for AMF3
static char *
get_integer_amf3 (char *buf, char *in, int *len)
{
  int n = 0;
//char *ptr;
  unsigned int num = 0;
  unsigned char byte = 0;

  AMFDL1("Method called. in = %p, len = %d", in, *len);

  GET_BYTES (1);
  byte = copy_to[0];
  while (((byte & 0x80) != 0) && (n < 3)) 
  {
    num <<= 7;
    num |= (byte & 0x7f);
    AMFDL1("Integer byte[%d]=[%02X] num=%d", n, byte, num);
    GET_BYTES (1);
    byte = copy_to[0];
    n++;
  }

  if (n < 3) 
  {
    num <<= 7;
    num |= byte;
    AMFDL1("Integer byte[%d]=[%02X] num=%d", n, byte, num);
  } 
  else 
  {
    num <<= 8;
    //num <<= 7;
    num |= byte;
    AMFDL1("Integer byte[%d]=[%02X] num=%d", n, byte, num);
    if ((num & 0x10000000) != 0)
      num |= 0xe0000000;
  }

  sprintf (buf, "%d", num);
  AMFDL1("Value of integer = %s\n", buf);

  AMFDL1("Method end. in = %p, len = %d", in, *len);

  return in;
}


static char *
get_bool (char *buf, char *in, int *len)
{
  GET_BYTES (1);

  if (copy_to[0] == 0)
    strcpy (buf, "0");
  else
    strcpy (buf, "1");

  return in;
}



// AMF0 Date format is
//    DOUBLE time-zone
// Note - Time zone should be filled with 0x0000 (not used)

// This method is also used by AMF3 to read the date part
static char *
get_date (char *buf, char *in, int *len)
{
time_t time_t_var;
double date_double;
//int timeoffset;
int i, left_ms;
short timezone = 0;
char *ptr;
char extn[128];
long long num;

  AMFDL1("Method called. amf_data_version = %d", amf_data_version);

  //printf ("Rcd Date is: ");
  //for (i =0; i < 10; i++) {
  //    printf ("%02X ", (unsigned char) in[i]);
  //}
  //printf ("\n");

  // Read date part which is double (8 bytes)
  GET_BYTES(8);

  ptr = (char *)&date_double;

  for (i = 0; i < 8; i++) {
    //For our machine host byte order is little endian
    ptr[i] = copy_to[7-i];
  }

  // In AMF0, date is followed by timezone. Read it and discard
  if(amf_data_version == AMF_VERSION0)
    GET_SHORT((unsigned short *)&timezone);

#if 0
  if (timezone > 720 )
      timeoffset = - (65536 - timezone);

  date_double += (double) timeoffset;
#endif

  num = date_double;
  time_t_var = num / 1000;
  left_ms = num %1000;

  AMFDL1("date_double = %1.0f, num=%llu, time_t=%lu, left_ms=%d\n", date_double, num, (u_ns_ts_t)time_t_var, left_ms);


  //Date format fixed for now yyyy-mm-dd
  if (strftime (buf, 8192, "%F-%T", localtime(&time_t_var)) == 0) {
      AMFEL("Unable to process date with time=%d \n", (int)time_t_var);
      return NULL;
  }

  sprintf(extn, ".%3.3d", left_ms);
  strcat (buf, extn);

  if(amf_data_version == AMF_VERSION0)
  {
    sprintf(extn, "+%hd", timezone);
    strcat (buf, extn);
  }

  AMFDL1("Value of date = %s", buf);

  return in;
}


static char *
get_date_amf3(char *buf, char *in, int *len)
{
StoredObjTbl *result = NULL, result_buf;

  result = &result_buf;
  init_stored_obj_tbl(AMF3_DATE, result);

  // Get date reference id or length
  GET_INTEGER_AMF3(buf);
  int type = atoi(buf); 

  if((type & 0x01) == 0) // stored date (Reference)
  {
    int date_ref = type >> 1;
    result = getFromStoredObjects(date_ref);
    if(result == NULL) return NULL;
    get_data_val_from_obj_tbl(result, buf, -1);

    append_to_xml(" date_ref=\"%d\">", date_ref); // TODO - Fix quotes in Java

    AMFDL1("Date is a reference. ref_id = %d. date_len = %d, date = %s", date_ref, strlen(buf), buf);
  }
  else
  {
    in = get_date (buf, in, len); // Rest is same as AMF0 but no timezone will in AMF3
    addToStoredObjects(result);
  }

  return in;
}


// Sets data_len with the length of the string
static char *
get_string (char *buf, char *in, int *len)
{

  GET_SHORT ((unsigned short *) &data_len);

  GET_BYTES (data_len);
  bcopy (copy_to, buf, data_len);
  buf[data_len] = '\0';

  AMFDL1("Value of string (len = %d) = %s", data_len, buf);

  return in;
}

// Sets data_len with the length of the string
// TODO - We do not have enough space in buffer to read large strings
static char *
get_long_string (char *buf, char *in, int *len)
{
int local_data_len;

  GET_INT (&local_data_len);
  data_len = (unsigned int) local_data_len;

  GET_BYTES (data_len);
  bcopy (copy_to, buf, data_len);
  buf[data_len] = '\0';

  AMFDL1("Value of string (len = %d) = %s", data_len, buf);

  return in;
}


// AMF0 - This method is common for object, ecma array and typed object
static char *
write_object_element_amf0 (int indent, char *in, int *len)
{
  char buf[MAX_VAL];

  while (1) {
    GET_STRING (buf);
    if (in[0] == AMF0_OBJECT_END_MARKER) {
      in += 1;
      *len -= 1;
      break;
    }

    append_to_xml ("%s<objelement name=\"%s\">\n", leading_blanks (indent), buf);

    if ((in = write_data_amf0 (indent + XML_INDENT, in, len)) == NULL) {
      AMFEL("Bad data while writing objcet\n");
      return NULL;
    }

    append_to_xml ("%s</objelement>\n", leading_blanks (indent));
  }
  return in;
}


static char *
write_array_amf0 (int indent, char *in, int *len)
{
  int sz, i;

  GET_INT (&sz);

  append_to_xml ("%s<array size=\"%d\">\n", leading_blanks (indent), sz);

  for (i = 0; i < sz; i++) {
    if ((in = write_data_amf0 (indent + XML_INDENT, in, len)) == NULL) {
      AMFEL("Bad data while writing array\n");
      return NULL;
    }

  }

  append_to_xml ("%s</array>\n", leading_blanks (indent));

  return in;
}


static char *
write_object_amf0 (int indent, char *in, int *len)
{

  append_to_xml ("%s<object>\n", leading_blanks (indent));
  if ((in = write_object_element_amf0 (indent + XML_INDENT, in, len))) {
    append_to_xml ("%s</object>\n", leading_blanks (indent));
  }
  return in;
}

// ECMA or Associative Array for AMF0
static char *
write_ecma_array_amf0 (int indent, char *in, int *len)
{
  int size;

  AMFDL1("Method called.");

  GET_INT (&size);

  AMFDL1("AMF0 ecma array size = %d", size);
  //Ignore placeholder 4 byte

  append_to_xml ("%s<ecma_array size=\"%d\">\n", leading_blanks (indent), size);
  // After this ECMA array is same as AMF0 object
  if ((in = write_object_element_amf0 (indent + XML_INDENT, in, len))) {
    append_to_xml ("%s</ecma_array>\n", leading_blanks (indent));
  }
  return in;
}

static char *
write_typed_object_amf0 (int indent, char *in, int *len)
{
  char buf[MAX_VAL];

  GET_STRING (buf);
  append_to_xml ("%s<typed_object classname=\"%s\">\n", leading_blanks (indent), buf);
  if ((in = write_object_element_amf0 (indent + XML_INDENT, in, len))) {
    append_to_xml ("%s</typed_object>\n", leading_blanks (indent));
  }
  return in;
}

// Used by both AMF0 and AMF0. Checks if any char is not ASCII
// TODO - Handle UTF8 later
static void check_string(char *buf, unsigned int data_len)
{
int i;
  is_string = 1;
  for (i = 0; i < data_len; i++) {
// This was not working for new line     if (!isprint (buf[i])) 
    if (!isascii (buf[i])) 
    {
      AMFDL1("Character is not ascii. Char = 0x%x", buf[i]);
      AMFEL("Character is not ascii. Char = 0x%x\n", buf[i]);
      is_string = 0;
      break;
    }
  }
}

// Used by both AMF0 and AMF0. 
// Starts appedning from >
static void  write_string(char *buf, unsigned int data_len, char *tag_name)
{
int i;

  if(is_string) {
    append_to_xml (">%s</%s>\n", buf, tag_name);
  } else {
    append_to_xml (">CAVHEX:");
    for (i = 0; i < data_len; i++) {
      append_to_xml ("%02X", (unsigned char) buf[i]);
    }
    append_to_xml ("</%s>\n", tag_name);
  }
}

// Output:
//   buf is populated with string (NULL terminated)
//   data_len is set with the string length
//   is_string is set to 1 if all are string else 0
//   len is updated to take care of bytes consumed
//
static char *get_string_amf3(char *buf, char *in, int *len, int indent, int appendXml, char *attrRef, char *attrName)
{
StoredObjTbl *result, result_buf;

  xmlBuf[0] = '\0';

  AMFDL1("Method Called");
  result = &result_buf;
  init_stored_obj_tbl(AMF3_STRING, result);

  // Get string reference id or length
  GET_INTEGER_AMF3(buf);
  int type = atoi(buf); 

  if((type & 0x01) == 0) // stored string (Reference)
  {
    int str_ref = type >> 1;
    result = getFromStoredStrings(str_ref);
    if(result == NULL) return NULL;

    get_data_val_from_obj_tbl(result, buf, -1); // TODO - Pass correct len

    sprintf(xmlBuf, "%s %s=\"%d\"", xmlBuf, attrRef, str_ref);

    AMFDL1("String is a reference. ref_id = %d. str_len = %d, string = %s", str_ref, strlen(buf), buf);

  }
  else  // Instance of String
  {
    data_len = type >> 1;

    GET_BYTES (data_len);
    bcopy (copy_to, buf, data_len);
    buf[data_len] = '\0';

    set_data_val_in_obj_tbl(result, buf, data_len);

    if(data_len == 0) // Based on few testing with xactly amf samples, we found empty strings are not referenced
      AMFDL1("Not storing string in stored list as it is empty");
    else
      addToStoredStrings(result);

    check_string(buf, data_len);
    
    // TODO: Handle UTF strings as done in Java
    //AMFDL1("String is a instance. str_len = %d, is_string = %d, string = %s", data_len, is_string, buf);
    AMFDL1("String is a instance. str_len = %d, is_string = %d", data_len, is_string);

  }

  if(attrName != NULL)
    sprintf(xmlBuf, "%s %s=\"%s\"", xmlBuf, attrName, result->data_val);

  if(appendXml)
    append_to_xml("%s", xmlBuf);

  AMFDL1("Exitting Method");
  return(in);
}


// Output:
//   buf is populated with string (NULL terminated)
//   data_len is set with the string length
//   is_string is set to 1 if all are string else 0
//   len is updated to take care of bytes consumed

static char *
get_array_amf3(char *buf, char *in, int *len, int indent)
{
StoredObjTbl *result = NULL, result_buf;
//char *key;
int i;

// char buf[4096 + 1]; // TODO - Size

  GET_INTEGER_AMF3 (buf);
  int type = atoi (buf);

  result = &result_buf;
  init_stored_obj_tbl(AMF3_ARRAY, result);

  memset(result, 0, sizeof(result_buf));

  AMFDL1("Method called. indent = %d", indent);

  if((type & 0x01) == 0) // stored array.
  {
    int array_ref = type >> 1;
    append_to_xml(" array_ref=%d", array_ref);
    append_to_xml(">\n");
    result = getFromStoredObjects(array_ref);
    if(result == NULL) return NULL;
    AMFDL1("Array is a referece. ref_id = %d", array_ref);
  }
  else
  {
    int size = type >> 1; // Size of dense array
    AMFDL1("Array is a instance. size = %d", size);

    append_to_xml(" size=\"%d\">\n", size);

    // TODO - Store members of array in the  object (Low priority)
    addToStoredObjects(result);

    // It will get string in buf and attributes in xmlBuf
    GET_STRING_AMF3(buf, indent, APPEND_TO_XML_NO, "key_ref", "key");
    //key = buf;
    if(data_len > 0) // There are at least one associative member in the array (data_len is key length)
    {
      while(data_len > 0)
      {

        AMFDL1("Reading associative member with key = %s", buf);

        // map.put(key, readObject(indent, xmlBuf.toString()));
        WRITE_DATA_AMF3 (indent, in, len, xmlBuf);

        // Extract key name
        GET_STRING_AMF3(buf, indent, APPEND_TO_XML_NO, "key_ref", "key");
        //key = buf;
     }

      // for(int i = 0; i < size; i++)
        // map.put(Integer.valueOf(i), readObject(indent, NULL));

      // result = map;
    }
    //end_of_associative_array
    append_to_xml("%s<end_of_associative_array/>\n", leading_blanks (indent));

    for(i = 0; i < size; i++)
    {
      AMFDL1("Reading index member (%d) member.", i);

      // objects[i] = readObject(indent, NULL);
      WRITE_DATA_AMF3 (indent, in, len, NULL);

    // result = objects;
    }
  }

  return in;
}


// Output:
//   buf is populated with string (NULL terminated)
//   data_len is set with the string length
//   is_string is set to 1 if all are string else 0
//   len is updated to take care of bytes consumed

char *
get_object_amf3(char *buf, char *in, int *len, int indent)
{
  char dynamic[16] = "false";
  char externalizable[16] = "false";
  //char *classname = "";
  int propertiesCount = 0;
  int i;
  StoredClassDescriptors *desc = NULL, desc_buf;

  StoredObjTbl *result = NULL, result_buf;

  //Initializing the structure
  memset(&desc_buf, 0, sizeof(StoredClassDescriptors));

  desc = &desc_buf;
  result = &result_buf;
  init_stored_obj_tbl(AMF3_OBJECT, result);

  GET_INTEGER_AMF3 (buf);
  int type = atoi (buf);

  AMFDL1("Method called. type = 0x%x", type);

  if((type & 0x01) == 0) // object reference
  {
    int ref_id = type >> 1;
    AMFDL1("Object is a reference. ref_id = %d", ref_id);

    append_to_xml(" object_ref=\"%d\">\n", ref_id);
    result = getFromStoredObjects(ref_id);
    if(result == NULL) return NULL;
    AMFDL1("Object is a reference. ref_id = %d. classname = %s", ref_id, result->data_val);

    return in;
  }

  // else
  {
    int inlineClassDef = (((type >> 1) & 0x01) != 0);
    AMFDL1("inlineClassDef=%d", inlineClassDef);

    if(inlineClassDef)
    {
      propertiesCount = type >> 4;
      AMFDL1("propertiesCount=%d", propertiesCount);

      char encoding = (char)((type >> 2) & 0x03);

      AMFDL1("encoding=%d", encoding);

      // This will append classname in xml
      GET_STRING_AMF3(buf, indent, APPEND_TO_XML_YES, "classname_ref", "classname");

      strcpy(desc->classname, buf);
      //classname = desc->classname;
      desc->propertiesCount = propertiesCount;
      desc->encoding = encoding;

      AMFDL1("classname=%s", desc->classname);

      // Save object traits
      addToStoredClassDescriptors(desc);

      if(desc->dynamic)
        strcpy(dynamic, "true");
      if(desc->externalizable)
        strcpy(externalizable, "true");

      // In case of externalized class, some times we get
      if((encoding & AMF3_EXTERNALIZABLE_FLAG) != 0)
      {
        append_to_xml(" externalizable=\"%s\" dynamic=\"%s\">\n",
                      externalizable, dynamic);
      }
      else
      {
        append_to_xml(" sealed_members=\"%d\" externalizable=\"%s\" dynamic=\"%s\">\n",
                      propertiesCount, externalizable, dynamic);

        for(i = 0; i < propertiesCount; i++)
        {
          append_to_xml("%s<member", leading_blanks (indent));

          GET_STRING_AMF3(buf, indent, APPEND_TO_XML_YES, "str_ref", NULL);
          AMFDL1("Adding member %s", buf);
          append_to_xml(">%s</member>\n", buf);
        }
        if(propertiesCount != 0)
          append_to_xml("%s<end_of_sealed_member_names/>\n", leading_blanks (indent));
      }
    }
    else // traits are coming as reference
    {
      //in case we get stored class descriptor
      int trait_id = type >> 2;
      desc = getFromStoredClassDescriptors(trait_id);
      if(desc == NULL) return NULL;
      propertiesCount = desc->propertiesCount;

      AMFDL1("Object traits is a reference. ref_id = %d, propertiesCount = %d", trait_id, propertiesCount);

      if(desc->dynamic)
        strcpy(dynamic, "true");
      if(desc->externalizable)
        strcpy(externalizable, "true");

      // We are not putting classname intentionally in this case
      // We can add classname for debugging purpose later but java code will also need change
      //we need to set seal_member count so that netstorm can send member count
      append_to_xml(" traits_ref=\"%d\" sealed_members=\"%d\" externalizable=\"%s\" dynamic=\"%s\">\n", trait_id, desc->propertiesCount, externalizable, dynamic);
    }

    // AMFDL1("ActionScriptClassDescriptor=", desc);

    int objectEncoding = desc->encoding;

    addToStoredObjects(result);

    // read object content...
    if((objectEncoding & AMF3_EXTERNALIZABLE_FLAG) != 0)
    {
      /*int externalSize = 0;
      if(body_len == -1)
        externalSize = available(*len);
      else
        externalSize = body_len - (body_left - available(*len));
      AMFEL("Size of external data = %d", externalSize);
      */
  
      in = readExternalData(indent, len, desc->classname, in, body_len, body_left, available(*len));
      if(in == NULL)
      {      
         AMFEL("ERROR: In read externalizable data \n");
         return NULL;
      }

/*
      append_to_xml("%s<cavhex>", leading_blanks (indent));
      while(externalSize)
      {
        GET_BYTES(1);
        append_to_xml("%02X", copy_to[0]);
        externalSize--;
      }
      append_to_xml("</cavhex>\n");
*/
    }
    else
    {
      // Sealed member values
      if(propertiesCount > 0)
      {
        AMFDL1("Reading sealed member values.");
        for(i = 0; i < propertiesCount; i++)
        {
          AMFDL1("Reading sealed member %d value.", i);

          WRITE_DATA_AMF3 (indent, in, len, NULL);
          // desc.setPropertyValue(i, result, value);
        }
        if(propertiesCount != 0)
          append_to_xml("%s<end_of_sealed_member_values/>\n", leading_blanks (indent));
      }

      // dynamic values...
      if(objectEncoding & AMF3_DYNAMIC_FLAG)
      {
        int dynamicCount = 0;
        AMFDL1("Reading dynamic properties ...");
        while(1)
        {
          GET_STRING_AMF3(buf, indent, APPEND_TO_XML_NO, "dyn_member_ref", "dyn_member");
          if(data_len == 0) // Dyn member lenghr is 0 so no more dyn members
            break;
          AMFDL1("Reading dynamic member %d value with name = %s", dynamicCount, buf);

          WRITE_DATA_AMF3 (indent, in, len, xmlBuf);

          dynamicCount++;
          // desc.setPropertyValue(name, result, value);
        }
        append_to_xml("%s<end_of_dyn_members/>\n", leading_blanks (indent));
      }
    }
  }

  return in;
}

// This will also append to XML
static char  *get_byte_array_amf3(char *buf, char *in, int *len, int indent, int appendXml, char *attrRef)
{
int data_len;
int i;
StoredObjTbl *result, result_buf;

  xmlBuf[0] = '\0';

  AMFDL1("Method called");

  result = &result_buf;
  init_stored_obj_tbl(AMF3_BYTE_ARRAY, result);

  // Get string reference id or length
  GET_INTEGER_AMF3(buf);
  int type = atoi(buf);

  if((type & 0x01) == 0) // stored byte array (Reference)
  {
    int byte_arr_ref = type >> 1;
    result = getFromStoredObjects(byte_arr_ref);
    if(result == NULL) return NULL;

    data_len = get_data_val_from_obj_tbl(result, buf, -1); // TODO - Pass correct len

    sprintf(xmlBuf, "%s %s=\"%d\"", xmlBuf, attrRef, byte_arr_ref);

    AMFDL1("ByteArray  is a reference. ref_id = %d. byte_arr_len = %d", byte_arr_ref, strlen(buf));

  }
  else  // Instance of Byte Array
  {
    data_len = type >> 1;

    GET_BYTES (data_len);
    bcopy (copy_to, buf, data_len);
    buf[data_len] = '\0';

    set_data_val_in_obj_tbl(result, buf, data_len);

    if(data_len == 0) // Based on few testing with xactly amf samples, we found empty strings are not referenced
      AMFDL1("Not storing Byte Array in stored list as it is empty");
    else
      addToStoredObjects(result);

    AMFDL1("Byte Array is a instance. byte_array_len = %d", data_len);

  }

  if(appendXml)
    append_to_xml("%s", xmlBuf);

  append_to_xml (">");
  for (i = 0; i < data_len; i++) 
    append_to_xml ("%02X", (unsigned char) buf[i]);

  append_to_xml ("</byte_array>\n");

  return(in);
}

char* 
write_data_amf3 (int indent, char *in, int *len, char *attr)
{
int type;
int i;
char buf[STR_VAL];

  char *attrStr = "";

  if(attr != NULL)
    attrStr = attr;

  GET_BYTES (1);
  type = copy_to[0];

  AMFDL1("Method called. type = 0x%x, indent = %d, in = %p, len = %d, atrr = %s", type, indent, in, *len, attr);

  // All data type can be dynamic member in a object or associative member on a array
  // attrStr is set this

  switch (type) {
    // Not referenceable data types
    case AMF3_UNDEFINED:
      append_to_xml ("%s<undefined%s/>\n", leading_blanks (indent), attrStr);
      break;
    case AMF3_NULL:
      append_to_xml ("%s<null%s/>\n", leading_blanks (indent), attrStr);
      break;
    case AMF3_FALSE:
      append_to_xml ("%s<false%s/>\n", leading_blanks (indent), attrStr);
      break;
    case AMF3_TRUE:
      append_to_xml ("%s<true%s/>\n", leading_blanks (indent), attrStr);
      break;
    case AMF3_INTEGER:
      GET_INTEGER_AMF3 (buf);
      append_to_xml ("%s<integer%s>%s</integer>\n", leading_blanks (indent), attrStr, buf);
      break;
    case AMF3_DOUBLE:
      GET_DOUBLE (buf);
      // TODO: Handle NAN
      append_to_xml ("%s<double%s>%s</double>\n", leading_blanks (indent), attrStr, buf);
      break;

    // Referenceable - Strings
    case AMF3_STRING:
      append_to_xml ("%s<string%s", leading_blanks (indent), attrStr);
      GET_STRING_AMF3(buf, indent, APPEND_TO_XML_YES, "str_ref", NULL);
      write_string(buf, data_len, "string");
      break;

    case AMF3_DATE:
      append_to_xml ("%s<date%s", leading_blanks (indent), attrStr);
      GET_DATE_AMF3 (buf);
      append_to_xml (">%s</date>\n", buf);
      break;

    case AMF3_ARRAY:
      append_to_xml ("%s<array%s", leading_blanks (indent), attrStr);
      GET_ARRAY_AMF3(buf, indent + XML_INDENT);
      append_to_xml ("%s</array>\n", leading_blanks (indent));
      break;

    case AMF3_OBJECT:
      append_to_xml ("%s<object%s", leading_blanks (indent), attrStr);
      GET_OBJECT_AMF3(buf, indent + XML_INDENT);
      append_to_xml ("%s</object>\n", leading_blanks (indent));
      break;

    case AMF3_XML_DOC:
      break; // TODO


    case AMF3_XML:
      break; // TODO
    case AMF3_BYTE_ARRAY:
      append_to_xml ("%s<byte_array%s", leading_blanks (indent), attrStr);
      in = get_byte_array_amf3(buf, in, len, indent, APPEND_TO_XML_YES, "byte_array_ref"); 
      // append_to_xml (">%s</byte_array>\n", buf);
      break; // TODO
    default:
       //In case of error in decoding, dumping remaining bytes in cavhex
       AMFEL("Bad format: unexpected type(%02X), still left %d bytes\n", type, *len);
       append_to_xml ("%s<cavhex>%02X", leading_blanks (indent), (unsigned char)copy_to[0]);  //dumping the byte which is already read
       int  left_len = *len;
       GET_BYTES (left_len);
       AMFDL1("Bytes left (to dump in cavhex) = %d\n", left_len);
       if(left_len <= 0)  
         return NULL;

       for(i=0; i<left_len; i++)  
         append_to_xml ("%02X", (unsigned char)copy_to[i]);

       append_to_xml ("</cavhex>\n");
       AMFDL1("remaining bytes= %s\n", in); 
       in = NULL;
       break;
  }
  return in;
}

static char *
write_data_amf0 (int indent, char *in, int *len)
{
  int type;
  short snum;
  char buf[MAX_VAL];

  GET_BYTES (1);
  type = copy_to[0];

  switch (type) {
  case AMF0_NUMBER:
    GET_NUMBER (buf);
    append_to_xml ("%s<number>%s</number>\n", leading_blanks (indent), buf);
    break;
  case AMF0_BOOLEAN:
    GET_BOOL (buf);
    append_to_xml ("%s<bool>%s</bool>\n", leading_blanks (indent), buf);
    break;
  case AMF0_STRING:
    GET_STRING (buf);
    check_string(buf, data_len);
    append_to_xml ("%s<string", leading_blanks (indent));
    write_string(buf, data_len, "string");
    break;
  case AMF0_OBJECT:
    return (write_object_amf0 (indent, in, len));
    break;
/* case AMF0_MOVIECLIP: // Not used any more */
  case AMF0_NULL:
    append_to_xml ("%s<null/>\n", leading_blanks (indent));
    break;
  case AMF0_UNDEFINED:
    append_to_xml ("%s<undefined/>\n", leading_blanks (indent));
    break;
  case AMF0_REFERENCE:
    GET_SHORT ((unsigned short *) &snum);
    append_to_xml ("%s<reference>%hd</reference>\n", leading_blanks (indent), snum);
    break;
  case AMF0_ECMA_ARRAY:
    return (write_ecma_array_amf0(indent, in, len));
    break;
  case AMF0_STRICT_ARRAY:
    return (write_array_amf0 (indent, in, len));
    break;
  case AMF0_DATE:
    GET_DATE (buf);
    append_to_xml ("%s<date>%s</date>\n", leading_blanks (indent), buf);
    break;
  case AMF0_LONG_STRING:
    GET_LONG_STRING (buf);
    check_string(buf, data_len);
    append_to_xml ("%s<long_string", leading_blanks (indent));
    write_string(buf, data_len, "long_string");
    break;
  case AMF0_UNSUPPORTED:
    append_to_xml ("%s<unsupported/>\n", leading_blanks (indent));
    break;

/* case AMF0_RECORDSET: // Not used any more */

  case AMF0_XML_DOCUMENT:
    GET_LONG_STRING (buf);
    check_string(buf, data_len);
    append_to_xml ("%s<xml_doc", leading_blanks (indent));
    write_string(buf, data_len, "xml_doc");
    break;
   case AMF0_TYPED_OBJECT:
    return (write_typed_object_amf0 (indent, in, len));
    break;
  case AMF0_TO_AMF3:
    amf_data_version = AMF_VERSION3;
    AMFDL1("Switching to AMF3. amf_data_version is now %d", amf_data_version);
    append_to_xml ("%s<switch_to_amf3/>\n", leading_blanks (indent));
    WRITE_DATA_AMF3 (indent, in, len, NULL);
    break;
  default:
    AMFEL("Error: Bad format - unexpected type(%02X), still left %d bytes\n", type, *len);
    return NULL;
  }
  return in;
}

//Writes bodies message to to by reading binary  from in char array
static char *
write_body (int indent, char *in, int *len)
{
  char buf1[MAX_VAL], buf2[MAX_VAL];

  GET_STRING (buf1); // target_method
  GET_STRING (buf2); // response
  GET_INT (&body_len); // Body length

  AMFDL1("length is %d\n", body_len);

  //<body target_method=... response=..>
  append_to_xml ("%s<body target_method=\"%s\" response=\"%s\" body_len=\"%d\">\n",
	      leading_blanks (indent), buf1, buf2, body_len);

  if (amf_data_version == AMF_VERSION3) {
    WRITE_DATA_AMF3 (indent + XML_INDENT, in, len, NULL);
  } else {
    if ((in = write_data_amf0 (indent + XML_INDENT, in, len)) == NULL) {
      append_to_xml ("%s</body>\n", leading_blanks (indent));
      AMFEL("Bad format: data write failed\n");
      return NULL;
    }
  }

  append_to_xml ("%s</body>\n", leading_blanks (indent));
  return in;
}


#define MAX_AMF_ASC 4*1024*1024
// Converts AMF binary to xml
// Args:
//   in -> pointer to bianry amf buffer
//   *len -> Pointer to the lenght of the input
// Output
//  amf_asc_ptr buffer is filled with XML
//  Return the length of the XML
//  In case of error, hex code of AMF is filled
int
ns_amf_binary_to_xml (char *in, int *len)
{
//int amf_bin_len = *len, dbytes = 0, i;
int indent = 0; // Start with 0 space for indent
//char buf[128];


  AMFDL1("Method called. in = %p, len = %d", in, *len);

  if (!amf_asc_ptr)  // Allocate only once. Once allocated, it is used again (So do not free it)
    // TODO - Replace by Macro MY_MALLOC
    amf_asc_ptr = malloc (MAX_AMF_ASC);

  if (!amf_asc_ptr)
  {
    AMFEL("amf_binary_to_xml: Error in malloc\n");
    return -1;
  }

  // mlen is global variable filled by this method
  if (!amf_decode (MAX_AMF_ASC, amf_asc_ptr, indent, in, len))
  {
/*
    sprintf (buf, "AMF Hex to Asc failed at byte =%d (total=%d)\n<debug>\n",
       *len, amf_bin_len);
    if (mlen + strlen (buf) >= MAX_AMF_ASC)
      return mlen;

    // In case of error, copy hex dump of amf message for debugging purpose
    // TODO - we can remove this and store the amf message in a file for debugging.

    strcpy (amf_asc_ptr + mlen, buf);
    mlen += strlen (buf);

    dbytes = amf_bin_len * 3 + 9;

    if (mlen + dbytes >= MAX_AMF_ASC)
      return mlen;

    for (i = 0; i < amf_bin_len; i++)
    {
      if (i % 16 == 0) { // Keep 16 bytes in one line
	      if (i != 0)
	  mlen += sprintf (amf_asc_ptr + mlen, "\n");
        } else {
		mlen += sprintf (amf_asc_ptr + mlen, " ");
      }
      mlen += sprintf (amf_asc_ptr + mlen, "%02X", (unsigned char) in[i]);
    }
    strcpy (amf_asc_ptr + mlen, "</debug>\n");
    mlen += 9;
    return mlen;
*/
  }
  return mlen;
}

#if 0
// TODO - Review how this is used
char *
skip_amf_debug ()
{

  GET_LINE;
  if (strcmp (line, "<debug>")) {
    AMFEL("Expecting <debug> at line %d\n", amfin_lineno);
    return NULL;
  }

  while (1) {
    GET_LINE;
    if (strcmp (line, "</debug>") == 0) {
      break;
    }
  }
  return NULL;
}

#endif

// This is to create hex dump (ASCII) of amf data in debug tag

// Called from monitor.c (Need to remove)
void
put_amf_debug (FILE * fp, char *buf, int len)
{
  int i;

  fprintf (fp, "<debug>\n");
  for (i = 0; i < len; i++) {
    if (i != 0 && i % 16 == 0)
      fprintf (fp, "\n");
    fprintf (fp, "%02X ", (unsigned char) buf[i]);
  }
  fprintf (fp, "\n");
  fprintf (fp, "</debug>\n");
}


void
show_buf (char *buf, int len)
{
  int i;

  for (i = 0; i < len; i++) {
    if (i != 0 && i % 16 == 0)
      printf ("\n");
    printf ("%02X ", (unsigned char) buf[i]);
  }
  printf ("\n");
}

static int amf_decode_init()
{

  // We need to start with AMF_VERSION0 in each body
  amf_data_version = AMF_VERSION0; // Start with 0

  // Since these variable get set, we need to reset to 0 for every decode amf packet
  // Init total of all reference tables
  // All references are per body only, so we need to reset them in every new body
  totalStoredStrings = totalStoredObjects = totalStoredClassDescriptors = 0;
  return 0;
}

//Read binary AMF message from infp and writes amf xml into out
//out_type =0 for file out and out is is outfp in that case
//out_type !=0 is for memory buf out.  In this case out_type
//is outlen and out is output mem pointer

// This method is used for
//  1. Classic recorder (monitor.c)
//  2. Test tool (nsi_amf.c)
//  3. Netstorm to convert binary amf to XML in two cases:
//       - For storing HTTP AMF body in url_req_amf_body* file for debugging (ns_log_req_rep.c)
//       - For doing content check/parameters for AMF page response (ns_url_resp.c -> ns_amf_decde.c:amf_binary_to_xml())
char *
amf_decode (int out_type, void *out, int indent, char *in, int *len)
{
short version, num_bodies, num_hdr;
int i;

  AMFDL1("Method called. out_type = %d, out = %p, indent = %d, in = %p, len = %d", out_type, out, indent, in, *len);

  if (out_type == 0) { // Write xml in a file
    outfp = out;
  } else { // Write XML in a buffer with max len is out_type
    outfp = NULL;
    max_mlen = out_type;
    mlen = 0;
    out_buf = out;
  }


  GET_SHORT ((unsigned short *) &version);
  GET_SHORT ((unsigned short *) &num_hdr);
  GET_SHORT ((unsigned short *) &num_bodies);

  amf_pkt_version = version; // Save in global variable

  if ((version != AMF_VERSION0) && (version != AMF_VERSION3)) {
    AMFEL("Error: AMF version (%d) is not correct.\n", version);
    return NULL;
  }

  if (num_hdr) {
    AMFEL("Error: Header not supported in AMF. num_hdr=%d\n", num_hdr);
    return NULL;
  }

  append_to_xml ("%s<msg version=\"%hd\" headers=\"%hd\">\n", leading_blanks (indent), amf_pkt_version, num_hdr);

  append_to_xml ("%s<body_info bodies=\"%hd\"/>\n", leading_blanks (indent + XML_INDENT), num_bodies);

  for (i = 0; i < num_bodies; i++) {
    amf_decode_init();
    body_left = available(*len);
    if ((in = write_body (indent + XML_INDENT, in, len)) == NULL) {
      append_to_xml ("%s</msg>\n", leading_blanks (indent));
      AMFEL("Error - Bad format: while writing body \n");
      return NULL;
    }
  }

  append_to_xml ("%s</msg>\n", leading_blanks (indent));

  return in;
}
