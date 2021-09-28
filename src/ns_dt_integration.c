/****************************************************************************
 * Name	     : ns_dt_integration.c 
 * Purpose   : 
 * Code Flow : 
 * Author(s) : 
 * Date      : 
 * Copyright : (c) Cavisson Systems
 * Modification History :
 *     Author: 
 *      Date : 
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "nslb_sock.h"
#include "nslb_util.h"
#include "ns_log.h"
#include "ntlm.h"
#include "util.h"
#include "ns_trace_level.h"
#include "ns_rbu_api.h"
#include "ns_alloc.h"
#include "ns_exit.h"
#include "nslb_ssl_lib.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
//This pointer is filled when dynaTraceSetting.mode is 5/7 and Group is ALL i.e. this is not group dependent
//this is used to send start/stop recording dynaTrace request
//Start/stop recording Msg will be sent once when G_ENABLE_DT ALL 5/7
//So malloc it not need to free it
static DynaTraceSettings *gdynaTraceSettings = NULL;
static char stop_dt_recording = 1;

#define DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, err_msg) \
{\
  if(startStopDTRecording && !continue_on_dt_recording_failure)\
  {\
    fprintf(stderr, "dynaTrace Recording Failed due to [%s] and continue on dynaTrcae Recording Fail is disable, so aborting the Test\n", err_msg);\
    NSTL1(NULL, NULL, "dynaTrace Recording Failed due to [%s] and continue on dynaTrcae Recording Fail is disable, so aborting the Test", err_msg);\
    NSDL2_PARSING(NULL, NULL, "dynaTrace Recording Failed due to [%s] and continue on dynaTrcae Recording Fail is disable, so aborting the Test", err_msg);\
    NS_EXIT(-1, "dynaTrace Recording Failed due to [%s] and continue on dynaTrcae Recording Fail is disable, so aborting the Test", err_msg);\
  }\
  else if(startStopDTRecording && continue_on_dt_recording_failure)\
  {\
    fprintf(stderr, "dynaTrace Start Recording Failed due to [%s] and continue on dynaTrcae Recording Fail is enable, so continue the Test\n", err_msg);\
    NSTL1(NULL, NULL, "dynaTrace Start Recording Failed due to [%s] and continue on dynaTrcae Recording Fail is enable, so continue the Test", err_msg);\
    NSDL2_PARSING(NULL, NULL, "dynaTrace Start Recording Failed due to [%s] and continue on dynaTrcae Recording Fail is enable, so continue the Test", err_msg);\
    stop_dt_recording = 0;\
  }\
  else if(!startStopDTRecording)\
  {\
    fprintf(stderr, "Fail To Stop dynaTrace Recording");\
    NSTL1(NULL, NULL, "Fail To Stop dynaTrace  Recording");\
    NSDL2_PARSING(NULL, NULL, "Fail To Stop dynaTrace Recording");\
  }\
}

/***********************************************************************
 *  parseDynaTraceArgs()
 *    This method parses the dynaTraceArgs (3rd field of the KW) and 
 *    sets in the dynaTraceSettings structure
 *    ID;VU;SI=<value>;GR;SN 
 **********************************************************************/
static inline int parseDynaTraceArgs(DynaTraceSettings *dynaTraceSettings, char *inStr, char *buf, char *err_msg, int runtime_flag)
{
  if(!inStr || !(*inStr)){
    NSDL2_PARSING(NULL, NULL, "Method called. inStr is NULL. Returning");
    return -1;
  }
    
  NSDL2_PARSING(NULL, NULL, "Method called. inStr = [%s]", inStr);

  //Initialize flags
  dynaTraceSettings->requestOptionsFlag = 0;
  bzero(dynaTraceSettings->sourceID, 128);

  char *fields[9];
  int num_fields = get_tokens_with_multi_delimiter(inStr, fields, ";", 9);
  
  if(!num_fields) // Just a safety, should never happen
  {
    NSDL2_PARSING(NULL, NULL, "No fields found, returning");
    return -1;
  }

  int i;
  for(i = 0; i < num_fields; i++)
  {
    char *ptr = fields[i];
    if(!strncasecmp(ptr, "SI", 2))
    {
      ptr += 2;
      if(*ptr == '=')
      {
        ptr++;
        //if(!(*(++ptr))) gEnableDTUsage("Value missing for SI! For default value, '=' should be omitted.");
        if(*ptr == ';' || *ptr == '\0')
        {
          NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_DT_USAGE, CAV_ERR_1011228, "", "dynaTrace");
        }
        else
          strcpy(dynaTraceSettings->sourceID, ptr);
      }
      else if(*ptr == ';' || *ptr == '\0')
        strcpy(dynaTraceSettings->sourceID, "Netstorm");
      else
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_DT_USAGE, CAV_ERR_1011134, CAV_ERR_MSG_3);
      }
    } 

    else if(!strcasecmp(ptr, "ID"))
        dynaTraceSettings->requestOptionsFlag |= DT_REQUEST_ID_ENABLED;

    else if(!strcasecmp(ptr, "VU"))
        dynaTraceSettings->requestOptionsFlag |= DT_VIRTUAL_USER_ID_ENABLED;

    else if(!strcasecmp(ptr, "GR"))
        dynaTraceSettings->requestOptionsFlag |= DT_LOCATION_ENABLED;

    else if(!strcasecmp(ptr, "SN"))
        dynaTraceSettings->requestOptionsFlag |= DT_SCRIPT_NAME_ENABLED;

    else if(!strcasecmp(ptr, "TE") || !strcasecmp(ptr, "NA")) 
        continue;
   
    else if(!strcasecmp(ptr, "PC"))
        dynaTraceSettings->requestOptionsFlag |= DT_PAGE_NAME_ENABLED;
    
    else if(!strcasecmp(ptr, "AN"))
	dynaTraceSettings->requestOptionsFlag |= DT_AGENT_NAME_ENABLED;
    else
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_DT_USAGE, CAV_ERR_1011134, CAV_ERR_MSG_3);
  }

  NSDL2_PARSING(NULL, NULL, "requestOptionsFlag = '0x%x'", dynaTraceSettings->requestOptionsFlag);
  return 0;
}


/******************************************************************************
G_ENABLE_DT ALL 7 ID;VU;SI=NSDebug;GR;SN user=dtuser passwd=cav123 host=myhost001:8888 profile=myprofile01 presentableName=mydashboard description=abcdefg is_time_stamp_allowed=1 option=xyz is_session_locked=1 label=mylabel continue_on_DT_recording_failure=1
********************************************************************************/
int inline setOtherFieldOptions(DynaTraceSettings *dynaTraceSettings, char *inBuf, char *buf, char *err_msg, int runtime_flag)
{
  struct {
    char keyword[64];
  }FieldName[__dtFieldOptionsCount__] =  {{"USER"},
                      {"PASSWD"},
                      {"HOST"},
                      {"PROFILE"},
                      {"PRESENTABLENAME"},
                      {"DESCRIPTION"},
                      {"IS_TIME_STAMP_ALLOWED"},
                      {"OPTION"},
                      {"IS_SESSION_LOCKED"},
                      {"LABEL"},
                      {"CONTINUE_ON_DT_RECORDING_FAILURE"},
                      {"IS_SSL"}};

  inBuf[strlen(inBuf) - 1] = '\0';
  NSDL2_PARSING(NULL, NULL, "Method Called. numOfFields = %d, buf = '%s'", __dtFieldOptionsCount__, inBuf);

  char *fields[32], *ptr;
  int i, j;
  int num_fields = get_tokens_with_multi_delimiter(inBuf, fields, " \t", 12);
  for(i = 0; i < num_fields; i++)
  {
    for(j = 0; j < __dtFieldOptionsCount__; j++)
    {
      int nameLen = strlen(FieldName[j].keyword);
      if(!strncasecmp(fields[i], FieldName[j].keyword, nameLen))
      {
        ptr = fields[i] + nameLen;
        while(*ptr == ' ' || *ptr == '\t' || *ptr == '=') ptr++;
        CLEAR_WHITE_SPACE_FROM_END(ptr);
        strcpy(dynaTraceSettings->fieldOptions[j], ptr);
        break;
      }
    }
    if(j == __dtFieldOptionsCount__)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_DT_USAGE, CAV_ERR_1011134, CAV_ERR_MSG_3);
  }
 
  /* VALIDATIONS */ 
  if((dynaTraceSettings->mode == 5 ||
        dynaTraceSettings->mode == 7) && 
     (!dynaTraceSettings->fieldOptions[DT_OPT_USER][0] || 
       !dynaTraceSettings->fieldOptions[DT_OPT_PASSWD][0] || 
        !dynaTraceSettings->fieldOptions[DT_OPT_HOST][0] || 
         !dynaTraceSettings->fieldOptions[DT_OPT_PROFILE][0])) 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_DT_USAGE, CAV_ERR_1011134, CAV_ERR_MSG_1);
  }

  NSDL4_PARSING(NULL, NULL, "username = '%s', passwd = '%s', hostWithPort = '%s', "
                            "profile = '%s', presentableName = '%s', description = '%s', "
                            "is_time_stamp_allowed=%s, option = '%s', "
                            "is_session_locked = '%s', label = '%s', "
                            "continue_on_dt_recording_failure = '%s', "
                            "sourceID = '%s', Req Header Bits = '0x%x', is_ssl = '%s'", 
                            dynaTraceSettings->fieldOptions[DT_OPT_USER],
                            dynaTraceSettings->fieldOptions[DT_OPT_PASSWD],
                            dynaTraceSettings->fieldOptions[DT_OPT_HOST],
                            dynaTraceSettings->fieldOptions[DT_OPT_PROFILE],
                            dynaTraceSettings->fieldOptions[DT_OPT_PRESENTABLENAME],
                            dynaTraceSettings->fieldOptions[DT_OPT_DESCRIPTION],
                            dynaTraceSettings->fieldOptions[DT_OPT_IS_TIME_STAMP_ALLOWED],
                            dynaTraceSettings->fieldOptions[DT_OPT_OPTION],
                            dynaTraceSettings->fieldOptions[DT_OPT_IS_SESSION_LOCKED],
                            dynaTraceSettings->fieldOptions[DT_OPT_LABEL],
                            dynaTraceSettings->fieldOptions[DT_OPT_CONTINUE_ON_DT_RECORDING_FAILURE],
                            dynaTraceSettings->sourceID,
                            dynaTraceSettings->requestOptionsFlag,
                            dynaTraceSettings->fieldOptions[DT_OPT_IS_SSL]);
  return 0;
}
/*********************************************************************************
 * G_ENABLE_DT <Group> <Mode> <DynaTraceArgs> <OtherFields>
 *    Example:
 *    G_ENABLE_DT ALL 7 ID;VU;SI=NSDebug;GR;SN user=dtuser passwd=cav123 host=myhost001:8888 profile=myprofile01 presentableName=mydashboard description=abcdefg is_time_stamp_allowed=1 continue_on_DT_recording_failure=1
 *
 * where, 
 *   Mode 
 * 	0: Disable dynaTrace Integration (Default)
 * 	1: Enable dynaTrace Integration 
 *	   Synatx: G_ENABLE_DT ALL 1 NA;TE
 * 	3: Capture dynaTrace information from HTTP response header.
 *	   Synatx: G_ENABLE_DT ALL 3 NA;TE
 * 	5: Enable session recording on start of test execution. Recording will stop after completion of the test.
 *         Synatx: G_ENABLE_DT ALL 5 NA;TE user=cavisson passwd=cavisson host=10.10.70.8:8021 is_ssl=1 profile=work 
 *		   presentableName=work is_time_stamp_allowed=0 option=all is_session_locked=0 continue_on_DT_recording_failure=0
 * 	7: mode 3 & 5 (Both are applicable)
 *         Syntax: G_ENABLE_DT ALL 7 NA;TE user=cavisson passwd=cavisson host=10.10.70.8:8021 is_ssl=1 profile=work 
 *                 presentableName=work is_time_stamp_allowed=0 option=all is_session_locked=0 continue_on_DT_recording_failure=0
 *
 *   DynaTraceArgs
 *      ID: Unique RequestID
 *	PC: Page Context
 *	VU: Virtual UserID
 *	GR: Geographic Region
 *	AN: AgentName
 *	SN: Script Name
 *	TE: Test Name
 *	SI: SouceID
 ********************************************************************************/
int kw_set_g_enable_dt(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[16] = "";
  int mode;
  char groupName[64] = "";
  char dynaTraceArgsStr[512] = "";
  int numFields;
  char *fifthFieldPtr = NULL;

  NSDL2_PARSING(NULL, NULL, "Method Called.");
 
  numFields = sscanf(buf, "%s %s %d %s", keyword, groupName, &mode, dynaTraceArgsStr);
  
  NSDL3_PARSING(NULL, NULL, "buf = '%s', keyword ='%s', groupName = '%s', mode = '%d', dynaTraceArgsStr = '%s'", 
                             buf, keyword, groupName, mode, dynaTraceArgsStr);
  if(numFields < 3)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_DT_USAGE, CAV_ERR_1011134, CAV_ERR_MSG_1);

  if(mode != 0 && mode != 1 && mode != 3 && mode != 5 && mode != 7)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_DT_USAGE, CAV_ERR_1011134, CAV_ERR_MSG_3);
  
  if(mode == 0)
  {
    NSDL1_PARSING(NULL, NULL, "Mode 0, hence skipping parsing G_ENABLE_DT keyword.");
    return 0;
  }

  gset->dynaTraceSettings.mode = mode; 

  //Now find the pointer to fifth field
  int fieldnum = 0;
  char *ptr = buf;
  while(1)
  {
    while (*ptr == ' ' || *ptr == '\t')  ptr++;
    // Now we are at next field
    fieldnum++;

    if(fieldnum == 5)
      break;
    
     ptr = strpbrk(ptr, " \t");
     if(!ptr) // This is the case when fifth field does not exist
       break;
  }
  fifthFieldPtr = ptr;
  
  CLEAR_WHITE_SPACE_FROM_END(dynaTraceArgsStr);
  if((parseDynaTraceArgs(&gset->dynaTraceSettings, dynaTraceArgsStr, buf, err_msg, runtime_flag)) == -1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_DT_USAGE, CAV_ERR_1011134, CAV_ERR_MSG_1);
  }

  if(fifthFieldPtr){
    CLEAR_WHITE_SPACE_FROM_END(fifthFieldPtr);
    setOtherFieldOptions(&gset->dynaTraceSettings, fifthFieldPtr, buf, err_msg, runtime_flag);
  }
  else if(!fifthFieldPtr && (mode == 5 || mode == 7))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_DT_USAGE, CAV_ERR_1011134, CAV_ERR_MSG_1);
  }      

  //if Mode is 5/7 and groupName is ALL, fill gdynaTraceSettings 
  if(!strcmp(groupName, "ALL") && (gset->dynaTraceSettings.mode & 0x0004)) {
    NSDL1_PARSING(NULL, NULL, "groupName is ALL and Enable recording is On");
    //we will not free this as it will only used by parent and used for start/stop dynaTrace recording 
    //At the end of the test it will free itself
    MY_MALLOC(gdynaTraceSettings, sizeof(DynaTraceSettings), "Malloc dynaTraceSettings For All", -1);
    memcpy(gdynaTraceSettings, &gset->dynaTraceSettings, sizeof(DynaTraceSettings));
  }

  return 0;
}

static inline void genratebase64AuthString(unsigned char *base64EncodedString)
{  
  char userPassBuf[1024];
 
  sprintf(userPassBuf, "%s:%s", gdynaTraceSettings->fieldOptions[DT_OPT_USER], gdynaTraceSettings->fieldOptions[DT_OPT_PASSWD]); 

  int len = strlen((const char *)userPassBuf);
  to64frombits(base64EncodedString, (unsigned char *)&userPassBuf, len); 

}
  
static void createDTAutoRecordSessionMsg(char *msgBuf, int startStopDTRecording)
{
  unsigned char base64EncodedString[1024]; 
  char postData[4098];
  int len = 0;

  NSDL2_REPORTING(NULL, NULL, "Method called");
  
  genratebase64AuthString(base64EncodedString);

  if(startStopDTRecording){

    char label[256] = {0};
    if(gdynaTraceSettings->fieldOptions[DT_OPT_LABEL][0])
      sprintf(label, "&label=%s", gdynaTraceSettings->fieldOptions[DT_OPT_LABEL]);


    NSTL1(NULL, NULL, "Making DT startrecording Request");
    len = sprintf(postData, "presentableName=%s&description=%s&recordingOption=%s&isTimeStampAllowed=%s&isSessionLocked=%s%s",
                             gdynaTraceSettings->fieldOptions[DT_OPT_PRESENTABLENAME], gdynaTraceSettings->fieldOptions[DT_OPT_DESCRIPTION],
                             gdynaTraceSettings->fieldOptions[DT_OPT_OPTION],
                             atoi(gdynaTraceSettings->fieldOptions[DT_OPT_IS_TIME_STAMP_ALLOWED])?"true":"false",
                             atoi(gdynaTraceSettings->fieldOptions[DT_OPT_IS_SESSION_LOCKED])?"true":"false",
                             label[0]?label:"");
  
    sprintf(msgBuf, "POST /rest/management/profiles/%s/startrecording HTTP/1.1\r\n"
                    "Accept-Language: en-us\r\n"
                    "HOST: %s\r\n"
                    "Content-Type: application/x-www-form-urlencoded\r\n"
                    "Content-Length: %d\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Authorization: Basic %s\r\n\r\n"
                    "%s",
                    gdynaTraceSettings->fieldOptions[DT_OPT_PROFILE], gdynaTraceSettings->fieldOptions[DT_OPT_HOST], 
                    len, base64EncodedString, postData);

  }
  else {

      NSTL1(NULL, NULL, "Making DT stoprecording Request");
      sprintf(msgBuf, "GET /rest/management/profiles/%s/stoprecording HTTP/1.1\r\n"
                      "HOST: %s\r\n"
                      "Authorization: Basic %s\r\n\r\n", 
                      gdynaTraceSettings->fieldOptions[DT_OPT_PROFILE], gdynaTraceSettings->fieldOptions[DT_OPT_HOST],  
                      base64EncodedString);
  }
  
  NSDL2_REPORTING(NULL,NULL, "msgBuf = [%s]", msgBuf);

}

static void parse_dynaTrace_recording_response(char *read_msg, int startStopDTRecording, int continue_on_dt_recording_failure)
{
  NSDL1_REPORTING(NULL, NULL, "Method called, continue_on_dt_recording_failure = %d", continue_on_dt_recording_failure);

  //HTTP/1.1 200 OK (Response line)
  char *status_code_ptr = read_msg + 9;

  NSDL1_REPORTING(NULL, NULL, "status_code_ptr = %s", status_code_ptr);

  if(!strncmp(status_code_ptr, "200 OK", 6))
  {
    char *presenTableNamePtr = NULL;

    if((presenTableNamePtr = strstr(read_msg, "result value=")) != NULL)
    {
      presenTableNamePtr = presenTableNamePtr + 14;

      NSDL1_REPORTING(NULL, NULL, "presenTableNamePtr = [%s]", presenTableNamePtr);

      if(!strncmp(presenTableNamePtr, gdynaTraceSettings->fieldOptions[DT_OPT_PRESENTABLENAME], 
                                      strlen(gdynaTraceSettings->fieldOptions[DT_OPT_PRESENTABLENAME]))) {
        if(startStopDTRecording) {
          fprintf(stderr, "dynaTrace Recording Started Successfully\n");
          NSTL1(NULL, NULL, "dynaTrace Recording Started Successfully");
          NSDL1_REPORTING(NULL, NULL, "dynaTrace Recording Started Successfully");
        }
        else {
          fprintf(stderr, "dynaTrace Recording Stopped Successfully\n");
          NSTL1(NULL, NULL, "dynaTrace Recording Stopped Successfully");
          NSDL1_REPORTING(NULL, NULL, "dynaTrace Recording Stopped Successfully");
        }
      }
      else
        DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "Presentable Name is not matched with 'result value' attribute")
    }
    else
      DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "In Response Body 'result value' tag not found")
  }
  else
    DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "200 OK not found in response")

}

static int sendDTAutoRecordSessionReq(int restFD, int startStopDTRecording, int is_ssl, SSL *ssl, int continue_on_dt_recording_failure)
{
  char SendMsg[2048]="\0";

  NSDL1_REPORTING(NULL, NULL, "Method called is_ssl = %d", is_ssl);

  createDTAutoRecordSessionMsg(SendMsg, startStopDTRecording);

  NSDL2_REPORTING(NULL, NULL, "Sending message to DynaTrace Server. Message = %s", SendMsg);
  NSTL1(NULL, NULL, "Sending Request to DynaTrace Server");

  if(is_ssl) 
  {
    ERR_clear_error();
    if(SSL_write(ssl, SendMsg, strlen(SendMsg)) != strlen(SendMsg))
    {
      DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "Error in sending request for dynaTrace Auto Recording Session")

      close(restFD);
      restFD = -1;
      return -1; //error
    }
  }
  else
  {
    if(send(restFD, SendMsg, strlen(SendMsg), 0) != strlen(SendMsg))
    {
      DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "Error in sending request for dynaTrace Auto Recording Session")
 
      close(restFD);
      restFD = -1;
      return -1; //error
    }
  }

  NSTL1(NULL, NULL, "Request Sent to DT server is [%s]", SendMsg);  
  char read_msg[1024];
  int byte_read = 0;
  
  while(1)
  {
    NSDL1_REPORTING(NULL, NULL, "Reading msg from DT Server.");
    NSTL1(NULL, NULL, "Reading msg from DT Server.");

    if(is_ssl) 
    {
      if((byte_read = SSL_read(ssl, read_msg, 1024)) == 0)
      {
        DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "Error in receiving response from dynaTrace Server. Connection got disconnected from dynaTrace Server")

        close(restFD);
        restFD = -1;
        return (-1); //error
      }
      if(byte_read >= 0)
      {
        break;
      }
    }
    else 
    {
      if((byte_read = read(restFD, read_msg, 1024)) == 0) 
      {
        DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "Error in receiving response from dynaTrace Server. Connection got disconnected from dynaTrace Server")
 
        close(restFD);
        restFD = -1;
        return (-1); //error
      }
      if(byte_read >= 0)
      {
        break;
      }
    }
  }

  read_msg[byte_read] = '\0';
  NSDL2_REPORTING(NULL, NULL, "Recieved message from DynaTrace Server. Message = %s", read_msg);
  NSTL1(NULL, NULL, "Recieved Response from DynaTrace Server = [%s]", read_msg);

  close(restFD);
  restFD = -1;

  parse_dynaTrace_recording_response(read_msg, startStopDTRecording, continue_on_dt_recording_failure);

  return 0;
}

void connectToDTandStartStopSessionRecording(int startStopDTRecording, char *err_msg)
{
  NSDL2_REPORTING(NULL, NULL, "Method called hostWithPort = [%s]", gdynaTraceSettings->fieldOptions[DT_OPT_HOST]);

  char *hptr = NULL;
  int port = 0;
  int restFD = 0;
  SSL_CTX *ctx;
  SSL *ssl;
  char hostWithPort[1024];
  int is_ssl = atoi(gdynaTraceSettings->fieldOptions[DT_OPT_IS_SSL]);
  int continue_on_dt_recording_failure = atoi(gdynaTraceSettings->fieldOptions[DT_OPT_CONTINUE_ON_DT_RECORDING_FAILURE]);

  strcpy(hostWithPort, gdynaTraceSettings->fieldOptions[DT_OPT_HOST]);

  if((hptr = nslb_split_host_port(hostWithPort, &port)) == NULL)
  {
    NSDL2_REPORTING(NULL, NULL,"In case of ip address hptr value is = %s", hptr);
    DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "Unable to split host and port from HOST as given in Keyword")
    return;
  }

  write_log_file(NS_SCENARIO_PARSING, "Sending StartRecording/StopRecording message to dynatrace server %s", hptr);
  restFD = nslb_tcp_client_ex(hptr, port, 60, err_msg);
  NSDL2_REPORTING(NULL, NULL,"hptr = [%s], port [%d], restFD = [%d], is_ssl = %d", hptr, port, restFD, is_ssl);
  if(restFD == -1)
  {
    fprintf(stderr, "Error: Unable to connect to REST Server IP %s, Port = %d. Error = %s\n", hptr, port, err_msg);
    NSTL1(NULL, NULL, "Error: Unable to connect to REST Server IP %s, Port = %d. Error = %s\n", hptr, port, err_msg);
    DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "Unable to connect to dynaTrace Server")
    return;
  }

  if(is_ssl)
  {
    ctx = InitCTX();
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, restFD);
    if (SSL_connect(ssl) <= 0)
    {
      SSL_CTX_free(ctx);
      DYNATRACE_RECORDING_FAIL(continue_on_dt_recording_failure, startStopDTRecording, "Unable to connect ssl")
      return;
    }
  }
  
  NSTL1(NULL, NULL, "Connected to REST Server IP %s, Port = %d", hptr, port);
  sendDTAutoRecordSessionReq(restFD, startStopDTRecording, is_ssl, ssl, continue_on_dt_recording_failure);
  
  if(is_ssl)
    SSL_CTX_free(ctx);
}

//Function that makes connection to the DT server. 
void autoRecordingDTServer(int startStopDTRecording)
{
  char err_msg[1024];
  err_msg[0] = '\0';

  NSDL2_REPORTING(NULL, NULL,"Method Called. %s", startStopDTRecording?"StartRecording":"StopRecording");

  //If Recording is not started, don't send stop recording MSG
  if(!stop_dt_recording) {
    NSDL2_REPORTING(NULL, NULL, "Don't Send StopRecording request to server as we don't StartRecording for this Test Run");
    return;
  }

  //If G_ENABLE_DT ALL 5/7, than start/stop dynaTrace recording
  if(gdynaTraceSettings) {
    NSDL2_REPORTING(NULL, NULL, "Mode bit 3rd(Auto Record Session) is set");
    connectToDTandStartStopSessionRecording(startStopDTRecording, err_msg);
  }
  else {
    NSDL2_REPORTING(NULL, NULL, "Don't send StartRecording/StopRecording Msg as Recording Settings are Not applied for ALL group");
    return;
  }
}
