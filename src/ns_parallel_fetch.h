#ifndef __NS_PARALLEL_FETCH_H__
#define __NS_PARALLEL_FETCH_H__

int try_url_on_cur_con (connection* cptr, action_request_Shr* cur_url, u_ns_ts_t now);
int try_hurl_on_cur_con (connection* cptr, u_ns_ts_t now);
int try_hurl_on_any_con (VUser *vptr, HostSvrEntry* cur_host, u_ns_ts_t now);
int try_url_on_any_con (VUser *vptr, action_request_Shr* cur_url, u_ns_ts_t now, int need_to_honour_req);
void add_to_hlist(VUser *vptr, HostTableEntry_Shr *hel);
extern inline void repeat_inline_url(connection* cptr, u_ns_ts_t now);
HostSvrEntry* next_from_hlist(VUser* vptr, HostSvrEntry* hptr);


#define CHECK_AND_SET_INLINE_BLOCK_TIME                                                             \
    NSDL3_CONN(NULL, cptr, "schedule time  = [%d], now = [%d]", cptr->url_num->schedule_time, now); \
    {                                                                                               \
      int block_time = 0;                                                                           \
      if((cptr->url_num->schedule_time != 0) && (now > cptr->url_num->schedule_time))               \
         block_time = now - cptr->url_num->schedule_time;                                           \
      else if((cptr->url_num->schedule_time == 0) && (now > vptr->pg_main_url_end_ts))               \
        block_time = now - vptr->pg_main_url_end_ts;						    \
      if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED)                               \
         set_page_based_counter_for_block_time(vptr, block_time);                                   \
      if(global_settings->log_inline_block_time)                                                    \
         dump_inline_block_time(vptr, cptr, block_time);                                            \
      NSDL2_CONN(NULL, cptr, "Inline block time = [%d]", block_time);                               \
    }                                                                                           

#define SEND_URL_REQ(cptr, cur_url, vptr, now, done) {                  \
    SET_URL_NUM_IN_CPTR(cptr, cur_url);                                 \
    cptr->redirect_count = vptr->redirect_count;                        \
    vptr->redirect_count = 0;                                           \
    vptr->urls_left--;                                                  \
    NSDL1_CONN(vptr, cptr, "urls_left dec at try_url_on_cur_con: %d", vptr->urls_left); \
    renew_connection(cptr, now);                                        \
    done = 1;                                                           \
  }

#define SEND_HURL_REQ(cptr, cur_url, cur_host, vptr, now, done) {       \
    if(!cur_host->cur_url->is_url_parameterized)			\
      cur_url = cur_host->cur_url;                                      \
    SET_URL_NUM_IN_CPTR(cptr, cur_url);                                 \
    CHECK_AND_SET_INLINE_BLOCK_TIME                                     \
    hurl_done(vptr, cur_url, cur_host, now);                            \
    renew_connection(cptr, now);                                        \
    IW_UNUSED(done = 1);                                                \
  }

#define NS_DO_NOT_HONOR_REQUEST 0
#define NS_HONOR_REQUEST 1
#endif /* __NS_PARALLEL_FETCH_H__ */
