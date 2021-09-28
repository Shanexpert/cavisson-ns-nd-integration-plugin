#ifndef NS_STATIC_VAR_RTC_H
#define NS_STATIC_VAR_RTC_H

#define FIELD_MODE               0
#define FIELD_FPARAM             1
#define FIELD_DATA_FILE          2
#define FIELD_DONE               3

#define INSERT_MODE              0
#define APPEND_MODE              1
#define REPLACE_MODE             2

#define MAX_FPRAM_BUF_SIZE       5 * 1024
#define MAX_ERR_MSG_LEN          64 * 1024
#define MAX_BUF_LEN_1K           1 * 1024
#define MAX_BUF_LEN_2K           2 * 1024
#define MAX_BUF_LEN_5K           5 * 1024
#define MAX_BUF_LEN_10K          10 * 1024

#define MAX_NUM_DATA_FILES       10

#define DO_NOT_AUTO_DEL_SHM      0
#define AUTO_DEL_SHM             1


#define FILL_MERGED_BUF(dest, src, n) \
{ \
  len = strlcpy(dest, src, n); \
  dest += len; \
  src += len; \
  num_values += n; \
}

#define RTC_MALLOC_AND_MEMSET(new, size, msg, index) { \
  if (size < 0) \
  { \
    fprintf(stderr, "Trying to malloc a negative size (%d) for index %u\n", (int)size, index); \
  } \
  else if (size == 0) \
  { \
    NSDL1_MEMORY(NULL, NULL, "Trying to malloc a 0 size for index %u", index); \
    new = NULL; \
  } \
  else \
  { \
    new = (void *)malloc( size ); \
    if ( new == (void*) 0 ) \
    { \
      fprintf(stderr, "Out of Memory (size = %d): %s for index %u\n", (int )(size), msg, index); \
    } \
    memset(new, 0, size); \
    if (NULL == new) \
    { \
      fprintf(stderr, "Initialization Error: %s for index %u, size=%d", msg, index, (int)size); \
    } \
    NSDL1_MEMORY(NULL, NULL, "MY_MALLOC'ed (%s) done. ptr = $%p$, size = %d for index %u", msg, new, (int)size, index); \
    INC_AND_LOG_ALLOC_STATS(size); \
  } \
}

#define RTC_REALLOC(buf, size, msg, index)  \
{ \
  if (size <= 0) {  \
    fprintf(stderr, "Trying to realloc a negative or 0 size (%d) for index  %d\n", (int )(size), index); \
  } else {  \
    buf = (void*)realloc(buf, size); \
    if ( buf == (void*) 0 )  \
    {  \
      NSTL1_OUT(NULL, NULL, "Out of Memory (size = %d): %s for index %d\n", (int )(size), msg, (int )index); \
      return -1; \
    }  \
    NSDL1_MEMORY(NULL, NULL, "MY_REALLOC'ed (%s) done. ptr = $%p$, size = %d for index %d", msg, buf, (int)size, index); \
    INC_AND_LOG_ALLOC_STATS(size); \
  } \
}

typedef struct FileParamRTCTable
{
  int sess_idx;
  int fparam_grp_idx;
  int mode; 
  int num_values;
  int data_buf_size;
  char *data_file_list;
  char *data_buf;
}FileParamRTCTable;

//extern Msg_com_con *fpram_rtc_mccptr;

extern PerProcVgroupTable *per_proc_vgroup_table_rtc;
extern FileParamRTCTable *fparam_rtc_tbl;

#ifndef CAV_MAIN
 extern bigbuf_t file_param_value_big_buf;
#else
extern __thread bigbuf_t file_param_value_big_buf;
#endif
//extern int fparam_rtc_done_successfully;

extern int handle_fparam_rtc(Msg_com_con *mccptr, char *tool_msg); 
extern int is_rtc_applied_on_group(int num_users, int grp_idx);
extern void process_fparam_rtc_attach_shm_done_msg(parent_msg *msg);
extern void process_fparam_rtc_attach_shm_msg(parent_child *msg);
extern void update_fparam_rtc_struct();
extern void ns_child_reset_fparam_rtc();
extern void dump_per_proc_vgroup_table_internal(PerProcVgroupTable *proc_vgroup);
extern void handle_fparam_rtc_done(int ignore_nvm);
extern int rtc_reset_child_on_resumed();
extern void dump_rtc_group_table_values();
extern int total_fparam_rtc_tbl_entries;
extern int is_global_vars_reset_done;
extern void *attach_shm_by_shmid(int shmid);
extern void *attach_shm_by_shmid_ex(int shmid, int auto_del);
extern int get_shm_info(int shmid);


#endif
