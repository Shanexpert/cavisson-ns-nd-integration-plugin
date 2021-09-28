/******************************************************************
 * Name    :    ipmgmt_utils.h
 * Author  :    Sanjay
 * Purpose :    Contain data structure and functions declaration for ipmgmt_utils.c
 * Modification History:
   * 01/05/06: Sanjay - Initial Version
   * 05/05/06  Sanjay - Added write_ip_entries() function
   * 10/05/06: Sanjay - Added function delete_range() and delete_IPRange_table_entry()
   * 25/04/06: Sanjay - Addded function do_del_ip()
   * 24/07/06: Sanjay - Added one extra argument load interface to do_add_ip, do_del_ip
                        function for support multiple interface
 *****************************************************************/

#ifndef _ipmgmt_utils_h
#define _ipmgmt_utils_h
//Utilities

#define MAX_LINE_LENGTH 35000 
#define MAX_MSG 512
extern int total_client_iprange_entries ; //current IP entres for client (netstorm)
extern int total_server_iprange_entries ; //current IP entres for server (netocean)


typedef struct ip_range {
        unsigned int start_ip;
        unsigned int end_ip;
        unsigned int num_ip;
        unsigned int net_id;
        unsigned int netbits;
        unsigned int vlan_id;
        unsigned int gateway;
        unsigned int primary_ip;
        unsigned int exclude_ip; //IF Non-zero, excluded from range
                                 //Gateway may be falling within IP range.
                                 // if exlude ip exist, it is still counted in num IP.
                                //Though it must not be assigned.

        char LoadIF[32];
        int status;
        short net_idx;  //Add 03/17/2007 - Ipmgmt
}IPRange;

//Adding structure containing ip entries information 
typedef struct assigned_ip_info{
        char client_ipv4 ; // Client IPV4 address is configured or no
        char server_ipv4 ; // Server IPV6 address is configured or no
        char client_ipv6 ; // Client IPV4 address is configured or no
        char server_ipv6 ; // Server IPV6 address is configured or no
}AssignedIpInfo;

//we will use dynamic sized tables for MyIPRange for client and server

//When more rows need to be created and current buffer run short, tables buffer is expanded by 5 more
#define DELTA_IPRANGE_ENTRIES 5

extern IPRange *cIPrangeTable;
extern IPRange *sIPrangeTable;

extern AssignedIpInfo assignedIPinfo; 
//Entities
#define NS_CLIENT 0
#define NS_SERVER 1
#define NS_SELF_ENTITY 0

#define NS_OPPOSITE_ENTITY 1

#define NS_NO_VLAN 0xFFFFFFFF
#define NS_NO_GATEWAY 0xFFFFFFFF
#define NS_SELF_GATEWAY 0X00000000

#define NS_NEW 1  // no present on self  prsent on opposite
#define NS_NEW_UNIQ 2 //not prsent on any
#define NS_OLD 3 //present on both
#define NS_OLD_UNIQ 4 //present on self only

//A:B is exclusive in X:Y
#define IS_EXCLUSIVE(A, B, X, Y) ((X < A && Y< A ) || (X > B && Y > B))

//A:B is includeive in X:Y
#define IS_INCLUSIVE(A,B, X, Y) ((A >= X && A <= Y) && (B >= X && B <= Y))

void Create_client_iprange_entry(int *row_num);
void Create_server_iprange_entry(int *row_num);

extern int ns_ip_addr (char * addr, unsigned int * out);
extern char * ns_char_ip (unsigned int addr);
extern char * ns_char_vlanid (int vlan_id);
extern int ns_get_netid (char* netid, int netbits, unsigned int *out);
extern int ns_get_netbits (unsigned int addr);
extern int is_valid_net_range (unsigned int nid, unsigned int nbits, unsigned int sip, unsigned int eip);
extern int is_netid_consistent (unsigned int nip, unsigned int nbits);
extern int is_iprange_exclusive (unsigned int sip, unsigned int eip, unsigned int gateway, unsigned int nid);
extern int is_gateway_consistent (int entity, unsigned int gateway, unsigned int nid);
extern int get_netid_map (int entity, unsigned int nid);
extern int create_routes( int entity, unsigned int *gw, unsigned int nid, unsigned int nbits);
extern unsigned int get_netid (unsigned int start_ip, int netbits);
extern int read_ip_entries ();
extern int ns_autofill (char *start_ip, char *end_ip, char *total_ip, char *netbits, char *netid, char *vlanid, char *gateway, char *loadif);
extern int write_ip_entries ();
extern void delete_IPRange_table_entry(int entity, unsigned int sip);
extern int delete_range(int entity, char *start_ip);
extern int do_add_ip (int entity, unsigned int start_ip, unsigned int end_ip, int vid, int nbits, unsigned int exclude_ip, unsigned int *pip, unsigned int nid, unsigned int gw, char *load);
extern int do_del_ip (int entity, unsigned int start_ip, unsigned int end_ip,
        unsigned int vid, int nbits, unsigned int exclude_ip, unsigned int nid,
        unsigned int gw, char *load );
extern int do_get_primary(int entity, unsigned int start_ip, int nbits, int vid, unsigned int *pip, char *ethx);
extern void init_cav();
extern int is_netid_only_on_one_load_interface (int entity, unsigned int nid, char *interface);


#endif
