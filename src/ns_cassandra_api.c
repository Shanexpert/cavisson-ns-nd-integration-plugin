/************************************************************************************
 * Name          : ns_cassandra_api.c 

 * Purpose       : This file contains all API related to cassandradb protocol. 

                                       ____    connect to CassandraDB     ________
                     O        query   |    |---------------------------->|        |
                    -|- ------------> | NS |   send query to execute     |   DB   |  
                    / \               |____|<--------------------------->|________|
                                                get query result            
               
                   Every VUser is equivalent to one cassandra client.

 * Author(s)     : Ayush Kumar

 * Date          : 23 May 2018  

 * Copyright     : (c) Cavisson Systems

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *           [1] : [Author: Ayush], [Date: 23 May 2018], [Version: NS 4.1.11 #B??],
                   [Description: Initial code, Added necessary APIs of cassandradb]  
***********************************************************************************/

#include <assert.h>
#include "util.h"
#include "netstorm.h"

#include "ns_string.h"
#include "ns_alloc.h"
#include "ns_cassandra_api.h"
#include "ns_proxy_server.h"

Credentials credentials;
int cell_size = 0;

void ns_cassdb_error(CassFuture* future) {
  const char* message = NULL;
  size_t message_length = 0;
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  
  cass_future_error_message(future, &message, &message_length);
  NSTL1(vptr, NULL, "Error: %.*s\n", (int)message_length, message);
  return;
}

int ns_cassdb_execute_query_internally(char* query) {
  CassError retcass = CASS_OK;
  CassSession* cass_session = NULL;
  CassFuture* query_future = NULL;
  const CassResult* query_result_handle = NULL;
  CassStatement* statement = NULL;
  cassdb_t *cassdb = NULL; 
  const char *message = NULL;
  size_t msg_len;
  
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  
  if(!vptr)
  {
    NSTL1(vptr,NULL, "Error: ns_cassdb_execute_query_internally()- vptr not set.\n");
    return -1;
  }

  cassdb = cassdbt(vptr->httpData->cassdb);
  if(!cassdb)
  {
    NSTL1(vptr,NULL, "Error: ns_cassdb_execute_query_internally() - Failed to get CassandraDB value since cassdb is NULL\n");
    return -1;
  }
  
  cass_session = cassdb->cass_session;
  if(!cass_session)
  {
    fprintf(stderr, "Error: ns_cassdb_execute_query_internally() - Cassandra Session doesnot exist.\n");
    return -1;
  }

  statement = cass_statement_new(query, 0);
  query_future = cass_session_execute(cass_session, statement);
  NSDL2_API(vptr, NULL, "futrue = %p", query_future);
  cass_future_wait(query_future);
  retcass = cass_future_error_code(query_future);
  if (retcass != CASS_OK) {
    cass_future_error_message(query_future, &message, &msg_len);
    /*Error msg cannot be traced because it print large chunk of garbage
      for http request on console.*/
    fprintf(stderr, "Error: %.*s\n", (int)msg_len, message);
    cass_statement_free(statement);
    cass_future_free(query_future);
    query_future = NULL;
    return -1;
  }
  NSDL2_API(vptr, NULL, "query = '%s' run successfully.", query);
  query_result_handle = cass_future_get_result(query_future);
  cass_statement_free(statement);
  cass_future_free(query_future);

  if (query_result_handle == NULL){
    /* Handle error */
    NSTL1(vptr, NULL, "Error: ns_cassdb_execute_query_internally()- Failed to cass_future_get_result() execute query '%s'\n", query);
    return -1;
  }
  
  NSDL2_API(vptr, NULL, "query_result_handle = '%p'.", query_result_handle);
  cassdbt(vptr->httpData->cassdb)->result = query_result_handle;
 
  return 0;
}

void on_auth_initial(CassAuthenticator* auth,
                       void* data) {
  /*
   * This callback is used to initiate a request to begin an authentication
   * exchange. Required resources can be acquired and initialized here.
   *
   * Resources required for this specific exchange can be stored in the
   * auth->data field and will be available in the subsequent challenge
   * and success phases of the exchange. The cleanup callback should be used to
   * free these resources.
   */

  /*
   * The data parameter contains the credentials passed in when the
   * authentication callbacks were set and is available to all
   * authentication exchanges.
   */
  const Credentials* credentials = (const Credentials *)data;

  size_t username_size = strlen(credentials->username);
  size_t password_size = strlen(credentials->password);
  size_t size = username_size + password_size + 2;

  char* response = cass_authenticator_response(auth, size);

  /* Credentials are prefixed with '\0' */
  response[0] = '\0';
  memcpy(response + 1, credentials->username, username_size);

  response[username_size + 1] = '\0';
  memcpy(response + username_size + 2, credentials->password, password_size);
}

void on_auth_challenge(CassAuthenticator* auth,
                       void* data,
                       const char* token,
                       size_t token_size) {
  /*
   * Not used for plain text authentication, but this is to be used
   * for handling an authentication challenge initiated by the server.
   */
}

void on_auth_success(CassAuthenticator* auth,
                     void* data,
                     const char* token,
                     size_t token_size ) {
  /*
   * Not used for plain text authentication, but this is to be used
   * for handling the success phase of an exchange.
   */
}

void on_auth_cleanup(CassAuthenticator* auth, void* data) {
  /*
   * No resources cleanup is necessary for plain text authentication, but
   * this is used to cleanup resources acquired during the authentication
   * exchange.
   */
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_cassdb_create_obj()

 * Purpose       : This function will allocate memory for Cassandradb data structure. 

 * Input         : NA

 * Output        : NA

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Ayush], [Date: 22 May 2018], [Version: NS 4.1.11 #B??],
                   [Description: Adding inital version of this function]
*------------------------------------------------------------------------------------------------------------*/
static void ns_cassdb_create_obj()
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called, vptr = %p, Allocating CassandraDB, vptr->httpData->cassdb = %p", 
                             vptr, cassdbt(vptr->httpData->cassdb));

  MY_MALLOC_AND_MEMSET(vptr->httpData->cassdb, sizeof(cassdb_t),  "vptr->httpData->cassdb", -1);
  NSDL2_API(vptr, NULL, "After allocation of CassandraDB, vptr->httpData->cassdb = %p", cassdbt(vptr->httpData->cassdb));

  MY_MALLOC_AND_MEMSET(cassdbt(vptr->httpData->cassdb)->query_result, NS_CASSDB_MAX_LEN_10M + 1, "CassandraDB query result buffer", -1);
  cassdbt(vptr->httpData->cassdb)->query_result_len = NS_CASSDB_MAX_LEN_10M;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_cassdb_new_cluster()

 * Purpose       : This function will create new cluster for connection.   

 * Input         : NA 
 
 * Output        : return cluster for creating new session.                   

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Ayush], [Date: 23 May 2018], [Version: NS 4.1.11 #B??],
                   [Description: Adding inital version of this function] 
*------------------------------------------------------------------------------------------------------------*/
int ns_cassdb_new_cluster()
{
  CassCluster* cluster = NULL;
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method called, vptr = %p", vptr);
  
  /********************************************************************
    1. Allocate memory for CassandraDB client on vptr 
    2. Allocate memory to store query result 
  ********************************************************************/
  if(!cassdbt(vptr->httpData->cassdb))
  {
    ns_cassdb_create_obj();
    cluster = cass_cluster_new();
    if(!cluster)
    {
      NSTL1(vptr, NULL, "Error: ns_cassdb_new_cluster()- cluster not create.\n");
      return -1; 
    }
    cassdbt(vptr->httpData->cassdb)->cluster = cluster;
  }

  NSDL2_API(vptr, NULL, "Method End: client = %p", cluster);
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_cassdb_connect()

 * Purpose       : This function creates a connection to node using string as provided by the user. 

 * Input         : Host and port will make connection and check the credentials for giver user name and password.

 * Output        : Create connection and store into vptr    
                  
 * Return        : NA 

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Ayush], [Date: 23 May 2018], [Version: NS 4.1.11 #B??],
                   [Description: Adding inital version of this function] 
 *------------------------------------------------------------------------------------------------------------*/
int ns_cassdb_connect(char *host, int port, char *user, char *pass)
{
  cassdb_t *cassdb = NULL;
  CassFuture* connect_future = NULL;
  CassError retcass = CASS_OK;
  CassSession* cass_session = NULL;
  
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");

  /* Take authentication call backup and register with cluster */
  CassAuthenticatorCallbacks auth_callbacks = {
    on_auth_initial,
    on_auth_challenge,
    on_auth_success,
    on_auth_cleanup
  };

  int len = 0;
  char cassdb_host[] = "127.0.0.1";
  char l_user[NS_CASSDB_MAX_LEN_256B + 1] = "";
  char l_pass[NS_CASSDB_MAX_LEN_256B + 1] = "";

  NSDL2_API(vptr, NULL, "Method called, ip = %s, port = %d, user = %s, password = %s, protocol_enabled = 0x%0x",
                         host, port, user, pass, global_settings->protocol_enabled);
  
  if(!(global_settings->protocol_enabled & CASSDB_PROTOCOL_ENABLED))
    return -1;
 
  if(!vptr)
  {
    NSTL1(vptr,NULL, "Error: ns_cassdb_connection()- vptr not set.\n");
    return -1;
  }

  if(!user || (*user == '\0'))
  {
    NSTL1(vptr,NULL, "Error: ns_cassdb_connect() - Give valid Username and Retry!!!\n");
    return -1;
  }

  if(!pass || (*pass == '\0'))
  {
    NSTL1(vptr,NULL, "Error: ns_cassdb_connect() - Give valid Password and Retry!!!\n");
    return -1;
  }
  
  // Creating cluster 
  if(!cassdbt(vptr->httpData->cassdb))
  {
    ns_cassdb_new_cluster();
  }
 
  cassdb = cassdbt(vptr->httpData->cassdb); 
  if(!cassdb)
  {
    NSTL1(vptr,NULL, "Error: ns_cassdb_new_session()- CASSDBT POINTER IS NULL.\n");
    return -1;
  }

  if((host != NULL) && is_valid_ip(host))
    snprintf(cassdb_host, 16, "%s", host);
  else
  {
    //TODO: add to debug trace
   NSTL1(vptr,NULL, "Warning: ns_cassdb_connect() - Cassandra server Host is invalid so using localhost as Cassnadra server.\n"); 
   NS_DUMP_WARNING("ns_cassdb_connect() - Cassandra server Host is invalid so using localhost as Cassnadra server.");
  }

  if((port <= 0) || (port > 65535))
  {
    port = 9042;

    //TODO: add to debug trace
    NSTL1(vptr,NULL, "Warning: ns_cassdb_connect() - Cassnadra server port is invalid so using default port 9240.\n");
    NS_DUMP_WARNING("ns_cassdb_connect() - Cassnadra server port is invalid so using default port 9240.");
  }

  len = snprintf(l_user, NS_CASSDB_MAX_LEN_256B + 1, "%s", ns_eval_string(user));
  if(len >= NS_CASSDB_MAX_LEN_256B)
  {
     NSTL1(vptr,NULL,"Error: ns_cassdb_connect() - USER value should not exceed 256B!!!\n");
     return -1;
  }
 
  len = snprintf(l_pass, NS_CASSDB_MAX_LEN_256B + 1, "%s", ns_eval_string(pass));
  if(len >= NS_CASSDB_MAX_LEN_256B)
  {
     NSTL1(vptr,NULL,"Error: ns_cassdb_connect() - PASSWORD value should not exceed 256B!!!\n");
     return -1;
  }
 
  // connection to the host and port
  NSDL2_API(vptr, NULL, "making connection to Host = %s, Port = %d, User = %s and Password = %s", cassdb_host, port, l_user, l_pass);
  cass_cluster_set_contact_points(cassdb->cluster, cassdb_host);
  cass_cluster_set_port(cassdb->cluster, port);

  strcpy(credentials.password, l_pass);
  strcpy(credentials.username, l_user);

  NSDL2_API(vptr, NULL, "Checking credentials for given password = %s, and user = %s", credentials.password, credentials.username);
  cass_cluster_set_authenticator_callbacks(cassdb->cluster, &auth_callbacks, NULL, &credentials);
 
   //store above cluster into vptr
  if (!cassdb->cass_session) {
    cass_session = cass_session_new();
    cassdb->cass_session = cass_session; 
    if(!cassdb->cass_session)
    {
      NSTL1(vptr,NULL, "Error: ns_cassdb_connect() - Cassandra Session does not exist.\n");
      return -1;
    }
 
    connect_future = cass_session_connect(cassdb->cass_session, cassdb->cluster);
    cass_future_wait(connect_future);
    retcass = cass_future_error_code(connect_future);
    if (retcass != CASS_OK) {
      ns_cassdb_error(connect_future);
      cass_future_free(connect_future);
      return -1;
    }
    cass_future_free(connect_future);
  } 
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_cassandra_execute_query()

 * Purpose       : This function will execute user provided query. 
                   Sample URI: _cassandra_execute_queryquery);

 * Input         : query 

 * Output        : Result of the executed query.

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Ayush], [Date: 23 May 2018], [Version: NS 4.1.11 #B??],
                   [Description: Adding inital version of this function] 
*------------------------------------------------------------------------------------------------------------*/

int ns_cassdb_execute_query(char *query){
  cassdb_t *cassdb = NULL;
 
  VUser *vptr = TLS_GET_VPTR();
  
  char l_query[NS_CASSDB_MAX_LEN_4K + 1]= "";
  int len = 0, ret = 0;
  
  NSDL2_API(vptr, NULL, "Method called, vptr = %p, cassdb = %p, and query = %p", vptr, cassdb, query);

  if(!vptr)
  {
    NSTL1(vptr,NULL,"Error: ns_cassandra_execute_query()- vptr not set.\n");
    return -1;
  }

  cassdb = cassdbt(vptr->httpData->cassdb);

  if(!cassdb)
  {
    NSTL1(vptr,NULL,"Error: ns_cassandra_execute_query() - Failed to get CassandraDB value since cassdb is NULL\n");
    return -1;
  }
 
  if(!query || (*query == 0))
  {
    NSTL1(vptr,NULL,"Error: ns_cassandra_execute_query() - No query is provided.\n");
    return -1;
  }
  
  //Parameterisation  
  len = snprintf(l_query, NS_CASSDB_MAX_LEN_4K + 1, "%s", ns_eval_string(query));
  if(len >= NS_CASSDB_MAX_LEN_4K)
  {
    NSTL1(vptr,NULL,"Error: ns_cassandra_execute_query() - QUERY value should not exceed 4KB!!!\n");
    return -1;
  }
  
  ret = ns_cassdb_execute_query_internally(l_query); 
  if (ret == -1)
    return -1;
  return 0;
}


#define FILL_CASS_RESULT \
{ \
  if(free < cell_size + 1) \
  { \
    cassdb->query_result_len += NS_CASSDB_MAX_LEN_10M; \
    MY_REALLOC(cassdb->query_result, cassdb->query_result_len, "cassDB query result buffer", -1); \
    free += NS_CASSDB_MAX_LEN_10M; \
  } \
  widx += snprintf(cassdb->query_result + widx, free, "%.*s", cell_size, cell_buffer); \
  if (count_tmp < count) { \
    widx  += snprintf(cassdb->query_result + widx, free, ","); \
    NSDL4_API(vptr, NULL, "query_result = %s", cassdb->query_result); \
  } \
  free = cassdb->query_result_len - widx; \
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_cassdb_get_val()

 * Purpose       : This function will execute user provided query. 
                   Sample URI: _cassandra_execute_queryquery);

 * Input         : query 

 * Output        : Result of the executed query.

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Ayush], [Date: 23 May 2018], [Version: NS 4.1.11 #B??],
                   [Description: Adding inital version of this function] 
*------------------------------------------------------------------------------------------------------------*/
char *ns_cassdb_get_val(){
  CassIterator* iterator = NULL;
  int count = 0;
  int widx = 0;
  int free = 0;

  cassdb_t *cassdb;
  
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
 

  if(!vptr)
  {
    NSTL1(vptr,NULL,"Error: ns_cassdb_get_val()- vptr not set.\n");
    return NULL;
  }

  cassdb = cassdbt(vptr->httpData->cassdb);
  if(!cassdb)
  {
    NSTL1(vptr,NULL,"Error: ns_cassdb_get_val() - Failed to get CassandraDB value since cassdb is NULL\n");
    return NULL;
  }
 
  if(!cassdb->result)
  {
    NSTL1(vptr,NULL,"Error: ns_cassdb_get_val() - Failed to get CassandraDB value since cassdb->result is NULL\n");
    return NULL;
  }
 
  free = cassdb->query_result_len;

  /* Get values from result... */
  iterator = cass_iterator_from_result(cassdb->result);
  if (!cass_iterator_next(iterator)){
    cass_result_free(cassdb->result);
    cassdb->result = NULL;
    return NULL;
  }
 
  /*Get total no of column in table*/ 
  count = cass_result_column_count(cassdb->result);
  NSDL4_API(vptr, NULL, "Total column=%d", count); 
   /*get query data*/
  do {
    int i=0;
    int count_tmp = 1; 
    cass_bool_t bool_val = {0};
    const char* str_val = NULL;
    CassUuid u = {0};
    char us[CASS_UUID_STRING_LENGTH] = {0};
    const cass_byte_t* bytes = NULL;
    char b_local[18+1] = {0};
    char *b_ptr;
    char *cell_buffer;
    const CassValue* value = NULL;
    const CassRow* row = NULL;

    row = cass_iterator_get_row(iterator);
 
    for(i = 0; i< count; i++){
      cass_int32_t int_val = 0;
      cass_double_t double_val = 0;
      size_t b_length = 0;
      size_t s_length = 0;

      value = cass_row_get_column(row, i);
      NSDL4_API(vptr, NULL, "query_result = %p, free = %d, widx = %d", value, free, widx);
      
      CassValueType type = cass_value_type(value);
      switch (type) {
        case CASS_VALUE_TYPE_INT:
          cass_value_get_int32(value, &int_val);
          cell_size = sprintf(b_local, "%d", int_val ?int_val : 0);
          NSDL4_API(vptr, NULL, "cass_value = %d", int_val);
          cell_buffer = b_local;
          break;

        case CASS_VALUE_TYPE_BOOLEAN:
          cass_value_get_bool(value, &bool_val);
          if(!bool_val)
          {
            cell_size = 5;
            strcpy(b_local, "false");
            NSDL4_API(vptr, NULL, "cass_value = false");
          }
          else{
            cell_size = 4;
            strcpy(b_local, "true");
            NSDL4_API(vptr, NULL, "cass_value = true");
          }
          cell_buffer = b_local;
        break;

        case CASS_VALUE_TYPE_DOUBLE:
          cass_value_get_double(value, &double_val);
          cell_size = sprintf(b_local, "%f", double_val?double_val:0);
          NSDL4_API(vptr, NULL, "cass_value = %f", double_val);
          cell_buffer = b_local;
        break;

        case CASS_VALUE_TYPE_TEXT:
        case CASS_VALUE_TYPE_ASCII:
        case CASS_VALUE_TYPE_VARCHAR:
          if (cass_value_get_string(value, &str_val, &s_length) == CASS_OK)
          {
            cell_size = s_length;
            cell_buffer = (char *)str_val;
          }
          else
          {
            cell_size = 4;
            sprintf(b_local, "%s", "null");
            cell_buffer = b_local;
          }
          NSDL4_API(vptr, NULL, "cass_value = %.*s, string length =%d",(int)s_length, cell_buffer, cell_size);
        break;

        case CASS_VALUE_TYPE_UUID:
          cass_value_get_uuid(value, &u);
          cass_uuid_string(u, us);
          cell_size = strlen(us);
          NSDL4_API(vptr, NULL, "cass_value = %s cell_size = %d", us, cell_size); 
          cell_buffer = us;
        break;

        case CASS_VALUE_TYPE_BLOB:
          if(cass_value_get_bytes(value, &bytes, &b_length) == CASS_OK)
          { b_ptr = b_local;
            cell_size = 18;
            strcpy(b_ptr, "0x");
            b_ptr +=2;
            for (i = 0; i < b_length; ++i)
            {
              b_ptr += sprintf(b_ptr, "%02x", bytes[i]);
            }
            NSDL4_API(vptr, NULL, "blob data = %s", b_local);
          }
          else 
          {
            cell_size = 4;
            sprintf(b_local, "%s", "null"); 
          }
          cell_buffer = b_local;
          break;

        default:
        if (cass_value_is_null(value)) {
          cell_size = 4;
          sprintf(b_local, "%s", "null"); 
          NSDL4_API(vptr, NULL, "cass_value = null");
        } else {
          cell_size = 14;
          sprintf(b_local, "%s", "unhandle type"); 
          NSDL4_API(vptr, NULL, "cass_value = unhandle type");
        }
        cell_buffer = b_local;
        break;
      }
      FILL_CASS_RESULT;
      count_tmp++;
    }
    cell_size = 1;
    strcpy(b_local, "\n");
    cell_buffer = b_local;
    FILL_CASS_RESULT;
  } while (cass_iterator_next(iterator));

  cassdb->query_result[widx - 1] = '\0'; // reduce -1 to terminate last new line

  NSDL4_API(vptr, NULL, "query_result_len = %d, query_result = %s", widx, cassdb->query_result);
  cass_result_free(cassdb->result);
  cassdb->result = NULL; 
  return cassdb->query_result;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_cassdb_disconnect()
 
 * Purpose       : This function will free cass_*_t structures and its associated resources.

 * Input         : cluster- To point connection node
                   session- session for a cluster node. 

 * Output        : NA

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Ayush], [Date: 23 May 2018], [Version: NS 4.1.11 #B??],
                   [Description: Adding inital version of this function] 
*------------------------------------------------------------------------------------------------------------*/
int ns_cassdb_disconnect() {
  
  cassdb_t *cassdb = NULL;

  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");

  if(!vptr)
  {
    NSTL1(vptr,NULL,"Error: ns_cassdb_disconnect()- vptr not set.\n");
    return -1;
  }

  cassdb = cassdbt(vptr->httpData->cassdb);

  if(!cassdb)
  {
    NSTL1(vptr,NULL,"Error: ns_cassdb_disconnect() - Failed to get CassandraDB value since cassdb is NULL\n");
    return -1;
  }

  if(!cassdb->cass_session)
  {
    NSTL1(vptr,NULL,"Error: ns_cassdb_disconnect() - Failed to get CassandraDB value since session is NULL\n");
    return -1;
  }
  cass_session_free(cassdb->cass_session);
  cassdb->cass_session = NULL; 
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_cassdb_free_cluster_and_session()
 
 * Purpose       : This function will free cass_*_t structures and its associated resources.

 * Input         : cluster- To point connection node
                   session- session for a cluster node. 

 * Output        : NA

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Ayush], [Date: 23 May 2018], [Version: NS 4.1.11 #B??],
                   [Description: Adding inital version of this function] 
*------------------------------------------------------------------------------------------------------------*/

void ns_cassdb_free_cluster_and_session(VUser *vptr)
{
  NSDL2_API(vptr, NULL, "Method called, vptr = %p", vptr);

  if(cassdbt(vptr->httpData->cassdb)->result)
    cass_result_free(cassdbt(vptr->httpData->cassdb)->result);

  if(cassdbt(vptr->httpData->cassdb)->cass_session != NULL)
    cass_session_free(cassdbt(vptr->httpData->cassdb)->cass_session);
 
  if(cassdbt(vptr->httpData->cassdb)->cluster)
    cass_cluster_free(cassdbt(vptr->httpData->cassdb)->cluster);
  
  if(cassdbt(vptr->httpData->cassdb)->query_result)
  {
    FREE_AND_MAKE_NULL_EX(cassdbt(vptr->httpData->cassdb)->query_result, cassdbt(vptr->httpData->cassdb)->query_result_len,
                          "Freeing CassandraDB query result buffer", -1);
    cassdbt(vptr->httpData->cassdb)->query_result_len = 0;
  }
  
  FREE_AND_MAKE_NULL_EX(vptr->httpData->cassdb, sizeof(cassdb_t), "Destrying CassandraDB client", -1); 
  return;
}

