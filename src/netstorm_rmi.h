/*********************************************************************
* Name: netstorm_rmi.h
* Purpose: RMI related functions
* Author: Archana
* Intial version date: 22/12/07
* Last modification date: 22/12/07
*********************************************************************/

#ifdef RMI_MODE

#define USER_BUFFER_SIZE 4096

#define CNST_JBOSS_CONN_READ_FIRST 10
#define CNST_JBOSS_CONN_GET_IP_ADDR_LENGTH_ONE 11
#define CNST_JBOSS_CONN_GET_IP_ADDR_LENGTH_TWO 12
#define CNST_JBOSS_CONN_GET_IP_ADDR 13
#define CNST_JBOSS_CONN_GET_PORT 14
#define CNST_JBOSS_CONN_READ_SECOND 15
#define CNST_RMI_CONN_READ_VERIFY 16
#define CNST_RMI_CONN_GET_IP_ADDR_LENGTH_ONE 17
#define CNST_RMI_CONN_GET_IP_ADDR_LENGTH_TWO 18
#define CNST_RMI_CONN_GET_IP_ADDR 19
#define CNST_RMI_CONN_READ_SECOND 20
#define CNST_RMI_READ 21
#define CNST_PING_ACK_READ 22

extern int max_bytevar_hash_code;
extern unsigned int (*bytevar_hash)(const char*, unsigned int);
extern int (*in_bytevar_hash)(const char*, unsigned int);

extern void handle_rmi_connect( connection* cptr, u_ns_ts_t now );
extern void handle_jboss_read( connection* cptr, u_ns_ts_t now );
extern void handle_rmi_connect_read( connection* cptr, u_ns_ts_t now );
extern void handle_rmi_read( connection* cptr, u_ns_ts_t now );
extern void handle_ping_ack_read( connection* cptr, u_ns_ts_t now );
extern void packet_idle_connection( ClientData client_data, u_ns_ts_t now );
extern inline int make_rmi_request(connection* cptr, struct iovec* vector, int* free_array);
extern void rmi_connection_retry(connection *cptr, u_ns_ts_t now);
extern void next_rmi_connection( VUser *vptr, connection* cptr, u_ns_ts_t now);
extern void execute_next_rmi_page(VUser *vptr, connection* cptr, u_ns_ts_t now, PageTableEntry_Shr* next_page);

#endif

