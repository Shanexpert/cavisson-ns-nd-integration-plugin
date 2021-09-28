/************************************************************************************
 * Name	         : ns_mongodb_api.c 

 * Purpose       : This file contains all API related to mongodb protocol. 

                                       ____    connect to MongoDB         ________
                     O   json query   |    |---------------------------->|        |
                    -|- ------------> | NS |   send query to execute     |   DB   |  
                    / \               |____|<--------------------------->|________|
                                                get query result            
               
                   Every VUser is equivalent to one mongo clinet.

 * Author(s)     : Punnet Singh and Nisha Agarwal

 * Date          : 14 March 2017  

 * Copyright     : (c) Cavisson Systems

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *           [1] : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Initial code, Added necessary APIs of mongodb]  
             [2] : [Author: Puneet Singh],[Date: 6 April 2017],[Version: NS 4.1.8 #B??],
                   [Description: Initial code, Added necessary APIs of mongodb].
***********************************************************************************/

#include "util.h"
#include "netstorm.h"

#include "ns_string.h"
#include "ns_alloc.h"
#include "ns_mongodb_api.h" 

#include "ns_proxy_server.h" 


const char *mongodb_protocol = "mongodb://";

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_init()

 * Purpose       : This function will initlase VUser as mongo client. 
                   This function will be called once at start NVMs. 

 * Input         : NA

 * Output        : NA

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
 *          [2]  : [Author: Puneet Singh], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function]   
*------------------------------------------------------------------------------------------------------------*/
void ns_mongodb_init()
{
  NSDL2_API(NULL, NULL, "Method called");
  //This function will call from ns_child.c

  if(!(global_settings->protocol_enabled & MONGODB_PROTOCOL_ENABLED))
    return;

  mongoc_init();

  NSDL2_API(NULL, NULL, "MondoDB initialized for NVM %d", my_port_index);
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_create_obj()

 * Purpose       : This function will allocate memory for mongodb data structure. 

 * Input         : NA

 * Output        : NA

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function]
 *          [2]  : [Author: Puneet Singh], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
*------------------------------------------------------------------------------------------------------------*/
static int ns_mongodb_create_obj() 
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  if(!mongodbt(vptr->httpData->mongodb))
  {
    NSDL2_API(vptr, NULL, "Allocating MongoDB, vptr->httpData->mongodb = %p", mongodbt(vptr->httpData->mongodb));
    MY_MALLOC_AND_MEMSET(vptr->httpData->mongodb, sizeof(mongodb_t),  "vptr->httpData->mongodb", -1);
    NSDL2_API(vptr, NULL, "After allocation of MongoDB, vptr->httpData->mongodb = %p", mongodbt(vptr->httpData->mongodb));

    MY_MALLOC_AND_MEMSET(mongodbt(vptr->httpData->mongodb)->query_result, NS_MONGODB_MAX_LEN_10M + 1, "MongoDB query result buffer", -1);
    mongodbt(vptr->httpData->mongodb)->query_result_len = NS_MONGODB_MAX_LEN_10M;
  }
return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_connect()

 * Purpose       : This function creates a client using URI string as provided by the user. 
                                       Username:Password@  IP:  Port /? auth_DatabaseName    
                   Sample URI: "mongodb://Puneet:arrow@127.0.0.1:27017/?authSource=mydb"

 * Input         : uri_string (A string containing the MongoDB connection URI).

 * Output        : Create mongodb client (i.e. mongoc_client_t) and store into vptr    
                  
 * Return        : 0 -> On success
                  -1 -> On failure

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
            [2]  : [Author: Puneet Singh], [Date: 22 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding argument user, pass, dbname in function ns_mongodb_connect] 
 *------------------------------------------------------------------------------------------------------------*/
int ns_mongodb_connect(char *ip, int port, short auth_type, char *user, char *pass, char *dbname)
{
  char mongodb_ip[16] = "127.0.0.1"; 
  char auth_source[13] = "?authSource=";
  char uristr[NS_MONGODB_MAX_LEN_1K + 1] = "";
  char l_user[NS_MONGODB_MAX_LEN_256B + 1] = "";
  char l_pass[NS_MONGODB_MAX_LEN_256B + 1] = "";
  char l_dbname[NS_MONGODB_MAX_LEN_256B + 1] = "";
  mongoc_client_t *client;
  int len = 0;
  
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  
  
  /********************************************************************
   Input agrument validation: 
     1. If ip is not provided set it to 127.0.0.1 (default)
     2. If port is not provided set it to 27017 (default)
     3. All agrument of type char pointer can be - 
           NULL, ""(i.e. empty string), valid string or parameter
  ********************************************************************/
  if((ip != NULL) && is_valid_ip(ip)) 
    snprintf(mongodb_ip, 16, "%s", ip);
  else
  {
    //TODO: add to debug trace
    fprintf(stderr, "Warning: ns_mongodb_connect() - MongoDB server IP is invalid so using localhost as MongoDB server.\n");
    NS_DUMP_WARNING("In API ns_mongodb_connect()- MongoDB server IP is invalid so using localhost as MongoDB server.");
  }

  if((port <= 0) || (port > 65535))
  {
    port = 27017; 

    //TODO: add to debug trace
    fprintf(stderr, "Warning: ns_mongodb_connect() - MongoDB server port is invalid so using default port 27017.\n");
    NS_DUMP_WARNING("In API ns_mongodb_connect()- MongoDB server port is invalid so using default port 27017.");
  }
  
  if(!user || (*user == '\0'))
  {
    fprintf(stderr, "Error: ns_mongodb_connect() - Give valid Username and Retry!!!\n");
    return -1;
  }

  if(!pass || (*pass == '\0'))
  {
    fprintf(stderr, "Error: ns_mongodb_connect() - Give valid Password and Retry!!!\n");
    return -1;
  } 

  if(!dbname || (*dbname == '\0'))
  {
    fprintf(stderr, "Error: ns_mongodb_connect() - Give valid DataBase name and Retry!!!\n");
    return -1;
  }
 
  //Paramerterization of input args

  len = snprintf(l_user, NS_MONGODB_MAX_LEN_256B + 1, "%s", ns_eval_string(user));
  if(len >= NS_MONGODB_MAX_LEN_256B)  
  {
     fprintf(stderr, "Error: ns_mongodb_connect() - USER value should not exceed 256B!!!\n");
     return -1;
  }

  len = snprintf(l_pass, NS_MONGODB_MAX_LEN_256B + 1, "%s", ns_eval_string(pass));
  if(len >= NS_MONGODB_MAX_LEN_256B)
  {
     fprintf(stderr, "Error: ns_mongodb_connect() - PASSWORD value should not exceed 256B!!!\n");
     return -1;
  }
  
  len = snprintf(l_dbname, NS_MONGODB_MAX_LEN_256B + 1, "%s", ns_eval_string(dbname));
  if(len >= NS_MONGODB_MAX_LEN_256B)
  {
     fprintf(stderr, "Error: ns_mongodb_connect() - DATABASE NAME value should not exceed 256B!!!\n");
     return -1;
  }
  
  /********************************************************************
    1. Allocate memory for MongoDB client on vptr 
    2. Allocate memory to store query result 
  ********************************************************************/
  ns_mongodb_create_obj();  

  NSDL2_API(vptr, NULL, "Connecting MongoDB server-> mongodb_ip = [%s], mongodb_port = [%d], "
                            "username = [%s], password = [%s] , authenticating_db = [%s]", 
                             mongodb_ip, port, l_user, l_pass, l_dbname );

  if(BASIC == auth_type)
    snprintf(uristr, NS_MONGODB_MAX_LEN_1K, "%s%s:%s@%s:%d/%s%s", mongodb_protocol, l_user, l_pass, mongodb_ip, port, auth_source, l_dbname);
  else
  {
    snprintf(uristr, NS_MONGODB_MAX_LEN_1K, "%s%s:%d/", mongodb_protocol, mongodb_ip, port);
    fprintf(stderr, "Error: ns_mongodb_connect() - Authentication Failed !!! \n");
    return -1;
  } 
  
  NSDL2_API(vptr, NULL, "uristr = [%s]", uristr);

  /********************************************************************
    Create Mongodb client i.e. mongoc_client_t and store pointer on
    vptr
  ********************************************************************/
  if(!(client = mongoc_client_new(uristr))) 
  {
    fprintf(stderr, "Error: ns_mongodb_connect() - Failed to connect MongoDB server with URI '%s'.\n", uristr);
    return -1;
  }

  //store above client into vptr
  mongodbt(vptr->httpData->mongodb)->client = client;

  NSDL2_API(vptr, NULL, "Method End: client = %p", client);
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_select_db_coll()

 * Purpose       : This function will get a collection and database as provided by the user.   
                                        CAPI  (Database Name, Collection Name)  
                   Sample URI: ns_mongodb_select_db_coll(mydb, einfo);

 * Input         : dbname - Provide name of desired database
                 : collname - Provide name of desired collection.
 
 * Output        : On success - Returns a newly allocated collection (i.e. mongoc_collection_t)
                   On Failure - NULL

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
 *          [2]  : [Author: Puneet Singh], [Date: 22 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding local buffer for parameterization]
*------------------------------------------------------------------------------------------------------------*/
int ns_mongodb_select_db_coll(char *dbname , char *collname)
{
  mongoc_collection_t  *collection = NULL;
  mongodb_t *mongodb = NULL;
  mongoc_client_t *client = NULL;
  char *db = NULL;
  char l_dbname[NS_MONGODB_MAX_LEN_256B + 1] = "";
  char l_collname[NS_MONGODB_MAX_LEN_256B +1] = "";
  int len = 0;
  
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called."); 

  if(!vptr)
  {
    fprintf(stderr, "Error: ns_mongodb_select_db_coll()- vptr not set.\n"); 
    return -1;
  }

  mongodb = mongodbt(vptr->httpData->mongodb);

  if(!mongodb)
  {
    fprintf(stderr, "Error: ns_mongo_select_db_coll() - Mongo Client doesnot exist.\n");
    return -1;
  }
 
  client =  mongodb->client;

  if(!client)
  {
    fprintf(stderr, "Error: ns_mongo_select_db_coll() - Mongo Client doesnot exist.\n");
    return -1;
  }
 
  db = (!dbname || (*dbname == 0))?((mongodb->dbname)?mongodb->dbname:NULL):dbname;

  /* DataBase and collection name is mandatory */
  if(!db)
  {
    fprintf(stderr, "Error: ns_mongodb_select_db_coll() - DataBase name is mandatory argument "
                    "so please provide a valid MongoDB name and retry!\n");
    return -1;
  }

  if(!collname || (*collname == 0))
  {
    fprintf(stderr, "Error: ns_mongodb_select_db_coll() - Collection name is mandatory argument "
                    "so please provide a valid collection name and retry!\n");
    return -1;
  }
  
  //Parameterisation
  len = snprintf(l_dbname, NS_MONGODB_MAX_LEN_256B + 1, "%s", ns_eval_string(db));
  if(len >=NS_MONGODB_MAX_LEN_256B)
  {
     fprintf(stderr, "Error: ns_mongodb_select_db_coll() - DATABASE NAME value should not exceed 256B!!!\n");
     return -1;
  }
 
  len = snprintf(l_collname, NS_MONGODB_MAX_LEN_256B + 1, "%s", ns_eval_string(collname));
  if(len >= NS_MONGODB_MAX_LEN_256B)
  {
     fprintf(stderr, "Error: ns_mongodb_select_db_coll() - COLLECTION NAME value should not exceed 256B!!!\n");
     return -1;
  }
 
  NSDL2_API(vptr, NULL, "Select database = [%s], and collection = [%s]", l_dbname, l_collname); 

  if(!(collection = mongoc_client_get_collection(client, l_dbname, l_collname)))
  {
    fprintf(stderr, "Error: ns_mongodb_select_db_coll() - Failed to select collection '%s' of database '%s'\n", l_collname, l_dbname);
    return -1;
  }

  //Store this dbname and allocated collection in vptr
  int l_len = strlen(l_dbname);  

  if(!mongodb->dbname_len || (mongodb->dbname_len < l_len))
  {
    MY_REALLOC(mongodb->dbname, l_len + 1, "MongoDB database name buffer", -1);
    mongodb->dbname_len = l_len;
  }

  strncpy(mongodb->dbname, l_dbname, l_len);
  mongodb->dbname[l_len] = '\0';

  mongodb->collection = collection;

  NSDL2_API(vptr, NULL, "Method End: dbname = %s, collection = %p", mongodb->dbname, collection);
  
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_execute_direct()

 * Purpose       : This function will execute user provided JSON query. 
                   Sample URI: ns_mongodb_execute_direct(query);

 * Input         : query - Query in json format.

 * Output        : Result of the executed query.

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
 *          [2]  : [Author: Puneet Singh], [Date: 23 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding local buffers for parameterization]
*------------------------------------------------------------------------------------------------------------*/
int ns_mongodb_execute_direct(char *query)
{
  bson_t *pipeline = NULL;
  bson_error_t  error;
  mongoc_cursor_t *cursor = NULL;
  mongodb_t *mongodb = NULL;
  mongoc_collection_t *collection = NULL;
  
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");

  mongodb = mongodbt(vptr->httpData->mongodb);
  char l_query[NS_MONGODB_MAX_LEN_4K + 1]= "";    
  int len = 0;


  if(!mongodb)
  {
    fprintf(stderr, "Error: ns_mongodb_execute_direct()- MONGODBT POINTER IS NULL.\n");
    return -1;
  }

  collection = mongodb->collection;

  if(!collection)
  {
    fprintf(stderr, "Error: ns_mongodb_execute_direct()- No collection is selected please "
                    "select a collection by using API ns_mongodb_select_db_coll().\n"); 
    return -1;    
  }

  if(!query || (*query == 0))
  {
    fprintf(stderr, "Error: ns_mongodb_execute_direct()- No query is provided.\n"); 
    return -1;    
  }
  
  //Parameterisation
  len = snprintf(l_query, NS_MONGODB_MAX_LEN_4K + 1, "%s", ns_eval_string(query));
  if(len >= NS_MONGODB_MAX_LEN_4K)
  {
     fprintf(stderr, "Error: ns_mongodb_execute_direct() - QUERY value should not exceed 4KB!!!\n");
     return -1;
  }

  NSDL2_API(vptr, NULL, "Coverting provided json query into bson query = %s", l_query);

  //query length will be take care by mongo API bson_new_from_json so passing it -1

  if(!(pipeline = bson_new_from_json((uint8_t *)l_query, -1, &error)))
  {
    fprintf(stderr, "Error: ns_mongodb_execute_direct()- Failed to convert json query into bson.\n"); 
    return -1;    
  }

  cursor = mongoc_collection_aggregate(collection, MONGOC_QUERY_NONE, pipeline, NULL, NULL);
 
  if(!cursor)
  {
    fprintf(stderr, "Error: ns_mongodb_execute_direct()- Failed to execute query '%s'\n", l_query); 
    return -1;    
  }

  //Set cursor into vptr 
  NSDL2_API(vptr, NULL, "cursor = %p", cursor); 
  mongodbt(vptr->httpData->mongodb)->cursor = cursor;

  NSDL2_API(vptr, NULL, "Method End: Query = %s", l_query);

  //Free memory allocated for pipline
  bson_destroy (pipeline);
  
  return 0; 
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_collection_find()

 * Purpose       : This function will execute user provided JSON query and do FIND operation. 
                                                CAPI  (Query, Limit)     
                   Sample URI: ns_mongodb_collection_find({"name":"Puneet"},2).

 * Input         : query - Query in json format.

 * Output        : Result will be displayed which the cursor pointer is pointing from the database if it is valid.

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
 *          [2]  : [Author: Puneet Singh], [Date: 25 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding local buffer for parameterization and \
                                 Add limit to fetch the data from database.]
*------------------------------------------------------------------------------------------------------------*/
int ns_mongodb_collection_find(char *query, int limit)
{
  bson_t *pipeline = NULL;
  bson_error_t  error;
  bson_t *opts = NULL;
  mongodb_t *mongodb = NULL;
  mongoc_cursor_t *cursor = NULL;
  mongoc_collection_t  *collection = NULL;
  char l_query[NS_MONGODB_MAX_LEN_4K + 1] = "";
  int len = 0;
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  mongodb = mongodbt(vptr->httpData->mongodb);

  
  if(!mongodb)
  {
    fprintf(stderr, "Error: ns_mongodb_collection_find()- MONGODBT POINTER IS NULL.\n");
    return -1;
  }
  
  collection = mongodb->collection;
 
  if((!collection) || (collection == 0))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_find()- No collection is selected please "
                    "select a collection by using API ns_mongodb_select_db_coll().\n");
    return -1;
  }

  if(!query || (*query == 0))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_find()- No query is provided.\n");
    return -1;
  }
  
  if(limit<0)
  {
    fprintf(stderr, "Error: ns_mongodb_collection_find()- Limit value must be non-negative, but received: %d\n",limit);
    return -1;
  }

  if(limit == 0)
  {
    fprintf(stderr, "Warning: ns_mongodb_collection_find()- Limit field value is 0.So retrieving all data.\n");
    NS_DUMP_WARNING("In API ns_mongodb_collection_find()- Limit field value is 0.So retrieving all data.");
  }
 
  //Parameterisation 
  len = snprintf(l_query, NS_MONGODB_MAX_LEN_4K + 1, "%s", ns_eval_string(query));
  if(len >= NS_MONGODB_MAX_LEN_4K)
  {
     fprintf(stderr, "Error: ns_mongodb_collection_find() - QUERY value should not exceed 4KB!!!\n");
     return -1;
  }

  opts = BCON_NEW ("limit", BCON_INT64 (limit));
 
  NSDL2_API(vptr, NULL, "Coverting provided json query into bson query = %s",l_query);

  //query length will be take care by mongo API bson_new_from_json so passing it -1

  if(!(pipeline = bson_new_from_json((uint8_t *)l_query, -1, &error)))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_find()- Failed to convert json query into bson.\n");
    return -1;
  }
  
  if(!(cursor = mongoc_collection_find_with_opts(collection, pipeline, opts, NULL)))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_find() - Failed to find query '%s'\n", l_query);
    return -1;
  }

  //Set cursor into vptr 
  NSDL2_API(vptr, NULL, "cursor = %p", cursor);
  mongodbt(vptr->httpData->mongodb)->cursor = cursor;

  NSDL2_API(vptr, NULL, "Method End: Query = %s ,Limit = %d \n", l_query, limit);

  //Free memory allocated for pipline
  bson_destroy (pipeline);

  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_collection_insert()

 * Purpose       : This function will insert user provided JSON query. 
                   Sample URI: ns_mongodb_collection_insert(query);

 * Input         : query - Query in json format.

 * Output        : Does not give any output it just insert the data into Database.

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Nisha], [Date: 27 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
 *          [2]  : [Author: Puneet Singh], [Date: 29 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding local buffer for parameterization]
*------------------------------------------------------------------------------------------------------------*/
int ns_mongodb_collection_insert(char *query)
{
  bson_t *pipeline = NULL;
  bson_error_t  error;
  char l_query[NS_MONGODB_MAX_LEN_4K + 1] = "";    
  mongoc_collection_t *collection = NULL;
  mongodb_t *mongodb = NULL;
  VUser *vptr = TLS_GET_VPTR();
  int len = 0;
  
  NSDL2_API(vptr, NULL, "Method called.");  
  
  mongodb = mongodbt(vptr->httpData->mongodb);

  if(!mongodb)
  {
    fprintf(stderr, "Error: ns_mongodb_collection_insert()- MONGODBT POINTER IS NULL.\n");
    return -1;
  }

  collection = mongodb->collection;

  if(!collection)
  {
    fprintf(stderr, "Error: ns_mongodb_collection_insert()- No collection is selected please "
                    "select a collection by using API ns_mongodb_select_db_coll().\n");
    return -1;
  }

  if(!query || (*query == 0))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_insert()- No insert query is provided.\n");
    return -1;
  }

  //Parameterisation
  len = snprintf(l_query, NS_MONGODB_MAX_LEN_4K + 1, "%s", ns_eval_string(query));
  if(len >= NS_MONGODB_MAX_LEN_4K)
  {
    fprintf(stderr, "Error: ns_mongodb_collection_insert() - QUERY value should not exceed 4KB!!!\n");
    return -1;
  }

  NSDL2_API(vptr, NULL, "Coverting provided json query into bson query = %s", l_query);

  //query length will be take care by mongo API bson_new_from_json so passing it -1
 
  if(!(pipeline = bson_new_from_json((uint8_t *)l_query, -1, &error)))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_insert()- Failed to convert json query into bson.\n");
    return -1;
  }

  if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, pipeline, NULL, &error))
  {
    fprintf (stderr, "%s\n", error.message);
    return -1;
  }

  NSDL2_API(vptr, NULL, "Method End: Query = %s", l_query); 

  //Free memory allocated for pipline
  bson_destroy (pipeline);

  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_delete()

 * Purpose       : This function will delete user provided JSON query. 
                                  CAPI  (Database Name, Collection Name, query)
                   Sample URI: ns_mongodb_delete(mydb,einfo,{"name":"Puneet"});

 * Input         : Query - Query in json format.

 * Output        : If the Query is Matched in the Database then that document,collection will be deleted.

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Puneet Singh], [Date: 6 April 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function and supporting parameterization] 
*------------------------------------------------------------------------------------------------------------*/
int ns_mongodb_delete(char *dbname, char *collname, char *query, int del_type)
{
  mongoc_collection_t *collection = NULL;
  bson_error_t error;
  bson_t *pipeline = NULL;
  mongoc_database_t *database = NULL;
  mongodb_t *mongodb = NULL;
  mongoc_client_t *client = NULL;
  char *db = NULL;
  char l_dbname[NS_MONGODB_MAX_LEN_256B + 1] = "";
  char l_collname[NS_MONGODB_MAX_LEN_256B + 1] = "";
  char l_query[NS_MONGODB_MAX_LEN_4K + 1]= "";
  int len = 0;
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");


  if(!vptr)
  {
    fprintf(stderr, "Error: ns_mongodb_delete()- vptr not set.\n");
    return -1;
  }

  mongodb = mongodbt(vptr->httpData->mongodb);

  if(!mongodb)
  {
    fprintf(stderr, "Error: ns_mongodb_delete() - Mongo Client doesnot exist.\n");
    return -1;
  }

  client =  mongodb->client;

  if(!client)
  {
    fprintf(stderr, "Error: ns_mongodb_delete() - Mongo Client doesnot exist.\n");
    return -1;
  }

  db = (!dbname || (*dbname == 0))?((mongodb->dbname)?mongodb->dbname:NULL):dbname;

  /* DataBase and collection name is mandatory */
  if(!db)
  {
    fprintf(stderr, "Error: ns_mongodb_delete() - DataBase name is mandatory argument "
                    "so please provide a valid MongoDB name and retry!\n");
    return -1;
  }
  
  if((!collname) || (collname == 0))
  {
    fprintf(stderr, "Error: ns_mongodb_delete()- No collection is selected please "
                    "select a collection by using API ns_mongodb_delete().\n");
    return -1;
  }

  if(!query || (*query == 0))
  {
    fprintf(stderr, "Error: ns_mongodb_delete- No query is provided.\n");
    return -1;
  }

  //Parameterisation
  len = snprintf(l_dbname, NS_MONGODB_MAX_LEN_256B + 1, "%s", ns_eval_string(db));
  if(len >= NS_MONGODB_MAX_LEN_256B)
  {
     fprintf(stderr, "Error: ns_mongodb_delete() - DATABASE NAME value should not exceed 256B!!!\n");
     return -1;
  }

  len = snprintf(l_collname, NS_MONGODB_MAX_LEN_256B + 1, "%s", ns_eval_string(collname));
  if(strlen(collname) >= NS_MONGODB_MAX_LEN_256B)
  {
     fprintf(stderr, "Error: ns_mongodb_delete() - COLLECTION NAME value should not exceed 256B!!!\n");
     return -1;
  }

  len = snprintf(l_query, NS_MONGODB_MAX_LEN_4K + 1, "%s", ns_eval_string(query));
  if(len >= NS_MONGODB_MAX_LEN_4K)
  {
     fprintf(stderr, "Error: ns_mongodb_delete() - QUERY value should not exceed 4KB!!!\n");
     return -1;
  }
 
  NSDL2_API(vptr, NULL, "Select database = [%s], Collection = [%s] and Query = [%s] ", l_dbname, l_collname, l_query);

  if (!(collection = mongoc_client_get_collection(client, l_dbname, l_collname)))
  {
    fprintf(stderr, "Error: ns_mongodb_delete() - Failed to select collection '%s' of database '%s'\n", l_collname, l_dbname);
    return -1;
  }
  
  if (!(database = mongoc_client_get_database (client, l_dbname)))
  {
    fprintf(stderr, "Error: ns_mongodb_delete() - Failed to select Database '%s'\n", l_dbname);
    return -1; 
  }

  //Now Converting the given deleting document.

  if(!(pipeline = bson_new_from_json((uint8_t *)l_query, -1, &error)))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_insert()- Failed to convert json query into bson.\n");
    return -1;
  }

  if(0 == del_type)
  {
    if (!mongoc_collection_remove (collection, MONGOC_REMOVE_SINGLE_REMOVE , pipeline, NULL, &error)) 
    {
      fprintf (stderr, "Deleting Document is failed: %s\n", error.message);
    }
  }
  
  else if(1 == del_type)
  {
    if (!mongoc_collection_drop_with_opts (collection,NULL, &error)) 
    {
      fprintf (stderr, "Deleting Collection is failed: %s\n", error.message);
    }
  }

  else if(2 == del_type)
  {
    if(!mongoc_database_drop_with_opts(database, NULL, &error))
    {
      fprintf (stderr, "Deleting Database is failed: %s\n", error.message);
    }

  }
 
  else 
  {  
    fprintf(stderr, "Enter Correct DEL_TYPE: %s\n", error.message); 
  } 

  NSDL2_API(vptr, NULL, "Method End: DBname using reference with mongodb = %s, Collection = %p, "
                        "Deleted Query = %s, DBname = %s", mongodb->dbname, collection, l_query, l_dbname);  

  bson_destroy (pipeline);
  mongoc_collection_destroy (collection);

  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_collection_update()

 * Purpose       : This function will update user provided JSON query if the given query is right. 
                                     CAPI  (query,updator)
                   Sample URI: ns_mongodb_collection_update({"name":"Puneet"},{"name":"Puneet SIngh"});

 * Input         : Query - Query in json format in which you want to update,
                   Updator- In this the given query is update.

 * Output        : Result of the updated query.

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Puneet Singh and Nisha Agarwal], [Date: 27 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function and supporting parameterization] 
*------------------------------------------------------------------------------------------------------------*/
int ns_mongodb_collection_update(char *query,char *updator)
{
  bson_t *pipeline1 = NULL;
  bson_t *pipeline2 = NULL;
  bson_error_t  error;
  mongodb_t *mongodb = NULL;
  mongoc_collection_t  *collection = NULL;
  char l_query[NS_MONGODB_MAX_LEN_4K + 1] = "";
  char l_updator[NS_MONGODB_MAX_LEN_4K + 1] = "";
  int len = 0;
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  
  mongodb = mongodbt(vptr->httpData->mongodb);
  
  //Its not Sure its right or not yet - Puneet Singh
  if(!mongodb)
  {
    fprintf(stderr, "Error: ns_mongodb_collection_update()- MONGODBT POINTER IS NULL.\n");
    return -1;
  }

  collection = mongodb->collection;

  if(!collection)
  {
    fprintf(stderr, "Error: ns_mongodb_collection_update()- No collection is selected please "
                    "select a collection by using API ns_mongodb_select_db_coll().\n");
    return -1;
  }

  if(!query || (*query == 0))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_update()- No query is provided.\n");
    return -1;
  }
 
  if(!updator || (*updator == 0))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_update()- No updator is provided.\n");
    return -1;
  }
  
  //Parameterisation
  len = snprintf(l_query, NS_MONGODB_MAX_LEN_4K + 1, "%s", ns_eval_string(query));
  if(len >= NS_MONGODB_MAX_LEN_4K)
  {
     fprintf(stderr, "Error: ns_mongodb_collection_update() - QUERY value should not exceed 4KB!!!\n");
     return -1;
  }
  
  len = snprintf(l_updator, NS_MONGODB_MAX_LEN_4K + 1, "%s", ns_eval_string(updator));
  if(len >= NS_MONGODB_MAX_LEN_4K)
  {
     fprintf(stderr, "Error: ns_mongodb_collection_update() - UPDATOR value should not exceed 4KB!!!\n");
     return -1;
  }
 
  //query length will be take care by mongo API bson_new_from_json so passing it -1
 
  NSDL2_API(vptr, NULL, "Coverting provided json query into bson query = %s", l_query);
   
  if(!(pipeline1 = bson_new_from_json((uint8_t *)l_query, -1, &error)))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_update()- Failed to convert json query into bson in query.\n");
    return -1;
  }
 
  NSDL2_API(vptr, NULL, "Coverting provided json updator into bson query = %s", l_updator);

  if(!(pipeline2 = bson_new_from_json((uint8_t *)l_updator, -1, &error)))
  {
    fprintf(stderr, "Error: ns_mongodb_collection_update()- Failed to convert json query into bson in updator.\n");
    return -1;
  }

  if (!mongoc_collection_update(collection, MONGOC_UPDATE_NONE, pipeline1, pipeline2, NULL, &error))
  {
      fprintf (stderr, "%s\n", error.message);
      goto fail;
  }

  NSDL2_API(vptr, NULL, "Method End, query = %s, updator = %s", query, updator);
  
  fail:
      bson_destroy(pipeline1);
      bson_destroy(pipeline2);

  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_get_val()

 * Purpose       : This function will execute user provided JSON query. 

 * Output        : Result of the executed query.

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
 *          [2]  : [Author: Puneet Singh], [Date: 22 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function]
*------------------------------------------------------------------------------------------------------------*/
char *ns_mongodb_get_val()
{
  int len = 0;
  int wbytes = 0;
  int widx = 0;
  int free = 0;
  char *result = NULL;
  const bson_t *doc = NULL;
  mongoc_cursor_t *cursor = NULL;
 
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  mongodb_t *mongodb = mongodbt(vptr->httpData->mongodb);


  if(!mongodb)
  {
    fprintf(stderr, "Error: ns_mongodb_get_val() - Failed to get MongoDB value since mongodb is NULL\n");
    return NULL;
  }

  cursor = mongodb->cursor;
  if(!cursor)
  {
    fprintf(stderr, "Error: ns_mongodb_get_val() - Failed to get MongoDB value since cursor is NULL\n");
    return NULL;
  }

  //query_result 
  free = mongodb->query_result_len;
  while(mongoc_cursor_next(cursor, &doc)) 
  {
    result = bson_as_json(doc, NULL);
    len = result?strlen(result):0; 

    free = mongodb->query_result_len - widx; 

    NSDL4_API(vptr, NULL, "result = %p, len = %d, free = %d, widx = %d", result, len, free, widx);
    if(free < len)
    {
      MY_REALLOC(mongodb->query_result, mongodb->query_result_len + NS_MONGODB_MAX_LEN_10M, "MongoDB query result buffer", -1);
      mongodb->query_result_len += NS_MONGODB_MAX_LEN_10M;
      free += NS_MONGODB_MAX_LEN_10M; 
    }
    
    wbytes = snprintf(mongodb->query_result + widx, free, "%s\n", result);
    widx += wbytes; 
    
    NSDL4_API(vptr, NULL, "widx = %d", widx);
    //TODO: Why every time we need to free result
    if(result)
      bson_free(result);
  }

  mongodb->query_result[widx - 1] = '\0'; // reduce -1 to terminate last new line
  
  NSDL4_API(vptr, NULL, "query_result_len = %d, query_result = %s", widx, mongodb->query_result);
  return mongodb->query_result;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_cleanup()
 
 * Purpose       : This function will destroy mongoc_*_t and bson_t structures and its associated resources.

 * Input         : cursor- To point first collection from the database
                   collection- Name of the desired collection.
                   client - Provide client return by API ns_mongodb_connect() 

 * Output        : NA

 * Mod. History  : [Author: ], [Date: ], [Version: ], [Description: ]
 *          [1]  : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
 *          [2]  : [Author: Puneet Singh], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function]
*------------------------------------------------------------------------------------------------------------*/
void ns_mongodb_client_cleanup(VUser *vptr)
{
  NSDL4_API(vptr, NULL, "Method called, vptr = %p", vptr);

  /* Don't change sequence of destoring memory because each depend on other */
  if(mongodbt(vptr->httpData->mongodb)->cursor)
    mongoc_cursor_destroy(mongodbt(vptr->httpData->mongodb)->cursor);

  if(mongodbt(vptr->httpData->mongodb)->collection)
    mongoc_collection_destroy(mongodbt(vptr->httpData->mongodb)->collection);

  if(mongodbt(vptr->httpData->mongodb)->client)
    mongoc_client_destroy(mongodbt(vptr->httpData->mongodb)->client);

  if(mongodbt(vptr->httpData->mongodb)->dbname)
  {
    FREE_AND_MAKE_NULL_EX(mongodbt(vptr->httpData->mongodb)->dbname, mongodbt(vptr->httpData->mongodb)->dbname_len, \
    "Freeing MongoDB database name buffer", -1);
    mongodbt(vptr->httpData->mongodb)->dbname_len = 0;
  }

  if(mongodbt(vptr->httpData->mongodb)->query_result)
  {
    FREE_AND_MAKE_NULL_EX(mongodbt(vptr->httpData->mongodb)->query_result, mongodbt(vptr->httpData->mongodb)->query_result_len, 
                          "Freeing MongoDB query result buffer", -1);
    mongodbt(vptr->httpData->mongodb)->query_result_len = 0;
  }

  FREE_AND_MAKE_NULL_EX(vptr->httpData->mongodb, sizeof(mongodb_t), "Destrying MongoDB client", -1);
}

/*------------------------------------------------------------------------------------------------------------- 
 * Name          : ns_mongodb_close()
 * Purpose       : This function will clean and release any lingering allocated memory.
 * Input         : NA
 * Output        : NA
 * Mod. History  : [Author: ], [Date: ], [Version: , [Description: ]
 *          [1]  : [Author: Nisha], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function] 
 *          [2]  : [Author: Puneet Singh], [Date: 14 March 2017], [Version: NS 4.1.8 #B??],
                   [Description: Adding inital version of this function]
*------------------------------------------------------------------------------------------------------------*/
void ns_mongodb_close()
{
  //TODO: need to think
  mongoc_cleanup();
}
