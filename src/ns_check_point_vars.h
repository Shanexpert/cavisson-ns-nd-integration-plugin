/******************************************************************
 * Name    :    ns_check_point_vars.h
 * Purpose :    nsl_check_point - parsing, shared memory, run time
 * Author  :    Archana
 * Intial version date:    05/06/08
 * Last modification date: 05/06/08
*****************************************************************/

#ifndef NS_CHECK_POINT_VARS_H
#define NS_CHECK_POINT_VARS_H

#define INIT_CHECKPOINT_ENTRIES 64
#define DELTA_CHECKPOINT_ENTRIES 32
#define INIT_CHECKPAGE_ENTRIES 64
#define DELTA_CHECKPAGE_ENTRIES 32


//Values for report field
#define NS_CP_REPORT_SUCCESS 0
#define NS_CP_REPORT_FAILURE 1
#define NS_CP_REPORT_ALWAYS 2
// Used during search in the response for checkpoints
#define FIND_LB 0 
#define FIND_RB 1 
#define FIND_DONE 2 

#define TEXT_PFX_FLAG 0
#define TEXT_SFX_FLAG 1 

// Search_IN options with nsl_web_find
#define NS_CP_SEARCH_BODY 1  // Search only in the body of the response
#define NS_CP_SEARCH_HEADER 2  // Search in response header
#define NS_CP_SEARCH_ALL 3  // Search in complete response

typedef struct CheckPointTableEntry {
 // int text; /* index into the big buffer */
  ns_bigbuf_t text_pfx; /* prefix or text index into the big  buffer */
  ns_bigbuf_t text_sfx; /* suffix index into the big  buffer */
  ns_bigbuf_t id; /* index into the big buffer */
  ns_bigbuf_t save_count_var; /* SaveCountindex into the big buffer */ 
  ns_bigbuf_t compare_file; /*compare_file into the big buffer*/
  int compare_file_size;
  ns_bigbuf_t checksum_cookie; /* checksum cookie index into the big  buffer */
  ns_bigbuf_t checksum; /*checksum index into the big  buffer*/
  int action_on_fail; /* ActionOnFail index into the big buffer */ 
  short fail;
  short report;
  short pgall;
  short ignorecase_textpfx;
  short regexp_textpfx;
  regex_t preg_textpfx;
  short ignorecase_textsfx;
  short regexp_textsfx;
  regex_t preg_textsfx;
  int sess_idx; // Used to get the hash code of save_count_var for validation
  short search;
  short search_in; //BugId:51978 Add this field to config search in HEADER, BODY, or ALL in response body
} CheckPointTableEntry;

typedef struct CheckPointTableEntry_Shr {
//  char* text;
  char* text_pfx;
  char* text_sfx;
  char* id;
  char *save_count_var;
  char *compare_file; /*compare_file into the big buffer*/
  int compare_file_size;
  char *checksum_cookie; /* checksum cookie index into the big  buffer */
  char *checksum; /*Indicate checksum value*/
  short action_on_fail;
  short fail;
  short report;
  short ignorecase_textpfx;
  short regexp_textpfx;
  regex_t preg_textpfx;
  short ignorecase_textsfx;
  short regexp_textsfx;
  short search;
  short search_in; //BugId:51978 Add this field to config search in HEADER, BODY, or ALL in response body
  regex_t preg_textsfx;
} CheckPointTableEntry_Shr;

typedef struct PerPageChkPtTableEntry_Shr {
  CheckPointTableEntry_Shr* checkpoint_ptr;
} PerPageChkPtTableEntry_Shr;

typedef struct PerPageChkPtTableEntry {
  int checkpoint_idx; /* index into the check point table */
} PerPageChkPtTableEntry;

extern int input_checkpoint_data(char* line, int line_number, int sess_idx, char *script_filename);
extern PerPageChkPtTableEntry_Shr *copy_checkpoint_into_shared_mem(void);
extern void init_checkpoint_info(void);
extern int process_checkpoint_table_per_session(int session_id, char* err_msg);
extern int process_checkpoint_table();

#ifndef CAV_MAIN
extern PerPageChkPtTableEntry_Shr *perpagechkpt_table_shr_mem;
extern PerPageChkPtTableEntry* perPageChkPtTable;
extern CheckPointTableEntry* checkPointTable;
#else
extern __thread PerPageChkPtTableEntry_Shr *perpagechkpt_table_shr_mem;
extern __thread PerPageChkPtTableEntry* perPageChkPtTable;
extern __thread CheckPointTableEntry* checkPointTable;
#endif
extern void my_regcomp(regex_t *preg, short ignorecase, char *data, char *msg);
extern char *checkpoint_to_str(CheckPointTableEntry_Shr* checkpoint_ptr, char *buf);
#endif
