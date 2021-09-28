#include <libssh/libssh.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

int ns_rte_ssh_connect(ns_rte *rte, char *host , char *user , char *password);
int ns_rtr_ssh_disconnect(ns_rte *rte);
int ns_rte_ssh_open(ns_rte *rte);
int ns_rte_ssh_close(ns_rte *rte);
int ns_ssh_rte_login(ns_rte *rte);
int ns_rte_ssh_send_text(ns_rte *rte, char *text);
int ns_rte_ssh_wait_text(ns_rte *rte, char *text , int duration);
