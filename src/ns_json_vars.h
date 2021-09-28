/******************************************************************
 * Name    :    ns_json_vars.h
 * Purpose :    JSON_VAR - parsing, shared memory, run time
 * Note    :    JSON Variables are initialized as a result of
                searching a response document for user specified filter.
 * Syntax  :    nsl_json_var (jsonvar, PAGE=page1, PAGE=page2, FILTER=.categories[1].children.link, ORD=ALL, SaveOffset=5,SaveLen=20, Convert=<>, Ignore_Redirections=<>, NotFound=<>);
 * Intial version date:    
 * Last modification date: 
*****************************************************************/

#ifndef NS_JSON_VARS_H
#define NS_JSON_VARS_H

#include "nslb_json_parser.h"


#define INIT_JSONVAR_ENTRIES 64
#define DELTA_JSONVAR_ENTRIES 32
#define INIT_JSONPAGE_ENTRIES 64
#define DELTA_JSONPAGE_ENTRIES 32
#define DELTA_PERPAGEJSONVAR_ENTRIES 32


#define TOTAL_CHARS 128
#define ENCODE_ALL       0 // Encode all special chars except . - _ +
#define ENCODE_NONE      1 // Do not encode any chars
#define ENCODE_SPECIFIED 2 // Encode specified chars only

// Search options
#define SEARCH_BODY     1  // Search only in the body of the response
#define SEARCH_VARIABLE 2  // Search in another variable
#define SEARCH_HEADER 	3  // Search in response header 
/* Not implelmented
#define SEARCH_HEADERS  3  // Search only in the headers of the response
#define SEARCH_ALL      4  // Search only in the complete response
#define SEARCH_NO_RESOURCE 5  // Search only in the complete response
*/

#define ELEMENT_TYPE_OBJECT 1
#define ELEMENT_TYPE_ARRAY 2
#define ELEMENT_TYPE_ELEMENT 3


#define JSON_ARRAY_CONDITION -2
#define JSON_ARRAY_ALL -1

typedef struct JSONVarTableEntry {
  json_expr_cond *json_path;
  int json_path_entries;
  ns_bigbuf_t name; /* index into the big buffer */
  ns_bigbuf_t filter; /* index into the big buffer */
  int ord;
 // int relframeid; //Not yet implemented
  short search;  // Where to search ? 
  char *search_in_var_name; // Var name if search is to be done in variable
  short pgall;
  int saveoffset;
  int savelen;
  int sess_idx;
  int jsonvar_rdepth_bitmask; /*which redirections depth (bit mask of depths,
  int var;                                      given by user in json var) */

  char encode_type;
  char encode_chars[TOTAL_CHARS];
  ns_bigbuf_t encode_space_by;
  char action_on_notfound; //index of action to be taken
  int retain_pre_value; /* flag to retain previous search value default is 1 */
  char convert; //index of convert HTML to URL or HTML to TEXT
} JSONVarTableEntry;

typedef struct JSONPageTableEntry {
  int jsonvar_idx; /* index into the jsonvar table */
  int page_name; /* index into the temp big buffer */
  int page_idx;
  int sess_idx;  
  //int sess_name; /* index into the temp big buffer */
} JSONPageTableEntry;

typedef struct PerPageJSONVarTableEntry {
  int jsonvar_idx; /* index into the jsonvar table */
} PerPageJSONVarTableEntry;


typedef struct JSONVarTableEntry_Shr {
  json_expr_cond *json_path;
  int json_path_entries;
  char* var_name;
  int ord;
 // int relframeid;
  int saveoffset;
  int savelen;
  int hash_idx;
  short search; /* Where to search? */
  int search_in_var_hash_code;    /* Hash code of the variable if search is to be done on var */
  unsigned int jsonvar_rdepth_bitmask; /*which redirections depth (bit mask of depths,
                                        given by user in search var) */
  char encode_type;
  char encode_chars[TOTAL_CHARS];
  char *encode_space_by;
  char action_on_notfound; //What action should be taken
  int retain_pre_value;
  char convert;            //Either convert HTML to URL or HTML to TEXT
} JSONVarTableEntry_Shr;

typedef struct PerPageJSONVarTableEntry_Shr {
  JSONVarTableEntry_Shr* jsonvar_ptr;
} PerPageJSONVarTableEntry_Shr;

extern int find_jsonvar_idx(char* name, int len, int sess_idx);
extern int input_jsonvar_data(char* line, int line_number, int sess_idx, char *script_filename);
extern PerPageJSONVarTableEntry_Shr *copy_jsonvar_into_shared_mem(void);
extern int process_jsonvar_table(void);
extern void init_jsonvar_info();
int find_jsonvar_idx(char* name, int len, int sess_idx);
//static int jsonpage_cmp(const void* ent1, const void* ent2);

#ifndef CAV_MAIN
extern JSONVarTableEntry* jsonVarTable;
extern JSONPageTableEntry* jsonPageTable;
extern PerPageJSONVarTableEntry* perPageJSONVarTable;
extern PerPageJSONVarTableEntry_Shr *perpagejsonvar_table_shr_mem;
extern JSONVarTableEntry_Shr *jsonvar_table_shr_mem;
#else
extern __thread JSONVarTableEntry* jsonVarTable;
extern __thread JSONPageTableEntry* jsonPageTable;
extern __thread PerPageJSONVarTableEntry* perPageJSONVarTable;
extern __thread PerPageJSONVarTableEntry_Shr *perpagejsonvar_table_shr_mem;
extern __thread JSONVarTableEntry_Shr *jsonvar_table_shr_mem;
#endif

//prototype

#endif /* JSON_VARS_H */
