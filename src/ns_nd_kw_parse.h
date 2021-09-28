
#ifndef NS_ND_KW_PARSE_H

#define NS_ND_KW_PARSE_H

#define NDC_CONF  "ndc.conf"


extern int kw_set_g_enable_net_diagnostics(char *buf, GroupSettings *gset, char *err_msg);
extern inline long long compute_flowpath_instance();
extern int kw_set_net_diagnostics_server(char *buf, int flag);
#endif //NS_ND_KW_PARSE_H
