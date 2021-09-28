#ifndef __NS_FTP_PARSE_H__
#define  __NS_FTP_PARSE_H__

extern int parse_ftp_get(FILE *cap_fp, int sess_idx, int *line_num);
extern int kw_set_ftp_timeout(char *buf, int *to_change, char *err_msg);
extern int parse_ns_ftp_put(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern int parse_ns_ftp_get(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);

#endif  /*  __NS_FTP_PARSE_H__ */
