#ifndef USER_TABLES_H
#define USER_TABLES_H

/* This file is exported to include */

// Following struct is moved from util.h on Tue Nov 24 20:29:38 IST 2009, As we dont want to export util in include dir 

// struct ArrayValEntry, val_type, UserVarEntry are moved to libnscore/nslb_uservar_table.h for search parameter(NS & NO).
//#include "nslb_uservar_table.h"
#include "nslb_uservar_table.h"

#define TOTAL_CHARS 128
typedef struct {
  int cur_val_idx;
  int remaining_valid_accesses;
} UserGroupEntry;

typedef struct {
  char *cookie_value;
  unsigned int length;
} UserCookieEntry;

typedef struct {
  char* dynvar_value;
  unsigned int length;
} UserDynVarEntry;

#ifdef RMI_MODE
typedef struct {
  char* bytevar_value;
  unsigned int length;
} UserByteVarEntry;
#endif

#ifdef WS_MODE_OLD
typedef struct {
  char* value;
  unsigned int length;
} UserTagAttrEntry;
#endif


typedef int (*chkfn_type)(char**, UserVarEntry*, char*, int*, void*, int); /* the void* is actually a UserVarEntry* */

/* This is a structure since these has to be individual per process.
 * Since NodeVarTableEntry_Shr goes into shared memory, and multiple
 * processes will be updating xml vars values at the same time this
 * haveing fields like total_tmp_tagvar.. etc in NodeVarTableEntry_Shr
 * will cause serious corruption and coredumps. */
typedef struct TempArrayNodeVar {
  int count_ord;         /* Count of number of vars found. In pass 2 count means 
                          * the random ord to be selected Used only for ORD ALL */
  int total_tmp_tagvar; /* Total used entries. */
  int max_tmp_tagvar;    /* Max entries */
  ArrayValEntry *tempTagArrayVal; /* temp array used for ORD=ALL */
} TempArrayNodeVar;

typedef struct NodeVarTableEntry_Shr {
  int vuser_vartable_idx;
  chkfn_type check_func; //check function pointer that would set the var value using the qualifiers & node or attribute values
  int ord;               /* Order */
  TempArrayNodeVar *tmp_array;
  unsigned int tagvar_rdepth_bitmask; //redirectiondepth values
  char convert;             //convert
  char action_on_notfound;  //action on notfound
  /*This notfound flag is used per process[ie per NVM] since it may be
   corrupted by some other process[NVM] thats why we have 
   taken this as a pointer so that allocated memory will be
   different for every process.
   */
//Fields for special characters encoding
  char encode_type;
  char encode_chars[TOTAL_CHARS];
  char* encode_space_by;

  char *found_flag;            
} NodeVarTableEntry_Shr;

#endif
