#ifndef NS_RDP_API_H
#define NS_RDP_API_H


#include "ns_desktop.h"

#define MAX_ID_LEN              32
#define  RDP_MAX_ATTR_LEN     1024


//#define RDP_CONNECT		0
//#define RDP_DISCONNECT		1

/*APIs*/
#define NS_RDP_CONNECT_STR		"ns_rdp_connect"
#define NS_RDP_DISCONNECT_STR	"ns_rdp_disconnect"
#define NS_KEY_STR			"ns_key"
#define NS_KEY_DOWN_STR		"ns_key_down"
#define NS_KEY_UP_STR		"ns_key_up"
#define NS_TYPE_STR		"ns_type"
#define NS_MOUSE_DOWN_STR		"ns_mouse_down"
#define NS_MOUSE_UP_STR		"ns_mouse_up"
#define NS_MOUSE_CLICK_STR		"ns_mouse_click"
#define NS_MOUSE_DOUBLE_CLICK_STR	"ns_mouse_double_click"
#define NS_MOUSE_MOVE_STR		"ns_mouse_move"
#define NS_MOUSE_DRAG_STR		"ns_mouse_drag"
#define NS_SYNC_STR			"ns_sync"
#define NS_MOUSE		"ns_mouse"

typedef enum {
  NS_RDP_CONNECT,
  NS_RDP_DISCONNECT,
  NS_KEY,
  NS_KEY_DOWN, 
  NS_KEY_UP,
  NS_TYPE,
  NS_MOUSE_DOWN,
  NS_MOUSE_UP,
  NS_MOUSE_CLICK,
  NS_MOUSE_DOUBLE_CLICK,
  NS_MOUSE_MOVE,
  NS_MOUSE_DRAG,
  NS_SYNC,
  RDP_API_COUNT
} eRdpApi;

typedef struct
{
  int max_rdp_conn;
  int max_api_id_entries;
  int key_api_enteries;
  int mouse_api_entries;
  int *proto_norm_id_mapping_2_action_tbl;
  NormObjKey key;
}GlobalRdpData;

extern GlobalRdpData g_rdp_vars;
extern char *rdp_api_arr[]; /*bug 79149*/

int ns_parse_rdp_connection_apis(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type);
int ns_parse_rdp_disconnect_api(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type);
int ns_get_values_from_segments(VUser* vptr, connection *cptr, StrEnt_Shr* seg_tab_ptr, char *buffer, int buf_size);
void rdp_init(int row_num, int proto, int rdp_api_type);
void nsi_rdp_execute_ex(VUser* vptr);
void segment_line_ex(StrEnt* segtable, char* line, int line_number, int sess_idx, char *fname, int id_flag, int *curr_flag);
#endif
