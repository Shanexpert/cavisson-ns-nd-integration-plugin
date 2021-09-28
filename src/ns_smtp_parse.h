/********************************************************************
 * Name            : ns_smtp_parse.h 
 * Purpose         : - 
 * Initial Version : Wednesday, January 06 2010 
 * Modification    : -
 ********************************************************************/

#ifndef NS_SMTP_PARSE_H 
#define NS_SMTP_PARSE_H 


#define B64_DEF_LINE_SIZE 72

// Need to be uniqe ?? 
// #define ATTACHMENT_BOUNDARY "NetStormAttachment" 
// #define ATTACHMENT_END_BOUNDARY "\r\n--NetStormAttachment--"  // for now we are directly append it after the body ends. in function smtp_send_data_body 

extern char attachment_boundary[];
extern char attachment_end_boundary[];
extern char smtp_body_hdr_begin[];
extern int smtp_body_hdr_begin_len;

extern int kw_set_smtp_timeout(char *buf, int *to_change, char *err_msg, int runtime_flag);
extern int parse_smtp_send(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname);
extern void encodeblock( unsigned char in[3], unsigned char out[4], int len );
extern int search_comma_as_last_char(char *ptr, int *fn_end);
extern int parse_ns_smtp_send(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);

#endif
