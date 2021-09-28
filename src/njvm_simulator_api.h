#ifndef NJVM_SIMULATOR_API_H
#define NJVM_SIMULATOR_API_H

extern int get_nvm_id();
extern int get_user_id();

extern int ns_web_url(int page_id);
extern int ns_exit_session();
extern int ns_page_think_time(double time);
extern char *ns_eval_string(char *var_name);
extern int ns_save_string(char *var_value, char *var_name);
#endif
