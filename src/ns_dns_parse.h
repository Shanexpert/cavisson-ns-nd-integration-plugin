#ifndef __NS_DNS_PARSE_H__
#define  __NS_DNS_PARSE_H__

extern int parse_dns_query(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname);
extern int parse_ns_dns_query(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);

#endif  /*  __NS_DNS_PARSE_H__ */
