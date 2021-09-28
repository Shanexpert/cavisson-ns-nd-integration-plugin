extern Msg_com_con *free_thread_pool;
extern Msg_com_con *ceased_thread_info;
extern Msg_com_con *free_thread_pool_tail;
extern int total_free_thread;
extern int total_busy_thread;
extern int num_ceased_thread;
extern void *vutd_worker_thread(void *tmp_ptr);
extern int vutd_send_msg_to_nvm(int type, char *send_msg, int size); 
extern pthread_mutex_t thread_mutex_lock;
