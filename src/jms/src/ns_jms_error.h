/******************************************************************
 * Name    :    error.h
 * Purpose :    This file contains error codes and methods
 * Author  :    Neha Rawat
 * Intial version date:    16/05/2019
 * Last modification date: 16/05/2019
 *****************************************************************/
#ifndef NS_JMS_ERROR_H
#define NS_JMS_ERROR_H

//Error codes
#define NS_JMS_ERROR_WRONG_USER_CONFIG     -1
#define NS_JMS_ERROR_WRONG_USER_CONFIG_MSG "Wrong user config"
#define NS_JMS_ERROR_WRONG_QT_LENGTH_MSG "Wrong Queue/topic length"
#define NS_JMS_ERROR_MAX_KEY_LEN_RCHD      -2
#define NS_JMS_ERROR_MAX_KEY_LEN_RCHD_MSG   "Entry length made by attributes passed is greater than 2048 bytes"
#define NS_JMS_ERROR_MAX_POOL_SIZE_RCHD    -3
#define NS_JMS_ERROR_MAX_POOL_SIZE_RCHD_MSG "max_pool_size cannot be smaller than or equal to 0 or greater than 0xFFFF"
#define NS_JMS_ERROR_ADD_ENTRY_HASH_MAP_FAIL -4      
#define NS_JMS_ERROR_ADD_ENTRY_HASH_MAP_FAIL_MSG "Add entry in hash map failed"
#define NS_JMS_ERROR_CONN_POOL_FINISHED    -5      
#define NS_JMS_ERROR_CONN_POOL_FINISHED_MSG "Connection pool is exhausted"
#define NS_JMS_ERROR_CONN_NOT_CONNECTED    -6      
#define NS_JMS_ERROR_CONN_NOT_CONNECTED_MSG "Not connecting"
#define NS_JMS_ERROR_LIB_ERROR_CONN_FAIL   -7  
#define NS_JMS_ERROR_LIB_ERROR_CONN_FAIL_MSG "JMS library error: Connection failed"
#define NS_JMS_ERROR_LIB_ERR_OPEN_QUEUE_FAIL -8
#define NS_JMS_ERROR_LIB_ERR_OPEN_QUEUE_FAIL_MSG "Unable to open queue for output"
#define NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL    -9      
#define NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL_MSG "Unable to put message"
#define NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL    -10      
#define NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL_MSG "Unable to get message"
#define NS_JMS_ERROR_INVALID_JPID    -11      
#define NS_JMS_ERROR_INVALID_JPID_MSG "Invalid pool id passed"
#define NS_JMS_ERROR_INVALID_JPCID    -12      
#define NS_JMS_ERROR_INVALID_JPCID_MSG "Invalid connection id passed"
#define NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE    -13      
#define NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG "Atleast one connection is made so no conf setting allowed now"
#define NS_JMS_ERROR_SETTING_VALUE_NULL      -14      
#define NS_JMS_ERROR_SETTING_VALUE_NULL_MSG "Setting value null"
#define NS_JMS_ERROR_SETTING_CONFIG          -15      
#define NS_JMS_ERROR_SETTING_CONFIG_MSG     "Error in setting config"
#define NS_JMS_ERROR_HEADER_NAME_NULL          -16      
#define NS_JMS_ERROR_HEADER_NAME_NULL_MSG   "Header name missing"  
#define NS_JMS_ERROR_HEADER_VALUE_TYPE_INVALID          -17      
#define NS_JMS_ERROR_HEADER_VALUE_TYPE_INVALID_MSG   "Header value type is invalid"  
#define NS_JMS_ERROR_HEADER_VALUE_SET_FAIL          -18      
#define NS_JMS_ERROR_HEADER_VALUE_SET_FAIL_MSG   "Failed to set header value"  
#define NS_JMS_ERROR_INVALID_CLIENT_TYPE          -19      
#define NS_JMS_ERROR_INVALID_CLIENT_TYPE_MSG   "Invalid jms_client_type(PRODUCER/CONSUMER) "  
#define NS_JMS_ERROR_CLOSE_CONNECTION          -20      
#define NS_JMS_ERROR_CLOSE_CONNECTION_MSG   "Error closing connection"  
#define NS_JMS_ERROR_MESSAGE_NULL          -21      
#define NS_JMS_ERROR_MESSAGE_NULL_MSG   "Message is NULL"  
#define NS_JMS_ERROR_LIB_ERR_CLOSE_QUEUE_FAIL -22
#define NS_JMS_ERROR_LIB_ERR_CLOSE_QUEUE_FAIL_MSG "Unable to close queue for output"
#define NS_JMS_ERROR_DIFFERENT_VPTR          -23
#define NS_JMS_ERROR_DIFFERENT_VPTR_MSG   "Virtual user is not the owner of the connection"
#define NS_JMS_ERROR_LIB_ERR_CLOSE_FAIL          -24
#define NS_JMS_ERROR_LIB_ERR_CLOSE_FAIL_MSG   "IBMMQ Close Fail"
#define NS_JMS_ERROR_HDR_ADD_FAIL          -25

#endif
