#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include <regex.h>
#include <libgen.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "url.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "amf.h"
#include "ns_trans_parse.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_vars.h"
#include "ns_log.h"
#include "ns_cookie.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_parse_scen_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_error_codes.h"
#include "ns_page.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_ftp_parse.h"
#include "ns_ftp.h"
#include "ns_smtp_parse.h"
#include "ns_script_parse.h"
#include "ns_vuser.h"
#include "ns_group_data.h"
#include "ns_connection_pool.h"
#include "ns_user_define_headers.h"

/*********************************************************************************
Purpose: This function malloced the data structure and stored the header content
	 data in the buffer and set idx.
Input:	 header -> which is passed by web add header API for added in request header.
         content-> which is passed by web add header API for added in request header 
                    content.    
         flag   -> shows for which URL type.

for example: connection: close\r\n
**********************************************************************************/
void ns_web_add_hdr_data(VUser *vptr, char *header, char *content, int flag)
{
  int hdr_len, content_len, requested_len; 
  int prev_used_len, malloced_len;
  int idx;
  
  NSDL1_API(NULL, NULL, "Method called. Header = %s, content = %s, flag = %d", header, content, flag);
  
  hdr_len = strlen(header) + 1;  //+1 for :
  content_len = strlen(content) + 1 + 2; //+1 for space and +2 for \r\n

  //malloc HTTPData_t structure if not malloced before it 
  if(!vptr->httpData)
  {
    MY_MALLOC_AND_MEMSET(vptr->httpData, sizeof(HTTPData_t), "vptr->httpData", -1);
  }
  //malloc User_header structure
  if(!vptr->httpData->usr_hdr)
  {
    MY_MALLOC_AND_MEMSET(vptr->httpData->usr_hdr, (sizeof(User_header) * 4), "vptr->httpData->usr_hdr", -1);
  }

  if(flag == NS_MAIN_URL_HDR_IDX) 
  {
    idx = 0;
  }
  else if(flag == NS_EMBD_URL_HDR_IDX) 
  { 
    idx = 1;
  }
  else if(flag == NS_ALL_URL_HDR_IDX) 
  {
    idx = 2;
  }

  prev_used_len = vptr->httpData->usr_hdr[idx].used_len;
  malloced_len = vptr->httpData->usr_hdr[idx].malloced_len;
  requested_len = (prev_used_len + hdr_len + content_len);

  NSDL1_API(NULL, NULL, "prev_used_len = %d, malloced_len = %d, hdr_len = %d, content_len = %d", prev_used_len, malloced_len, hdr_len, content_len);
  if(malloced_len < requested_len + 1)
  {
    MY_REALLOC(vptr->httpData->usr_hdr[idx].hdr_buff, requested_len + 1, "hdr_buff", -1);
    vptr->httpData->usr_hdr[idx].malloced_len = requested_len + 1;
  }

  sprintf(vptr->httpData->usr_hdr[idx].hdr_buff + prev_used_len, "%s: %s\r\n", header, content);
  vptr->httpData->usr_hdr[idx].used_len = requested_len;
  NSDL1_API(NULL, NULL, "idx = %d, hdr_buff = %s, used_len = %d", idx, vptr->httpData->usr_hdr[idx].hdr_buff, vptr->httpData->usr_hdr[idx].used_len);
}

/*********************************************************************************
Purpose: This function is reset used len, when page is completed. It is called from
	 handle_page_complete().
**********************************************************************************/
void reset_header_flag(VUser *vptr)
{
  int i;
  NSDL1_API(NULL, NULL, "Method called");
  for(i = 0; i < 3; i++)
  {
    vptr->httpData->usr_hdr[i].used_len = 0;
  }
}

/*********************************************************************************
Purpose: This function is create a new node in the linked list. when user added 
	 header via auto.
Input:	 header   -> header data which is stored in the node
	 content  -> content data which is stored in the node
	 used_len -> shows the complete lenth (header + content)
for Ex:  newnode->hdr_buff => connection: jagat\r\n
**********************************************************************************/
User_header* create_node(char *header, char *content, int requested_len)
{
  User_header *newnode;
  NSDL1_API(NULL, NULL, "Method called. Header = %s, content = %s, requested_len = %d", header, content, requested_len);
  MY_MALLOC_AND_MEMSET(newnode, sizeof(User_header), "newnode", -1);
  MY_MALLOC_AND_MEMSET(newnode->hdr_buff, requested_len + 1, "newnode->hdr_buf", -1);
  sprintf(newnode->hdr_buff, "%s: %s\r\n", header, content);
  newnode->used_len = requested_len;
  NSDL1_API(NULL, NULL, "newnode->hdr_buff = %s, newnode->used_len = %d", newnode->hdr_buff, newnode->used_len);
  newnode->next = NULL;
  return newnode;
}

/*********************************************************************************
Purpose: This function calculate header and content length and added the header,
	 content and used_len in the linked list.
	 we can set the bits according to flag variable. If flag is 0 then set the
	 the MAIN_BIT for the particular node.
Input:	 header  -> which we added in the request header.
	 content -> which we added in the request header content.
	 flat    -> shows for which URL type.
**********************************************************************************/
void add_header_to_list(char *header, char *content, VUser *vptr, int flag)
{ 
  User_header *head;
  int hdr_len, content_len;
  User_header* newnode;

  hdr_len = strlen(header) + 1;  //+1 for :
  content_len = strlen(content) + 1 + 2; //+1 for space and +2 for \r\n

  NSDL1_API(NULL, NULL, "Method called. Header = %s, content = %s, flag= %d", header, content, flag);
  newnode = create_node(header, content, hdr_len + content_len);

  if(flag == NS_MAIN_URL_HDR_IDX)
    newnode->flag |= MAIN_URL_HDR;
  else if(flag == NS_EMBD_URL_HDR_IDX)
    newnode->flag |= EMBD_URL_HDR;
  else if(flag == NS_ALL_URL_HDR_IDX) {
    newnode->flag |= MAIN_URL_HDR;
    newnode->flag |= EMBD_URL_HDR;
  }
  //If node is head
  if(vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next == NULL)
  {
    NSDL3_API(NULL, NULL, "No node exist in list");
    vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next = newnode;
  }
  else
  {
    head = vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next;
    while(head->next != NULL)
      head = head->next;
    head->next = newnode;
  }

  NSDL3_API(NULL, NULL, "newnode->flag = 0x%x", newnode->flag);
}

#if 0
/*********************************************************************************
Purpose: This function is just shows a linked list, how much node is exist in
	 the linked list. 
**********************************************************************************/
static void display_linked_list(VUser *vptr)
{
  NSDL1_API(NULL, NULL, "Method called");
  User_header *head;
  if(vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next == NULL)
  {
    fprintf(stderr, "list is empty\n");
    return;
  }
  head = vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next;
  while(head != NULL)
  {
    NSDL4_API(NULL, NULL, "linked_list data = %s, flag = %d, used_len = %d", head->hdr_buff, head->flag, head->used_len);
    head = head->next;
  }
}
#endif

/*********************************************************************************
Purpose: This function is malloced structure and added the content in the list.
Input:	 vptr    -> pointing current vuser
	 header  -> which is passed by auto header API for added in request header.
	 content -> which is passed by auto header API for added in request header 
		    content.	
	 flag    -> shows for which URL type.
**********************************************************************************/
void ns_web_add_auto_header_data(VUser *vptr, char *header, char *content, int flag)
{
  NSDL1_API(NULL, NULL, "Method called. Header = %s, content = %s, flag = %d", header, content, flag);
  
  //malloc HTTPData_t structure if not malloced before it 
  if(!vptr->httpData)
  {
    MY_MALLOC_AND_MEMSET(vptr->httpData, sizeof(HTTPData_t), "vptr->httpData", -1);
  }
  //malloc User_header structure
  if(!vptr->httpData->usr_hdr)
  {
    MY_MALLOC_AND_MEMSET(vptr->httpData->usr_hdr, (sizeof(User_header) * 4), "vptr->httpData->usr_hdr", -1);
  }
  
  add_header_to_list(header, content, vptr, flag);
  //display_linked_list(vptr);              //used for debugging purpose
}

/*********************************************************************************
Purpose: This function insert the auto headers according to URL type in vector.
	 which is inserted by ns_web_add_auto_header_data() function.
Calling: make_request() in ns_url_req.c
Input:	 vector -> it is stored the buffer & length.
	 next_idx -> pointing the next index.
**********************************************************************************/
inline int insert_auto_headers(connection* cptr, VUser* vptr)
{
  NSDL2_HTTP(NULL, cptr, "Method called");
  User_header *cnode;
  cnode = vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next; 
  while(cnode != NULL)
  {
    //check when node contain main url bit
    if(((cptr->url_num->proto.http.type == MAIN_URL) && (cnode->flag & MAIN_URL_HDR)) || ((cptr->url_num->proto.http.type == EMBEDDED_URL) && (cnode->flag & EMBD_URL_HDR))) 
    {
      NS_FILL_IOVEC(g_req_rep_io_vector, cnode->hdr_buff, cnode->used_len);
    }
    cnode = cnode->next;
  }
  NSDL2_HTTP(NULL, cptr, "No node exist");
  return 0;
}

//This function reset the bits
void reset_bit (int flag, User_header *cnode)
{
  NSDL1_API(NULL, NULL, "Method called");
  switch(flag)
  {
    //for main URL
    case 0:
      cnode->flag &= ~MAIN_URL_HDR;
    break; 
   
    //for embedded URL
    case 1:
      cnode->flag &= ~EMBD_URL_HDR;
    break; 
      
    //for both URL
    case 2:
      cnode->flag = 0;
    break; 
  }
}

/*********************************************************************************
Purpose: This function get the starting string content of header_buff which is 
         seperated with ":" because user want to delete header with header name.
For example: 
         header_buff -> connection: close\r\n
         header      -> connection

Return:  1  -> when header name match with node header buffer 
	 0  -> when not match
**********************************************************************************/
int get_header_data(User_header *cnode, char *header_buff, char *header, int flag, int *status_flg)
{
  char *ptr;
  
  NSDL1_API(NULL, NULL, "Method called: header_buff = %s, header = %s", header_buff, header);
  ptr = strchr(header_buff, ':');
  if(*ptr == '\0')
  {
    NSDL1_API(NULL, NULL, "strchr() is failed to locate the : from header buffer");
    return 1;
  }
  *ptr = '\0';

  if(strcmp(header_buff, header))
  {
    NSDL1_API(NULL, NULL, "%s not match with deleted header = %s", header_buff, header);
    *ptr = ':';
    return 1;     // it will return 1 when header name not match with header_buff
  }

  *ptr = ':';
  *status_flg = 1;
  reset_bit(flag, cnode);
  //If cnode flag become 0 then it means this node will not go in any request. Delete this node
  //To delete this node return 0
  if(cnode->flag == 0)
    return 0;
  else
    return 1;
}

#define CONTINUE_TO_NXT_NODE \
   NSDL1_API(NULL, NULL, "Continue to next node");\
   prev_node = cnode;\
   cnode = cnode->next;\
   continue;

/*********************************************************************************
Purpose: This function delete the header from the request according to flag bits
	 from the list.
	 If flag is 0, then it should be deleted from Main URL request onwards.
Input:	 header -> contain string data which you want to delete from list.
	 flag   -> shows from where you deleted (Main, Embedded or Both URL).
**********************************************************************************/
void ns_web_remove_auto_header_data(VUser *vptr, char *header, int flag, connection* cptr)
{  
  int hdr_status;
  //status_flg is used for check header is exist or not in list
  //url_flag is used to set the auto header bits 
  int status_flg = 0, url_flag = 0;

  User_header *prev_node, *cnode, *tmp;
  NSDL1_API(vptr, NULL, "Method called: header = %s, flag = %d", header, flag);

  //if httpData and usr_hdr both are NULL then return
  if(!(vptr->httpData && vptr->httpData->usr_hdr))
  {
    fprintf(stderr, "Warning: No custom auto header is added, not deleting anything.\n");
    return;
  }

  prev_node = cnode = vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next;
  switch(flag)
  {
    //for main URL
    case 0:
      url_flag |= MAIN_URL_HDR;
    break; 
   
    //for embedded URL
    case 1:
      url_flag |= EMBD_URL_HDR;
    break; 
      
    //for both URL
    case 2:
      url_flag |= MAIN_URL_HDR;
      url_flag |= EMBD_URL_HDR;
    break; 
  }

  if(vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next == NULL)
  {
    fprintf(stderr, "Warning: No custom auto header is added, not deleting anything.\n");
    return;
  }

  NSDL1_API(vptr, NULL, "cnode->hdr_buff = %s, cnode->flag = %0x, url_flag = %0x", cnode->hdr_buff, cnode->flag, url_flag);
  while(cnode != NULL)
  { 
    if(cnode->flag & url_flag)
    {
      NSDL1_API(vptr, NULL, "cnode->hdr_buff = %s, cnode->flag = %0x, url_flag = %0x", cnode->hdr_buff, cnode->flag, url_flag);
      hdr_status = get_header_data(cnode, cnode->hdr_buff, header, flag, &status_flg);
    
      // check if header is not match then we can continue with next node
      if(hdr_status) 
      {
        CONTINUE_TO_NXT_NODE
      }

      // if node is head then delete node
      if(cnode == vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next)
      {
        NSDL1_API(vptr, NULL, "Found header at head, Deleting cnode->hdr_buff = %s", cnode->hdr_buff);
        tmp = cnode;
        vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next = (void *)cnode->next;
        prev_node = cnode = cnode->next;
        FREE_AND_MAKE_NULL(tmp->hdr_buff, "tmp->hdr_buff", -1);
        FREE_AND_MAKE_NULL(tmp, "tmp", -1);
        continue;
      }
      else
      {
        NSDL1_API(vptr, NULL, "Found header is not at head, Deleting cnode->hdr_buff = %s", cnode->hdr_buff);
        tmp = cnode;
        prev_node->next = cnode = cnode->next;
        FREE_AND_MAKE_NULL(tmp->hdr_buff, "tmp->hdr_buff", -1);
        FREE_AND_MAKE_NULL(tmp, "tmp", -1);
        continue;
      }
    }
    CONTINUE_TO_NXT_NODE
  }

  //if header is not found in the list then shows infromation message on console.
  if(status_flg == 0)
  {
    fprintf(stderr, "Warning: ns_web_remove_auto_header_data() API failed to remove header = %s.\n", header);
  }
}

/*********************************************************************************
Purpose: This API deleted all data from the list either user added header in Main,
	 Embedded or Both URL
**********************************************************************************/
void delete_all_auto_header(VUser *vptr)
{
  User_header *ptr, *old;
  NSDL1_API(vptr, NULL, "Method called");

  //if httpData and usr_hdr both are NULL then return
  if(!(vptr->httpData && vptr->httpData->usr_hdr))
  {
    fprintf(stderr, "Warning: No custom auto header is added, not deleting anything.\n");
    return;
  }
  
  ptr = vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next;
  while (ptr)
  {
    old = ptr;
    ptr = ptr->next;
    FREE_AND_MAKE_NULL(old->hdr_buff, "old->hdr_buff", -1);
    FREE_AND_MAKE_NULL(old, "old", -1);
  } 
  vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next = NULL;
}

/*********************************************************************************
Purpose: This function delete all API header when new user start.
calling: user_cleanup() from ns_vuser.c
**********************************************************************************/
void delete_all_api_headers(VUser *vptr)
{
  int i;
  NSDL1_API(NULL, NULL, "Method called");
  //free ns_web_add_header() malloced structure.
  for(i = 0; i < 3; i++)
  {
    FREE_AND_MAKE_NULL(vptr->httpData->usr_hdr[i].hdr_buff, "vptr->httpData->usr_hdr[i].hdr_buff", -1);
  }
  
  //free auto header malloced structure.
  delete_all_auto_header(vptr);
  //free usr_hdr structure
  FREE_AND_MAKE_NULL(vptr->httpData->usr_hdr, "vptr->httpData->usr_hdr", -1);
}

/*********************************************************************************
Purpose: This function reset the all api headers when if same user used API.
Calling: reuse_user() from ns_vuser.c
**********************************************************************************/
void reset_all_api_headers(VUser *vptr)
{
  NSDL1_API(NULL, NULL, "Method called");
  //just reset the used length of herder API
  reset_header_flag(vptr);
  //Remove auto headers 
  delete_all_auto_header(vptr);
}
