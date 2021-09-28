#ifndef NS_RANDOM_STRING_H
#define NS_RANDOM_STRING_H

#define INIT_RANDOMSTRING_ENTRIES 64
#define DELTA_RANDOMSTRING_ENTRIES 32


typedef struct RandomStringTableEntry {
    ns_bigbuf_t name; /* index into the big buffer */
    int max;
    int min;
    ns_bigbuf_t char_set;
    int len;// check it 
    int sess_idx;
    int refresh;
} RandomStringTableEntry;


extern int input_randomstring_data(char* line, int line_number, int sess_idx, char *script_filename);
extern RandomStringTableEntry_Shr *copy_randomstring_into_shared_mem(void);
extern void init_randomstring_info();
extern char* get_random_string_value(RandomStringTableEntry_Shr* var_ptr, VUser* vptr,
    int var_val_flag, int* total_len);

#ifndef CAV_MAIN
extern RandomStringTableEntry* randomStringTable;
#else
extern __thread RandomStringTableEntry* randomStringTable;
#endif
extern int find_randomstring_idx(char* name, int sess_idx);
extern void clear_uvtable_for_random_string_var(VUser *vptr);
#endif /* RANDOM_STRING_H */

