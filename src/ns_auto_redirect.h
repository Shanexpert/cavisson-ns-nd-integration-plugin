#ifndef _NS_AUTO_REDIRECT_H_
#define _NS_AUTO_REDIRECT_H_

#define NS_HTTP_REDIRECT_URL             0x00000001
#define NS_HTTP_REDIRECT_URL_IN_CACHE    0x00000002

/*bug 54315: added _url_num_ == cptr->url_num*/
#define FREE_REDIRECT_URL(cptr, request_line, _url_num_) {                       \
    NSDL3_HTTP(NULL, NULL, "Called FREE_REDIRECT_URL from %s: request_line = %s, url_num = %p", \
                __FUNCTION__, request_line, _url_num_);                   \
    if (request_line) {                                                 \
      NSDL2_HTTP(NULL, NULL, "Freeing redirect URL request line %s", request_line); \
      FREE_AND_MAKE_NULL(_url_num_->proto.http.request_line, "url_num->proto.http.request_line", -1); \
      NSDL2_HTTP(NULL, NULL, "Freeing redirect URL %p cptr->url_num=%p", _url_num_, cptr->url_num);                  \
      if(_url_num_ == cptr->url_num) \
        cptr->url_num = cptr->url_num->parent_url_num; \
      FREE_AND_MAKE_NULL(_url_num_, "url_num", -1);                       \
    }                                                                   \
  }

void reset_redirect_count(VUser *vptr);
//Changed return type for dynamic host
//void auto_redirect_connection(connection* cptr, VUser *vptr, u_ns_ts_t now , int done, int is_redirect);
int auto_redirect_connection(connection* cptr, VUser *vptr, u_ns_ts_t now , int done, int is_redirect);
extern inline void http_setup_auto_redirection(connection *cptr, int *status, int *is_redirect);

#endif /* _NS_AUTO_REDIRECT_H_ */
