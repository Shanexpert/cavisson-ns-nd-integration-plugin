
#ifndef CAV_MAIN
extern short* large_location_array;
extern InuseSvrTableEntry* inuseSvrTable;
extern InuseUserTableEntry* inuseUserTable;
extern int max_inusesvr_entries;
extern int total_inuseuser_entries;
extern int max_inuseuser_entries;
extern int total_inusesvr_entries;
extern ProfilePctCountTable *prof_pct_count_table; 
extern int total_userprofshr_entries;
#else
extern __thread short* large_location_array;
extern __thread InuseSvrTableEntry* inuseSvrTable;
extern __thread InuseUserTableEntry* inuseUserTable;
extern __thread int max_inusesvr_entries;
extern __thread int max_inuseuser_entries;
extern __thread int total_inuseuser_entries;
extern __thread int total_inusesvr_entries;
extern __thread ProfilePctCountTable *prof_pct_count_table; 
extern __thread int total_userprofshr_entries;
#endif
extern void insert_user_proftable_shr_mem(void);
extern void dump_user_profile_data(void);
extern int find_inusesvr_idx(int location_idx);
extern void  copy_user_profile_attributes_into_shm();
extern void fill_server_loc_idx();
extern int create_inusesvr_table_entry(int *row_num);

extern int ns_get_ua_string_ext (char *ua_string_buf, int ua_string_len, VUser *vptr);
extern void ns_set_ua_string_ext (char *ua_string_buf, int ua_string_len, VUser *vptr);
