#ifndef NS__DNS_H
#define NS__DNS_H

typedef struct {
  uint16_t assert_rr_type; 
  u_char assert_rr_data[NS_MAXCDNAME]; 
} DnsReqAssert;


// DNS Query request. Saved for checking response
typedef struct {
  u_char tcp_len_buf[2];
  uint16_t query_len;
  u_char header_buf[HFIXEDSZ]; //macros will work correctly on the buffer
  HEADER header;		// <arpa/nameser_compat.h> 
  u_char qname[NS_MAXCDNAME];	//encoded domain name(e.g. 3www6google3com0)
  uint16_t qtype; // Query type (e.g. A/SOA etc)
  uint16_t qclass; // Query class. Currently always IN
  DnsReqAssert assert;
} DnsReq;

/* This contains the no of times a given value was found in the response 
 * received -as requested by the ASSER_RR_XX keyword in the 
 * script. Since the rdata is processed and thrown away, we keep this count 
 * across rr's that we received and check after all rr's are processed.
 */

typedef struct {
  u_char rr_type_found;
  u_char rr_data_found;
  //uint16_t assert_rr_type; 
  //u_char *assert_rr_data; 
} DnsRespAssert;

// DNS Query respose. Saved for validation/parsing
typedef struct {
  // Fields coming in response
  uint16_t len;    //length of response taken from the first 2 bytes of message
  uint16_t tcp_count;
  u_char tcp_len_buf[2];
  HEADER header;	// <arpa/nameser_compat.h> 
  u_char *buf;    //buffer to recieve data for response
  //u_char *qname;     // Allocated (Compressed e.g. 3www6google3com0)
  //uint16_t name_len;	// length of the encoded name (including the last 0)
  uint16_t qtype;        // Query type coming in response
  uint16_t qclass;       // Query class coming in response
  //u_char indir_buf[2];   //2 bytes of indirection for name
  //u_char indir_flag;     //tells whether we found indirection in name
  ns_rr	rr;	     // <arpa/nameser.h> (Resource record)

  // Other variables used for saving/processing response
  // TODO: Eliminate qfixed_buf
  //u_char rr_fixed_buf[RRFIXEDSZ]; //use for reading if struct doesnt work
  //u_char header_buf[HFIXEDSZ]; //use for reading if struct doesnt work
  //u_char qfixed_buf[QFIXEDSZ]; // temp areas to store type and class
  //uint16_t rr_count;		// Number of RR processed so far
  uint16_t count;	    	//common counter for all states
  DnsRespAssert assert;		// keep count of matches in response for asserts
} DnsRes;


// This structure is allocated for each DNS query and stored
// in cptr in cptr->data. It is deallocated after action (page) is complete

typedef struct {
  DnsReq dns_req;
  DnsRes dns_resp;
} DnsData;

#define DNSDATA ((DnsData*)(cptr->data))

/* DNS protocol states */

#define ST_DNS_HEADER	1
#define ST_DNS_QNAME	2	
#define ST_DNS_QFIXED	3	
#define ST_DNS_RR_NAME	4	
#define ST_DNS_RR_FIXED	5	
#define ST_DNS_RR_RDATA	6	

#if 0
/* states for reading the labels and length octets  in the name */
#define ST_NAME_LABEL_LEN	1
#define ST_NAME_LABEL		2
#define ST_NAME_DONE		3
#endif

/* strings to describe states above for DNS */
extern char g_dns_st_str[][0xff];

extern int handle_dns_read( connection *cptr, u_ns_ts_t now );
extern void dns_init_connection_data (connection *cptr);
extern void delete_dns_timeout_timer(connection *cptr);
extern char *dns_state_to_str(int state);
extern void dns_timeout_handle( ClientData client_data, u_ns_ts_t now );
extern int parse_dns_query(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname);
extern int kw_set_dns_timeout(char *buf, int *to_change, char *err_msg);
extern int dns_make_request(connection *cptr, u_ns_ts_t now, int nb_mode, char *server_name);
extern void debug_log_dns_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);
extern  int get_value_from_segments(connection *cptr, char *buf, StrEnt_Shr* seg_tab_ptr, char *for_which, int max);  
#endif /* NS__DNS_H */

