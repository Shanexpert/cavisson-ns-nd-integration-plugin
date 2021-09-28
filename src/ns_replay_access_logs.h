#ifndef NS_REPLAY_ACCCES_LOGS_H
#define NS_REPLAY_ACCCES_LOGS_H

/* ReplayAccessLog: In release 3.9.0 changes are done for replay access log,
 * if replay_mode is enable and url is redirecting then we need to make_request for
 * that redirected URL.
 *
 * In case of replay, we need to use NS code in following cases
 *  1. redirected_url != NULL  -> This for auto redirect case
 *  2. cptr->flags & NS_CPTR_AUTH_MASK != 0  -> This for authentication case
 *  3. url_type == EMBEDDED_URL -> Fetching inline (both auto fetch and as per log case)
 */

#define IS_REPLAY_TO_USE_NS_CODE() \
  ((global_settings->replay_mode) &&  /*Replay mode*/ \
    ((cptr->url_num->proto.http.redirected_url != NULL) ||  /* Redirected URL*/ \
     (cptr->flags & NS_CPTR_AUTH_MASK) || /* Authentication case */ \
     (cptr->url_num->proto.http.type == EMBEDDED_URL) /* URL is inline URL */ \
    ) \
  )

/*
 * In case of replay, we need to use Replay code in both these are TRUE
 *   redirected_url == NULL  (Not redirect)
 *     and
 *  cptr->flags & NS_CPTR_AUTH_MASK == 0  (Not authentcation)
 *     and
 *   url_type != EMBEDDED_URL then call make_request (Main (Not inline url))
 */
#define IS_REPLAY_TO_USE_REPLAY_CODE() \
  ((global_settings->replay_mode) && \
    ((cptr->url_num->proto.http.redirected_url == NULL) &&  /* Not Redirected URL*/ \
     ((cptr->flags & NS_CPTR_AUTH_MASK) == 0) && /* Authentication case */ \
     (cptr->url_num->proto.http.type != EMBEDDED_URL) /* URL is Not inline URL */ \
    ) \
  )

extern int ns_get_replay_page_ext(VUser *vptr);
extern int get_num_usrs_to_replay(int *iid, int *start_ramp_down); 
extern void set_user_replay_user_idx(VUser *vptr);
extern void replay_mode_user_generation(Schedule *schedule_ptr);
extern void process_replay_req(connection *cptr, u_ns_ts_t now);
extern void validate_replay_access_log_settings();
extern int set_next_replay_page(VUser *vptr);
extern void send_replay_req_after_partial_write(connection *cptr, u_ns_ts_t now);

#endif
