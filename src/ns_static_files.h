#ifndef __ns_static_file
#define __ns_static_file

extern char* get_file_name(VUser *vptr, SegTableEntry_Shr* seg_ptr);
extern char* get_file_content_type(int index);
extern char* get_file_content(int index);
extern int get_file_size(int index);
extern int add_static_file_entry(char *file, int sess_idx);
extern void create_static_file_table_shr_mem();
extern int get_file_norm_id(char *file_path, int file_path_len);

#endif 
