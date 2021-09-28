#ifndef NS_TAG_VARS_H
#define NS_TAG_VARS_H

#include "user_tables.h"
#define TAG_TYPE_NONE 0
#define TAG_TYPE_ALL 1
#define TAG_TYPE_PAGE 2


#define INIT_GUTT_ENTRIES 64
#define DELTA_GUTT_ENTRIES 32
#define INIT_WEBSPEC_ENTRIES 32
#define DELTA_WEBSPEC_ENTRIES 32
#define INIT_TABLEID_ENTRIES 128
#define DELTA_TABLEID_ENTRIES 128
#define INIT_NODEVAR_ENTRIES 64
#define DELTA_NODEVAR_ENTRIES 32
#define INIT_GLOBALTHI_ENTRIES 128
#define DELTA_GLOBALTHI_ENTRIES 128
#define INIT_NODE_ENTRIES 128
#define DELTA_NODE_ENTRIES 128
#define INIT_TEMPBUFFER 32*1024
#define DELTA_TEMPBUFFER 5*1024

//for special characters encoding
#define TOTAL_CHARS 128
#define ENCODE_ALL       0 // Encode all special chars except . - _ +
#define ENCODE_NONE      1 // Do not encode any chars
#define ENCODE_SPECIFIED 2 // Encode specified chars only


//TagTableEntry, is the table of xml variables.
//Session Table has the start pointer (& num_entries) of this table for a session
//AttrQualTableEntry is the table of qualifier attributes (if present)
//TagTable has start pointer (& num entries) of AttrQualTable
//TagPageTable has the page association of tag vars. This table has index of TagTable
//SessionTable has the start (& numentries) of TagPageTable for a session
//All three tables get initialized in input_tagvar_data().
typedef struct TagTableEntry {
  int name; /* index into the temp buffer xml_var name*/
  int tag_name; /* xml node name. index into the temp buffer */
  int getvalue; /* index into the temp buffer, -1 if only getting the element value, and non -1 it getting an attribute value */
  int attr_qual_start; /* index into the attrQual table */
  int num_attr_qual; //Number of entries in attrQual Table
  int value_qual; /* If one of the qualifier is node value, itsindex into the temp buffer. else -1 */
  int order;
  //int notfound_str; /* index into the temp buffer */
  unsigned int tagvar_rdepth_bitmask; /*which redirections depth (bit mask of depths,
                                        given by user in xml var) */
  char action_on_notfound; //index of action to be taken
  char convert; //index of convert HTML to URL or HTML to TEXT
  short type;  /* either applies to ALL pages, or to only specific ones */

//supporting special characters encoding   
  char encode_type;
  char encode_chars[TOTAL_CHARS];
  char* encode_space_by;

} TagTableEntry;

typedef struct AttrQualTableEntry {
  int attr_name; /* index into the temp buffer */
  int qual_str; /* index into the temp buffer */
} AttrQualTableEntry;

typedef struct TagPageTableEntry {
  int tagvar_idx;  /* index into the tag table */
  int page_name;  /*index into the temp buffer */
  int page_idx; /* index into the page table */
} TagPageTableEntry;

extern int input_tagvar_data(char* line, int line_number, int sess_idx, char *script_filename); 
extern void copy_all_tag_vars(UserVarEntry* uservartable_entry, NodeVarTableEntry_Shr *nodevar_ptr);

#ifndef CAV_MAIN
extern TagTableEntry* tagTable;
extern AttrQualTableEntry* attrQualTable;
extern TagPageTableEntry* tagPageTable;
#else
extern __thread TagTableEntry* tagTable;
extern __thread AttrQualTableEntry* attrQualTable;
extern __thread TagPageTableEntry* tagPageTable;
#endif
extern int create_tagtables();

//************from tag_vars.c file****************


typedef int (*hashfn_type)(const char*, unsigned int);

//There is one entry for each elemnt in tag path 
//Node <A><B><C> will have 3 entries.
typedef struct TempTagTableEntry {
  int table_id;  /* index into TableIdTable. This is the Parent Table Name index */
  int tag_name_index;  /* offset into temp buf, can be overloaded w/ attr name . Tag Elemnt or Attribute name*/
  int child_table_id;  /* index into TableIdTable */
  hashfn_type attr_qual_hashfn; //hash code for attributes for this node. 
				//It is valid, only if it is the last elemnt in the tagpath.
				//Else it is NULL>
  int num_attr_table_entries; //number of hash table entries
  int nodevar_idx_start;  /* index into the NodeVarTable . Lists the NS Variables that woud be initialized with this tag entry*/
  int num_nodevars;
} TempTagTableEntry;

typedef struct TableIdTableEntry {
  int name;   /* offset into temp buf */
} TableIdTableEntry;

typedef struct THITableEntry {
  int node_index; /* index into node table. There will be as many entries as the possible hash codes */
  hashfn_type tag_hashfn;
  //hashfn_type attr_qual_hashfn; moved to NodeTableEntry_Shr
  //int attr_table_size; moved to NodeTableEntry_shr
  int prev_index; /* index into the THI Table */
  int prevcode;
  int nodevar_idx_start;        /* index into start of node var table */
} THITableEntry;

typedef struct NodeTableEntry {
  int THITable_child_idx; /* index into globalTHITable */
  int nodevar_idx_start; /* index into the NodeVarTable */
  int num_nodevars;
  hashfn_type attr_qual_hashfn;
  int attr_table_size;
} NodeTableEntry;

typedef struct NodeVarTableEntry {
  int vuser_vartable_idx;  /* index into the per user variable table . points to NS var table associated with this sess.*/
			  //should this be hash fun of NS var or min uniq idx for accessing the uvar table
  chkfn_type check_func; //check function pointer that would set the var value using the qualifiers & node or attribute values
  int ord;
  //bhushan
  int page_idx;
  unsigned int tagvar_rdepth_bitmask;
  char convert;
  char action_on_notfound;

//fields for encode
  char encode_type;
  char encode_chars[TOTAL_CHARS];
  char *encode_space_by;

} NodeVarTableEntry;

typedef struct NodeTableEntry_Shr {
  void* child_ptr; /* ptr into the shared_thitable */
  NodeVarTableEntry_Shr* nodevar_ptr; /* ptr into the nodevartable */
  int num_nodevars;
  hashfn_type attr_qual_hashfn;
  int attr_table_size;
} NodeTableEntry_Shr;

typedef struct THITableEntry_Shr {
  NodeTableEntry_Shr* node_ptr;
  hashfn_type tag_hashfn;
  //hashfn_type attr_qual_hashfn;
  //int attr_table_size;
  struct THITableEntry_Shr* prev_ptr;
  int num_tag_entries;     /* Number of variables */
  //NodeVarTableEntry_Shr* start_nodevar_ptr;
  int nodevar_idx_start;   /* start of variables in variable array */
  int prevcode;
} THITableEntry_Shr;

extern NodeTableEntry_Shr* node_table_shr_mem;
extern THITableEntry_Shr* thi_table_shr_mem;

#define THI_TABLE_MEMORY_CONVERSION(index) (thi_table_shr_mem + index)

extern int max_tagattr_hash_code;
extern NodeVarTableEntry_Shr* nodevar_table_shr_mem;

extern int insert_tag_shr_tables();
extern int insert_default_ws_errorcodes();

#endif /* NS_TAG_VARS_H */
