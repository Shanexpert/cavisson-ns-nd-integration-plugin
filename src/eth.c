/******************************************************************
 * 17/02/06	1.0	Initial version
 * Author: Sanjay Gupta
 *****************************************************************/

/*ASSUMPTION: it is assumed that /proc/net/delv will not have more than 32 lines, 
otherwise interfaces appearing on lines after 32 will not be accounted for*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "url.h"
#include "eth.h"
#include "ns_log.h"
#include "nslb_cav_conf.h"
#include "ns_alloc.h"

extern void end_test_run( void );

//#define MAX_LINE_LENGTH 1024

struct eth_data {
    unsigned short line_num;
    unsigned short data_offset;
    u_ns_8B_t  rx_bytes_overruns;
    u_ns_8B_t  tx_bytes_overruns;
    u_ns_8B_t  rx_packet_overruns;
    u_ns_8B_t  tx_packet_overruns;
    u_ns_8B_t first_eth_rx_bytes;
    u_ns_8B_t first_eth_tx_bytes;
    u_ns_8B_t last_eth_rx_bytes;
    u_ns_8B_t last_eth_tx_bytes;
    u_ns_8B_t next_eth_rx_bytes;
    u_ns_8B_t next_eth_tx_bytes;
    u_ns_8B_t first_eth_rx_packets;
    u_ns_8B_t first_eth_tx_packets;
    u_ns_8B_t last_eth_rx_packets;
    u_ns_8B_t last_eth_tx_packets;
    u_ns_8B_t next_eth_rx_packets;
    u_ns_8B_t next_eth_tx_packets;
};

static struct eth_data * interface_data;
static int num_interfaces;
static int get_eth_line (FILE *fp, char *eth_name);
static unsigned int get_default_eth ();
static unsigned int set_bit (unsigned int bit_mask, int bit_num);
static int get_num_interfaces (unsigned int mask);
static void init_interface_data(unsigned int eth_mask);
static void set_first_data();

/* parse_eth_data is called for parsing 
scenario config file for keywords of the format
ETH_INTERFACE eth1 eth2 ip0
This function would be passed the string excluding the
keyword, ETH_INTERFACE. so, input would be space
separated list of interfaces.

This function would parse the file /proc/net/dev which has the format

Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo:3258351974  655138    0    0    0     0          0         0 3258351974  655138    0    0    0     0       0          0
  eth0:3819100380 3890841662   38 14235233    0     2          0         0 3708614056 4206786443   40    0    6     0       3          0
dummy0:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0

Note that first two lines are the headers lines.
After the header, there is one line for each interface.
parse_eth_data would scan the list of all interfaces in the 
input and would find the corresponding line number in the
/proc/dev/net file. After finding the line number (for example
eth0 is on line number 4), it will set appropraite bit 
in an unsigned int variable.  After setting all appropriate bits,
it would return the value of the variable with set bits.
*/
//Load Interface is listed in cav.conf. Pick it from there and convert to eth_mask
static unsigned int 
parse_eth_data ()
{
unsigned int eth_bit_mask = 0;
int line_num;
char *eth_name;
FILE *eth_fp;
char eth[MAX_LINE_LENGTH];
//char read_buf[MAX_LINE_LENGTH];
//char temp[MAX_LINE_LENGTH];

NSDL2_ETHERNET(NULL, NULL, "Method called");

#if 0
char input[MAX_LINE_LENGTH];
    input[0]='\0';
    if ((cav_conf_fp = fopen("/home/cavisson/etc/cav.conf", "r")) == NULL) {
        fprintf(stderr, "ERROR: in opening file /home/cavisson/etc/cav.conf\n");
        perror("fopen");
        fprintf(stderr, "Ethernet data would not be reported\n");
        return eth_bit_mask;
    }

    // read interface name for client/server and IP address of server
    while (fgets(read_buf, MAX_LINE_LENGTH, cav_conf_fp)) {
        if (!strncmp(read_buf, "NSLoadIF", 8))
      	    sscanf(read_buf,"%s %s", temp, input);
    }

#endif
    if ((eth_fp = fopen("/proc/net/dev", "r")) == NULL) {
    	fprintf(stderr, "Error in opening file '/proc/net/dev'\n");
    	perror("fopen");
    	fprintf(stderr, "Ethernet data would not be reported\n");
	return eth_bit_mask;
    }
    strcpy(eth, g_cavinfo.NSLoadIF);
    eth_name = strtok (eth, "|");
    while (eth_name) {
	line_num = get_eth_line (eth_fp, eth_name);	
	if (line_num > 31)
    	    fprintf(stderr, "Data for interface %s (index >31) would not be reported\n", eth_name);
	else if (line_num < 0)
    	    fprintf(stderr, "Data for interface %s (invalid interface name) would not be reported)\n", eth_name);
	else 
	    eth_bit_mask = set_bit (eth_bit_mask, line_num);
    	eth_name = strtok (NULL, "|");
	rewind (eth_fp);
    }

    fclose (eth_fp);
    return eth_bit_mask;
}

static int
get_eth_line (FILE *fp, char *eth_name)
{
char read_buf[MAX_LINE_LENGTH];
char* read_ptr;
int line=-1;

    NSDL2_ETHERNET(NULL, NULL, "Method called, eth_name = %s", eth_name);
    while (fgets(read_buf, MAX_LINE_LENGTH, fp))
	{
	    line++; //count start from 0 as opposed to 1
    	read_ptr = read_buf;
    	while (*read_ptr == ' ') read_ptr++; //remove leading blanks
    	if (strchr(read_ptr, ':'))
      	    strchr(read_ptr, ':')[0] = 0; //read_ptr now contains eth name
    	else
      	    continue;
    
    	if (!strcmp(read_ptr, eth_name))
	    break;
    }
    return line;
}


static unsigned int
set_bit (unsigned int bit_mask, int bit_num)
{
//Achint: write coode
    NSDL2_ETHERNET(NULL, NULL, "Method called, bit_number = %d", bit_num);
    unsigned int num = pow(2,bit_num);
    bit_mask = bit_mask | num;
    return bit_mask;
}

/**************************************************************
init_eth_data creates data structure
**************************************************************/
void
init_eth_data ()
{
unsigned int mask;
unsigned int eth_mask;

    
    NSDL2_ETHERNET(NULL, NULL, "Method called");
    eth_mask = parse_eth_data ();
    //if eth_mask is zero, put all interfaces with names 
    //starting with eth is mask
    if (!eth_mask)
  	mask = get_default_eth ();	
    else
  	mask = eth_mask;	

    num_interfaces = get_num_interfaces( mask);
    if (!interface_data) 
      MY_MALLOC (interface_data , num_interfaces * sizeof (struct eth_data), "interface_data ", -1);
    //Initialize data structure
    init_interface_data(mask);
}



static unsigned int 
get_default_eth ()
{
unsigned int eth_bit_mask = 0;
char read_buf[MAX_LINE_LENGTH];
int line = -1;
FILE *eth_fp;
char *read_ptr;

    NSDL2_ETHERNET(NULL, NULL, "Method called");
    if ((eth_fp = fopen("/proc/net/dev", "r")) == NULL) {
    	fprintf(stderr, "Error in opening file '/proc/net/dev'\n");
    	perror("fopen");
    	fprintf(stderr, "Ethernet data would not be reported\n");
	return eth_bit_mask;
    }

    while (fgets(read_buf, MAX_LINE_LENGTH, eth_fp)) {
	line++; //count start from 0 as opposed to 1
    	read_ptr = read_buf;
    	while (*read_ptr == ' ') read_ptr++; //remove leading blanks
    	if (strchr(read_ptr, ':'))
      	    strchr(read_ptr, ':')[0] = 0; //read_ptr now contains eth name
    	else
      	    continue;
    
    	if (!strncmp(read_ptr, "eth", 3)) {
	    if (line > 31)
    	    	fprintf(stderr, "Data for interface %s (index >31) would not be reported\n", read_ptr);
	    else 
	    	eth_bit_mask = set_bit (eth_bit_mask, line);
	}
    }


    fclose (eth_fp);
    return eth_bit_mask;
}

static int
get_num_interfaces (unsigned int mask)
{
//Achint : write code
//Count the number of bits set
    int count = 0;
    NSDL2_ETHERNET(NULL, NULL, "Method called, mask = %lu", mask);
    while(mask)
    {
	count += mask & 0x1lu;
	mask >>= 1;
    }
    return count;
}

static void
init_interface_data(unsigned int eth_mask)
{
//Achint: write code
    int position = -1, offset;
    int flag;
    int index=0;
    int line = -1;
    FILE *eth_fp;
    char read_buf[MAX_LINE_LENGTH];
    char* read_ptr, *read_ptr1;

    NSDL2_ETHERNET(NULL, NULL, "Method called, mask = %lu", eth_mask);
    if ((eth_fp = fopen("/proc/net/dev", "r")) == NULL)
    {
	fprintf(stderr, "Error in opening file '/proc/net/dev'\n");
	perror("fopen");
	fprintf(stderr, "Ethernet data would not be reported\n");
        return ;
    }
    while(eth_mask)
    {
	position++;
	flag = eth_mask & 0x1lu;
	if(flag)
	{
            interface_data[index].line_num = position;	
	    while (line < position)
	    {
	       	fgets(read_buf, MAX_LINE_LENGTH, eth_fp);
	        line++;
	    }

	    read_ptr = read_buf;
            read_ptr1 = strchr(read_buf,':');
            read_ptr1++;
                                               
	    offset = read_ptr1 - read_ptr;
#ifdef TEST
	    printf ("\nPosition = %d ",position);
	    printf ("\nOffset = %d ",offset);
#endif            
            interface_data[index].data_offset = offset;
	    interface_data[index].rx_bytes_overruns = 0;
	    interface_data[index].tx_bytes_overruns = 0;
	    interface_data[index].rx_packet_overruns = 0;
	    interface_data[index].tx_packet_overruns = 0;
	    interface_data[index].first_eth_rx_bytes = 0;
	    interface_data[index].first_eth_tx_bytes = 0;
	    interface_data[index].last_eth_rx_bytes = 0;
	    interface_data[index].last_eth_tx_bytes = 0;
	    interface_data[index].next_eth_rx_bytes = 0;
	    interface_data[index].next_eth_tx_bytes = 0;
	    interface_data[index].first_eth_rx_packets = 0;
	    interface_data[index].first_eth_tx_packets = 0;
	    interface_data[index].last_eth_rx_packets = 0;
	    interface_data[index].last_eth_tx_packets = 0;
	    interface_data[index].next_eth_rx_packets = 0;
	    interface_data[index].next_eth_tx_packets = 0;
	    index++;
	}

	eth_mask >>= 1;
    }
    fclose (eth_fp);
//For each interface - fill in the line#, offset to data after interfcee:
//init all other fields to 0
//sort the table on line number

    update_eth_data();
    set_first_data();
}

void 
update_eth_data(void) 
{
char read_buf[MAX_LINE_LENGTH];
char* read_ptr;
char* read_ptr2;
int i, line = -1, if_num;
FILE* eth_fp;
struct eth_data *iptr;

    NSDL2_ETHERNET(NULL, NULL, "Method called");
    if (!interface_data) return;

    if ((eth_fp = fopen("/proc/net/dev", "r")) == NULL) return;

    iptr = interface_data;
    for (if_num = 0; if_num < num_interfaces; if_num++, iptr++ ) {
  	while (line < iptr->line_num) {
  	    fgets(read_buf, MAX_LINE_LENGTH, eth_fp);
	    line++;
	}
        read_ptr = read_buf;
        read_ptr += iptr->data_offset;
    
      	// Skip any leading blank
      	while (*read_ptr == ' ') read_ptr++;
      	read_ptr2 = strchr(read_ptr, ' ');
      	*read_ptr2 = 0;
      
      	iptr->last_eth_rx_bytes = iptr->next_eth_rx_bytes;
      	iptr->next_eth_rx_bytes = atoll(read_ptr);
      	iptr->next_eth_rx_bytes += iptr->rx_bytes_overruns;
      	if (iptr->next_eth_rx_bytes < iptr->last_eth_rx_bytes) {
	    // Counter rolls  back at 2^32
	    iptr->rx_bytes_overruns += 0xFFFFFFFF;
            iptr->next_eth_rx_bytes += 0xFFFFFFFF;
      	}

      	read_ptr = read_ptr2 + 1; //Set read_ptr past current data element

	while (*read_ptr == ' ') read_ptr++; //remove leading spaces
      	read_ptr2 = strchr(read_ptr, ' ');
      	*read_ptr2 = 0;

      	iptr->last_eth_rx_packets = iptr->next_eth_rx_packets;
      	iptr->next_eth_rx_packets = atoll(read_ptr);
      	iptr->next_eth_rx_packets += iptr->rx_packet_overruns;
      	if (iptr->next_eth_rx_packets < iptr->last_eth_rx_packets) {
	    // Counter rolls  back at 2^32
	    iptr->rx_packet_overruns += 0xFFFFFFFF;
            iptr->next_eth_rx_packets += 0xFFFFFFFF;
      	}

      	read_ptr = read_ptr2 + 1; //Set read_ptr past current data element

	//ignore next 6 data elements (RX data)
      	for (i = 0; i < 6; i++) { 
	    while (*read_ptr == ' ') read_ptr++;
	    read_ptr = strchr(read_ptr, ' ');
      	}
      
      	while (*read_ptr == ' ') read_ptr++;
     	read_ptr2 = strchr(read_ptr, ' ');
      	*read_ptr2 = 0;

      	iptr->last_eth_tx_bytes = iptr->next_eth_tx_bytes;
      	iptr->next_eth_tx_bytes = atoll(read_ptr);
      	iptr->next_eth_tx_bytes += iptr->tx_bytes_overruns;
      	if (iptr->next_eth_tx_bytes < iptr->last_eth_tx_bytes) {
	    // Counter rolls  back at 2^32
	    iptr->tx_bytes_overruns += 0xFFFFFFFF;
            iptr->next_eth_tx_bytes += 0xFFFFFFFF;
      	}

      	read_ptr = read_ptr2 + 1; //Set read_ptr past current data element

	while (*read_ptr == ' ') read_ptr++; //remove leading spaces
      	read_ptr2 = strchr(read_ptr, ' ');
      	*read_ptr2 = 0;

      	iptr->last_eth_tx_packets = iptr->next_eth_tx_packets;
      	iptr->next_eth_tx_packets = atoll(read_ptr);
      	iptr->next_eth_tx_packets += iptr->tx_packet_overruns;
      	if (iptr->next_eth_tx_packets < iptr->last_eth_tx_packets) {
	    // Counter rolls  back at 2^32
	    iptr->tx_packet_overruns += 0xFFFFFFFF;
            iptr->next_eth_tx_packets += 0xFFFFFFFF;
      	}
    }
    fclose(eth_fp);
}

static void
set_first_data()
{
    //Set first_... using next_ ... for all rows in interface_data
    struct eth_data *iptr;
    int i;

    NSDL2_ETHERNET(NULL, NULL, "Method called");
    if (!interface_data) return;
	iptr = interface_data;
    for (i = 0; i < num_interfaces; i++, iptr++ )
    {
	iptr->first_eth_rx_bytes = iptr->next_eth_rx_bytes;
	iptr->first_eth_tx_bytes = iptr->next_eth_tx_bytes;
	iptr->first_eth_rx_packets = iptr->next_eth_rx_packets;
	iptr->first_eth_tx_packets = iptr->next_eth_tx_packets;
    }
}

#if 0
u_ns_8B_t
get_eth_rx_bytes()
{
  u_ns_8B_t next_eth_rx_bytes = 0;
  u_ns_8B_t first_eth_rx_bytes = 0;
  int i;
  struct eth_data *iptr;

  NSDL2_ETHERNET(NULL, NULL, "Method called");
  if (!interface_data) return 0;
  iptr = interface_data;
  
  for (i = 0; i < num_interfaces; i++, iptr++ ) {
    first_eth_rx_bytes += iptr->first_eth_rx_bytes;
    next_eth_rx_bytes += iptr->next_eth_rx_bytes;
  }
  return next_eth_rx_bytes - first_eth_rx_bytes;
}
#endif

//mode = 0, last-next, 
//mode = 1, last - first
u_ns_8B_t 
get_eth_rx_bps (int mode, unsigned int duration)
{
    struct eth_data *iptr;
    int i;
    double eth_rx;
    double seconds;
    u_ns_8B_t first_eth_rx_bytes = 0;
    u_ns_8B_t last_eth_rx_bytes = 0;
    u_ns_8B_t next_eth_rx_bytes = 0;
    u_ns_8B_t ret;

    NSDL2_ETHERNET(NULL, NULL, "Method called, mode = %d, duration = %lu", mode, duration);  
    if (!interface_data) return 0;
    //sum up all data in all rows
    iptr = interface_data;
    for (i = 0; i < num_interfaces; i++, iptr++ )
    {
	first_eth_rx_bytes += iptr->first_eth_rx_bytes;
	last_eth_rx_bytes += iptr->last_eth_rx_bytes;
	next_eth_rx_bytes += iptr->next_eth_rx_bytes;
    }
#ifdef TEST
    printf("\n first_eth_rx_bytes = %llu",first_eth_rx_bytes);
    printf("\n last_eth_rx_bytes = %llu",last_eth_rx_bytes);
    printf("\n next_eth_rx_bytes = %llu",next_eth_rx_bytes);
#endif
    //and convert to  bps (Bits per sec)
    //assume duration variable contains the duration in milli-seconds
    seconds = (double)((double)(duration)/1000.0); 
    //Formula would be
    if (mode)
	eth_rx = ((double)((next_eth_rx_bytes - first_eth_rx_bytes) * 8))/(seconds);
    else
	eth_rx = ((double)((next_eth_rx_bytes - last_eth_rx_bytes) * 8))/(seconds);
    
    ret = (u_ns_8B_t) (eth_rx);
    return ret;
}

#if 0
u_ns_8B_t
get_eth_tx_bytes()
{
  u_ns_8B_t next_eth_tx_bytes = 0;
  u_ns_8B_t first_eth_tx_bytes = 0;

  int i;
  struct eth_data *iptr;

  NSDL2_ETHERNET(NULL, NULL, "Method called");  
  if (!interface_data) return 0;
  iptr = interface_data;
  
  for (i = 0; i < num_interfaces; i++, iptr++ ) {
    first_eth_tx_bytes += iptr->first_eth_tx_bytes;
    next_eth_tx_bytes += iptr->next_eth_tx_bytes;
  }
  return next_eth_tx_bytes - first_eth_tx_bytes;
}
#endif

//mode = 0, last-next, 
//mode = 1, last - first
u_ns_8B_t 
get_eth_tx_bps (int mode, unsigned int duration)
{
    struct eth_data *iptr;
    int i;
    double eth_tx;
    double seconds;
    u_ns_8B_t first_eth_tx_bytes = 0;
    u_ns_8B_t last_eth_tx_bytes = 0;
    u_ns_8B_t next_eth_tx_bytes = 0;
    u_ns_8B_t ret;

    NSDL2_ETHERNET(NULL, NULL, "Method called");  
    if (!interface_data) return 0;
    //sum up all data in all rows
    iptr = interface_data;
    for (i = 0; i < num_interfaces; i++, iptr++ )
    {
	first_eth_tx_bytes += iptr->first_eth_tx_bytes;
	last_eth_tx_bytes += iptr->last_eth_tx_bytes;
	next_eth_tx_bytes += iptr->next_eth_tx_bytes;
    }
#ifdef TEST
    printf("\n first_eth_tx_bytes = %llu",first_eth_tx_bytes);
    printf("\n last_eth_tx_bytes = %llu",last_eth_tx_bytes);
    printf("\n next_eth_tx_bytes = %llu\n",next_eth_tx_bytes);
#endif
    //and convert to  bps (Bits per sec)
    //assume duration variable contains the duration in milli-seconds
    seconds = (double)((double)(duration)/1000.0); 
    //Formula would be  

    if (mode)
	eth_tx = ((double)((next_eth_tx_bytes - first_eth_tx_bytes) * 8))/(seconds);
    else
	eth_tx = ((double)((next_eth_tx_bytes - last_eth_tx_bytes) * 8))/(seconds);

    ret = (u_ns_8B_t) (eth_tx);
    return ret;
}

#if 0
u_ns_8B_t
get_eth_rx_packets()
{
  int i;
  struct eth_data *iptr;
  u_ns_8B_t first_eth_rx_packets = 0;
  u_ns_8B_t next_eth_rx_packets = 0;

  NSDL2_ETHERNET(NULL, NULL, "Method called");  
  if (!interface_data) return 0;
  iptr = interface_data;
  for (i = 0; i < num_interfaces; i++, iptr++ )
    {
      first_eth_rx_packets += iptr->first_eth_rx_packets;
      next_eth_rx_packets += iptr->next_eth_rx_packets;
    }
  return next_eth_rx_packets - first_eth_rx_packets;
}

#endif
//mode = 0, last-next, 
//mode = 1, last - first
u_ns_8B_t 
get_eth_rx_pps (int mode, unsigned int duration)
{
    struct eth_data *iptr;
    int i;
    double eth_rx_pps;
    double seconds;
    u_ns_8B_t first_eth_rx_packets = 0;
    u_ns_8B_t last_eth_rx_packets = 0;
    u_ns_8B_t next_eth_rx_packets = 0;
    u_ns_8B_t ret;

    NSDL2_ETHERNET(NULL, NULL, "Method called");  
    if (!interface_data) return 0;
    //sum up all data in all rows
    iptr = interface_data;
    for (i = 0; i < num_interfaces; i++, iptr++ )
    {
	first_eth_rx_packets += iptr->first_eth_rx_packets;
	last_eth_rx_packets += iptr->last_eth_rx_packets;
	next_eth_rx_packets += iptr->next_eth_rx_packets;
    }
#ifdef TEST
    printf("\n first_eth_rx_packets = %llu",first_eth_rx_packets);
    printf("\n last_eth_rx_packets = %llu",last_eth_rx_packets);
    printf("\n next_eth_rx_packets = %llu",next_eth_rx_packets);
#endif
    //assume duration variable contains the duration in milli-seconds
    seconds = (double)((double)(duration)/1000.0); 
    //Formula would be 
    if (mode)
	eth_rx_pps = ((next_eth_rx_packets - first_eth_rx_packets))/(seconds);
    else
	eth_rx_pps = ((next_eth_rx_packets - last_eth_rx_packets))/(seconds);

    ret = (u_ns_8B_t) (eth_rx_pps);
    return ret;
}

#if 0
u_ns_8B_t
get_eth_tx_packets()
{
  int i;
  struct eth_data *iptr;
  u_ns_8B_t first_eth_tx_packets = 0;
  u_ns_8B_t next_eth_tx_packets = 0;

  NSDL2_ETHERNET(NULL, NULL, "Method called");  
  if (!interface_data) return 0;
  iptr = interface_data;
  for (i = 0; i < num_interfaces; i++, iptr++ )
    {
      first_eth_tx_packets += iptr->first_eth_tx_packets;
      next_eth_tx_packets += iptr->next_eth_tx_packets;
    }
  return next_eth_tx_packets - first_eth_tx_packets;
}

#endif
//mode = 0, last-next, 
//mode = 1, last - first
u_ns_8B_t 
get_eth_tx_pps (int mode, unsigned int duration)
{
    struct eth_data *iptr;
    int i;
    double eth_tx_pps;
    double seconds;
    u_ns_8B_t first_eth_tx_packets = 0;
    u_ns_8B_t last_eth_tx_packets = 0;
    u_ns_8B_t next_eth_tx_packets = 0;
    u_ns_8B_t ret;

    NSDL2_ETHERNET(NULL, NULL, "Method called, mode = %d, duration = %lu", mode, duration);  
    if (!interface_data) return 0;
    //sum up all data in all rows
    iptr = interface_data;
    for (i = 0; i < num_interfaces; i++, iptr++ )
    {
	first_eth_tx_packets += iptr->first_eth_tx_packets;
	last_eth_tx_packets += iptr->last_eth_tx_packets;
	next_eth_tx_packets += iptr->next_eth_tx_packets;
    }
#ifdef TEST
    printf("\n first_eth_tx_packets = %llu",first_eth_tx_packets);
    printf("\n last_eth_tx_packets = %llu",last_eth_tx_packets);
    printf("\n next_eth_tx_packets = %llu",next_eth_tx_packets);
#endif
    //assume duration variable contains the duration in milli seconds
    seconds = (double)((double)(duration)/1000.0); 
    //Formula would be
    if (mode)
	eth_tx_pps = ((next_eth_tx_packets - first_eth_tx_packets))/(seconds);
    else
	eth_tx_pps = ((next_eth_tx_packets - last_eth_tx_packets))/(seconds);

    ret = (u_ns_8B_t) (eth_tx_pps);
    return ret;
}

#ifdef TEST
int main()
{
    unsigned int eth_bit_mask;
    double eth_tx_kbps, eth_rx_kbps;
    double eth_tx_pps, eth_rx_pps;
    int duration = 10000;     // duration is in seconds
    int mode = 0;
    int i, samples = 5;

   // eth_bit_mask = parse_eth_data("eth1   eth0 ");
    //init_eth_data(eth_bit_mask);
    init_eth_data();

    printf("\n Num Of Interface = %d",num_interfaces);

    for (i=0; i<samples; i++) {
    sleep (10);
    update_eth_data();
    eth_tx_kbps = get_eth_tx_kbps(mode,duration);
    eth_rx_kbps = get_eth_rx_kbps(mode,duration);
    eth_tx_pps = get_eth_tx_pps(mode,duration);
    eth_rx_pps = get_eth_rx_pps(mode,duration);

    printf("\n eth_tx_kbps = %6.3f",eth_tx_kbps);
    printf("\n eth_rx_kbps = %6.3f",eth_rx_kbps);
    printf("\n eth_tx_pps = %1.0f",eth_tx_pps);
    printf("\n eth_rx_pps = %1.0f\n",eth_rx_pps);
    }

    eth_tx_kbps = get_eth_tx_kbps(1,5*duration);
    eth_rx_kbps = get_eth_rx_kbps(1,5*duration);
    eth_tx_pps = get_eth_tx_pps(1,5*duration);
    eth_rx_pps = get_eth_rx_pps(1,5*duration);

    printf("\n ALL eth_tx_kbps = %6.3f",eth_tx_kbps);
    printf("\n ALL eth_rx_kbps = %6.3f",eth_rx_kbps);
    printf("\n ALL eth_tx_pps = %1.0f",eth_tx_pps);
    printf("\n ALL eth_rx_pps = %1.0f\n",eth_rx_pps);

    return 0;
}
#endif
