#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <ctype.h>

#include "ns_data_types.h"

#define MAX_PAGE_ENTRIES 1024
#define MAX_PAGE_NAME 32 /*27chars_x (max 3 digits) + 1 null char*/
#define MAX_PAGE_BASE_NAME 28

struct {
	char page_name[MAX_PAGE_BASE_NAME];
	int repeat_count;
} pageTable[MAX_PAGE_ENTRIES];

static char page_name_buf[MAX_PAGE_NAME];

static int last_page_added= 0;


static int
page_hash_init()
{
             return hcreate(MAX_PAGE_ENTRIES);
}

//Caller of thic function need to make sure, page_name is atring of length <= 27 chars
static char *
get_uniq_page_name (char *pname)
{
ENTRY e, *ep;
ns_ptr_t base_page_idx;
char page_name[MAX_PAGE_NAME];
static int first =1;

	if (first) {
	        page_hash_init();
		first = 0;
	}
	strcpy (page_name, pname);
	e.key = page_name;
        ep = hsearch(e, FIND);
	if (ep) {
           base_page_idx = (ns_ptr_t)(ep->data);
	   //printf ("Found %s at idx = %d with actual name=%s count=%d\n", page_name, (int)ep->data);
	   pageTable[base_page_idx].repeat_count++;
	   sprintf (page_name_buf, "%s_%d", page_name, pageTable[base_page_idx].repeat_count);
	} else {
	    strcpy(pageTable[last_page_added].page_name, page_name);
	    pageTable[last_page_added].repeat_count = 1;
            e.key = pageTable[last_page_added].page_name;
            /* data is just an integer (repeat_count), instead of a
               pointer to something */
            e.data = (char *)((ns_ptr_t)last_page_added);
            ep = hsearch(e, ENTER);
            /* there should be no failures */
            if (ep == NULL) {
               fprintf(stderr, "Adding Hash Page entry failed\n");
               exit(1);
            }
	    base_page_idx = last_page_added;
	    last_page_added++;
	    strcpy(page_name_buf, page_name);
	}
	return page_name_buf;
}

char *get_page_from_url (char *src_url)
{
char ubuf[4096], *pptr;
int len, i;
	
	if ((!strcmp(src_url, "/")) || (src_url[0] == '\0')) {
	    strcpy(page_name_buf, "index");
	} else {
	    strcpy(ubuf, src_url);
	    strtok (ubuf, "?:;"); //limit the URL till ?,; or :
	    pptr = strrchr(ubuf, '/'); //start URL from the last /
	    if (pptr)
		pptr++;
	    else 
		pptr = &ubuf[0];
		
	    strncpy(page_name_buf, pptr, MAX_PAGE_BASE_NAME);
	    page_name_buf[MAX_PAGE_BASE_NAME-1] = '\0';
	    if (strlen(page_name_buf) == 0) 
	        strcpy(page_name_buf, "index");
	}
	len = strlen(page_name_buf);
	for (i = 0; i < len; i++) {
            if ((!isalnum(page_name_buf[i])) && (page_name_buf[i] != '_'))
                  page_name_buf[i] = '_';
        }
        if (!isalpha(page_name_buf[0]))
                  page_name_buf[0] = 'X';

	return get_uniq_page_name(page_name_buf);
}

#ifdef TEST
 
int main() {
 char *data[] = { "alpha", 
		"", 
		"/", 
	        "/tours",
		"/tours/",
		"/tours/test?century=anc",
		"/tours/test",
		"/tours/test1/test2/test567890abc4567890cde4567890xyz1234?charlie",
		"/tours/test1;test2",
		"/tours/test1;test2"
           };
             ENTRY e, *ep;
             int max, i;
 
             /* starting with small table, and letting it grow does not work */
             //page_hash_init();
             for (i = 0; i < 9; i++) {
		printf ("URL:%s Page=%s\n", data[i], get_page_from_url(data[i]));
             }
}
#endif
