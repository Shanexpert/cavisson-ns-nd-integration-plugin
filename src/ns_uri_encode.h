#ifndef NS_URI_ENCODE_H
#define NS_URI_ENCODE_H


#define HTTP_URI   1
#define HTTP_QUERY 2

extern int kw_set_uri_encoding(char *buf, char *err_msg, int runtime_flag);
extern int ns_encode_char_in_url(char *url, int str_len, char *new_url, int first_str_seg);

#endif
