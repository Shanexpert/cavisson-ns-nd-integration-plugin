#ifndef NS_BYTE_VARS_H
#define NS_BYTE_VARS_H 

#ifdef RMI_MODE
#define INIT_BYTEVAR_ENTRIES 16
#define DELTA_BYTEVAR_ENTRIES 16
#define INIT_REQBYTEVAR_ENTRIES 16
#define DELTA_REQBYTEVAR_ENTRIES 16
#endif

typedef struct ByteVarTableEntry {
  ns_bigbuf_t name; /* offset into big buf */
} ByteVarTableEntry;

#ifdef RMI_MODE
typedef struct ReqByteVarTableEntry {
  unsigned int name; /* offset into big buf */
  short length;
  short type;
  int offset;
  int byte_length;
} ReqByteVarTableEntry;
#endif

#ifdef RMI_MODE
typedef struct ReqByteVarTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  short length;
  short type;
  int offset;
  int byte_length;
  int bytevar_hash_code;
} ReqByteVarTableEntry_Shr;
#endif


#ifdef RMI_MODE
typedef struct ReqByteVarTab_Shr {
  ReqByteVarTableEntry_Shr* bytevar_start; /* pointer into the shared bytevar table */
  int num_bytevars;
} ReqByteVarTab_Shr;
#endif

#ifdef RMI_MODE
extern int create_bytevar_table_entry(int* row_num);
extern int create_reqbytevar_table_entry(int* row_num);
#endif

#ifdef RMI_MODE
extern int find_bytevar_idx(char* name, int name_length);
#endif

#ifdef RMI_MODE
extern ByteVarTableEntry* byteVarTable;
extern ReqByteVarTableEntry* reqByteVarTable;
#endif

#ifdef RMI_MODE
extern int max_bytevar_entries;
extern int total_bytevar_entries;
extern int max_reqbytevar_entries;
extern int total_reqbytevar_entries;
#endif

#endif
