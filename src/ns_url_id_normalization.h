#ifndef NS_URL_ID_NORM_H
#define NS_URL_ID_NORM_H

// Moved from logging_reader.h to avoid include of this file in ns_url_id_normalization.c
typedef struct {
  unsigned int url_id;
  unsigned int pg_id;
  unsigned int url_hash_id;
  unsigned int url_hash_code;
  int len; // URL Length
  char url_name[MAX_URL_LEN + 1]; 
} UrlTableLogEntry;


#define RETRIEVE_BUFFER_DATA_NORM(offset) (g_big_buf_dyn + offset)

typedef unsigned int UrlIndexTable;

extern unsigned int get_norm_index(UrlTableLogEntry*);
extern void url_norm_init(int);
extern int dynamic_norm_url_destroy();
extern unsigned int calc_fourth_byte(unsigned int url_index, int n);
 
extern int getIdxAndFlag(UrlTableLogEntry* utptr,int *flag);
extern int set_max_static_url_index(unsigned int id);
extern unsigned int get_url_norm_id(UrlTableLogEntry* utptr, int *is_new_url);
extern unsigned int get_url_norm_id_for_urc(unsigned int id, int *found);
extern unsigned int get_url_norm_id_for_generator(UrlTableLogEntry* utptr, int gen_id, int gen_url_index, int *is_new_url);
extern unsigned int get_norm_id_from_nvm_table(unsigned int nvm_url_index, unsigned int nvm_index);

#endif
