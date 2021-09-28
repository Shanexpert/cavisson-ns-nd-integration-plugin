
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/prctl.h>
#ifdef SLOW_CON
#include <linux/socket.h>
#include <netinet/tcp.h>
#define TCP_BWEMU_REV_DELAY 16
#define TCP_BWEMU_REV_RPD 17
#define TCP_BWEMU_REV_CONSPD 18
#endif
#ifdef NS_USE_MODEM
#include <linux/socket.h>
//#include <linux/cavmodem.h>
#include <netinet/tcp.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "user_tables.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "cavmodem.h"
#include "ns_wan_env.h"

#endif
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#ifdef USE_EPOLL
//#include <asm/page.h>
// This code has been commented for FC8 PORTING
//#include <linux/linkage.h>
#include <linux/unistd.h>
#include <sys/epoll.h>
#include <asm/unistd.h>
#endif
#include <math.h>
#include "runlogic.h"
#include "uids.h"
#include "cookies.h"
//#include "logging.h"
#include <gsl/gsl_randist.h>
#include "weib_think.h"
#include "netstorm.h"
#include <pwd.h>
#include <stdarg.h>
#include <sys/file.h>

#include "decomp.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "util.h" 
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "amf.h"
#include "eth.h"
#include "timing.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "ns_master_agent.h"
#include "ns_gdf.h"
#include "ns_custom_monitor.h"
#include "server_stats.h"
#include "ns_trans.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
#include "ns_parent.h"
#include "ns_child_msg_com.h"
#include "ns_http_hdr_states.h"
#include "ns_url_resp.h"
#include "ns_vars.h"
#include "ns_ssl.h"
#include "ns_auto_fetch_embd.h"
#include "ns_parallel_fetch.h"
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_auto_redirect.h"
#include "ns_url_req.h"
#include "ns_replay_access_logs.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_page.h"
#include "ns_vuser.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_global_dat.h"
#include "ns_ftp.h"
#include "ns_smtp_send.h"
#include "ns_smtp.h"
#include "ns_pop3_send.h"
#include "ns_pop3.h"
#include "ns_ftp_send.h"
#include "ns_dns.h"
#include "ns_http_pipelining.h"
#include "ns_http_status_codes.h"

#include "ns_server_mapping.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_http_cache.h"
#include "ns_http_cache_store.h"
#include "ns_http_cache_reporting.h"
#include "nslb_date.h"
#include "ns_sock_listen.h"


/*This function is to open a listen FD  */
/*cptr is control conn.*/

/* This method was added in 3.7.7 for FTP active mode */

// cptr: cptr for the listen socket
// svr_cptr: If this listen is releated to another cptr where we have connection made
//           and is related to listen socket, then pass that cptr else NULL
//
int 
start_listen_socket(connection* cptr, u_ns_ts_t now, connection* svr_cptr)
{
  VUser *vptr = cptr->vptr;
  int select_port;
  
  NSDL2_SOCKETS(NULL, cptr, "Method called, total_ip_entries = %d, cptr=%p, request_type = %hd",
                total_ip_entries, cptr, cptr->url_num->request_type);

  // Socket for listen should not be marked as reuse
  if (cptr->conn_state == CNST_REUSE_CON) {
    // Log some error so that can know if code is coming here or not
    NSDL2_SOCKETS(NULL, cptr, "Reuse cptr will not be used for listening socket ");
    close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);  
  }

  if (cptr->num_retries == 0){//This is the first try
    cptr->started_at = cptr->con_init_time = now;
  }

   /* Set timouts for all protocals*/
   set_cptr_for_new_req(cptr, vptr, now);

   cptr->conn_fd = get_socket(vptr->user_ip->ip_addr.sin6_family, vptr->group_num);
    
   if(cptr->url_num->request_type == CPE_RFC_REQUEST) {
     BIND_SOCKET ((char *)&(cptr->cur_server), 
                  v_port_table[my_port_index].min_listen_port,
                  v_port_table[my_port_index].max_listen_port);
   } else {
     BIND_SOCKET ((char *)&(cptr->cur_server), 
                  v_port_table[my_port_index].min_port,
                  v_port_table[my_port_index].max_port);
   }

#if 0

// MACRO
    bind_done = 0;
    test_bind = 0;
     while (!bind_done)
     {
       test_bind++;
       //Temp change. must change to IPV^ struct
       //cptr->sin.sin_addr.s_addr = vptr->user_ip->ip_addr.s_addr;
       //cptr->sin.sin_addr.s_addr = htonl (nslb_get_sigfig_addr(&vptr->user_ip->ip_addr));
       //memcpy ((char *)&(cptr->sin), (char *) &(vptr->user_ip->ip_addr), sizeof(struct sockaddr_in6));
       memcpy ((char *)&(cptr->sin), (char *)&(cptr->cur_server), sizeof(struct sockaddr_in6));
       if (total_ip_entries)
       {
         //printf("Asking pid %d: addr = 0x%x port=%d\n", getpid(), cptr->sin.sin_addr.s_addr, vptr->user_ip->port);
         if(global_settings->src_port_mode == SRC_PORT_MODE_RANDOM) {
            select_port = calculate_rand_number(v_port_table[my_port_index].min_port, v_port_table[my_port_index].max_port);
            cptr->sin.sin6_port = htons(select_port);
         } else {
            cptr->sin.sin6_port = htons(vptr->user_ip->port);
            vptr->user_ip->port++;
            if (vptr->user_ip->port > v_port_table[my_port_index].max_port)
              vptr->user_ip->port = v_port_table[my_port_index].min_port;
         }
       }

       /*if (fp) fprintf(fp, "%lu: nvm=%d sees_inst=%lu user_index=%lu src_ip=0x%x port=%hd",
       now, my_port_index, vptr->sess_inst, vptr->user_index,
             cptr->sin.sin_addr.s_addr,
       ntohs(cptr->sin.sin_port));*/
       if ( (bind( cptr->conn_fd, (struct sockaddr*) &cptr->sin, sizeof(cptr->sin))) < 0 )
       {
         if (((test_bind % 100) == 0) /*|| (test_bind == 1)*/)
         {
           fprintf(stderr, "Warning: bind (%d): err=%d serr= %s binding attempt %d addr =%s \n", getpid(), errno,
           nslb_strerror(errno), test_bind, nslb_sock_ntop((struct sockaddr *)&cptr->sin));
           if ((test_bind % 100000) == 0)
           {
             fprintf(stderr, "Error: Bind failed 100,000 times. Ending test run .....\n");
             end_test_run();
           }
         }
         //if (fp) fprintf (fp, " ERR\n");
       }
       else
       {
         //if (fp) fprintf (fp, " DONE\n");
         bind_done = 1;
       }
     }

     //nslb_Tcp_listen_ex(select_port, 100000, (char *)(vptr->user_ip->ip_addr));
     /* Set the file descriptor to no-delay mode. */
     if ( fcntl( cptr->conn_fd, F_SETFL, O_NDELAY ) < 0 )
     {
       fprintf(stderr, "Error: Setting fd to no-delay failed\n");
       perror( get_url_req_url(cptr) );
       end_test_run();
     }

// END MACRO
#endif
     /*First listen on the given port. */
     /* we will expect only one data con so pass 1 in listen */
     if (listen(cptr->conn_fd, 1) < 0) {
       perror("listening on ftp data fd");
       end_test_run();
     }

  // We should not check for HPM as we are not taking socket for list for HPM
  // Make sure close_fd() is not adding this socket in HPD sock list
  // if(global_settings->high_perf_mode == 0)
  {
    num_set_select++; /*TST */
    if (add_select(cptr, cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0)
    {
      fprintf(stderr, "Error: Set Select failed on WRITE EVENT\n");
      end_test_run();
    }
  }

#ifdef NS_USE_MODEM
  // If we do not have related svr_cptr, we cannot find svr entry. so we cannot use wan
  if(global_settings->wan_env && svr_cptr)
    set_socket_for_wan(cptr, svr_cptr->old_svr_entry);
#endif

    char srcip[128];
    strcpy(srcip, nslb_get_src_addr(cptr->conn_fd)); 
    NS_DT4(vptr, cptr, DM_L1, MM_SOCKETS, "Listening on IP: %s", srcip); 
  return 0;
}

int accept_connection(connection *cptr, u_ns_ts_t now) {

  int fd;

  if (cptr->url_num->request_type == FTP_DATA_REQUEST)
  {
 
    NSDL3_SOCKETS(NULL, cptr, "Method called, ctrl conn fd = %d", cptr->conn_fd);

    if((fd = accept(cptr->conn_fd, NULL, 0)) < 0)
    {
      if(errno == EAGAIN) return 0;
      fprintf(stderr, "Error: accept failed: err = %s\n", nslb_strerror(errno));
      goto cancel_action;
    }

    NSDL4_SOCKETS(NULL, cptr, "got fd from accept = %d", fd);

    if ( fcntl( fd, F_SETFL, O_NDELAY ) < 0 ) {
      printf("fcntl failed: err = %s", nslb_strerror(errno));
      close(fd);
      goto cancel_action;
    }

    /*Got new FD from accept. Close old FD and change
     *the cptr state to READING*/
    remove_select(cptr->conn_fd);
    if(close(cptr->conn_fd) < 0)
    {
      printf("Error in closing FD = %d\n", cptr->conn_fd);
      goto cancel_action;
    }

    cptr->conn_fd = fd;
    cptr->conn_state = CNST_READING;
    if (add_select(cptr, cptr->conn_fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET) < 0)
    {
      fprintf(stderr, "Error: Set Select failed on READ EVENT\n");
      end_test_run();
    }

    if (cptr->url_num->request_type == FTP_REQUEST ||
             cptr->url_num->request_type == FTP_DATA_REQUEST)
    {
      ftp_avgtime->ftp_num_con_initiated++; // Increament Number of FTP Connection initiated
      if(global_settings->wan_env)
        set_socket_for_wan(cptr, ((connection *)(cptr->conn_link))->old_svr_entry);
    }

    inc_con_num_and_succ(cptr); // Increament Number of TCP Connection success
    now = get_ms_stamp();//Need to calculate connect time
    cptr->connect_time = now - cptr->ns_component_start_time_stamp; //connection time diff
    cptr->ns_component_start_time_stamp = now;//Update NS component start time
    SET_MIN (average_time->url_conn_min_time, cptr->connect_time);
    SET_MAX (average_time->url_conn_max_time, cptr->connect_time);
    average_time->url_conn_tot_time += cptr->connect_time;
    average_time->url_conn_count++;
    return 0;
  }
  else
  {
    
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "Invalid request type %d", cptr->url_num->request_type);
    return -1;
  }

cancel_action:
  if(cptr->url_num->request_type == FTP_DATA_REQUEST)
  {
    ((connection *)(cptr->conn_link))->req_ok = NS_REQUEST_ERRMISC;
    ftp_send_quit(cptr->conn_link, now);
  } 
  return -1;
}

