
#include "ns_data_types.h"

#define MAX_EVENT_DESC_SIZE 4096 
#define MAX_EVENT_ATTR_SIZE 2048 
#define MAX_HOST_NAME_SIZE 256

#define MAX_BUF_LEN_FOR_EL  MAX_EVENT_DESC_SIZE + MAX_EVENT_ATTR_SIZE + MAX_HOST_NAME_SIZE+ 1024

// Filtering mode
#define NS_EVENT_NO_FILTER     0
#define NS_EVENT_STATE_FILTER  1
#define NS_EVENT_COUNT_FILTER  2
#define NS_EVENT_TIME_FILTER   3
#define NS_EVENT_ALL_FILTER    9

#define UNSIGNED_LONG 8
#define UNSIGNED_INT  4
#define UNSIGNED_SHORT 2
#define UNSIGNED_CHAR  1

// Linked List of Events
typedef struct events {
  char *event_value;        // Event Value
  char *attr_value;         // Attribute Value (comma separated)
  unsigned char state;      // Current State of an Event   
  unsigned int num_event;   // Number of events recieved of this event
  u_ns_ts_t last_event_time;   // Relative time of last event in milli-seconds
  char msg_loged;
  struct events* next;    // Pointer to next node of Event State Linked List
} Events;

//This struct is used to keep extracted fields from netstorm_events.dat and customer_evants.dat files
typedef struct {
  ns_bigbuf_t event_id;    // Unique Event ID of an Event
  ns_bigbuf_t event_name;         // Event Name
  ns_bigbuf_t attr_name;          // Attribute Name list (comma separated)
  char filter_mode;          // This is filtering mode
  unsigned int mode_based_param;
  ns_bigbuf_t future1;            // For Future Use
  ns_bigbuf_t future2;            // For Future Use
  ns_bigbuf_t future3;            // For Future Use
  ns_bigbuf_t  future4;            // For Future Use
  ns_bigbuf_t  event_detail_desc;  // Detail description of an event
} EventDefinition;

/*This struct is used to keep extracted fields from netstorm_events.dat and customer_evants.dat files*/
/* We can't fill pointers in event definition shared structure
 * because for nsa_event_logger we need to attach shared memory for this struct as well as big_buf_shr_mem
 * There is high possiblity of address change in attaching shared memory so keep indexes*/
typedef struct {
  unsigned int event_id;    // Unique Event ID of an Event
  unsigned int event_name;         // Event Name
  unsigned int attr_name;          // Attribute Name list (comma separated)
  int filter_mode;          // This is filtering mode
  //int sever;		    // INFO, CRITICAL, MAJOR, MINOR ...
  unsigned int mode_based_param;
  unsigned int future4;            // For Future Use
  unsigned int future3;            // For Future Use
  unsigned int future2;            // For Future Use
  unsigned int future1;            // For Future Use
  unsigned int event_detail_desc;  // Detail description of an event
  //Events *event_head[1]; // Linked list of Event State
  Events **event_head; // Linked list of Event State
} EventDefinitionShr;

typedef struct {
   unsigned int total_size;
   unsigned int eid;
   unsigned int ts;
   unsigned int sid;
   unsigned int uid;
   unsigned char  src;
   unsigned char severity;
   unsigned char nvm_id;
   unsigned short opcode;
   char ip[20];
   char attr[MAX_EVENT_ATTR_SIZE + 1];
   char msg[MAX_EVENT_DESC_SIZE + 1];
   char host[MAX_HOST_NAME_SIZE];
   unsigned short msg_len;
   unsigned short attr_len;
   unsigned short host_len;
} EventMsg;

extern int el_lfd;
extern int elog_fd;
extern int is_event_duplicate(u_ns_ts_t ts, int severity, char *event_val, char *event_id,
						char *attr_val, int *hashcode_index, int event_head_idx);
extern EventDefinition *event_def;
extern EventDefinitionShr *event_def_shr_mem;
extern int g_el_msg_com_epfd;
extern int num_total_event_id;
extern Events *delete_event(Events *head, char * attr_value);
extern Str_to_hash_code_ex var_event_hash_func;
extern void open_event_log_file(char*, int, int);
