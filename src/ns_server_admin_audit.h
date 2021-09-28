//We are taking 5 to avoid other macros because BLOCKED is somehow different feature.
#define BLOCKED 5

extern void audit_log(int flag, char *user, char *server_ip, char *command, int machine_flag, int raw_format_for_gui, char *ftp_src, char *ftp_dest);
