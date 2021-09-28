/******************************************************************
 * Name    :    ns_check_reply_size.h
 * Purpose :    nsl_check_relpy_size - parsing, shared memory, run time
 * Author  :    
 * Intial version date:    
 * Last modification date:
*****************************************************************/

#ifndef NS_CHECK_REPLYSIZE_VARS_H
#define NS_CHECK_REPLYSIZE_VARS_H

#define INIT_CHECK_REPLYSIZE_ENTRIES 16
#define DELTA_CHECK_REPLYSIZE_ENTRIES 16
#define INIT_CHECK_REPLYSIZE_PAGE_ENTRIES 64
#define DELTA_CHECK_REPLYSIZE_PAGE_ENTRIES 32

//Value for Action field
#define NS_CHK_REP_SZ_ACTION_STOP 0
#define NS_CHK_REP_SZ_ACTION_CONTINUE 1

typedef struct CheckReplySizeTableEntry {
  int sess_idx; // Used to get the hash code of var for validation
  int mode;     // Mode value index
  int value1;   // Min value index
  int value2;   // Max value index
  int action;   // Action index 
  ns_bigbuf_t var;      // Var name index 
  short pgall;
} CheckReplySizeTableEntry;


typedef struct CheckReplySizeTableEntry_Shr {
  int mode;     // Mode value  
  int value1;   // Min value used to compare with response size 
  int value2;   // Max value used to compare with response size
  short action; // Action stop or continue
  char *var;    //Var name that is used to save the returns value
} CheckReplySizeTableEntry_Shr;

typedef struct PerPageCheckReplySizeTableEntry{
  int checkreplysize_idx; /* index into the check reply size table */
} PerPageCheckReplySizeTableEntry;

typedef struct PerPageCheckReplySizeTableEntry_Shr{
  CheckReplySizeTableEntry_Shr* check_replysize_ptr;
} PerPageCheckReplySizeTableEntry_Shr;

typedef struct CheckReplySizePageTableEntry {
  int checkreplysize_idx; /* index into the checkreplysize table */
  int page_name; /* index into the temp big buffer */
  int page_idx;   
 //int sess_name; /* index into the temp big buffer */
} CheckReplySizePageTableEntry; 


extern int input_check_replysize_data(char* line, int line_number, int sess_idx);
extern PerPageCheckReplySizeTableEntry_Shr *copy_check_replysize_into_shared_mem(void);
extern void init_check_replysize_info(void);
extern int process_check_replysize_table_per_session(int session_id, char* err_msg);
extern int process_check_replysize_table();

extern int input_check_reply_size_data(char* line, int line_number, int sess_idx, char *script_filename);

extern PerPageCheckReplySizeTableEntry_Shr *perpagechkrepsz_table_shr_mem;

#ifndef CAV_MAIN
extern CheckReplySizePageTableEntry* checkReplySizePageTable;
extern PerPageCheckReplySizeTableEntry* perPageChkRepSizeTable;
extern PerPageCheckReplySizeTableEntry_Shr *perpagechk_replysize_table_shr_mem;
#else
extern __thread CheckReplySizePageTableEntry* checkReplySizePageTable;
extern __thread PerPageCheckReplySizeTableEntry* perPageChkRepSizeTable;
extern __thread PerPageCheckReplySizeTableEntry_Shr *perpagechk_replysize_table_shr_mem;
#endif

#endif
