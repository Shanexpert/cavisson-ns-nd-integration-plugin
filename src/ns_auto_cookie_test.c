/**
 * Compile with: gcc -DNS_DEBUG_ON -o ns_auto_cookie_test ns_auto_cookie_test.c ns_auto_cookie.c ns_log.c -ggdb -I./thirdparty/libs/libxml2-2.6.30/include
 */

#include "nslb_util.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_http_hdr_states.h"
#include "ns_auto_cookie.h"
#include "ns_exit.h"

/* depends */
char CRLFString[3] = "\r\n";
int CRLFString_Length = 2;
Global_data global_settings;
int total_cookie_entries = 0;
int CookieString_Length = 8;
char * CookieString = "Cookie: ";
char SCString[2] = "; ";
int SCString_Length = 2;
char EQString[2] = "=";
int EQString_Length = 1;

void end_test_run( ) { NS_EXIT(-1, ""); }

int testidx = -1;
char g_ns_wdir[1024] = ".";
unsigned char my_port_index = 0; // For testing

int debug = 4;

char *argv0 = "netstorm.debug";


// tokanize the buffer with token
//this function is not static because used in ns_test_gdf.c for test purpose.
int get_tokens(char *read_buf, char *fields[], char *token )
{
  int totalFlds = 0;
  char *ptr;

  ptr = read_buf;
  while((fields[totalFlds] = strtok(ptr, token)) != NULL)
  {
    ptr = NULL;
    totalFlds++;
  }
  return(totalFlds);
}


inline void free_vectors (int num_vectors, int *free_array, struct iovec* vector)
{
int j;
        for (j = 0; j < num_vectors; j++) {
          if (free_array[j]) {
             FREE_AND_MAKE_NULL_EX (vector[j].iov_base, vector[j].iov_len, "vector[j].iov_base", j);// added vector length
             free_array[j] = 0;
          }
        }
}

/* end depends */


FILE *log_fp, *fp;

#define MAX_COOKIE_LEN 8192
#define MAX_COOKIE_ENTRIES 100 /* at max 100 cookie entries can be defined in the test file */

static void log_cookies(struct iovec *vector, int size)
{
char cookie_buf[20000] = "";
char buf[800];
int i;

  if(size == 0)
  {
    NSDL2_COOKIES(NULL, NULL, "There are no cookies");
    return;
  }
  for(i = 0; i < size; i++)
  {
    sprintf(buf, "%*.*s", vector[i].iov_len, vector[i].iov_len, vector[i].iov_base);
    strcat(cookie_buf, buf);
  }
  NSDL2_COOKIES(NULL, NULL, "Cookie => %s", cookie_buf);
}
 
void run_test(int mode)
{
  char buf[MAX_COOKIE_LEN];
  char URL[MAX_COOKIE_LEN], *urlp;
  char HOST[MAX_COOKIE_LEN], *hostp;
  int NUM_READS[MAX_COOKIE_LEN], numr; char *nrp;
  char set_cookie[MAX_COOKIE_LEN]; char *scp;
  char *ptr;
  int i;
  connection *cptr = NULL;
  VUser *vptr = NULL;
  connection cptr_a[MAX_COOKIE_ENTRIES]; /* At max 100 entries in the test run file can work */
  VUser vptr_a[MAX_COOKIE_ENTRIES];
  int total_num_cookie_entries = 0;
    

  struct iovec vector[IOVECTOR_SIZE];
  struct iovec *vector_ptr;
  int free_array[IOVECTOR_SIZE];
 
  int next_counter = 4;
  g_auto_cookie_mode = mode;

  while (!feof(fp)) {
    if (fgets(buf, MAX_COOKIE_LEN, fp) != NULL) {
      buf[strlen(buf) - 1] = '\0';  // Replace new line by Null
      if(strchr(buf, '#') || buf[0] == '\0')
        continue;

      if (!strncmp("URL", buf, strlen("URL"))) {
        strcpy(URL, buf);
        urlp = URL;
        while (urlp[0] != '=') urlp++; urlp++;
        while (urlp[0] == ' ') urlp++;
        next_counter--;
      } else if (!strncmp("HOST", buf, strlen("HOST"))) {
        strcpy(HOST, buf);
        hostp = HOST;
        while (hostp[0] != '=') hostp++; hostp++;
        while (hostp[0] == ' ') hostp++;
        next_counter--;
      } else if (!strncmp("NUM_READS", buf, strlen("NUM_READS"))) {
        nrp = buf;
        while (nrp[0] != '=') nrp++; nrp++;
        while (nrp[0] == ' ') nrp++;
        //fprintf(log_fp, "nrp = %s\n", nrp);
        ptr = strtok(nrp, ",");
        i = 0;
        //printf("ptr = %s\n", ptr);
        while (ptr) {
          NUM_READS[i] = atoi(ptr);
          //printf("NUM_READS[%d] = %d\n", i, NUM_READS[i]);
          ptr = strtok(NULL, ",");
          i++;
        }
        if (i-1 == 0 && NUM_READS[i-1] == 0)
          numr = 0;
        else
          numr = i;

        next_counter--;
      } else if (!strncmp("Set-Cookie", buf, strlen("Set-Cookie"))) {
        strcpy(set_cookie, buf);
        scp = set_cookie;
        while (scp[0] != ':') scp++; scp++;
        while (scp[0] == ' ') scp++;
        next_counter--;
      }
    }

    if (!next_counter) {
      next_counter = 4;
      total_num_cookie_entries++;
      if (total_num_cookie_entries > MAX_COOKIE_ENTRIES)
        break;
      //printf("%s%s%s", urlp, hostp, scp);

      /* fill cptr, vptr here. */
      cptr = &cptr_a[total_num_cookie_entries - 1];
      vptr = &vptr_a[total_num_cookie_entries - 1];
/*       if (cptr) {  */
/*         free(cptr->url_num->proto.http.url.seg_start->seg_ptr.str_ptr); */
/*         free(cptr->url_num->proto.http.url.seg_start); */
/*         free(cptr->url_num->proto.http.index.svr_ptr); */
/*         free(cptr->url_num); */
/*         free(cptr); */
/*       } */
/*       if (vptr) { */
/*         free(vptr); */
/*       } */

      /* vptr */
/*       vptr = malloc(sizeof(VUser)); */

      /* cptr */
/*       cptr = malloc(sizeof(connection)); */

      MY_MALLOC (cptr->url_num, sizeof (action_request_Shr), "cptr->url_num for total_num_cookie_entries", total_num_cookie_entries);

      /* path */
      MY_MALLOC (cptr->url_num->proto.http.url.seg_start, sizeof(SegTableEntry_Shr), "cptr->url_num->url.seg_start for total_num_cookie_entries", total_num_cookie_entries);
      MY_MALLOC (cptr->url_num->proto.http.url.seg_start->seg_ptr.str_ptr, sizeof(PointerTableEntry_Shr), "cptr->url_num->url.seg_start->seg_ptr.str_ptr for total_num_cookie_entries", total_num_cookie_entries);
      cptr->url_num->proto.http.url.seg_start->seg_ptr.str_ptr->big_buf_pointer = urlp; 

      /* host */
      // cptr->url_num->proto.http.index.svr_ptr = malloc(sizeof(SvrTableEntry_Shr));
      // cptr->url_num->proto.http.index.svr_ptr->server_hostname = hostp;

      /* host - Mapped Host */
      MY_MALLOC (cptr->old_svr_entry,  sizeof(TotSvrTableEntry_Shr), "cptr->old_svr_entry for total_num_cookie_entries", total_num_cookie_entries);
      cptr->old_svr_entry->server_name = hostp;

      cptr->vptr = vptr;
      
      
      /* SAVE AUTO COOKIE */
      /* read in chunks if any */
      //printf("numr = %d\n", numr);

      cptr->header_state = HDST_SET_COOKIE_COLON_WHITESPACE;
      for (i = 0; i < numr; i++) {
        //printf("Chunk [%d] = %*.*s\n", i, 0, NUM_READS[i], scp);
        save_auto_cookie(scp , NUM_READS[i], cptr, 0); /* uncomment this */
        if(cptr->header_state != HDST_SET_COOKIE_MORE_COOKIE)
          printf("save_auto_cookie did not set state to HDST_SET_COOKIE_MORE_COOKIE\n");
        scp += NUM_READS[i];
      }
      //printf("Last Chunk = %s\n", scp);
      save_auto_cookie(scp , strlen(scp), cptr, 0); /* and this */
      if(cptr->header_state != HDST_CR)
        printf("save_auto_cookie did not set state to HDST_CR\n");
    }
  }


  /* INSERT AUTO COOKIE */
  int ret = 0;
  int j;
  int bytes_sent;
  //free_vectors (1, free_array, vector);
  for (i = 0; i < total_num_cookie_entries; i++)
  {
    cptr = &cptr_a[i];
    vptr = &vptr_a[i];
    NSDL2_COOKIES(vptr, cptr, "host = %s", cptr->old_svr_entry->server_name); //HOST name for requested url
    NSDL2_COOKIES(vptr, cptr, "path = %s\n", cptr->url_num->proto.http.url.seg_start->seg_ptr.str_ptr->big_buf_pointer); //Path for request
    ret = insert_auto_cookie(vector, free_array, 0, IOVECTOR_SIZE - 1, cptr, vptr);
    //NSDL3_COOKIES(vptr, cptr, "Returning next index = %d from insert auto cookie", ret);

    log_cookies(vector, ret); 

    //if(bytes_sent < 0) free_vectors (ret, free_array, vector);

    //NSDL3_COOKIES(vptr, cptr, "vector[%d]: iov_length = %d, iov_base: %*.*s ", i, vector[i].iov_len, vector[i].iov_len, vector[i].iov_len, vector[i].iov_base);

    //free_vectors (ret, free_array, vector);
     //for (j = 0; j < ret; j++) {
     // if (free_array[j]) free(vector[j].iov_base);
     // }

    delete_all_cookies_nodes(vptr);
  }
}

static void open_log_file()
{
  //log_fp = fopen("auto_cookie_test.log", O_WRONLY | O_APPEND);
}
static void init()
{
  char buf[1024];
  char err_msg[4098];

  kw_set_debug("4");
  strcpy(buf, "MODULEMASK COOKIES");
  if (kw_set_modulemask(buf, err_msg) != 0)
    NS_EXIT(-1, "kw_set_modulemask() failed");
}

/**
 * args: <file name> <mode>
 */
int 
main(argc, argv)
     int argc;
     char **argv;
{
  init();
  fp = fopen(argv[1], "r");
 
  run_test(atoi(argv[2]));
  return 0;
}
