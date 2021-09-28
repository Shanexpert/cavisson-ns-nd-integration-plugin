#ifndef TR069_DATA_FILE_H
#define TR069_DATA_FILE_H

typedef struct tr069_user_cpe_data_s{
   unsigned int start_offset;
   unsigned int start_idx; //Start idx from where NVM data started
   unsigned int total_entries;      //Total entries for one NVM; 
}tr069_user_cpe_data_t;

extern unsigned int *idx_table_ptr;

extern tr069_user_cpe_data_t *tr069_user_cpe_data;

extern void tr069_read_count_file();

extern unsigned int tr069_total_data_count;
#endif
