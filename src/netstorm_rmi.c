/*********************************************************************
* Name: netstorm_rmi.c
* Purpose: RMI related functions. Currently Not Used.
* Author: Archana
* Intial version date: 22/12/07
* Last modification date: 22/12/07
*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_error_codes.h"
#include "decomp.h"

#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "child_init.h"
#include "amf.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "util.h"
#include "ns_sock_list.h"
#include "ns_sock_com.h"
#include "netstorm_rmi.h"
#include "ns_connection_pool.h"
#include "ns_exit.h"
#ifdef RMI_MODE

int max_bytevar_hash_code;
unsigned int (*bytevar_hash)(const char*, unsigned int);
int (*in_bytevar_hash)(const char*, unsigned int);

void handle_jboss_read(connection* cptr, u_ns_ts_t now)
{
  char recv_buf[4096];
  char* buf_ptr;
  struct hostent *hp;
  int amt_recv;
  VUser* vptr = cptr->vptr;
  int port;
  char tmp[4096];
  int tmplen = 4096;
  struct hostent hostbuf;
  int herr, hres;

  dis_timer_reset( now, cptr->timer_ptr );

  while ((amt_recv = recv(cptr->conn_fd, recv_buf, 4096, 0))) {

    if (amt_recv == -1) {
      if (errno == EAGAIN) {
 	return;
      } else {
 	Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
 	return;
      }
    }

    buf_ptr = recv_buf;
    rx_bytes += amt_recv;

    copy_retrieve_data(cptr, recv_buf, amt_recv, cptr->bytes);

    switch (cptr->conn_state) {
    case CNST_JBOSS_CONN_READ_FIRST:
      if (amt_recv >= cptr->total_bytes) {
	cptr->first_byte_rcv_time = now;
 	buf_ptr = recv_buf + cptr->total_bytes;
 	amt_recv -= cptr->total_bytes;
 	cptr->bytes += cptr->total_bytes;
 	cptr->conn_state = CNST_JBOSS_CONN_GET_IP_ADDR_LENGTH_ONE;
      } else {
 	cptr->total_bytes -= amt_recv;
 	cptr->bytes += amt_recv;
 	continue;
      }
    case CNST_JBOSS_CONN_GET_IP_ADDR_LENGTH_ONE:
      if (amt_recv) {
 	cptr->total_bytes = *buf_ptr * 256;
 	amt_recv--;
 	buf_ptr++;
 	cptr->bytes ++;
 	cptr->conn_state = CNST_JBOSS_CONN_GET_IP_ADDR_LENGTH_TWO;
      } else
 	continue;
    case CNST_JBOSS_CONN_GET_IP_ADDR_LENGTH_TWO:
      if (amt_recv) {
 	cptr->total_bytes += *buf_ptr;
 	amt_recv--;
 	buf_ptr++;
 	vptr->buffer_ptr = vptr->buffer;
 	cptr->bytes ++;
 	if (cptr->total_bytes >= USER_BUFFER_SIZE) {
 	  fprintf(stderr, "handle_jboss_read: ip_address is too big for the user buffer\n");
 	  Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
 	  return;
 	}
 	cptr->conn_state = CNST_JBOSS_CONN_GET_IP_ADDR;
      } else
 	continue;
    case CNST_JBOSS_CONN_GET_IP_ADDR:
      if (amt_recv >= cptr->total_bytes) {
 	memcpy(vptr->buffer_ptr, buf_ptr, cptr->total_bytes);
 	vptr->buffer_ptr[cptr->total_bytes] = '\0';
 	hp = nslb_gethostbyname2(vptr->buffer, AF_INET, &herr);
        if (hp){
 	  bcopy(hp->h_addr, (char *)&(cptr->cur_server.sin_addr.s_addr),
 		hp->h_length);
 	}
 	amt_recv -= cptr->total_bytes;
 	buf_ptr += cptr->total_bytes;
 	cptr->bytes += cptr->total_bytes;
 	vptr->buffer_ptr = vptr->buffer;
 	cptr->total_bytes = 4;
 	cptr->conn_state = CNST_JBOSS_CONN_GET_PORT;
      } else {
 	memcpy(vptr->buffer_ptr, buf_ptr, amt_recv);
 	cptr->total_bytes -= amt_recv;
 	vptr->buffer_ptr += amt_recv;
 	cptr->bytes += amt_recv;
 	continue;
      }
     case CNST_JBOSS_CONN_GET_PORT:
       if (amt_recv >= cptr->total_bytes) {
	 memcpy(vptr->buffer_ptr, buf_ptr, cptr->total_bytes);
	 memcpy(&port, vptr->buffer, sizeof(int));
	 port = ntohl(port);
	 cptr->cur_server.sin_port = ntohs(port);
	 amt_recv -= cptr->total_bytes;
	 buf_ptr += cptr->total_bytes;
	 cptr->bytes += cptr->total_bytes;
	 vptr->buffer_ptr = vptr->buffer;
	 cptr->total_bytes = 24;
	 cptr->conn_state = CNST_JBOSS_CONN_READ_SECOND;
       } else {
	 memcpy(vptr->buffer_ptr, buf_ptr, amt_recv);
	 cptr->total_bytes -= amt_recv;
	 vptr->buffer_ptr += amt_recv;
	 cptr->bytes += amt_recv;
 	continue;
       }
    case CNST_JBOSS_CONN_READ_SECOND:
      if (cptr->total_bytes <= amt_recv) {  /* means that we are done with the jboss_connect url */
	cptr->bytes += cptr->total_bytes;
 	Close_connection(cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_CONTENT_LENGTH);
 	return;
      } else {
 	cptr->total_bytes -= amt_recv;
 	cptr->bytes += amt_recv;
      }
    }
  }
}

#define FIRST_RMI_MSG_LENGTH 7
static int
send_1st_rmi_connect_msg(connection* cptr, u_ns_ts_t now)
{
  static const char first_rmi_msg[FIRST_RMI_MSG_LENGTH] = {0x4a, 0x52, 0x4d, 0x49, 0x00, 0x02, 0x4b};

  cptr->write_complete_time = now;

  if (send(cptr->conn_fd, first_rmi_msg, FIRST_RMI_MSG_LENGTH, 0) != FIRST_RMI_MSG_LENGTH) {
    Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
    return 1;
  } else {
    return 0;
  }
}

static void
send_2nd_rmi_connect_msg(connection *cptr, u_ns_ts_t now)
{
  char second_rmi_msg[4096];
  short ip_length = cptr->bytes - 7;
  char* ip_address = ((VUser*)cptr->vptr)->buffer;
  char* buf_ptr = second_rmi_msg;

  if (ip_length > (4096 - 6)) {   /* the 6 is from 2 bytes for the short to say how big the ip address in the beginning of the message, and 4 '0' bytes at the end */
    fprintf(stderr, "send_2nd_rmi_connect_msg(): ip_address is too big for the buffer\n");
    end_test_run();
  }

  ip_length = ntohs(ip_length);

  memcpy(buf_ptr, &ip_length, sizeof(short));
  buf_ptr += 2;

  memcpy(buf_ptr, ip_address, ip_length);
  buf_ptr += ip_length;

  memset(buf_ptr, 0, 4);

  if (send(cptr->conn_fd, second_rmi_msg, cptr->bytes - 1, 0) != (cptr->bytes - 1)) {
    Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
  } else {
    Close_connection(cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_CONTENT_LENGTH);
  }
}

void
handle_rmi_connect_read(connection* cptr, u_ns_ts_t now)
{
  char recv_buf[4096];
  int amt_recv;
  char* buf_ptr;
  VUser* vptr = cptr->vptr;

  dis_timer_reset( now, cptr->timer_ptr );

  while ((amt_recv = recv(cptr->conn_fd, recv_buf, 4096, 0))) {

    if (amt_recv == -1) {
      if (errno == EAGAIN) {
 	return;
      } else {
 	Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
 	return;
      }
    }

    buf_ptr = recv_buf;
    rx_bytes += amt_recv;

    copy_retrieve_data(cptr, recv_buf, amt_recv, cptr->bytes);

    switch (cptr->conn_state) {
    case CNST_RMI_CONN_READ_VERIFY:
      if (amt_recv) {
	cptr->first_byte_rcv_time = now;
 	if (*buf_ptr != 0x4e) {
 	  fprintf(stderr, "Handle_rmi_connect_read(): Recieved invalid message from server\n");
 	  Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
 	  return;
 	}
 	buf_ptr++;
 	amt_recv--;
 	cptr->bytes ++;
 	cptr->conn_state = CNST_RMI_CONN_GET_IP_ADDR_LENGTH_ONE;
      } else
 	continue;
    case CNST_RMI_CONN_GET_IP_ADDR_LENGTH_ONE:
      if (amt_recv) {
 	cptr->total_bytes = *buf_ptr * 256;
 	amt_recv--;
 	buf_ptr++;
 	cptr->bytes ++;
 	cptr->conn_state = CNST_RMI_CONN_GET_IP_ADDR_LENGTH_TWO;
      } else
 	continue;
    case CNST_RMI_CONN_GET_IP_ADDR_LENGTH_TWO:
      if (amt_recv) {
 	cptr->total_bytes += *buf_ptr;
 	amt_recv--;
 	buf_ptr++;
 	vptr->buffer_ptr = vptr->buffer;
 	cptr->bytes ++;
 	if (cptr->total_bytes >= USER_BUFFER_SIZE) {
 	  fprintf(stderr, "handle_rmi_connect_read: ip_address is too big for the user buffer\n");
 	  Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
 	  return;
 	}
 	cptr->conn_state = CNST_RMI_CONN_GET_IP_ADDR;
      } else
 	continue;
    case CNST_RMI_CONN_GET_IP_ADDR:
      if (amt_recv) {
 	if (amt_recv >= cptr->total_bytes) {
 	  memcpy(vptr->buffer_ptr, buf_ptr, cptr->total_bytes);
 	  vptr->buffer_ptr[cptr->total_bytes] = '\0';
 	  amt_recv -= cptr->total_bytes;
 	  buf_ptr += cptr->total_bytes;
 	  cptr->bytes += cptr->total_bytes;
 	  vptr->buffer_ptr = vptr->buffer;
 	  cptr->total_bytes = 4;
 	  cptr->conn_state = CNST_RMI_CONN_READ_SECOND;
 	} else {
 	  memcpy(vptr->buffer_ptr, buf_ptr, amt_recv);
 	  cptr->total_bytes -= amt_recv;
 	  vptr->buffer_ptr += amt_recv;
 	  cptr->bytes += amt_recv;
 	  continue;
 	}
      }
    case CNST_RMI_CONN_READ_SECOND:
      if (cptr->total_bytes <= amt_recv) {  /* means that we are done with the rmi_connect reading */
 	cptr->total_bytes -= amt_recv;
 	cptr->bytes += amt_recv;
 	send_2nd_rmi_connect_msg(cptr, now);
 	return;
      } else {
 	cptr->total_bytes -= amt_recv;
 	cptr->bytes += amt_recv;
      }
    }
  }
}

void
handle_rmi_connect(connection* cptr, u_ns_ts_t now)
{
  if (send_1st_rmi_connect_msg(cptr, now) == 1)
    return;

  cptr->conn_state = CNST_RMI_CONN_READ_VERIFY;

  handle_rmi_connect_read(cptr, now);
}

void
handle_rmi_read(connection* cptr, u_ns_ts_t now)
{
  int bytes_read;
  ClientData client_data;
  char buf[4096];
  VUser* vptr = cptr->vptr;
  ReqByteVarTableEntry_Shr* cur_bytevar = vptr->cur_bytevar;
  UserByteVarEntry* vubvtable = vptr->ubvtable;

  if (global_settings->debug && (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
    printf("handle_rmi_read:cptr=%p \n", cptr);

  while ((bytes_read = read( cptr->conn_fd, buf, 4096 ))) {

    if (bytes_read == -1) {
      if (errno == EAGAIN) {
 	if (!cptr->url_num->proto.http.exact) {
 	  dis_timer_del(cptr->timer_ptr);
 	  client_data.p = cptr;
          ab_timers[AB_TIMEOUT_URL_IDLE].timeout_val = runprof_table_shr_mem[vptr->group_num].gset.url_idle_secs * 1000;
 	  dis_timer_add(AB_TIMEOUT_URL_IDLE, cptr->timer_ptr, now, packet_idle_connection, client_data, 0);
 	} else
 	  dis_timer_reset(now, cptr->timer_ptr);
 	return;
      } else {
 	Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
 	return;
      }
    }

    if (bytes_read && !cptr->first_byte_rcv_time)
      cptr->first_byte_rcv_time = now;

    copy_retrieve_data(cptr, buf, bytes_read, cptr->bytes);

    cptr->bytes += bytes_read;
    rx_bytes += bytes_read;

    if (cur_bytevar) {
      char** bytevar_value;
      short* amount_written;
      int left_to_write;
      int write_cursor;

      while (1) {
 	cur_bytevar = vptr->cur_bytevar;
 	bytevar_value = &vubvtable[cur_bytevar->bytevar_hash_code].bytevar_value;
 	amount_written = &vubvtable[cur_bytevar->bytevar_hash_code].length;

 	if (cptr->bytes >= cur_bytevar->offset) {
 	  if (*amount_written) {  /* means that the value is already filled in or already partially filled in */
 	    if (*amount_written == cur_bytevar->byte_length) {
 	      fprintf(stderr, "Warning: Ignoring byte var %s\n", cur_bytevar->name);
 	      vptr->cur_bytevar++;
 	      if (vptr->cur_bytevar != vptr->end_bytevar) {
 		vptr->cur_bytevar++;
 		continue;
 	      } else {
 		vptr->cur_bytevar = NULL;
 		break;
 	      }
 	    } else {  /* the value is partially filled in */
 	      left_to_write = cur_bytevar->byte_length - *amount_written;
 	      if (left_to_write >= bytes_read) {
 		memcpy(*bytevar_value + *amount_written, buf, left_to_write);
 		*amount_written = cur_bytevar->byte_length;
 		if (cur_bytevar->type == 1) {
		  if (*amount_written) {
		    ((*bytevar_value)[0]) = 0x00;
		    if (*amount_written > 13)
		      ((*bytevar_value)[13]) = 0x80;
		  }
 		} else {
		  if (*amount_written) {
		    ((*bytevar_value)[0]) = 0x02;
		    if (*amount_written > 13)
		      ((*bytevar_value)[13]) = 0x80;
		  }
 		}
 		vptr->cur_bytevar++;
 		if (vptr->cur_bytevar != vptr->end_bytevar) {
 		  continue;
 		} else {
 		  vptr->cur_bytevar = NULL;
 		  break;
 		}
 	      } else {
 		memcpy(*bytevar_value + *amount_written, buf, bytes_read);
 		*amount_written += bytes_read;
 		break;
 	      }
 	    }
 	  } else {  /* the value has not been filled in yet */
     // 		printf("TST: 2 alloc = %d\n", cur_bytevar->byte_length);
	    *bytevar_value = (char*) my_malloc(cur_bytevar->byte_length);
 	    write_cursor = cur_bytevar->offset - (cptr->bytes - bytes_read);
 	    if ((write_cursor + cur_bytevar->byte_length) <= cptr->bytes) {  /* we can copy in the whole bytes */
 	      memcpy(*bytevar_value, buf+write_cursor, cur_bytevar->byte_length);
 	      *amount_written = cur_bytevar->byte_length;
 	      if (cur_bytevar->type == 1) {
		if (*amount_written) {
		  ((*bytevar_value)[0]) = 0x00;
		  if (*amount_written > 13)
		    ((*bytevar_value)[13]) = 0x80;
		}
 	      } else {
		if (*amount_written) {
		  ((*bytevar_value)[0]) = 0x02;
		  if (*amount_written > 13)
		    ((*bytevar_value)[13]) = 0x80;
		}
 	      }
 	      vptr->cur_bytevar++;
 	      if (vptr->cur_bytevar != vptr->end_bytevar) {
 		continue;
 	      } else {
 		vptr->cur_bytevar = NULL;
 		break;
 	      }
 	    } else {
 	      memcpy(*bytevar_value, buf+write_cursor, bytes_read - write_cursor);
 	      *amount_written = bytes_read - write_cursor;
 	      break;
 	    }
 	  }
 	}
      }
    }

    if (!cptr->url_num->proto.http.exact)
      cptr->request_complete_time = now;

    if (cptr->url_num->proto.http.exact && (cptr->bytes >= cptr->url_num->bytes_to_recv)) { /* we have gotten the whole request */
      if (cptr->url_num->proto.http.bytes_to_recv == cptr->bytes) {
 	Close_connection(cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_CONTENT_LENGTH);
 	return;
      } else {
 	Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
 	return;
      }
    }
  }

  if (bytes_read == 0) {
    if (cptr->url_num->proto.http.exact) {
      if (cptr->bytes == cptr->url_num->proto.http.bytes_to_recv) {
 	Close_connection(cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_CONTENT_LENGTH);
      } else {
 	Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
      }
    } else { /* !cptr->url_num->proto.http.exact */
      if (cptr->bytes >= cptr->url_num->proto.http.bytes_to_recv) {
 	Close_connection(cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_CONTENT_LENGTH);
      } else {
 	Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
      }
    }
  }
}

void
handle_ping_ack_read( connection* cptr, u_ns_ts_t now ) {
  int bytes_read;
  char byte;

  dis_timer_reset( now, cptr->timer_ptr );

   if ((bytes_read = read(cptr->conn_fd, &byte, sizeof(char)))) {

     if (bytes_read == -1) {
       if (errno == EAGAIN) {
	 return;
       } else {
	 Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
	 return;
       }
     }

     rx_bytes += bytes_read;

     copy_retrieve_data(cptr, &byte, bytes_read, cptr->bytes);

     cptr->bytes ++;

     cptr->first_byte_rcv_time = now;
     if (byte == 0x53)
       Close_connection(cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_CONTENT_LENGTH);
     else
       Close_connection(cptr, 1, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
     return;
   } else {
     Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
   }
}

static void
packet_idle_connection( ClientData client_data, u_ns_ts_t now )
{
  connection* cptr = client_data.p;
  VUser* vptr = cptr->vptr;
  u_ns_ts_t addition_amt = global_settings->url_idle_secs * 1000;

  if (cptr->bytes >= cptr->url_num->proto.http.bytes_to_recv) {
      vptr->pg_begin_at += addition_amt;

    vptr->started_at += addition_amt;

    tx_update_begin_at (vptr, addition_amt);       // Defined in ns_trans.c: Anuj,

    Close_connection( cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_CONTENT_LENGTH);
  } else {
    Close_connection( cptr, 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
  }
}

FILE* record_logging_fptr;

#define RECORD_LOGGING_BUFFER_SIZE 16384
struct logging_table_entry* logging_table;   /* size of this table defined in global_settings->resp_logging_size */
int logging_table_idx = 0;
char logging_buffer[RECORD_LOGGING_BUFFER_SIZE];
char* logging_buffer_ptr = logging_buffer;
unsigned long long file_offset = 0;
char resp_logging_on = 0;

void start_resp_logging() {
  char buf[128];

  resp_logging_on = 1;

  sprintf(buf, "logs/TR%d/bfile.%d", testidx, my_port_index);
  if ((record_logging_fptr = fopen(buf, "w+")) == NULL) {
    NS_EXIT(1, "Error in creating record logging file");
    perror("fopen");
  }

  logging_table = (struct logging_table_entry*) malloc(sizeof(struct logging_table_entry) * global_settings->resp_logging_size);
  memset(logging_table, 0, sizeof(struct logging_table_entry) * global_settings->resp_logging_size);
}

void stop_resp_logging() {
  fwrite(logging_buffer, sizeof(char), logging_buffer_ptr - logging_buffer, record_logging_fptr);
  fclose(record_logging_fptr);
}

void start_logging_req(int url_id, int pg_id, int sess_inst_id, u_ns_ts_t start_time) {
  logging_table[logging_table_idx].url_id = url_id;
  logging_table[logging_table_idx].pg_id = pg_id;
  logging_table[logging_table_idx].sess_inst_id = sess_inst_id;
  logging_table[logging_table_idx].req_start_time = start_time;
  logging_table[logging_table_idx].req_len = 0;
}

void start_logging_resp(u_ns_ts_t start_time) {
  if (resp_logging_on == 0)
    printf("in start_logging_resp w/ resp_logging_on\n");
  logging_table[logging_table_idx].resp_start_time = start_time;
  logging_table[logging_table_idx].resp_len = 0;
}

void write_logging_data(const char* data, int size, int type) {
  int copy_length;

  if (resp_logging_on == 0)
    printf("in write_logging-data w/ resp_logging_on\n");

  if (type == 0)
    logging_table[logging_table_idx].req_len += size;
  else
    logging_table[logging_table_idx].resp_len += size;

  while (size > (RECORD_LOGGING_BUFFER_SIZE - (logging_buffer_ptr - logging_buffer))) {
    copy_length = RECORD_LOGGING_BUFFER_SIZE - (logging_buffer_ptr - logging_buffer);
    memcpy(logging_buffer_ptr, data, copy_length);
    data += copy_length;
    size -= copy_length;
    file_offset += copy_length;
    fwrite(logging_buffer, sizeof(char), RECORD_LOGGING_BUFFER_SIZE, record_logging_fptr);
    logging_buffer_ptr = logging_buffer;
  }

  if (size) {
    memcpy(logging_buffer_ptr, data, size);
    file_offset += size;
    logging_buffer_ptr += size;
    if (file_offset == RECORD_LOGGING_BUFFER_SIZE) {
      fwrite(logging_buffer, sizeof(char), RECORD_LOGGING_BUFFER_SIZE, record_logging_fptr);
      logging_buffer_ptr = logging_buffer;
    }
  }
}

void stop_logging_entry(char success) {
  if (resp_logging_on == 0)
    printf("in stop_logging_entry w/ resp_logging_on\n");
  logging_table[logging_table_idx].status = success;
  logging_table_idx++;
  if (logging_table_idx == global_settings->resp_logging_size) {
    stop_resp_logging();
    resp_logging_on = 0;
    logging_table_idx --;
  }
}

inline int
make_rmi_request(connection* cptr, NSIOVector *ns_iovec) {
  http_request_Shr* request = cptr->url_num;
  return insert_segments(cptr->vptr, cptr, &request->url, ns_iovec, NULL, 0, 0, 0, request, 0);
}

void
retry_connection_callback(ClientData client_data, u_ns_ts_t now) {
  connection* cptr = client_data.p;
  start_new_connection(cptr, now);
}

static void
rmi_connection_retry(connection *cptr, u_ns_ts_t now) {
          ClientData cd;
          connection* conn_ptr;
          VUser* vptr = cptr->vptr;

  cptr->num_retries++;
  //Atul 08/16/2007 - max_url_retries set using conf file
  if (cptr->num_retries <= global_settings->max_url_retries ) {
          cd.p = cptr;
          close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
          conn_ptr = get_free_connection_slot(vptr);
          assert(cptr == conn_ptr);
          vptr->cnum_parallel++;
          (void) dis_timer_add( AB_TIMEOUT_RETRY_CONN, cptr->timer_ptr, now, retry_connection_callback, cd, 0);
  } else {
        Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
  }
}

void
next_rmi_connection( VUser *vptr, connection* cptr, u_ns_ts_t now)
{
  if (cptr == NULL) {
    cptr = get_free_connection_slot(vptr);
    cptr->url_num = vptr->cur_url;
  }

  vptr->cur_url++;
  cptr->url_num++;
  vptr->urls_left--;
  renew_connection(cptr, now);
}

void
execute_next_rmi_page(VUser *vptr, connection* cptr, u_ns_ts_t now, PageTableEntry_Shr* next_page)
{

  vptr->cur_page = next_page;
  on_page_start(vptr, now);

  if (cptr == NULL)
    cptr = get_free_connection_slot(vptr);

  cptr->url_num = vptr->first_page_url;
  vptr->cur_url = cptr->url_num;
  vptr->urls_left--;
  renew_connection(cptr, now);
}

static void
reuse_rmi_user( VUser *vptr, connection* cptr, u_ns_ts_t now )
{
  int i, j, max_idx;
  int init_page;
  //unique_group_table_type* unique_group_ptr = unique_group_table;

  if (global_settings->debug &&  (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
    printf("reuse_user:cptr=%p\n", cptr);

  // Since we are reusing VUser slot, let us inititialize link lists in this slot
  // This is usally done in get_free_user_slot
  // Initialize Host_Server table for this vuser slot


  // Initialize User group table for this vuser slot
  free_uniq_var_if_any (vptr);
  bzero ((char *)vptr->ugtable, user_group_table_size);
  set_uniq_vars(vptr->ugtable);

  // Commented by Neeraj on Oct 17, 07 as cookie should not be freed here
#if 0
  // Initialize User cookie table for this vuser slot
  for (i = 0; i < max_cookie_hash_code; i++) {
    if (vptr->uctable[i].cookie_value) {
      free(vptr->uctable[i].cookie_value);
    }
  }
  bzero ((char *)vptr->uctable, user_cookie_table_size);
#endif

  // Initialize User dynamic vars table for this vuser slot
  for (i = 0; i < max_dynvar_hash_code; i++) {
    if (vptr->udvtable[i].dynvar_value)
      free(vptr->udvtable[i].dynvar_value);
  }
  bzero ((char *)vptr->udvtable, user_dynamic_vars_table_size);
#ifdef RMI_MODE
  // Initialize User byte vars table for this vuser slot
  for (i = 0; i < max_bytevar_hash_code; i++) {
    if (vptr->ubvtable[i].bytevar_value)
      free(vptr->ubvtable[i].bytevar_value);
  }
  bzero ((char *)vptr->ubvtable, user_byte_vars_table_size);
#endif
  max_idx = vptr->sess_ptr->numUniqVars;
  for (i = 0; i < max_idx; i++) {
    if (vptr->sess_ptr->var_type_table_shr_mem[i]) {  /* is an array */
      if (vptr->uvtable[i].value.array) {
	for (j = 0; j < vptr->uvtable[i].length; j++) {
	  if (vptr->uvtable[i].value.array[j].value)
	    free(vptr->uvtable[i].value.array[j].value);
	}
	free(vptr->uvtable[i].value.array);
      }
    } else {
	if (vptr->uvtable[i].value.value)
	  free(vptr->uvtable[i].value.value);
    }
  }
  bzero((char *)vptr->uvtable, user_var_table_size);
  // Initailize User server table for this vuser slot
  bzero ((char *)vptr->ustable, user_svr_table_size);

  if (on_new_session_start(vptr, now)) return;

  if (cptr == NULL)
    cptr = get_free_connection_slot(vptr);

#ifdef RMI_MODE
  vptr->cur_url = vptr->first_page_url;
#endif
  cptr->url_num = vptr->first_page_url;
  vptr->urls_left--;
  vptr->start_url_ridx = 1;
  renew_connection(cptr, now);
}

#endif
