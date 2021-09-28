#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "amf.h"

//high bit must be 1 for next flag value
#define HAS_NEXT_FLAG 0x80

//for abstract message we are checking first byte
#define BODY_FLAG                 0x01
#define CLIENT_ID_FLAG            0x02
#define DESTINATION_FLAG          0x04
#define HEADERS_FLAG              0x08
#define MESSAGE_ID_FLAG           0x10
#define TIMESTAMP_FLAG            0x20
#define TIME_TO_LIVE_FLAG         0x40

// Second byte values for abstract msg
#define CLIENT_ID_BYTES_FLAG      0x01
#define MESSAGE_ID_BYTES_FLAG     0x02

// First byte values for asynchronous msg
#define CORRELATION_ID_FLAG       0x01
#define CORRELATION_ID_BYTES_FLAG 0x02

// First byte values for cmd msg
#define OPERATION_FLAG            0x01

// This is the default operation for new CommandMessage instances.
// #define UNKNOWN_OPERATION 10000


static int flagsArrayLen; 

/**
 * this method reads in the property flags from an ObjectInput
 * stream. Flags are read in one byte at a time. Flags make use of
 * sign-extension so that if the high-bit is set to 1 this indicates that
 * another set of flags follows.
 * @return The array of property flags.
 */
#define READ_FLAGS() if ((in = readFlags (indent, in, len, flagsArray)) == NULL) {\
    AMFEL("ERROR: failed to read flags of extenal object. still left %d bytes\n", *len);\
    return NULL;\
  }

static char *readFlags(int indent, char* in, int *len, unsigned char flagsArray[]) 
{
  
  AMFDL1("Method Called. readFlags");
  int hasNextFlag = 1;

  flagsArrayLen = 0;
  append_to_xml ("%s<flags>", leading_blanks (indent));
  while(hasNextFlag)
  {
    GET_BYTES(1);
    flagsArray[flagsArrayLen] = copy_to[0];
    AMFDL1("Integer value of flag[%d] = 0x%X", flagsArrayLen, flagsArray[flagsArrayLen]);
    append_to_xml ("%02X", flagsArray[flagsArrayLen]);
    if((copy_to[0] & HAS_NEXT_FLAG) != 0)
      hasNextFlag = 1;
    else
      hasNextFlag = 0;

    flagsArrayLen++;
  }
  append_to_xml ("</flags>\n");
  return in;
}

#define HANDLE_OTHER_FLAGS() if ((in = handle_other_flags(indent, in, len, reservedPosition, flags)) == NULL) {\
    AMFEL("ERROR: failed to read other flags of extenal object. still left %d bytes\n", *len);\
    return NULL;\
  }

static char *handle_other_flags(int indent, char *in, int *len, int reservedPosition, unsigned char flags)
{
int i;

  // For forwards compatibility, read in any other flagged objects to
  // preserve the integrity of the input stream...
  AMFDL1("Checking reservedPosition = %d, flags=0X%02X", reservedPosition, flags);
  if((flags >> reservedPosition) != 0) // Check if any bits left
  {
    AMFDL1("Bits left to be processed  = 0x%02X", flags);
    for(i = reservedPosition; i < 6; i++)
    {
      AMFDL1("Checking bit number %d, bit_val=%d", i, ((flags >> i) & 1));
      if(((flags >> i) & 1) != 0) // Check bit
      {
        AMFDL1("Bit number %d (starting from 0) is set", i);
        WRITE_DATA_AMF3(indent, in, len, " ext_member=\"reservedPosition\"");
      }
    }
  }
  return in;
}

static char *readExternalForAbsMsg(int indent, char* in, int *len) 
{
int reservedPosition;
int i;
unsigned char flagsArray[100]; 
unsigned char flags;

  AMFDL1("Method Called.");

  READ_FLAGS();

  for(i = 0; i < flagsArrayLen; i++)
  {
    flags = flagsArray[i];
    AMFDL1("Checking flag[%d] = 0x%X", i, flags);
    reservedPosition = 0;
    if(i == 0) // First byte
    {
      if((flags & BODY_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"body\"");
        AMFDL2("DSK Abstract Class - Element BODY_FLAG received");
      }

      if((flags & CLIENT_ID_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"clientId\"");
        AMFDL2("DSK Abstract Class - Element CLIENT_ID_FLAG received");
      }

      if((flags & DESTINATION_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"destination\"");
        AMFDL2("DSK Abstract Class - Element DESTINATION_FLAG received");
      }

      if((flags & HEADERS_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"headers\"");
        AMFDL2("DSK Abstract Class - Element HEADERS_FLAG received");
      }
  
      if((flags & MESSAGE_ID_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"messageId\"");
        AMFDL2("DSK Abstract Class - Element MESSAGE_ID_FLAG received");
      }

      if((flags & TIMESTAMP_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"timestamp\"");
        AMFDL2("DSK Abstract Class - Element TIMESTAMP_FLAG received");
      }

      if((flags & TIME_TO_LIVE_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"timeToLive\"");
        AMFDL2("DSK Abstract Class - Element TIME_TO_LIVE_FLAG received");
      }

      reservedPosition = 7; // For first byte, it should be 7
    }   
    else if(i == 1) // Second byte
    {
      if((flags & CLIENT_ID_BYTES_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"clientId\"");
        AMFDL2("DSK Abstract Class - Element CLIENT_ID_BYTES_FLAG received");
      }

      if((flags & MESSAGE_ID_BYTES_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"messageId\"");
        AMFDL2("DSK Abstract Class - Element MESSAGE_ID_BYTES_FLAG received");
      }

      reservedPosition = 2;
    }
    else
    {
      AMFEL("Error: Got more than 2 bytes in the flag. Ignored");
      continue;
    }
    HANDLE_OTHER_FLAGS();

  }
  return in;
}

static char *readExternalForAsyncMsg(int indent,  char* in, int *len)
{
int i, reservedPosition;
unsigned char flagsArray[100];
unsigned char flags;

  AMFDL1("Method Called.");

  in = readExternalForAbsMsg(indent, in, len);
  if(in == NULL) 
  {
    AMFDL2("ERROR: In decoding external class Abstract Message");
    return NULL;
  }

  READ_FLAGS();

  for( i = 0; i < flagsArrayLen; i++)
  {
    flags = flagsArray[i];
    AMFDL1("Checking flag[%d] = 0x%X", i, flags);

    reservedPosition = 0;

    if(i == 0)
    {
      if((flags & CORRELATION_ID_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"correlationId\"");
      }
 
      if((flags & CORRELATION_ID_BYTES_FLAG) != 0)
      {
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"correlationIdBytes\"");
      }
      reservedPosition = 2;
    }

    HANDLE_OTHER_FLAGS();
  }
  return in;
}

static char *readExternalAckMsg(int indent, char* in, int *len)
{
int i, reservedPosition;
unsigned char flagsArray[10];
unsigned char flags;

  in = readExternalForAsyncMsg(indent, in, len);
  if(in == NULL) 
  {
    AMFDL2("ERROR: In decoding external class Async Message");
    return NULL;
  }


  READ_FLAGS();

  for(i = 0; i < flagsArrayLen; i++)
  {
    flags = flagsArray[i];
    AMFDL1("Checking flag[%d] = 0x%X", i, flags);
    reservedPosition = 0;
    HANDLE_OTHER_FLAGS();
  }
  return in;
}

/**
 * reads external data for CommandMessage
 * @throws IOException
 * @exclude
*/
static char *readExternalForCmdMsg(int indent,char* in, int *len) 
{
  int i, reservedPosition;
  unsigned char flagsArray[10];
  unsigned char flags;

  in = readExternalForAsyncMsg(indent, in, len);
  if(in == NULL) 
  {
    AMFDL2("ERROR: In decoding external class Async Message");
    return NULL;
  }

  READ_FLAGS();

  for( i = 0; i < flagsArrayLen; i++)
  {
    flags = flagsArray[i];
    AMFDL1("Checking flag[%d] = 0x%X", i, flags);
    reservedPosition = 0;

    if(i == 0)
    {
      if((flags & OPERATION_FLAG) != 0)
        WRITE_DATA_AMF3(XML_INDENT + indent, in, len, " ext_member=\"operation\"");
      reservedPosition = 1;
    }

    HANDLE_OTHER_FLAGS();
  }
  return in;
}

/**
 * this method reads external data in case object of unknown structure comes
 * and puts all data into <cavhex></cavhex> tags.
 */

static char *readExternalForCustom(int indent, char* in, int *len, int body_len, int body_left, int available)
{
int externalSize = 0;

  if(body_len == -1)
    externalSize = available;
  else
    externalSize = body_len - (body_left - available);
  AMFDL1("Size of external data = %d", externalSize);
      
  while(externalSize)
  {
    GET_BYTES(1);
    append_to_xml("%s<cavhex>%02X</cavhex>",leading_blanks (indent), copy_to[0]);
    externalSize--;
  }
  return in;
}
 
// This is called from ns_amf_decode.c
//type is classname of the object type
char *readExternalData(int indent, int *len, char *type, char* in, int body_len, int body_left, int available)
{
//char buf[MAX_VAL];

  AMFDL1("Method_call. Type=%s", type);
  //for DSK (flex.messaging.messages.AcknowledgeMessageExt) class alias
  if((strcmp(type, "DSK") == 0) || (strcmp(type, "flex.messaging.messages.AcknowledgeMessageExt") == 0))
  {
    AMFDL1("Calling readExternalAckMsg. Type=%s", type);
    in = readExternalAckMsg(indent, in, len);
    if(in == NULL) 
    {
      AMFDL2("ERROR: In decoding external class Acknowlegement Message");
      return NULL;
    }
  }
  //for DSA (flex.messaging.messages.AsyncMessageExt) class alias
  else if((strcmp(type, "DSA") == 0) || (strcmp(type, "flex.messaging.messages.AsyncMessageExt") == 0))
  {
    AMFDL1("Callling readExternalForAsyncMsg method. Type=%s", type);
    in = readExternalForAsyncMsg(indent, in, len);
    if(in == NULL) 
    {
      AMFDL2("ERROR: In decoding external class Async Message");
      return NULL;
    }

  }
  //for DSC (flex.messaging.messages.CommandMessageExt) class alias
  else if((strcmp(type, "DSC") == 0) || (strcmp(type, "flex.messaging.messages.CommandMessageExt") == 0))
  {
    AMFDL1("Calling readExternalCmdMsg method. Type=%s", type);
    in = readExternalForCmdMsg(indent, in, len);
    if(in == NULL) 
    {
      AMFDL2("ERROR: In decoding external class Command Message");
      return NULL;
    }
  }
  //Extending support for Flex Messaging IO classes
   else if ((strcmp(type, "flex.messaging.messages.ArrayCollection") == 0) || (strcmp(type, "flex.messaging.messages.ArrayList") == 0) || (strcmp(type, "flex.messaging.messages.ObjectProxy") == 0) || (strcmp(type, "flex.messaging.io.ArrayCollection" ) == 0) || (strcmp(type, "flex.messaging.io.ArrayList") == 0))
  {
     AMFDL1("Calling WRITE_DATA_AMF3. Type=%s", type);
     WRITE_DATA_AMF3 (indent, in, len, NULL);
  }
  else if(strcmp(type,"") == 0)
  {
    AMFDL1("Class name not present. type = %s", type);
    return in;
  }
  //for Custom classes
  else
  {
    AMFDL1("For Custom Classes, Calling readExternalForCustom method. Type=%s", type);
    in = readExternalForCustom(indent, in, len, body_len, body_left, available);
    if(in == NULL) 
    {
      AMFDL2("ERROR: In decoding external class Custom Message");
      return NULL;
    }
 
  }
  return in;
}

