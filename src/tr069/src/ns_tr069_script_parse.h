#ifndef _NS_TR069_SCRIPT_PARSE_H_
#define _NS_TR069_SCRIPT_PARSE_H_

extern int parse_tr069(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern void read_tr069_registration_vars(int sess_idx);

#endif
