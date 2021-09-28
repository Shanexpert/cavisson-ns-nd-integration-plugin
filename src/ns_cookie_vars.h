#ifndef NS_COOKIE_VARS_H
#define NS_COOKIE_VARS_H

#define INIT_REQCOOK_ENTRIES 10*1024

/* for the cookie types */
#define NS_COOKIE_AUTOMATIC 1
#define NS_COOKIE_DECLARED 2

typedef struct CookieTableEntry {
  ns_bigbuf_t name; /* offset into big buf */
  int sess_idx;
  //int type;
} CookieTableEntry;

typedef struct ReqCookTableEntry {
  ns_bigbuf_t name; /* offset into big buf */
  short length;
} ReqCookTableEntry;

typedef struct ReqCookTableEntry_Shr {
  char* name; /* pointer into the shared big buf */
  short length;
} ReqCookTableEntry_Shr;

typedef struct ReqCookTab_Shr {
  ReqCookTableEntry_Shr* cookie_start; /* pointer into the shared reqcook table */
  int num_cookies;
} ReqCookTab_Shr;

extern int Create_cookie_entry(char* name, int sess_idx);

#ifndef CAV_MAIN
extern CookieTableEntry* cookieTable;
extern ReqCookTableEntry* reqCookTable;
#else
extern __thread CookieTableEntry* cookieTable;
extern __thread ReqCookTableEntry* reqCookTable;
#endif

#endif
