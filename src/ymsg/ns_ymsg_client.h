#ifndef NS_YMSG_CLIENT_H 
#define NS_YMSG_CLIENT_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#ifndef _WIN32
#include <netdb.h>
#include <sys/time.h>
#endif
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <termios.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <unistd.h>



#include "../thirdparty/libyahoo2-1.0.1/src/yahoo_debug.h"
#include "../thirdparty/libyahoo2-1.0.1/src/yahoo2.h"
#include "../thirdparty/libyahoo2-1.0.1/src/yahoo2_callbacks.h"
#include "../thirdparty/libyahoo2-1.0.1/src/yahoo_util.h" 

#ifndef _WIN32
int fileno(FILE * stream);
#endif

#define MAX_PREF_LEN 255


/* Pankaj add data structure here*/
/* this is a global variable storing the IM infromation */
/*
struct gReceiveIM{
int id;
const char me[100];
const char who[100];
const char msg[100];
int utf8;
int stat;
long tm;
};
*/

typedef struct {
	char yahoo_id[255];
	char password[255];
	int id;
	int fd;
	int status;
	char *msg;
} yahoo_local_account;


typedef struct {
	char yahoo_id[255];
	char name[255];
	int status;
	int away;
	char *msg;
	char group[255];
} yahoo_account;

typedef struct {
	int id;
	char *label;
} yahoo_idlabel;

typedef struct {
	int id;
	char *who;
} yahoo_authorize_data;

yahoo_idlabel yahoo_status_codes[] = {
	{YAHOO_STATUS_AVAILABLE, "Available"},
	{YAHOO_STATUS_BRB, "BRB"},
	{YAHOO_STATUS_BUSY, "Busy"},
	{YAHOO_STATUS_NOTATHOME, "Not Home"},
	{YAHOO_STATUS_NOTATDESK, "Not at Desk"},
	{YAHOO_STATUS_NOTINOFFICE, "Not in Office"},
	{YAHOO_STATUS_ONPHONE, "On Phone"},
	{YAHOO_STATUS_ONVACATION, "On Vacation"},
	{YAHOO_STATUS_OUTTOLUNCH, "Out to Lunch"},
	{YAHOO_STATUS_STEPPEDOUT, "Stepped Out"},
	{YAHOO_STATUS_INVISIBLE, "Invisible"},
	{YAHOO_STATUS_IDLE, "Idle"},
	{YAHOO_STATUS_OFFLINE, "Offline"},
	{YAHOO_STATUS_CUSTOM, "[Custom]"},
	{YPACKET_STATUS_NOTIFY, "Notify"},
	{0, NULL}
};

#ifndef _WIN32
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);
int gethostname(char *name, size_t len);
#endif


typedef struct {
        int id;
        char * me;
        char * room_name;
        char * host;
        YList * members;
        int joined;
} conf_room;


struct _conn {
        int fd;
        SSL *ssl;
        int use_ssl;
        int remove;
};

struct conn_handler {
        struct _conn *con;
        int id;
        int tag;
        yahoo_input_condition cond;
        int remove;
        void *data;
};

struct connect_callback_data {
        yahoo_connect_callback callback;
        void * callback_data;
        int id;
        int tag;
};


extern int ns_ymsg_login_ext(char *yahoo_id, char *password, int inital_status, int debug_level);
extern int ns_ymsg_send_chat_ext(char *my_yahoo_id, char *dest_yahoo_id, char *chat_msg);
extern int ns_ymsg_logout_ext();
//extern void ymsg_set_globals(char *ns_local_host, char *ns_ylad, int *ns_poll_loop);

extern void *ns_ymsg_get_local_host_ext();
extern void *ns_ymsg_get_ylad_ext();
extern void *ns_ymsg_get_buddies_ext();
extern int ns_ymsg_get_connection_tags_ext();

extern void ns_ymsg_set_local_host_ext(void *ptr);
extern void ns_ymsg_set_ylad_ext(void *ptr);
extern void ns_ymsg_set_buddies_ext(void *ptr);
extern void ns_ymsg_set_connection_tags_ext(int con_tag);

#endif
