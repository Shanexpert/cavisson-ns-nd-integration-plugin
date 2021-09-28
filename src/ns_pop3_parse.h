#ifndef __NS_POP3_PARSE_H__
#define __NS_POP3_PARSE_H__

extern int parse_pop3_stat(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname);
extern int parse_pop3_list(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname);
extern int parse_pop3_get (FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname);
extern int kw_set_pop3_timeout(char *buf, int *to_change, char *err_msg);
extern int parse_ns_pop3_stat(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern int parse_ns_pop3_list(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern int parse_ns_pop3_get(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);

#endif  /* __NS_POP3_PARSE_H__ */
