
#include "amf.h"

#ifdef NS_DEBUG_ON
static int amf_debug_on = 1;
#else
static int amf_debug_on = 0;
#endif

void amf_set_debug(int debug_level)
{
  amf_debug_on = debug_level;
}

void amf_debug_log(int level, int log_always, char *filename, int line, char *fname, char *format, ...) {
#define MAX_AMF_LOG_BUF_SIZE 2048
  va_list ap;
  char buffer[MAX_AMF_LOG_BUF_SIZE + 1] = "\0";
  int amt_written = 0, amt_written1=0;

  if(amf_debug_on < level && log_always == 0) return;

  amt_written1 = sprintf(buffer, "\n%s|%d|%s|", filename, line, fname);
  va_start(ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_AMF_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);
  buffer[MAX_AMF_LOG_BUF_SIZE] = 0;

  if(amt_written < 0) {
    amt_written = strlen(buffer) - amt_written1;
  }

  if(amt_written > (MAX_AMF_LOG_BUF_SIZE - amt_written1)) {
    amt_written = (MAX_AMF_LOG_BUF_SIZE - amt_written1);
  }

  fprintf(stdout, "%s", buffer );
}


char amf_data_version = 0;
char amf_pkt_version = 0;

char line[MAX_VAL * 4 + 1], copy_to[MAX_VAL * 4 + 1], copy_from[MAX_VAL * 4 + 1];
FILE *amf_infp = NULL;
int amfin_lineno = 0;
char *amfin_ptr;
int amfin_left;
int amf_out_mode = 0;	//0: standard AMF bytes.
int amf_no_param_flag = 0;	//0: standard AMF bytes.
unsigned short amf_seg_count = 0;
char *last_seg_start;	//ptr in out for last seg start
unsigned short seg_len;

FILE *outfp = NULL;
char *out_buf = NULL;
int max_mlen;
int mlen;


char *amf_asc_ptr = NULL;
