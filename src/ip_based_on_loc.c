/******************************************************************
 *
 * Name    :    ip_based_on_loc.c
 * Author  :    Abhishek/Achint/Neeraj
 * Purpose :    Generate a random IP corresponding to given area id
 * Usage:  ip_based_on_loc [Start_area_id End_area_id num]
 * This script use area_ip.txt file. From area_ip.txt file script read the area id and corresponding IPs and pick up a random IP.
 *
 * Modification History:
 *   09/16/06: Abhishek - Initial Version
 *   10/15/06: Abhishek/Neeraj - Bug fix for not handling missing area_ids in the file and code cleanup.
 *
 ******************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h" 
#include "util.h" // Anuj for removing warning function do_shmget

typedef struct
{
  unsigned int area_id;     // Aread is stored for validation of input area id.
  unsigned int start_ip;
  unsigned int total_ip;
  unsigned int start_idx;   // Index in IP_block for 1st block for this area
  unsigned int num_blocks;  // Number of blocks in IP_block array for this area
} IP_node;

typedef struct
{
  unsigned int start_ip;
  unsigned int num_ip;
} IP_block;

IP_node    *ip_node;   // This is array of IP_node (one node per Area Id)
IP_block   *ip_block;  // This is array of IP_block. This wills one or more blocks per Area Id
IP_node    *tmp_ip_node;
IP_block   *tmp_ip_block;


unsigned int total_ip_nodes;
static int running_as_tool = 0;

int read_area_ip_file(char *filename)
{
  char *tok, buf[4096];
  unsigned int total_ip_blocks;
  int cur_area_id = -1;
  unsigned int next_area_id, file_start_ip, file_num_ip;
  //unsigned int next_area_id, file_start_ip, file_num_ip, node_idx = 0;
  //int i = 0;
  FILE *fp_area_ip;
  unsigned int block_idx = 0;
  unsigned int total_rows = 0;


  fp_area_ip = fopen(filename, "r");

  if(fp_area_ip  == NULL)
  {
    printf("Unable to open file area_ip.txt\n");
    return -1;
  }
  //Ignore first line
  fgets(buf, 4096, fp_area_ip);

  fgets(buf, 4096, fp_area_ip);
  tok = strtok(buf, "=");
  tok = strtok(NULL, "=");
  total_ip_nodes = atoi(tok);

  fgets(buf, 4096, fp_area_ip);
  tok = strtok(buf, "=");
  tok = strtok(NULL, "=");
  total_ip_blocks = atoi(tok);

  //printf("Total Area Id = %d, Total IP Blocks = %d\n", total_ip_nodes, total_ip_blocks);
#ifdef USE_SHM
  ip_node = do_shmget(sizeof(IP_node) * total_ip_nodes, "IP Nodes for GeoIP");
#else
  ip_node = malloc(sizeof(IP_node) * total_ip_nodes);
#endif
  if(ip_node == NULL)
  {
    printf("Malloc failed\n");
    return -1;
  }
  memset(ip_node, 0, sizeof(IP_node) * total_ip_nodes);

#ifdef USE_SHM
   ip_block = do_shmget(sizeof(IP_block) * total_ip_blocks, "IP blocks for GeoIP");
#else
  ip_block = malloc(sizeof(IP_block) * total_ip_blocks);
#endif
  if(ip_block == NULL)
  {
    printf("Malloc failed\n");
    return -1;
  }
  memset(ip_block, 0, sizeof(IP_block) * total_ip_blocks);

  tmp_ip_node = ip_node;
  tmp_ip_block = ip_block;

  // Note - File does not have all Area Ids present. So there may be gaps in the files
  // Also it is possible that first area Id (0) may not be in the file.
  while (fgets(buf, 4096, fp_area_ip))
  {
    // printf("Processing row %d\n", total_rows);
    tok = strtok(buf, ",");
    next_area_id = atoi(tok);

    tok = strtok(NULL, ",");
    file_start_ip = atoi(tok);

    tok = strtok(NULL, ",");
    file_num_ip = atoi(tok);

    if(next_area_id != cur_area_id)  // If first is area id 0, then tmp_ip_ptr is already pointing to first node.
    {
      if(total_rows != 0)  // For first row, tmp_ip_ptr is already pointing to first node.
      {
        // Abhishek - check for skipped area id
        if((next_area_id - cur_area_id) == 1)  // Next area id is one more than prev area id.
          tmp_ip_node++;
        else   // Next area id is NOT one more than prev area id.
               // this will skip the node for missing area id such as event occur as 38 to 40
        {
          if(running_as_tool)
            printf("Missing Area Ids. Current Area Id =  %d, Next Area Id = %d\n", cur_area_id, next_area_id);
          tmp_ip_node = tmp_ip_node + (next_area_id - cur_area_id);
        }
      }
      cur_area_id = next_area_id;
      // printf("Processing Area Id =  %d\n", cur_area_id);
      tmp_ip_node->area_id = next_area_id;
      tmp_ip_node->start_ip = file_start_ip;
      tmp_ip_node->total_ip = file_num_ip;
      tmp_ip_node->num_blocks = 1;
      tmp_ip_node->start_idx = block_idx;
    }
    else
    {
      tmp_ip_node->total_ip = tmp_ip_node->total_ip + file_num_ip;
      tmp_ip_node->num_blocks++;
    }

    //  Store fields in IP_block
    tmp_ip_block->start_ip = file_start_ip;
    tmp_ip_block->num_ip = file_num_ip;
    //if (running_as_tool)
    //  printf("Area Id =  %d, Total Ip = %d\n", cur_area_id, tmp_ip_node->total_ip);

    tmp_ip_block++;

    block_idx++;
    total_rows++;
  }
  if(total_rows != total_ip_blocks)
  {
    printf("Total rows (%d) in file does not match with total blocks (%d)\n", total_rows, total_ip_blocks);
    return -1;
  }
  fclose(fp_area_ip);
  return 0;
}

// Changed return to unsigned int, as IP can be -ve
// Returns 0 for error
unsigned int get_an_IP_address_for_area(unsigned int area_id)
{
static int file_read = 0;

  if(file_read == 0)
  {
    char buf[128];

    if (getenv("NS_WDIR")) {
	sprintf(buf, "%s/etc/area_ip.txt", getenv("NS_WDIR"));
    } else {
	strcpy(buf, "/home/cavisson/work/etc/area_ip.txt");
    }
    if(read_area_ip_file(buf) != 0)
      return -1;
    file_read = 1;

  }

  if ( area_id < 0 || area_id >= total_ip_nodes )
  {
      printf("Area Id [%d] is not valid. It should be between 0 and %d\n", area_id, total_ip_nodes - 1);
      return -1;
  }

  tmp_ip_node = ip_node + area_id;
  tmp_ip_block = ip_block + tmp_ip_node->start_idx;

  unsigned int total_ip = tmp_ip_node->total_ip;
  unsigned int start_ip = tmp_ip_node->start_ip;
  // abhishek - this will check the existence of area id in file
  if(tmp_ip_node->area_id != area_id)  // This will happens for area id for which there was no entry (No Ips)
  {
    printf("Area Id [%d] is not valid. There was entry for this Area Id in the file\n", area_id);
    return 0;
  }

  // Since we are doing %, random_num will be from 0 to (total_ip - 1)
  unsigned int random_num = rand() % total_ip;

  unsigned int ip_count = 0;
  unsigned int i, ip;
  unsigned int num_blocks = tmp_ip_node->num_blocks;

  if (running_as_tool)
    printf("Calculating random IP for Area = %d, Start IP = %d, Total IP = %d, Random number = %d\n", area_id, start_ip, total_ip, random_num);

  for(i = 0; i < num_blocks; i++)
  {
    //printf("Checking IP Block #%d, Start IP = %d, Num Ip = %d\n", i + 1, tmp_ip_block->start_ip, tmp_ip_block->num_ip);
    // Abhishek - to handle the ip igeneration when random number is 0
    // random number should not equals to  max ip number
    if((random_num >= ip_count) && (random_num < (ip_count + tmp_ip_block->num_ip)))
    {
      ip = tmp_ip_block->start_ip + random_num - ip_count;
      return (ip);
    }
    ip_count = ip_count + tmp_ip_block->num_ip;
    tmp_ip_block++;
  }
  printf("Could not find IP. IP Block #%d, Start IP = %d, Num Ip = %d\n", i + 1, tmp_ip_block->start_ip, tmp_ip_block->num_ip);
  return 0;
}


#ifdef TEST
// This function is copied from other file. Can be deleted later.
char *ns_char_ip(unsigned int addr)
{
  static char str_address[16];
  unsigned int a, b, c,d;
  a = (addr >>24) & 0x000000FF;
  b = (addr >>16) & 0x000000FF;
  c = (addr >>8) & 0x000000FF;
  d = (addr) & 0x000000FF;
  sprintf(str_address, "%d.%d.%d.%d", a,b,c,d);
  return str_address;
}

int main(int argc, char *argv[])
{
  unsigned int start_area_id;
  unsigned int end_area_id;
  unsigned int i, num, area_id;
  running_as_tool = 1;

  if(argc == 4)
  {
    start_area_id = atoi(argv[1]);
    end_area_id = atoi(argv[2]);
    num = atoi(argv[3]);
    if (start_area_id > end_area_id)
    {
      printf("Start Area Id %d should be less than End Area Id %d\n", start_area_id, end_area_id);
      exit (-1);
    }
  }
  else
  {
    printf("Error: Usage -  ip_based_on_loc <start_area_id> <end_area_id> <num>\n");
    exit (-1);
  }

  int ip;
  for(area_id = start_area_id; area_id <= end_area_id; area_id++)
  {
    for (i = 0; i < num; i++)
    {
      ip = get_an_IP_address_for_area(area_id);
      if(ip == 0)
        continue; // this will continue even when area id not exist 
      else
        printf("IP Address for area_id = %d is %d (%s)\n", area_id, ip, ns_char_ip(ip));
    }
  }
  exit (0);
}
#endif
