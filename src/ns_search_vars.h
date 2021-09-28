/******************************************************************
 * Name    :    ns_search_vars.h
 * Purpose :    SEARCH_VAR - parsing, shared memory, run time
 * Note    :    Search Variables are initialized as a result of
                searching a response document for user specified criteria.
 * Syntax  :    nsl_search_var (svar, PAGE=page1, PAGE=page2, LB=left-boundary, RB=right-boundary, ORD=ALL, SaveOffset=5,SaveLen=20, Convert=<>, Ignore_Redirections=<>, NotFound=<>);
 * Intial version date:    05/06/08
 * Last modification date: 07/12/09
*****************************************************************/

#ifndef NS_SEARCH_VARS_H
#define NS_SEARCH_VARS_H

#define INIT_SEARCHVAR_ENTRIES 64
#define DELTA_SEARCHVAR_ENTRIES 32
#define INIT_SEARCHPAGE_ENTRIES 64
#define DELTA_SEARCHPAGE_ENTRIES 32

#define TOTAL_CHARS 128
#define ENCODE_ALL       0 // Encode all special chars except . - _ +
#define ENCODE_NONE      1 // Do not encode any chars
#define ENCODE_SPECIFIED 2 // Encode specified chars only

// Search options
#define SEARCH_BODY     1  // Search only in the body of the response
#define SEARCH_VARIABLE 2  // Search in another variable
#define SEARCH_HEADER 	3  // Search in response header 
#define SEARCH_ALL 	4  // Search in complete response 
/* Not implelmented
#define SEARCH_NO_RESOURCE 5  // Search only in the complete response
*/
#define LBMATCH_CLOSEST 0 // Search for the LB which is nearest to RB
#define LBMATCH_FIRST 1 // Search for the first LB

#define SEARCH_IN_HEADER  0x00000001
#define SEARCH_IN_RESP    0x00000002

#define MAX_CONVERT_BUF_LENGTH 10240

typedef struct SearchVarTableEntry {
  ns_bigbuf_t name; /* index into the big buffer */
  ns_bigbuf_t lb; /* index into the big buffer */
  ns_bigbuf_t rb; /* index into the big buffer */
  int ord;
 // int relframeid; //Not yet implemented
  short search;  // Where to search ? 
  char *search_in_var_name; // Var name if search is to be done in variable
  short pgall;
  short lb_rb_type;
  int saveoffset;
  int savelen;
  int sess_idx;
  int searchvar_rdepth_bitmask; /*which redirections depth (bit mask of depths,
  int var;                                      given by user in search var) */
  
  char encode_type;
  char encode_chars[TOTAL_CHARS];
  ns_bigbuf_t encode_space_by;

  char action_on_notfound; //index of action to be taken
  int retain_pre_value; /*Manish: flag to retain previous search value default is 1 */
  char convert; //index of convert HTML to URL or HTML to TEXT
  char search_flag;        //flag for regular expression, lb match & binary data search: 1st bit SET for lb_regular_exp, 
                           // 2nd bit SET for rb_regular_exp, 3rd bit for binary lb, 4th bit for binary rb.
  regex_t preg_lb;         //regex type variable for lb, which store the regular expression after compilation by regcomp()
  regex_t preg_rb;         //regex type variable for rb, which store the regular expression after compilation by regcomp()
  int lbmatch;             /* store the LBMATCH value*/
} SearchVarTableEntry;

typedef struct SearchPageTableEntry {
  int searchvar_idx; /* index into the searchvar table */
  int page_name; /* index into the temp big buffer */
  int page_idx;
  int sess_idx;  /*Manish: */
  //int sess_name; /* index into the temp big buffer */
} SearchPageTableEntry;

typedef struct PerPageSerVarTableEntry {
  int searchvar_idx; /* index into the searchvar table */
} PerPageSerVarTableEntry;


typedef struct SearchVarTableEntry_Shr {
  char* var_name;
  char* lb;
  char* rb;
  int ord;
 // int relframeid;
  int saveoffset;
  int savelen;
  int hash_idx;
  short search; /* Where to search? */
  short lb_rb_type; 
  int search_in_var_hash_code;    /* Hash code of the variable if search is to be done on var */
  unsigned int searchvar_rdepth_bitmask; /*which redirections depth (bit mask of depths,
                                        given by user in search var) */
  char encode_type;
  char encode_chars[TOTAL_CHARS];
  char *encode_space_by;

  char action_on_notfound; //What action should be taken
  int retain_pre_value;
  char convert;            //Either convert HTML to URL or HTML to TEXT
  char search_flag;        //flag for regular expression, lb match & binary data search: 1st bit SET for lb_regular_exp, 
                           // 2nd bit SET for rb_regular_exp, 3rd bit for binary lb, 4th bit for binary rb.
  regex_t preg_lb;       //regex type variable for lb, which store the regular expression after compilation by regcomp()
  regex_t preg_rb;      //regex type variable for rb, which store the regular expression after compilation by regcomp()
  int lbmatch; /* store the LBMATCH value*/

} SearchVarTableEntry_Shr;

typedef struct PerPageSerVarTableEntry_Shr {
  SearchVarTableEntry_Shr* searchvar_ptr;
} PerPageSerVarTableEntry_Shr;

extern int find_searchvar_idx(char* name, int len, int sess_idx);
extern int input_searchvar_data(char* line, int line_number, int sess_idx, char *script_filename);
extern PerPageSerVarTableEntry_Shr *copy_searchvar_into_shared_mem(void);
extern int process_searchvar_table(void);
extern void init_searchvar_info();

#ifndef CAV_MAIN
extern SearchVarTableEntry* searchVarTable;
extern SearchPageTableEntry* searchPageTable;
extern PerPageSerVarTableEntry* perPageSerVarTable;
extern PerPageSerVarTableEntry_Shr *perpageservar_table_shr_mem;
extern SearchVarTableEntry_Shr *searchvar_table_shr_mem;
#else
extern __thread SearchVarTableEntry* searchVarTable;
extern __thread SearchPageTableEntry* searchPageTable;
extern __thread PerPageSerVarTableEntry* perPageSerVarTable;
extern __thread PerPageSerVarTableEntry_Shr *perpageservar_table_shr_mem;
extern __thread SearchVarTableEntry_Shr *searchvar_table_shr_mem;
#endif

//prototype

#endif /* SEARCH_VARS_H */
