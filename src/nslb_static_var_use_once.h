#ifndef NSLB_STATIC_VAR_USE_ONCE_H
#define NSLB_STATIC_VAR_USE_ONCE_H

extern void nslb_uo_get_ctrl_file_name(char *data_fname, char *ctrl_file, int gen_id);
extern void nslb_uo_get_gen_ctrl_file_name(char *data_fname, char *ctrl_file);
extern void nslb_uo_get_last_file_name(char *data_fname, char *last_file, int nvm_idx, int gen_idx);
extern void nslb_uo_create_data_file (int start_val, int num_val_remaining, int total_val, FILE *org_data_fp, FILE *used_data_fp, FILE *unused_data_fp, int *line_number, int *num_hdr_line);
extern int nslb_uo_create_data_file_frm_last_file(char *data_fname, int total_hdr_line, char *error_msg, int api_idx);

#endif


