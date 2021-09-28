/************************************************************************************************
 * File Name            : ns_dbu_monitor.c
 * Author(s)            : Jharana/Rupinder 
 * Date                 : 
 * Copyright            : (c) Cavisson Systems
 * Purpose             
 *        To start the ns_dbu_monitor and send data related to gdf regarding Appliance Monitoring for db_upload module
 *                        
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
 ***********************************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <regex.h>
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/epoll.h>

#include "nslb_util.h"
#include "nslb_db_util.h"
#include "nslb_signal.h"
#include "nslb_multi_thread_trace_log.h"
#include "nslb_partition.h"
#include "nslb_db_upload_common.h"
#include "nslb_db_util.h"
#include "nslb_sock.h"
#include "nslb_alloc.h"
#include "nslb_log.h"
#include "nslb_dbu_monitor_stats.h"
//#include "ndc_common_msg_com_util.h"
#include "nslb_mon_registration_util.h"
#include "ns_db_upload.h"
#include "nslb_mon_registration_con.h"
//Structure for dbu monitor registration
MonRegCon *mrcptr = NULL;

//Purpose - To remove epoll_fd and close sock_fd 
void remove_fd_and_close_monitor_connection()
{ 
  if(mrcptr->mon_reg_fd > 0)
  { 
    nslb_mon_reg_remove_select(v_epoll_fd, mrcptr->mon_reg_fd);
    close(mrcptr->mon_reg_fd);
    mrcptr->mon_reg_fd = -1;
  }
}


//Purpose - To remove dbu_monitor structure
void destroy_monitor_structure(char *err_msg)
{ 
  if((nslb_mon_reg_destroy(&mrcptr, err_msg)) == NSLB_MON_REG_FAILURE)   
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_ERROR, "Error : %s", err_msg);
}


void init_dbu_monitor()
{
  char IP[128] = "";
  char msg[BUFFER_LEN] = "";
  int ret;

  //malloc the MonRegCon structure and filling values
  if(mrcptr == NULL)
    mrcptr = nslb_mon_reg_init(ns_db_upload.controller, "cm_hm_ns_dbu_monitor_stats.gdf", "NSDBU", getpid(), msg, ns_db_upload.tr_num);

  if(!mrcptr)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_ERROR, "Error : %s", msg);
    return;
  }

  ret = nslb_mon_reg_create_and_send(mrcptr, msg);


  if(ret == NSLB_MON_REG_PORT_NOT_FOUND)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO, "%s", msg);

  }
  else if(ret == NSLB_MON_REG_PARTIAL_DATA_SENT || ret == NSLB_MON_REG_CONN_CONNECTING)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO, "%s", msg);

    //add fd to epoll 
    nslb_mon_reg_add_select(v_epoll_fd, mrcptr->mon_reg_fd, EPOLLIN|EPOLLHUP|EPOLLERR|EPOLLOUT);
  }
  //FOr port not found, log & let monitor try at next progress interval
  //connecting & data send error remove from epoll & destroy
  else if(ret == NSLB_MON_REG_CONN_STILL_CONNECTING || ret == NSLB_MON_REG_DATA_NOT_SENT)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO, "%s", msg);
    //add in epoll
    remove_fd_and_close_monitor_connection();
    destroy_monitor_structure(msg);
  }
  else if(ret == NSLB_MON_REG_DATA_SENT)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO, "%s", msg);

    //If previoulsy partially data sent or was in connecting state, then after successfylly data sent then remove EPOLLOUT from event
    nslb_mon_reg_mod_select(v_epoll_fd, mrcptr->mon_reg_fd, EPOLLIN|EPOLLHUP|EPOLLERR);
  }
  else if(ret == NSLB_MON_REG_PARTIAL_DATA_SENT || ret == NSLB_MON_REG_DATA_SENT)
  {
    strcpy(IP, nslb_get_src_addr(mrcptr->mon_reg_fd));
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO, "NDDBU IP Address - %s", IP);
  }
}


/* This method will fill dbu monitor stats in buffer of all thereds */
int fill_monitor_stat_buffer(char *buffer)
{
  int buffer_offset = 0;
  int i;
  double per_min = 60.0/(ns_db_upload.progress_interval/1000);
  double size_in_mb = 1024 * 1024;
   int threads ;
  // ns_db_upload.numThreads - 1 , because we are excluding analyze thread

  if(ns_db_upload.dbu_analyze_thread_main_table_on_off)
    threads = ns_db_upload.numThreads - 1;
  else
    threads = ns_db_upload.numThreads;
 
  for(i = 0; i < threads; i++)
  {
    pthread_mutex_lock(&(ns_db_upload.dbuMonitorStat[i].dbuMonitorStrucLock));
    if(i != threads - 1)
    {
      buffer_offset += sprintf(buffer + buffer_offset, "%d:%s|"
                           "%f %f %d %f "
                           "%d "
                           "%d %d %d %d "
                           "%d %d %f %d "
                           "%d "
                           "%lf "
                           "%lf %lf %lf %lf "
                           "%lf %lf %lf %lf "
                           "%lf %lf %lf %lf "
                           "%lf %lf %lf %lf\n",
      i, ns_db_upload.dbUploaderDynamicCSVInitStruct[i].csvtable,
      (ns_db_upload.dbuMonitorStat[i].records_miss_match_count*per_min),
      (ns_db_upload.dbuMonitorStat[i].miss_match_with_data_def_count*per_min),
      (ns_db_upload.dbuMonitorStat[i].partition_backlog), ns_db_upload.dbuMonitorStat[i].data_backlog_pct,
      ns_db_upload.dbuMonitorStat[i].num_db_connection,
      ns_db_upload.dbuMonitorStat[i].cur_start_constraint_missing, ns_db_upload.dbuMonitorStat[i].prev_start_constraint_missing,
      ns_db_upload.dbuMonitorStat[i]. prev_end_constraint_missing, ns_db_upload.dbuMonitorStat[i]. cur_start_constraint_diff,
      ns_db_upload.dbuMonitorStat[i].prev_start_constraint_diff,  ns_db_upload.dbuMonitorStat[i].prev_end_constraint_diff,
      ns_db_upload.dbuMonitorStat[i].child_tbl_analyze_time, ns_db_upload.dbuMonitorStat[i].master_tbl_analyze_time,
      ns_db_upload.dbuMonitorStat[i].incorrect_data_size,
      (ns_db_upload.dbuMonitorStat[i].data_upload_size/size_in_mb)*per_min,
      (ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.total_size)?(ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.total_size)/(ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.count):0,
      ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.min, ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.max,
      ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.count,
      (ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.total_size)?(ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.total_size)/(ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.count):0,
      ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.min, ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.max,
      ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.count,
      (ns_db_upload.dbuMonitorStat[i].data_correction_time.total_size)?(ns_db_upload.dbuMonitorStat[i].data_correction_time.total_size)/(ns_db_upload.dbuMonitorStat[i].data_correction_time.count):0,
      ns_db_upload.dbuMonitorStat[i].data_correction_time.min, ns_db_upload.dbuMonitorStat[i].data_correction_time.max,
      ns_db_upload.dbuMonitorStat[i].data_correction_time.count,
      (ns_db_upload.dbuMonitorStat[i].data_backlog.total_size)?(ns_db_upload.dbuMonitorStat[i].data_backlog.total_size)/(ns_db_upload.dbuMonitorStat[i].data_backlog.count):0,
      ns_db_upload.dbuMonitorStat[i].data_backlog.min, ns_db_upload.dbuMonitorStat[i].data_backlog.max,
      ns_db_upload.dbuMonitorStat[i].data_backlog.count);
    }
    else
    {
     // Fill buffer for metadata thread
      buffer_offset += sprintf(buffer + buffer_offset, "%d:%s|"
                           "%f %f %d %d "
                           "%d "
                           "%d %d %d %d "
                           "%d %d %d %d "
                           "%d "
                           "%lf "
                           "%lf %lf %lf %lf "
                           "%lf %lf %lf %lf "
                           "%lf %lf %lf %lf "
                           "%d %d %d %d\n",
      i, "Metadata",
      (ns_db_upload.dbuMonitorStat[i].records_miss_match_count*per_min),
      (ns_db_upload.dbuMonitorStat[i].miss_match_with_data_def_count*per_min),
      0, 0,
      //(ns_db_upload.dbuMonitorStat[i].partition_backlog), ns_db_upload.dbuMonitorStat[i].data_backlog_pct,
      ns_db_upload.dbuMonitorStat[i].num_db_connection,
      0, 0,
      0, 0,
      0, 0,
      0, ns_db_upload.dbuMonitorStat[i].master_tbl_analyze_time,
      ns_db_upload.dbuMonitorStat[i].incorrect_data_size,
      (ns_db_upload.dbuMonitorStat[i].data_upload_size/size_in_mb)*per_min,
      (ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.total_size)?(ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.total_size)/(ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.count):0,
      ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.min, ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.max,
      ns_db_upload.dbuMonitorStat[i].upload_time_taken_small_data.count,
      (ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.total_size)?(ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.total_size)/(ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.count):0,
      ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.min, ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.max,
      ns_db_upload.dbuMonitorStat[i].upload_time_taken_large_data.count,
      (ns_db_upload.dbuMonitorStat[i].data_correction_time.total_size)?(ns_db_upload.dbuMonitorStat[i].data_correction_time.total_size)/(ns_db_upload.dbuMonitorStat[i].data_correction_time.count):0,
      ns_db_upload.dbuMonitorStat[i].data_correction_time.min, ns_db_upload.dbuMonitorStat[i].data_correction_time.max,
      ns_db_upload.dbuMonitorStat[i].data_correction_time.count,
      0, 0, 0, 0);
    }
    pthread_mutex_unlock(&(ns_db_upload.dbuMonitorStat[i].dbuMonitorStrucLock));
  }

  NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO, "Fill monitor data. Buffer : '%s'", buffer);
  return buffer_offset;
}

void send_monitor_data_to_ns(char *msg)
{
  char buffer[25*BUFFER_LEN] = "";
  int size;
  int ret;

  if (!mrcptr || mrcptr->mon_reg_fd == -1 || mrcptr->flags & NSLB_MON_REG_CONNECTING)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO,
                                                                 "DBU monitor connection is not established with NS. Going to retry to make connection.");
    init_dbu_monitor();
  }

  //MON: add log  no cannection to sent data
  if(!mrcptr || (mrcptr->mon_reg_fd == -1 || mrcptr->flags & NSLB_MON_REG_CONNECTING))
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_ERROR, "Error : Error in making connection with NS."
                                               "No connection with NS while sending data.");
    return;
  }

  size = fill_monitor_stat_buffer(buffer);

  //Reseting values after data fill in buffer
  nslb_reset_dbu_monitor_data(ns_db_upload.dbuMonitorStat, ns_db_upload.numThreads - 1);

  //in this case it will send buffer 
  ret = nslb_mon_reg_send(mrcptr, buffer, size, msg);

  if(ret < 0)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_ERROR, "Error : %s", msg);
    remove_fd_and_close_monitor_connection();
    destroy_monitor_structure(msg);
  }
  else if(ret == NSLB_MON_REG_PARTIAL_DATA_SENT && mrcptr->flags & NSLB_MON_REG_WRITING)
  {
    NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO, "Partial buffer sent to NS.Going to add in epoll.");
    //add fd to epoll and wait for event
    nslb_mon_reg_mod_select(v_epoll_fd, mrcptr->mon_reg_fd, EPOLLOUT|EPOLLIN|EPOLLHUP|EPOLLERR);
  }
  else if(ret == NSLB_MON_REG_DATA_SENT)
  {
    NSLB_TRACE_LOG3(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO,
                                                     "Data sent completely.buf = %s", buffer);

    //data sent completely and goes in while loop and comes in case of 
    nslb_mon_reg_mod_select(v_epoll_fd, mrcptr->mon_reg_fd, EPOLLIN|EPOLLHUP|EPOLLERR);
  }
}


