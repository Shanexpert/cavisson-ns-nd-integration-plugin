/******************************************************************
 * Name    :    nsu_ipv6_utils.h
 * Author  :    Abhay
 * Purpose :    Contain data structure and functions declaration for nsu_ipv6_utils.c
 * Modification History:
 *
 ******************************************************************/

#define IPV4_ADDR 1
#define IPV6_ADDR 2
#define MAX_LINE_LENGTH 35000
#define MAX_MSG 512
#define DELTA_IPV6RANGE_ENTRIES 5


#define NS_CLIENT 0
#define NS_SERVER 1
#define NS_NO_VLAN_IPV6 "-"
#define NS_NO_GATEWAY_IPV6 "-"

extern int total_client_ipv6range_entries ; //current IPV6 entries for client (netstorm)
extern int total_server_ipv6range_entries ; //current IPV6 entries for server (netocean)


typedef struct ipv6_range {
               char start_ip[50];
               char end_ip[50];
               char num_ip[10];
               char net_id[50];
               char netbits[10];
               char vlan_id[10];
               char gateway[50];
               char primary_ip[50];
               char exclude_ip[50]; //IF Non-zero, excluded from range
                                 //Gateway may be falling within IP range.
                                 // if exlude ip exist, it is still counted in num IP.
                                //Though it must not be assigned.
              char LoadIF[32];
              int status;
              short net_idx;  
  }IPV6Range;


extern IPV6Range *cIPV6rangeTable;
extern IPV6Range *sIPV6rangeTable;

extern void Create_client_ipv6range_entry(int *row_num);
extern void Create_server_ipv6range_entry(int *row_num);

extern int check_ip_type(char *addr);
extern void Create_client_ipv6range_entry(int *row_num);
extern void Create_server_ipv6range_entry(int *row_num);
int find_end_ipv6( char * start_ip, int num_ips, char *end_ip );
int find_total_ips( char * start_ip, char * end_ip, char * total_ip );
extern int do_del_ipv6 (int entity, char *start_ip, char *end_ip, char *vid, char *nbits, char *exclude_ip, char *nid, char *gw, char *load );
extern void delete_IPV6Range_table_entry(int entity, char *start_ip);
extern int ns_autofill_ipv6 ( char *start_ip, char *end_ip, char *total_ip, char *netbits, char *netid, char *vlanid,
                              char *gateway, char *interface );
int do_add_ipv6 ( int entity, char *start_ip, char *end_ip, char *vlanid, char *netbits, char *exclude_ip, char *primary_ip,                   char *netid, char *gateway, char *interface );
extern int is_ipv6range_exclusive (int entity, char *start_ip, char *end_ip);


