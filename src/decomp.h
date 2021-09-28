#define NS_COMP_NONE 0
#define NS_COMP_GZIP 1
#define NS_COMP_DEFLATE 2

extern int ns_decomp_init();
extern int ns_decomp_do (char *in, int in_len, short comp_type);
extern int ns_decomp_do_new (char *in, int in_len, short comp_type, char *err_msg);
extern char *uncomp_buf;
extern int uncomp_max_len;
extern int uncomp_cur_len;
extern int ns_comp_do (char *in, int in_len, short comp_type);
extern int init_ns_decomp_do_continue (short comp_type);
extern int ns_decomp_do_continue (char *in, int in_len);
extern char *comp_buf;
extern int comp_max_len;
extern int comp_cur_len;
