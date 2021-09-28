#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shadow.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>
#include <unistd.h>

#include "ns_data_types.h"
#include "nsi_gen_scr8.h"

#define NAME_LEN 64
u_ns_ts_t  start_time;
u_ns_ts_t  total_time;
u_ns_ts_t  total_bytes;
int data_row_num = 0;
FILE *fpout;
char sbuf[64];

#define SUCCESS 1
#define FAILURE 0
#define INIT_URL_ENTRIES 50
#define INIT_PAGE_ENTRIES 10
#define DELTA_URL_ENTRIES 50
#define DELTA_PAGE_ENTRIES 10

typedef struct {
	int PageInst;
	char PageName[NAME_LEN];
	int stime; //Page Start Time relative to tranction start
	int intvl;
	int start_entry; //in URL table
	int num_entries; //in URL table
	//int status;
	char *status;
	int bytes;
} PageInfo;
	
	
typedef struct {
	int PageInst;
	char UrlName[NAME_LEN];
	int stime; //URL Start Time relative to tranction start
	int ConnectIntvl;
	int SSLIntvl;
	int WriteIntvl;
	int FirstByteIntvl;
	int ContentIntvl;
	//int status;
	//char status[10 + 1];
	char *status;
	int httpStatus;
	int bytes;
	int con_reused;
	int ssl_reused;
	int con_num;
        long long nd_fp_instance;
        long long nd_fp_signature;
} UrlInfo;

PageInfo *pageInfo;
UrlInfo *urlInfo;
int total_url_entries;
int max_url_entries;
int total_page_entries;
int max_page_entries;

static int init () {
  pageInfo = (PageInfo *)malloc(INIT_PAGE_ENTRIES * sizeof(PageInfo));
  urlInfo = (UrlInfo *)malloc(INIT_URL_ENTRIES * sizeof(UrlInfo));
  if (pageInfo && urlInfo) {
	max_url_entries = INIT_URL_ENTRIES;
	max_page_entries = INIT_PAGE_ENTRIES;
	return SUCCESS;
  } else {
	return FAILURE;
  }
}

static int create_page_table_entry(int *row_num) {
  if (total_page_entries == max_page_entries) {
    pageInfo = (PageInfo *)realloc((char *)pageInfo, 
			     (max_page_entries + DELTA_PAGE_ENTRIES) *
			     sizeof(PageInfo));
    if (!pageInfo) {
      fprintf(stderr,"create_page_table_entry(): Error allocating more memory\n");
      return(FAILURE);
    } else max_page_entries += DELTA_PAGE_ENTRIES;
  }
  *row_num = total_page_entries++;
  return (SUCCESS);
}

static int create_url_table_entry(int *row_num) {
  if (total_url_entries == max_url_entries) {
    urlInfo = (UrlInfo *)realloc((char *)urlInfo, 
			     (max_url_entries + DELTA_URL_ENTRIES) *
			     sizeof(UrlInfo));
    if (!urlInfo) {
      fprintf(stderr,"create_url_table_entry(): Error allocating more memory\n");
      return(FAILURE);
    } else max_url_entries += DELTA_URL_ENTRIES;
  }
  *row_num = total_url_entries++;
  return (SUCCESS);
}


//Copy the message from cmd output file to stdout
void 
print_msg (FILE *fp)
{
char buf[4096];

	while (fgets(buf, 4096, fp))
		fputs (buf, stdout);
}

FILE *
do_system (char * cmd, char *fname)
{
int status;
FILE *fp;

	status = system(cmd);
	fp = fopen (fname, "r");

	//comand output file is a temp file & need to be removed.
	//Since file is opened, file will remain in the system, 
	// till we close it.
	// Do not remove tmp file, if NS_DEBUG env is defined
	if (fp) 
	   if (getenv("NS_DEBUG"))
		unlink(fname);

	if (status == -1) {
		printf ("cmd '%s' could not execute\n", cmd);
		if (fp) print_msg(fp);
		fclose(fp);
		return NULL;
	}

	if (!(WIFEXITED(status))) {
		printf ("cmd '%s' did not exit normally\n", cmd);
		if (fp) print_msg(fp);
		fclose(fp);
		return NULL;
	}

	if (WEXITSTATUS(status) != 0) {
		printf ("cmd '%s' Failed with Error\n", cmd);
		if (fp) print_msg(fp);
		fclose(fp);
		return NULL;
	}

	return fp;
}

void pagebar_output (int page_num)
{
char *page_name = pageInfo[page_num].PageName;
int start_pix;
int intvl_pix;
double intvl_sec;

	intvl_sec = (((double)pageInfo[page_num].intvl)/((double)1000.0));
	start_pix = (int)rint(((double)pageInfo[page_num].stime * 600.0)/((double)total_time));
	intvl_pix = (int)rint(((double)pageInfo[page_num].intvl * 600.0)/((double)total_time));

	if (start_pix + intvl_pix > 600 ) intvl_pix--; 

	if (start_pix) {
		fprintf(fpout, pagebarB, page_num, page_name, start_pix, intvl_pix, page_num, page_name, intvl_sec);
	} else {
		fprintf(fpout, pagebarA, page_num, page_name, intvl_pix, page_num, page_name, intvl_sec);
	}
}

void urlbar_output (int url_num)
{
char *url_name = urlInfo[url_num].UrlName;
int start_pix;
int connect_pix;
int ssl_pix;
int write_pix;
int first_pix;
int content_pix;
double connect_sec;
double ssl_sec;
double write_sec;
double first_sec;
double content_sec;
//long long flowpath_instance;
//long long flowpath_signature;

	start_pix = (int)rint(((double)urlInfo[url_num].stime * 600.0)/((double)total_time));
	connect_pix = (int)rint(((double)urlInfo[url_num].ConnectIntvl * 600.0)/((double)total_time));
	connect_sec = (((double)urlInfo[url_num].ConnectIntvl)/((double)1000.0));
	ssl_pix = (int)rint(((double)urlInfo[url_num].SSLIntvl * 600.0)/((double)total_time));
	ssl_sec = (((double)urlInfo[url_num].SSLIntvl)/((double)1000.0));
	write_pix = (int)rint(((double)urlInfo[url_num].WriteIntvl * 600.0)/((double)total_time));
	write_sec = (((double)urlInfo[url_num].WriteIntvl)/((double)1000.0));
	first_pix = (int)rint(((double)urlInfo[url_num].FirstByteIntvl * 600.0)/((double)total_time));
	first_sec = (((double)urlInfo[url_num].FirstByteIntvl)/((double)1000.0));
	content_pix = (int)rint(((double)urlInfo[url_num].ContentIntvl * 600.0)/((double)total_time));
	content_sec = (((double)urlInfo[url_num].ContentIntvl)/((double)1000.0));
        //flowpath_instance = urlInfo[url_num].nd_fp_instance;
        //flowpath_signature = urlInfo[url_num].nd_fp_signature;

	if (start_pix + connect_pix + ssl_pix + write_pix + first_pix + content_pix > 600) {
		if (content_pix) content_pix--;
	 	else if (first_pix) first_pix--;
	 	else if (write_pix) write_pix--;
	 	else if (ssl_pix) ssl_pix--;
	 	else if (connect_pix) connect_pix--;
		else if (start_pix) start_pix--;
	}

	fprintf(fpout, urlbarBegin, urlInfo[url_num].con_num, url_name);
	if (start_pix)
	    fprintf(fpout, urlbarStart, start_pix);
	if (connect_pix)
	    fprintf(fpout, urlbarConnect, connect_pix, url_name, connect_sec);
	if (ssl_pix)
	    fprintf(fpout, urlbarSSL, ssl_pix, url_name, ssl_sec);
	if (write_pix)
	    fprintf(fpout, urlbarWrite, write_pix, url_name, write_sec);
	if (first_pix)
	    fprintf(fpout, urlbarFirst, first_pix, url_name, first_sec);
	if (content_pix)
	    fprintf(fpout, urlbarContent, content_pix, url_name, content_sec);
	fprintf(fpout, "%s", urlbarEnd);
}

void pagerow_output (int page_num)
{
char *page_name = pageInfo[page_num].PageName;
double intvl_sec;

	intvl_sec = (((double)pageInfo[page_num].intvl)/((double)1000.0));

	if (data_row_num % 2)
	    fprintf(fpout, "%s", evenhead);
        else
	    fprintf(fpout, "%s", oddhead);
	data_row_num++;
	fprintf(fpout, pagerow, page_num, page_name, intvl_sec, pageInfo[page_num].bytes);
}

void urlrow_output (int url_num)
{
char *url_name = urlInfo[url_num].UrlName;
double connect_sec;
double ssl_sec;
double write_sec;
double first_sec;
double content_sec;
long long flowpath_instance;
long long flowpath_signature;

	if (data_row_num % 2)
	    fprintf(fpout, "%s", evenhead);
        else
	    fprintf(fpout, "%s", oddhead);
	data_row_num++;

	connect_sec = (((double)urlInfo[url_num].ConnectIntvl)/((double)1000.0));
	ssl_sec = (((double)urlInfo[url_num].SSLIntvl)/((double)1000.0));
	write_sec = (((double)urlInfo[url_num].WriteIntvl)/((double)1000.0));
	first_sec = (((double)urlInfo[url_num].FirstByteIntvl)/((double)1000.0));
	content_sec = (((double)urlInfo[url_num].ContentIntvl)/((double)1000.0));
        flowpath_instance = urlInfo[url_num].nd_fp_instance;
        flowpath_signature = urlInfo[url_num].nd_fp_signature;

	fprintf(fpout, "\t\t<td>Conn %d:%s</td><td>%s</td><td>%d</td><td>%s</td><td>%s</td><td align=\"right\">%6.3f</td>", urlInfo[url_num].con_num, url_name, urlInfo[url_num].status, 
			urlInfo[url_num].httpStatus, urlInfo[url_num].con_reused?"Y":"N", urlInfo[url_num].ssl_reused?"Y":"N", connect_sec+ssl_sec+write_sec+first_sec+content_sec);

	if (urlInfo[url_num].ConnectIntvl)
	    fprintf(fpout, "<td align=\"right\">%6.3f</td>", connect_sec);
	else
	    fprintf(fpout, "<td>&nbsp;</td>");

	if (urlInfo[url_num].SSLIntvl)
	    fprintf(fpout, "<td align=\"right\">%6.3f</td>", ssl_sec);
	else
	    fprintf(fpout, "<td>&nbsp;</td>");

	if (urlInfo[url_num].WriteIntvl)
	    fprintf(fpout, "<td align=\"right\">%6.3f</td>", write_sec);
	else
	    fprintf(fpout, "<td>&nbsp;</td>");

	if (urlInfo[url_num].FirstByteIntvl)
         {
            if(flowpath_instance == -1)
	      fprintf(fpout, "<td align=\"right\">%6.3f</td>",  first_sec);
            else
	      fprintf(fpout, "<td align=\"right\"><a href=javascript:openFlowChart('%llu','%llu')>%6.3f</a></td>", flowpath_instance,flowpath_signature, first_sec);
         }
	else
	    fprintf(fpout, "<td>&nbsp;</td>");

	if (urlInfo[url_num].ContentIntvl)
	    fprintf(fpout, "<td align=\"right\">%6.3f</td>", content_sec);
	else
	    fprintf(fpout, "<td>&nbsp;</td>");

	fprintf(fpout, "<td align=\"right\">%d</td>\n\t</tr>\n", urlInfo[url_num].bytes);
}

void get_stime()
{
int hour, min, sec, ms, tot;

	tot = start_time;
	ms = tot % 1000;
	tot = tot/1000;
	hour = tot/3600;
	tot = tot%3600;
	min = tot/60;
	sec = tot%60;

	sprintf(sbuf,"%02d:%02d:%02d.%03d", hour, min, sec, ms);
}

void confirm_netstorm_id()
{
  struct passwd *pw;
  pw = getpwuid(getuid());
  if (pw == NULL)
  {
    printf("Error: Unable to get the real user name\n");
    exit (1);
  }
  if (strcmp(pw->pw_name, "netstorm"))
  {
    printf("Error: This command must be run as 'netstorm' user only. Currently being run as '%s'\n", pw->pw_name);
    exit (1);
  }
}

int main( int argc, char *argv[])
{
int first, len;
FILE *fp;
char buf[4096];
char *ptr;
int last_page = -1;
int pg_inst;
int num;
int cur_page_idx;
int cur_url_idx;
char *page_name;
char *url_name;
int sstamp, connectstamp, sslstamp, firststamp, writestamp, downloadstamp;
int page_max_stamp, page_start_time;
int i, j;
char obj_type[32];
int obj;
char cmd[1024];
char out_fname[1024];
char tmp_fname[128];

        confirm_netstorm_id();
	ptr = getenv("NS_WDIR");
	if (!ptr) {
	    printf("NS_WDIR env variable must be defined\n");
	    exit(1);
	} 

	if (argc < 7 ) {
	    printf("Usage: %s obj-tye obj-name user-id child_id tr_num session-inst ...\n", argv[0]);
	    exit (1);
	}

	sprintf(tmp_fname, "/tmp/nsi_get_8x.out.%d", getpid());
	obj = atoi(argv[1]);
	if (obj == 0) {
		strcpy(obj_type, "URL");
		if (argc < 9) {
	    	    printf("Usage: %s obj-tye obj-name user-id child_id tr_num session-inst page_instance urlindex\n", argv[0]);
	    	    exit (1);
		}
		sprintf (cmd, "%s/bin/nsi_get_8x %s %s %s %s %s %s > %s", ptr, argv[5], argv[1], argv[4], argv[6], argv[7], argv[8], tmp_fname); 
	} else if (obj == 1) {
		strcpy(obj_type, "Page");
		if (argc < 8) {
	    	    printf("Usage: %s obj-tye obj-name user-id child_id tr_num session-inst page_instance\n", argv[0]);
	    	    exit (1);
		}
		sprintf (cmd, "%s/bin/nsi_get_8x %s %s %s %s %s > %s", ptr, argv[5], argv[1], argv[4], argv[6], argv[7], tmp_fname); 
                //printf("Cmd goin to run = %s\n", cmd);
	} else if (obj == 2) {
		strcpy(obj_type, "Transaction");
		if (argc < 8) {
	    	    printf("Usage: %s obj-tye obj-name user-id child_id tr_num session-inst tx_instance\n", argv[0]);
	    	    exit (1);
		}
		sprintf (cmd, "%s/bin/nsi_get_8x %s %s %s %s %s > %s", ptr, argv[5], argv[1], argv[4], argv[6], argv[7], tmp_fname); 
	} else if (obj == 3) {
		strcpy(obj_type, "Session");
		sprintf (cmd, "%s/bin/nsi_get_8x %s %s %s %s > %s", ptr, argv[5], argv[1], argv[4], argv[6], tmp_fname); 
	} else {
	    printf("Error: obj_type can be 0-3 only\n");
	    exit(1);
	}

	sprintf(out_fname, "%s/webapps/netstorm/analyze/rptObjDetailsTime.html", ptr);

	fp = do_system(cmd, tmp_fname);
	if (!fp)
		exit (1);

	if (init() != SUCCESS) {
	    	printf("Unable initialize page table\n");
	    	exit (1);
	}

#if 0
	fp = fopen(tmp_fname, "r");
	if (!fp) {
	    printf("Unable to open %s\n", tmp_fname);
	    exit (1);
	}
#endif

	first = 1;
	while (fgets(buf, 4096, fp)) {
          //printf("buf = [%s]\n",buf);
	    //Ignore header line
	    if (first) {
		first = 0;
		continue;
	    }
	
	    //Remove trailing new line char
	    len = strlen(buf);
	    if (buf[len-1] == '\n') buf[len-1] = '\0';

	    ptr = strtok (buf, "|");
            //printf("buf is = [%s]",buf);
	    if (!ptr) {
		printf("expecting page instance\n");
		exit(1);
	    }
	    pg_inst = atoi(ptr);
            //printf("page instance is equal to [%d]", pg_inst); 

	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting page name\n");
		exit(1);
	    }
	    page_name = ptr;
            //printf("page name is equal to [%s]", page_name); 
	
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting URL name\n");
		exit(1);
	    }
	    url_name = ptr;
	
	    if (create_url_table_entry(&cur_url_idx) != SUCCESS) {
	    	printf("Unable to alloc url entry\n");
	    	exit (1);
	    }

	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting start time\n");
		exit(1);
	    }
	    sstamp = atoi(ptr);

	    if (cur_url_idx == 0)
		start_time = sstamp;

	    if (pg_inst != last_page) {
		if (create_page_table_entry(&cur_page_idx) != SUCCESS) {
		    printf("Unable to alloc page entry\n");
		    exit (1);
		}
		last_page = pg_inst;
		pageInfo[cur_page_idx].PageInst = pg_inst;
		strncpy(pageInfo[cur_page_idx].PageName, page_name, NAME_LEN);
		pageInfo[cur_page_idx].PageName[NAME_LEN-1] = '\0';
		pageInfo[cur_page_idx].stime = sstamp - start_time;
		pageInfo[cur_page_idx].start_entry = cur_url_idx;
		pageInfo[cur_page_idx].num_entries = 0;
		pageInfo[cur_page_idx].bytes = 0;
		if (cur_page_idx)
		    pageInfo[cur_page_idx-1].intvl = page_max_stamp - page_start_time;
	        page_max_stamp = page_start_time = sstamp;
	    }
	    pageInfo[cur_page_idx].num_entries++;
		
	    urlInfo[cur_url_idx].PageInst = pg_inst;
	    //If url_name has ? or :, ignore rest of it
	    if ((ptr = index (url_name, '?'))) *ptr = '\0';
	    if ((ptr = index (url_name, ':'))) *ptr = '\0';
	    strncpy(urlInfo[cur_url_idx].UrlName, url_name, NAME_LEN);
	    urlInfo[cur_url_idx].UrlName[NAME_LEN-1] = '\0';

	    urlInfo[cur_url_idx].stime = sstamp - start_time;
	
	    //get connect start time
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting connect start time\n");
		exit(1);
	    }
	    //ignore connect start time

	    //get connected time
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting connected time\n");
		exit(1);
	    }
	    connectstamp = atoi(ptr);
	    if (connectstamp < sstamp) connectstamp = sstamp;
	    urlInfo[cur_url_idx].ConnectIntvl = connectstamp - sstamp;

	    //get ssl time
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting ssl time\n");
		exit(1);
	    }
	    sslstamp = atoi(ptr);
	    if (sslstamp < connectstamp) sslstamp = connectstamp;
	    urlInfo[cur_url_idx].SSLIntvl  = sslstamp - connectstamp;

	    //get write time
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting write time\n");
		exit(1);
	    }
	    writestamp = atoi(ptr);
	    if (writestamp < sslstamp) writestamp = sslstamp;
	    urlInfo[cur_url_idx].WriteIntvl  = writestamp - sslstamp;

	    //get first byte time
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting firstbyte time\n");
		exit(1);
	    }
	    firststamp = atoi(ptr);
	    if (firststamp < writestamp) firststamp = writestamp;
	    urlInfo[cur_url_idx].FirstByteIntvl  = firststamp - writestamp;

	    //get download time
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting content time\n");
		exit(1);
	    }
	    downloadstamp = atoi(ptr);
	    if (downloadstamp < firststamp) downloadstamp = firststamp;
	    urlInfo[cur_url_idx].ContentIntvl  = downloadstamp - firststamp;

	    if (page_max_stamp < downloadstamp) //Set to high water mark. there may intermediate urls end last
	    	page_max_stamp = downloadstamp;

	    //get status
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting status\n");
		exit(1);
	    }
            //Nikita:
            //We are supporting both error message(char type) and error code(int type) so,
            //we have to need the data type of status field of urlInfo to for supporting both.
	    //num = atoi(ptr);
	    //urlInfo[cur_url_idx].status = ptr;
	    //strcpy(urlInfo[cur_url_idx].status,ptr);
	    urlInfo[cur_url_idx].status = strdup(ptr);
	    //get http code
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting http status\n");
		exit(1);
	    }
	    num = atoi(ptr);
	    urlInfo[cur_url_idx].httpStatus  = num;

	    //get bytes
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting bytes\n");
		exit(1);
	    }
	    num = atoi(ptr);
	    urlInfo[cur_url_idx].bytes  = num;

	    pageInfo[cur_page_idx].bytes += num;
	    total_bytes += num;

	    //get ConReused
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting bytes\n");
		exit(1);
	    }
	    if (strcmp (ptr, "t"))
	        urlInfo[cur_url_idx].con_reused  = 0;
	    else
	        urlInfo[cur_url_idx].con_reused  = 1;

	    //get SSLReused
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting bytes\n");
		exit(1);
	    }
	    if (strcmp (ptr, "t"))
	        urlInfo[cur_url_idx].ssl_reused  = 0;
	    else
	        urlInfo[cur_url_idx].ssl_reused  = 1;

	    //get con num
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting bytes\n");
		exit(1);
	    }
	    num = atoi(ptr);
	    urlInfo[cur_url_idx].con_num  = num;
	    
            //Added By Nikita on date Tue Feb 28 12:43:29 IST 2012 
            //get flow path instance
	    ptr = strtok (NULL, "|");
	    if (!ptr) {
		printf("expecting flow path instance\n");
		exit(1);
	    }
//	    num = atoll(ptr);
	    urlInfo[cur_url_idx].nd_fp_instance  = atoll(ptr);
	    
            ptr = strtok (NULL, "|");
#if 0
In case of Netdiagnostics enabled test run, flowpath signature is passed as the last argument. 
In non ND test run, this argument is absent.

In future, in case another argument is added to this program, this needs to be taken care.
	    if (!ptr) {
		printf("expecting flow path signature\n");
		exit(1);
	    }
#endif
//	    num = atoll(ptr);
	    if (ptr) 
		urlInfo[cur_url_idx].nd_fp_signature  = atoll(ptr);

	}

        pageInfo[cur_page_idx].intvl = page_max_stamp - page_start_time;
	total_time = page_max_stamp - start_time;
	
#ifdef TEST
	for (i = 0; i < total_page_entries; i++) {
	    printf ("Page %d: %s > Start = %d (%6.3f s) Duration = %d (%6.3f s) bytes = %d\n",i ,
		pageInfo[i].PageName,
		(int)rint(((double)pageInfo[i].stime * 600.0)/((double)total_time)),
		(((double)pageInfo[i].stime)/((double)1000.0)),
		(int)rint(((double)pageInfo[i].intvl * 600.0)/((double)total_time)),
		(((double)pageInfo[i].intvl)/((double)1000.0)),
	      	pageInfo[i].bytes);
	      num = pageInfo[i].start_entry;
	    for (j = num; j < (num + pageInfo[i].num_entries); j++) {
	      printf ("    URL %d: %s > Start = %d (%6.3f s) ",j ,
		urlInfo[j].UrlName,
		(int)rint(((double)urlInfo[j].stime * 600.0)/((double)total_time)),
		(((double)urlInfo[j].stime)/((double)1000.0)));

		if (urlInfo[j].ConnectIntvl)
	      	  printf ("Connect = %d (%6.3f s) ", 
		(int)rint(((double)urlInfo[j].ConnectIntvl * 600.0)/((double)total_time)),
		(((double)urlInfo[j].ConnectIntvl)/((double)1000.0)));

		if (urlInfo[j].SSLIntvl)
	      	  printf ("SSL = %d (%6.3f s) ", 
		(int)rint(((double)urlInfo[j].SSLIntvl * 600.0)/((double)total_time)),
		(((double)urlInfo[j].SSLIntvl)/((double)1000.0)));

		if (urlInfo[j].WriteIntvl)
	      	  printf ("WRITE = %d (%6.3f s) ", 
		(int)rint(((double)urlInfo[j].WriteIntvl * 600.0)/((double)total_time)),
		(((double)urlInfo[j].WriteIntvl)/((double)1000.0)));

		if (urlInfo[j].FirstByteIntvl)
	      	  printf ("FIRST = %d (%6.3f s) ", 
		(int)rint(((double)urlInfo[j].FirstByteIntvl * 600.0)/((double)total_time)),
		(((double)urlInfo[j].FirstByteIntvl)/((double)1000.0)));

		if (urlInfo[j].ContentIntvl)
	      	  printf ("FIRST = %d (%6.3f s) ", 
		(int)rint(((double)urlInfo[j].ContentIntvl * 600.0)/((double)total_time)),
		(((double)urlInfo[j].ContentIntvl)/((double)1000.0)));

   		if (urlInfo[j].flowpath_instance)
                  printf ("FIRST = %llu", flowpath_instance = urlInfo[url_num].nd_fp_instance);
   		if (urlInfo[j].flowpath_signature)
                  printf ("FIRST = %llu", flowpath_signature = urlInfo[url_num].nd_fp_signature);
                  
	        printf ("staus = %s http = %d bytes = %c\n", 
	      	urlInfo[j].status,
	      	urlInfo[j].httpStatus,
	      	urlInfo[j].bytes);
	    }
	}

	for (i = 1; i <= 10; i++) {
		printf("Marker %d: %6.3f s\n", i, ((double) total_time * (double) i)/((double) 10000.0));
	}
#endif

	fclose (fp);	
	//unlink(tmp_fname);
	//fpout = fopen("/var/www/html/test/test.html", "w");
	//fpout = fopen("/home/anil/jakarta-tomcat-5.0.14/webapps/netstorm/gui3/rptObjDetailsTime.html", "w");
	fpout = fopen(out_fname, "w");
	if (!fpout) {
	    printf("Unable to open %s\n", out_fname);
	    exit (1);
	}
//Args: obj-type obj-type obj-type obj-name user-id obj-type obj-start-time
	get_stime();
/*
 * Selected hpd_tour_legacy:  4
 * User Id:  Session
 * 00:00:01.284 Start Time (HH:MM:SS.ms):  4 (from test start time)
 *
 */


	// fprintf(fpout, partA, obj_type, obj_type, obj_type, argv[2], argv[3], obj_type, sbuf);
	//fprintf(fpout, partA, obj_type, argv[2], argv[3], obj_type, sbuf);
	fprintf(fpout, partA, obj_type, argv[2], argv[4], argv[3], obj_type, sbuf);
	for (i = 0; i < total_page_entries; i++) {
	    pagebar_output (i);
	    num = pageInfo[i].start_entry;
	    for (j = num; j < (num + pageInfo[i].num_entries); j++)
	        urlbar_output (j);
	}
	fprintf(fpout, "\n%s\n\n<tr>\n", partB);
	for (i = 1; i <= 10; i++)
	    fprintf(fpout, "\t<td align='right'>%6.3f</td>\n", 
		((double) total_time * (double) i)/((double) 10000.0));
        fprintf(fpout, "</tr>\n\n%s\n\n", partC1);
        fprintf(fpout, row_header, obj_type, 
		(((double)total_time)/((double)1000.0)), total_bytes);
        fprintf(fpout, "\n\n%s\n\n", partC2);
	for (i = 0; i < total_page_entries; i++) {
	    pagerow_output (i);
	    num = pageInfo[i].start_entry;
	    for (j = num; j < (num + pageInfo[i].num_entries); j++)
	        urlrow_output (j);
	}
	fprintf(fpout, "\n%s\n", partD);
	printf("Report generated in %s file.\n", out_fname);
	exit (0);
}
