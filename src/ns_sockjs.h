#ifndef ns_sockjs_h__
#define ns_sockjs_h__


#define SOCKJS_CONNECT_ENTRIES         1024
#define SOCKJS_CLOSE_ENTRIES           1024
#define MAX_URL_LENGTH                 1024

typedef struct sockjs_connect_table
{
  int conn_id;             /*Connection ID */
  char url[MAX_URL_LENGTH];
}sockjs_connect_table;

typedef struct sockjs_close_table
{
  int conn_id;             /*Connection ID */
} sockjs_close_table;

typedef struct sockjs_close_table_shr
{
  int conn_id;             /*Connection ID */
} sockjs_close_table_shr;

extern int max_sockjs_conn;
extern unsigned short int sockjs_idx_list[65535];    //TODO:handle its size

extern int parse_ns_sockjs_connect(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx); 
extern int parse_ns_sockjs_send(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_sockjs_close(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int nsi_sockjs_close(VUser *vptr);
extern void copy_sockjs_close_table_to_shr(void);
extern void sockjs_close_connection(connection* cptr);
extern void sockjs_close_connection_ex(VUser *vptr);
#endif
