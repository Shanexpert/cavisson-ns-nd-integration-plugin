/****************************************************************************
 * Name    :    nsu_check_ip.c
 * Author  :    Sanjay
 * Purpose :    This script checks if the assigned aliased IP's are consistent 
                with the ip_entries file
 * Usage   :    nsu_assign_ip
                This script takes no argument and exits with 0,
		if consistent else exit with status 1.
 * Modification History:
 *   20/05/06: Sanjay - Initial Version
 *   25/05/06: Sanjay - Added function do_check_route()
 *   25/05/06: Sanjay - Added code to check existense of route for admin net ip
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include<sys/wait.h>
#include "ipmgmt_utils.h"
#include "nslb_util.h"

static int is_netid_nonoverlap (int entity, int index, unsigned int nip, unsigned int nbits);
static int is_iprange_nonoverlap (int entity, int index, unsigned int sip, unsigned int eip, unsigned int nid);
int nsi_check_ip();
static int do_check_route(int entity, unsigned int nid, unsigned int nbits, unsigned int gw);
char work_dir[1024];

int nsi_check_ip()
{
    char cmd[512], temp[20], s_ethx[20], c_ethx[20];
    char read_buf[MAX_LINE_LENGTH], SRAdminIP[20], NSAdminIP[20], NSAdminGW[20], SRAdminGW[20];
    int i, j, total, status; 
    IPRange *iprange_table;
    char *ns_admin_ip, *sr_admin_ip, *sr_admin_nbits, *ns_admin_nbits;
    unsigned int ns_admin_netid, sr_admin_netid, sr_adminip, ns_adminip, sr_admin_gw, ns_admin_gw;
    FILE *fp_conf;
    char err_msg[1024]= "\0";

    //Get NS work directory
    if (getenv("NS_WDIR") != NULL)
        strcpy(work_dir, getenv("NS_WDIR"));
    else{
        printf("ERROR: Did not found netstorm working directory\n");
        return 1;
    }
    //Read ip_entries file
    if (read_ip_entries ()){
        printf("ERROR: read_ip_entries() failed\n");
        return 1;
    }

    // Open config file
    if ((fp_conf = fopen("/home/cavisson/etc/cav.conf", "r")) == NULL) {
        fprintf(stderr, "ERROR: in opening file /home/cavisson/etc/cav.conf\n");
        perror("fopen");
        return 1;
    }
    // read from config file 
    while (fgets(read_buf, MAX_LINE_LENGTH, fp_conf)) {
         if (!strncmp(read_buf, "NSAdminIP", 9)){
             sscanf(read_buf,"%s %s", temp, NSAdminIP);
         }else if (!strncmp(read_buf, "SRAdminIP", 9)){
             sscanf(read_buf,"%s %s", temp, SRAdminIP);
         }else if (!strncmp(read_buf, "NSAdminGW", 9)){
             sscanf(read_buf,"%s %s", temp, NSAdminGW);
         }else if (!strncmp(read_buf, "SRAdminGW", 9)){
             sscanf(read_buf,"%s %s", temp, SRAdminGW);
         }else if (!strncmp(read_buf, "NSLoadIF", 8)) {
           sscanf(read_buf,"%s %s", temp, c_ethx);
         }else if (!strncmp(read_buf, "SRLoadIF", 8)){
           sscanf(read_buf,"%s %s", temp, s_ethx);
         }
    }
    //Get NS admin netid
    ns_admin_ip = strtok(NSAdminIP,"/");
    ns_admin_nbits = strtok(NULL,"/");
    ns_ip_addr(ns_admin_ip, &ns_adminip);
    ns_admin_netid = get_netid (ns_adminip, atoi(ns_admin_nbits));

    //Get SR admin netid 
    sr_admin_ip = strtok(SRAdminIP,"/");
    sr_admin_nbits = strtok(NULL,"/");
    ns_ip_addr(sr_admin_ip, &sr_adminip);
    sr_admin_netid = get_netid (sr_adminip, atoi(sr_admin_nbits));

    //Check all netid non-overlapping 
    for (i = 0; i < 2; i++) {
        //Check for all NS ranges, NO ranges non-overlapping 
        if (i == 0) {
            total = total_client_iprange_entries;
            iprange_table = cIPrangeTable;
        } else {
            total = total_server_iprange_entries;
            iprange_table = sIPrangeTable;
        }
        for (j = 0; j < total; j++) {
            if(!is_netid_nonoverlap(i, j, iprange_table[j].net_id, iprange_table[j].netbits)){
	        printf("ERROR: Net ids are overlapping\n");
                return 1;
	    }
	}
    }
    //Check for NS admin ip non-overlapping 
    if(!is_netid_nonoverlap(0, 0, ns_admin_netid, atoi(ns_admin_nbits))){
        printf("ERROR: Net ids are overlapping\n");
        return 1;
    }
    //Check for NO admin ip non-overlapping 
    if(!is_netid_nonoverlap(0, 0, sr_admin_netid, atoi(sr_admin_nbits))){
        printf("ERROR: Net ids are overlapping\n");
        return 1;
    }

    //check if each IP range within netid for NS ranges, NO ranges
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            total = total_client_iprange_entries;
            iprange_table = cIPrangeTable;
        } else {
            total = total_server_iprange_entries;
            iprange_table = sIPrangeTable;
        }
        for (j = 0; j < total; j++) {
            if (!(is_valid_net_range (iprange_table[j].net_id, iprange_table[j].netbits, iprange_table[j].start_ip, iprange_table[j].end_ip))){
                printf("ERROR: IP ranges are not within net id ranges \n");
                return 1;
            }
	}
    }

    //check if each IP range within netid for NS admin, NO Admin

    //Check ip ranges for non-overlapping
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            total = total_client_iprange_entries;
            iprange_table = cIPrangeTable;
        } else {
            total = total_server_iprange_entries;
            iprange_table = sIPrangeTable;
        }
        for (j = 0; j < total; j++) {
            if (!(is_iprange_nonoverlap (i, j, iprange_table[j].start_ip, iprange_table[j].end_ip, iprange_table[j].net_id))){
                printf("ERROR: IP ranges are overlapping \n");
                return 1;
            }
	}
    }

    //Check gateways are either excluded or not in range
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            total = total_client_iprange_entries;
            iprange_table = cIPrangeTable;
        } else {
            total = total_server_iprange_entries;
            iprange_table = sIPrangeTable;
        }
        for (j = 0; j < total; j++) {
            if (IS_INCLUSIVE(iprange_table[j].gateway,iprange_table[j].gateway, iprange_table[j].start_ip, iprange_table[j].end_ip)){
                if (iprange_table[j].gateway != iprange_table[j].exclude_ip) {
		    printf("ERROR: Gateway is within ip range, but not excluded \n");
                    return 1;
                }
            }else{
                if (iprange_table[j].exclude_ip) {
		    printf("ERROR: Gateway is not within ip range, but excluded \n");
                    return 1;
                }
            }
            //NS Admin IP's are either excluded or not in range.
	    if (IS_INCLUSIVE(ns_adminip, ns_adminip, iprange_table[j].start_ip, iprange_table[j].end_ip)){
                if (ns_adminip != iprange_table[j].exclude_ip) {
		    printf("ERROR: NS Admin Ip is not excluded \n");
                    return 1;
                }
            }
	    //NO Admin IP's are either excluded or not in range.
	    if (IS_INCLUSIVE(sr_adminip, sr_adminip, iprange_table[j].start_ip, iprange_table[j].end_ip)){
                if (sr_adminip != iprange_table[j].exclude_ip) {
	            printf("ERROR: NO Admin Ip is not excluded \n");
                    return 1;
                }
            }
   	}
    }

    //Get a list of IP's using IP entries and 'ip address show', see if they are same
    //Use nsi_check_ip
  
    sprintf(cmd, "%s/bin/nsi_check_ip", work_dir);
    status = nslb_system(cmd,1,err_msg);

    if (status) {
        printf("ERROR: There is difference between actual assigned and configured ip\n");
        return 1;
    }
   
    //Check gateways , Same netid should have same gateway
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            total = total_client_iprange_entries;
            iprange_table = cIPrangeTable;
        } else {
            total = total_server_iprange_entries;
            iprange_table = sIPrangeTable;
        }
        for (j = 0; j < total; j++) {
            if (!is_gateway_consistent (i, iprange_table[j].gateway, iprange_table[j].net_id)){
                printf("ERROR: Gateway not consistent\n");
                return 1;
            }
	}
    }

    if (strcmp(NSAdminGW,"-"))
       ns_admin_gw = atoi(NSAdminGW);
    else
       ns_admin_gw = NS_NO_GATEWAY;

    if (strcmp(SRAdminGW,"-"))
       sr_admin_gw = atoi(SRAdminGW);
    else
       sr_admin_gw = NS_NO_GATEWAY;

    //Check gateways for NS admin netid, Same netid should have same gateway
    if (!is_gateway_consistent (0, ns_admin_gw, ns_admin_netid)){
        printf("ERROR: Gateway not consistent\n");
        return 1;
    }
    //Check gateways for NO admin netid, Same netid should have same gateway
    if (!is_gateway_consistent (1, sr_admin_gw, sr_admin_netid)){
        printf("ERROR: Gateway not consistent\n");
        return 1;
    }

    //Check routes 
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            total = total_client_iprange_entries;
            iprange_table = cIPrangeTable;
        } else {
            total = total_server_iprange_entries;
            iprange_table = sIPrangeTable;
        }
        for (j = 0; j < total; j++) {
            if ( iprange_table[j].gateway != NS_NO_GATEWAY) {
                if (do_check_route (i, iprange_table[j].net_id, iprange_table[j].netbits, iprange_table[j].gateway)){
                    printf("ERROR: route does not exist\n");
                    return 1;
                }
             }
        }
    } 

    //check routes for admin ip
    if ( ns_admin_gw != NS_NO_GATEWAY) {
        if (do_check_route (0, ns_admin_netid, atoi(ns_admin_nbits), ns_admin_gw)){
            printf("ERROR: NS Admin route does not exist\n");
            return 1;
        } 
    }

    if (ns_admin_netid != sr_admin_netid) {
        if ( sr_admin_gw != NS_NO_GATEWAY) {
            if (do_check_route (1, sr_admin_netid, atoi(sr_admin_nbits), sr_admin_gw)){
                printf("ERROR: Sr Admin route does not exist\n");
                return 1;
            } 
        }
    }

    //check primary for each range in ip_entries 
    //if in range must be stored as primary else must not be any primary
/*    for (i = 0; i < 2; i++) {
        if (i == 0) {
            total = total_client_iprange_entries;
            iprange_table = cIPrangeTable;
			ethx = c_ethx;
        } else {
            total = total_server_iprange_entries;
            iprange_table = sIPrangeTable;
			ethx = s_ethx;
        }
        for (j = 0; j < total; j++) {
	    do_get_primary (i, iprange_table[j].start_ip, iprange_table[j].netbits, iprange_table[j].vlan_id, &pip, ethx);
	    //Primary IP is in range or not.
	    if (IS_INCLUSIVE(pip, pip, iprange_table[j].start_ip, iprange_table[j].end_ip)){
                if (pip != iprange_table[j].primary_ip) {
		    printf("ERROR: Invalid primary ip \n");
                    return 1;
                }
            }else {
	        if (iprange_table[j].primary_ip != 0) {
		    printf("ERROR: Invalid primary ip \n");
                    return 1;
                }
	    }
        }
    }*/

    return 0;
}

//Rteurns 1 on failure 0 on success
//Calls nsi_route_exist
int do_check_route(int entity, unsigned int nid, unsigned int nbits, unsigned int gw)
{
    char cmd[512], gateway[20];
    int status;
    char err_msg[1024]= "\0";

    if ( gw != NS_NO_GATEWAY && gw != NS_SELF_GATEWAY)
        strcpy(gateway, ns_char_ip(gw));
    else
        strcpy(gateway, "-");

    //Check route on same entity
    sprintf(cmd, "%s/bin/nsi_route_exist %s %s/%d %s", work_dir, entity?"S":"C", 
             ns_char_ip(nid), nbits, gateway);
    status = nslb_system(cmd,1,err_msg);

    if (status) {
        return 1;
    }
    
    return 0;
}

//checks IP ranges are unique. Check it with 
//ip range for other ip ranges at clinet and server for same netid and 
//return 1 if valid, else 0
int
is_iprange_nonoverlap (int entity, int index, unsigned int sip, unsigned int eip, unsigned int nid)
{
    int i, j, total, indx;
    IPRange *iprange_table;

    for (i = entity; i < 2; i++) {
        if (i == 0) {
            total = total_client_iprange_entries;
            iprange_table = cIPrangeTable;
        } else {
            total = total_server_iprange_entries;
            iprange_table = sIPrangeTable;
        }
	//Find the index from where exclusion have to check
	if (i == entity) {
   	    indx = index + 1;
	} else {
	    indx = 0;
	}

        for (j = indx; j < total; j++) {
            if (nid == iprange_table[j].net_id ) { 
                if (!IS_EXCLUSIVE (sip, eip, iprange_table[j].start_ip, iprange_table[j].end_ip)) {
	            printf("ERROR: IP ranges are overlapping\n");
  	            return 0;
	        }
            }
        }//Inner for loop
    }
    return 1;
}




//checks net-id IP ranges for net/nbits is exclsuive with other netid/nbits or 
//they are same nets.
//return 1 if valid, else 0
int
is_netid_nonoverlap (int entity, int index, unsigned int nip, unsigned int nbits)
{
    unsigned int Snip1, Enip1, Snip2, Enip2, num1, num2;
    int i, j, total, indx;
    IPRange *iprange_table;

    for (i = entity; i < 2; i++) {
        if (i == 0) {
            total = total_client_iprange_entries;
            iprange_table = cIPrangeTable;
        } else {
            total = total_server_iprange_entries;
            iprange_table = sIPrangeTable;
        }
	//Find the index from where exclusion have to check
	if (i == entity) {
	    indx = index + 1;
	} else {
	    indx = 0;
	}

        for (j = indx; j < total; j++) {
            num1 = 1;
            num2 = 1;
            Snip1 = nip;
            Snip2 = iprange_table[j].net_id;
            //num contains max hosts in the netid.
            num1 = num1 << (32-nbits);
            num2 = num2 << (32- (iprange_table[j].netbits));
 
            Enip1 = Snip1 + num1 - 1;
            Enip2 = Snip2 + num2 - 1;

            if ((Snip1 == Snip2) && (nbits == iprange_table[j].netbits))
	        continue;
            else if (IS_EXCLUSIVE (Snip1, Enip1, Snip2, Enip2))
	        continue;
            else {
                printf("Net-id IP ranges for net/nbits not Exclusive");
                return 0;
            }
        }// End inner for loop
    }
  return 1;
}


int main()
{
    int result;
    result = nsi_check_ip();
    return result;
}

