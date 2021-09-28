#ifndef STRUCT_CON_H
#define STRUCT_CON_H

// Created this file so that we can include this file in ns_server_admin_utils.c for macro NS_STRUCT_HEART_BEAT as server_admin is a standalone program, we can't include any other file directly.

/* con type for parent */

#define NS_STRUCT_TYPE_CPTR           1   // Connection structure used for http and other protocols connections
#define NS_STRUCT_TYPE_LOG_MGR_COM   2  // Connection structure used for NVM to log mgr
#define NS_STRUCT_TYPE_NVM_PARENT_COM      3  // Msg_com_con structure used for NVM to Parent control communication
#define NS_STRUCT_TYPE_VUSER_THREAD_LISTEN  4  // Msg_com_con structure used for NVM to listen for connections from threads
#define NS_STRUCT_TYPE_VUSER_THREAD_COM  5  // Msg_com_con structure used for NVM toVuser threads
#define NS_STRUCT_CUSTOM_MON 6
#define NS_STRUCT_CHECK_MON 7
#define NS_STRUCT_HEART_BEAT 9
#define NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM       10  // Msg_com_con structure used for Client to Master communication
#define NS_STRUCT_TYPE_TOOL                       11  // Msg_com_con structure used for tools
#define NS_NDC_TYPE                               12  // Msg_com_con structure used for NDC
#define NS_STRUCT_TYPE_NJVM_LISTEN                               13  // Msg_com_con structure used for njvm listen 
#define NS_STRUCT_TYPE_NJVM_CONTROL                               14  // Msg_com_con structure used for njvm listen 
#define NS_STRUCT_TYPE_NJVM_THREAD                               15  // Msg_com_con structure used for njvm listen 
#define NS_STRUCT_TYPE_DYNAMIC_VECTOR_MON                        16 
#define NS_STRUCT_TYPE_LISTEN                       17  // Msg_com_con structure used for tools (Made for listner)
#define NS_NDC_DATA_CONN			    18  //NS-NDC data connection type, used in case of outbound connection.
#define NS_JMETER_DATA_CONN                         19  //NS-JMETER data connection
#define NS_LPS_TYPE				    20  //Msg_com_con structure used for LPS
#define NS_STRUCT_TYPE_NVM_PARENT_DATA_COM      21  // Msg_com_con structure used for NVM to Parent data communication
#define NS_STRUCT_TYPE_CHILD			22 /*bug 92660*/
#define NS_STRUCT_TYPE_LISTEN_CHILD		23
#define NS_STRUCT_TYPE_SM_THREAD_LISTEN            24  // Msg_com_con structure used for SM Thread to NVM Listen
#define NS_STRUCT_TYPE_SM_THREAD_COM            25  // Msg_com_con structure used for SM Threads to NVM communication
#endif
