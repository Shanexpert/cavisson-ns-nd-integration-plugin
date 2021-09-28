#define PAGE_COMPLETE 0
#define PAGE_THINK_TIME 1
#define END_SESSION 2

#define NS_START_TRANSACTION 50
#define NS_END_TRANSACTION 51
#define NS_END_TRANSACTION_AS 52
#define NS_GET_TRANSACTION_TIME 53
#define NS_SET_TX_STATUS 54
#define NS_GET_TX_STATUS 55

extern int read_thread_msg(int fd, char *read_msg);
extern int handle_msg_from_vuser_thread(Msg_com_con *mcctptr, char *read_buf);
extern int vutd_create_listen_fd(Msg_com_con *mccptr, int con_type, unsigned short *listen_port); 
extern int vutd_accept_connetion(int fd);
extern int vutd_create_thread(int num_thread);
extern int vutd_stop_thread(VUser *tmp_vptr, Msg_com_con *mcctptr, int status);
extern Msg_com_con *get_thread();
extern void free_thread(Msg_com_con *to_fre_pool_ptr);
extern void send_msg_nvm_to_vutd(VUser *vptr, int type, int ret_val);


extern int vutd_write_msg(int fd, char *buf, int size);
extern int vutd_read_msg(int fd, char *read_buf);

extern int kw_set_vuser_thread_pool(char *buf, int flag, char *err_msg);
/* This declaration has  been added to avoid implicit declaration warning */
extern int ns_advance_param_internal(const char *param_name, VUser *api_vptr);
