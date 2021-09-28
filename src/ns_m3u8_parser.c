/*****************************************************************************
 * m3u8_manifest_parser.c: HTTP Live Streaming stream filter
 *
 * Author    :  Atul Sharma
 * Date      :  25 Aug 2017
 * Purpose   :  This file is responsible for parsing .m3u8 extension file 
 * Refrences :  RFC8216 and  draft-pantos-http-live-streaming-00 to 23
 *****************************************************************************/

#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ns_embd_objects.h"
#include "util.h"
#include "netstorm.h"
#include "ns_alloc.h"
#include "ns_trace_level.h"
#include "ns_log.h"


#define MAX_M3U8_BUF_LEN       4 * 1024
#define MAX_URI_LEN            4 * 1024
#define MAX_DATA_LEN           128
#define DELTA_HLS_ENTRIES      1024
#define MAX_HLS_SIZE           2 * 1024

#define NS_M3U8_ATTR_PROGRAM_ID      0x00000001 
#define NS_M3U8_ATTR_BANDWIDTH       0x00000002 
#define NS_M3U8_ATTR_RESOLUTION      0x00000004 

//TAGS
#define NS_M3U8_TAG_M3U8_FILE                   0x00000001
#define NS_M3U8_TAG_TS_FILE                     0x00000002
#define NS_M3U8_TAG_EXTM3U                      0x00000004
#define NS_M3U8_TAG_VERSION                     0x00000008
#define NS_M3U8_TAG_STREAM_INF                  0x00000010
#define NS_M3U8_TAG_TARGET_DURATION             0x00000020 
#define NS_M3U8_TAG_MEDIA_SEQUENCE              0x00000040
#define NS_M3U8_TAG_DISCONTINUITY_SEQUENCE      0x00000080
#define NS_M3U8_TAG_ENDLIST                     0x00000100
#define NS_M3U8_TAG_PLAYLIST_TYPE               0x00000200
#define NS_M3U8_TAG_FRAMES_ONLY                 0x00000400
#define NS_M3U8_TAG_ALLOW_CACHE                 0x00000800
#define NS_M3U8_TAG_EXTINF                      0x00001000

//Return type
#define NS_M3U8_ERROR        -1
#define NS_M3U8_SUCCESS       0
#define NS_M3U8_EXTINF        1
#define NS_M3U8_STREAM_INF    2 
#define NS_M3U8_ENDLIST       3

//#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=514000,RESOLUTION=640x360
typedef struct m3u8_data
{
  //char           tag[MAX_DATA_LEN + 1];
  unsigned int   id;
  unsigned int   bandwidth;
  char           resolution[MAX_DATA_LEN + 1]; //Assumed it will not exceed
  char           uri[MAX_URI_LEN + 1];               
} m3u8_data;

//#EXTM3U
//#EXT-X-MEDIA-SEQUENCE:0
//#EXTINF:11,
//http://10.10.30.58/macys/24953835001_4005726991001_s-1.ts
typedef struct playlist_data
{
  double  media_seg_duration; 
  char    uri[MAX_URI_LEN + 1];
} playlist_data;

static int sort_m3u8_data (const void *P1, const void *P2)
{
  //Logic for increasing on the basis of bandwidth
  return ((m3u8_data *)P1)->bandwidth - ((m3u8_data *)P2)->bandwidth; 
}

static int create_m3u8_table_entry(int *row_num, int *total, int *max, char **ptr, int size)
{
  NSDL2_HLS(NULL, NULL, "Method called");
  if (*total == *max)
  {
    MY_REALLOC_AND_MEMSET(*ptr, (*max + DELTA_HLS_ENTRIES) * size, (*max) * size, "m3u8_attr", -1);
    *max += DELTA_HLS_ENTRIES;
  }
  *row_num = (*total)++;
  NSDL2_HLS(NULL, NULL, "row_num = %d, total = %d, max = %d, ptr = %p, size = %d", *row_num, *total, *max, &ptr, size);
  return 0;
}

//This method also adjust buffer to pointing next line
static void read_m3u8_line(char *buffer, char **pos, const size_t len, char *fill_buf, int *fill_buf_len)
{
  char *start = buffer;
  char *end = start + len;

  NSDL3_HLS(NULL, NULL, "Method called, buffer = [%s], pos = [%p], len = [%d]", buffer, pos, len);

  *fill_buf_len = 0;
  /*Get line from buffer*/
  while ((start < end) && (*fill_buf_len < MAX_M3U8_BUF_LEN))
  {
    *fill_buf = *start;

    if((*start == '\r') || (*start == '\n') || (*start == '\0'))
      break;

    start++;
    fill_buf++;
    (*fill_buf_len)++;
  }
  *fill_buf = '\0';

  /*Skip all \r \n before next line*/
  while((start < end) && ((*start == '\r') || (*start == '\n') || (*start == '\0')))
  {
    if(*start == '\0')
    {
      *pos = end;
      break;
    }
    else
    {
      /* next pass start after \r and \n */
      start++;
      *pos = start;
    }
  }

  return;
}

/* Parsing of master file */
//#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=514000,RESOLUTION=640x360
static int parse_Attributes(char *buffer, m3u8_data *m3u8_attr, char *errMsg)
{
  char *begin = buffer;
  char *start_ptr = NULL;
  int i = 0;
  char *attr_fields[20 + 1];
  int flag_m3u8 = 0;
  errMsg[0] = '\0';

  NSDL2_HLS(NULL, NULL, "Method called, filled_line = [%s]", buffer);

  int num_tokens = get_tokens(begin, attr_fields, ",",  21);

  NSDL3_HLS(NULL, NULL, "num_token = [%d]", num_tokens);
  if(num_tokens > 20)
  {
    NSDL3_HLS(NULL, NULL, "Error: Domain_list having equal or more than 20 tokens, it should be <= 20.\n");
    return NS_M3U8_ERROR;
  }
  else 
  {
    for(i=0 ; i < num_tokens ; i++)
    {
      start_ptr = attr_fields[i];

      if(!strncmp("PROGRAM-ID=", attr_fields[i], 11))
      {
        if(flag_m3u8 & NS_M3U8_ATTR_PROGRAM_ID)
        {
          sprintf(errMsg, "HLS RFC- 8216, 4.2- PROGRAM-ID should not be appear more than once");
          return NS_M3U8_ERROR;
        }

        flag_m3u8 |= NS_M3U8_ATTR_PROGRAM_ID;
        m3u8_attr->id = atoi(start_ptr+11); 
      }
      else if(!strncmp("BANDWIDTH=", attr_fields[i], 10))
      {
        if(flag_m3u8 & NS_M3U8_ATTR_BANDWIDTH)
        {
          sprintf(errMsg, "HLS RFC- 8216, 4.2- BANDWIDTH should not be appear more than once");
          return NS_M3U8_ERROR;
        }

        flag_m3u8 |= NS_M3U8_ATTR_BANDWIDTH;
        m3u8_attr->bandwidth = atoi(start_ptr+10); 
      }
      else if(!strncmp("RESOLUTION=",attr_fields[i], 11))
      {
        if(flag_m3u8 & NS_M3U8_ATTR_RESOLUTION)
        {
          sprintf(errMsg, "HLS RFC- 8216, 4.2- RESOLUTION should not be appear more than once");
          return NS_M3U8_ERROR;
        }

        flag_m3u8 |= NS_M3U8_ATTR_PROGRAM_ID;
        strcpy(m3u8_attr->resolution, start_ptr+11); 
      }
      else
      {
        NSDL3_HLS(NULL, NULL, "Unidentified token is found");
      }
    }
    NSDL2_HLS(NULL, NULL, "id = [%d], bandwidth = [%d], resolution = [%s]", m3u8_attr->id, m3u8_attr->bandwidth, m3u8_attr->resolution);
  } 

  return NS_M3U8_SUCCESS;
}

static int validate_m3u8_tags(char *filled_line, int *flag, char *errMsg)
{
  errMsg[0] = '\0';

  NSDL2_HLS(NULL, NULL, "Method called, filled_line = [%s], flag = [%d]", filled_line, *flag);

  if (filled_line[0] != '#' ) 
  {
    sprintf(errMsg, "Error: First line should be starts with '#'.");
    return NS_M3U8_ERROR;
  }

  if(!strncmp(filled_line, "#EXTM3U", 7))
  { 
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d]", (*flag & NS_M3U8_TAG_EXTM3U));
    if(*flag & NS_M3U8_TAG_EXTM3U)
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.1.1, There MUST be present, \"#EXT3MU\" and it cannot be apply multiple times");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_EXTM3U;
  }
  else if (!strncmp(filled_line, "#EXT-X-VERSION", 14))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_STREAM_INF = [%d], NS_M3U8_TAG_EXTINF = [%d], NS_M3U8_TAG_VERSION = [%d]",
                           !(*flag & NS_M3U8_TAG_EXTM3U), (*flag & NS_M3U8_TAG_STREAM_INF), (*flag & NS_M3U8_TAG_EXTINF), 
                           (*flag & NS_M3U8_TAG_VERSION));
    if(!(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_STREAM_INF) || (*flag & NS_M3U8_TAG_EXTINF) || (*flag & NS_M3U8_TAG_VERSION))
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.3, There MUST NOT be more than one version tag, \"#EXT-X-VERSION\" "
                      "or EXT3MU is absent or EXTINF is absent or Master's playlist tag is coming in media playlist file");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_VERSION;
  }   
  else if (!strncmp(filled_line, "#EXT-X-STREAM-INF", 17))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_TS_FILE = [%d]", (*flag & NS_M3U8_TAG_EXTM3U), 
                           (*flag & NS_M3U8_TAG_TS_FILE));
    if(!(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_TS_FILE))
    {
      sprintf(errMsg, "Master tags MUST NOT be present with media playlist tag or EXT3MU is absent");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_STREAM_INF;
    *flag |= NS_M3U8_TAG_M3U8_FILE;
    return NS_M3U8_STREAM_INF;
  }
  else if(!strncmp(filled_line, "#EXT-X-TARGETDURATION:", 22))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_M3U8_FILE = [%d], NS_M3U8_TAG_EXTINF = [%d], "
                          "NS_M3U8_TAG_TARGET_DURATION = [%d]", 
                           !(*flag & NS_M3U8_TAG_EXTM3U), (*flag & NS_M3U8_TAG_M3U8_FILE), (*flag & NS_M3U8_TAG_EXTINF), 
                           (*flag & NS_M3U8_TAG_TARGET_DURATION));

    if(!(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_M3U8_FILE) || (*flag & NS_M3U8_TAG_EXTINF) || (*flag & NS_M3U8_TAG_TARGET_DURATION))
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.3, There MUST NOT be more than one Media Playlist tag, \"#EXT-X-TARGETDURATION\" "
                      "or EXT3MU is absent or EXTINF is absent or Media playlist's tag is coming in master file");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_TARGET_DURATION;
    *flag |= NS_M3U8_TAG_TS_FILE;
  }
  else if(!strncmp(filled_line, "#EXT-X-MEDIA-SEQUENCE:", 22))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_M3U8_FILE = [%d], NS_M3U8_TAG_EXTINF = [%d], "
                          "NS_M3U8_TAG_MEDIA_SEQUENCE = [%d]", 
                           !(*flag & NS_M3U8_TAG_EXTM3U), (*flag & NS_M3U8_TAG_M3U8_FILE), (*flag & NS_M3U8_TAG_EXTINF), 
                           (*flag & NS_M3U8_TAG_MEDIA_SEQUENCE));

    if(!(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_M3U8_FILE) || (*flag & NS_M3U8_TAG_EXTINF) || (*flag & NS_M3U8_TAG_MEDIA_SEQUENCE))
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.3, There MUST NOT be more than one Media Playlist tag, \"#EXT-X-MEDIA-SEQUENCE\" "
                      "or EXT3MU is absent or EXTINF is absent or Media playlist's tag is coming in master file");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_MEDIA_SEQUENCE;
    *flag |= NS_M3U8_TAG_TS_FILE;
  }
  else if(!strncmp(filled_line, "#EXT-X-DISCONTINUITY-SEQUENCE:", 30))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_M3U8_FILE = [%d], NS_M3U8_TAG_EXTINF = [%d], "
                          "NS_M3U8_TAG_DISCONTINUITY_SEQUENCE = [%d]", 
                           !(*flag & NS_M3U8_TAG_EXTM3U), (*flag & NS_M3U8_TAG_M3U8_FILE), (*flag & NS_M3U8_TAG_EXTINF), 
                           (*flag & NS_M3U8_TAG_DISCONTINUITY_SEQUENCE));

    if(!(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_M3U8_FILE) || (*flag & NS_M3U8_TAG_EXTINF) || (*flag & NS_M3U8_TAG_DISCONTINUITY_SEQUENCE))
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.3, There MUST NOT be more than one Media Playlist tag, \"#EXT-X-DISCONTINUITY-SEQUENCE\" "
                      "or EXT3MU is absent or EXTINF is absent or Media playlist's tag is coming in master file");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_DISCONTINUITY_SEQUENCE;
    *flag |= NS_M3U8_TAG_TS_FILE;
  }
  else if(!strncmp(filled_line, "#EXT-X-ENDLIST", 14))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_M3U8_FILE = [%d], NS_M3U8_TAG_ENDLIST = [%d]", 
                           !(*flag & NS_M3U8_TAG_EXTM3U), (*flag & NS_M3U8_TAG_M3U8_FILE), (*flag & NS_M3U8_TAG_ENDLIST));

    if(!(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_M3U8_FILE) || (*flag & NS_M3U8_TAG_ENDLIST))
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.3, There MUST NOT be more than one Media Playlist tag, \"#EXT-X-ENDLIST\" "
                      "or EXT3MU is absent or Media playlist's tag is coming in master file");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_ENDLIST;
    *flag |= NS_M3U8_TAG_TS_FILE;
    return NS_M3U8_ENDLIST;
  }
  else if(!strncmp(filled_line, "#EXT-X-PLAYLIST-TYPE:", 21))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_M3U8_FILE = [%d], NS_M3U8_TAG_EXTINF = [%d], "
                          "NS_M3U8_TAG_PLAYLIST_TYPE = [%d]", 
                           !(*flag & NS_M3U8_TAG_EXTM3U), (*flag & NS_M3U8_TAG_M3U8_FILE), (*flag & NS_M3U8_TAG_EXTINF), 
                           (*flag & NS_M3U8_TAG_PLAYLIST_TYPE));

    if(!(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_M3U8_FILE) || (*flag & NS_M3U8_TAG_EXTINF) || (*flag & NS_M3U8_TAG_PLAYLIST_TYPE))
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.3, There MUST NOT be more than one Media Playlist tag, \"#EXT-X-PLAYLIST-TYPE\" "
                      "or EXT3MU is absent or EXTINF is absent or Media playlist's tag is coming in master file");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_PLAYLIST_TYPE;
    *flag |= NS_M3U8_TAG_TS_FILE;
  }
  else if(!strncmp(filled_line, "#EXT-X-I-FRAMES-ONLY:", 21))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_M3U8_FILE = [%d], NS_M3U8_TAG_EXTINF = [%d], NS_M3U8_TAG_FRAMES_ONLY = [%d]",                           !(*flag & NS_M3U8_TAG_EXTM3U), (*flag & NS_M3U8_TAG_M3U8_FILE), (*flag & NS_M3U8_TAG_EXTINF),
                           (*flag & NS_M3U8_TAG_FRAMES_ONLY));

    if(!(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_M3U8_FILE) || (*flag & NS_M3U8_TAG_EXTINF) || (*flag & NS_M3U8_TAG_FRAMES_ONLY))
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.3, There MUST NOT be more than one Media Playlist tag, \"#EXT-X-I-FRAMES-ONLY\" "
                      "or EXT3MU is absent or EXTINF is absent or Media playlist's tag is coming in master file");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_FRAMES_ONLY;
    *flag |= NS_M3U8_TAG_TS_FILE;
  }
  else if(!strncmp(filled_line, "#EXT-X-ALLOW-CACHE:", 19))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_M3U8_FILE = [%d], NS_M3U8_TAG_EXTINF = [%d], NS_M3U8_TAG_ALLOW_CACHE = [%d]",
                           !(*flag & NS_M3U8_TAG_EXTM3U), (*flag & NS_M3U8_TAG_M3U8_FILE), (*flag & NS_M3U8_TAG_EXTINF), 
                           (*flag & NS_M3U8_TAG_ALLOW_CACHE));

    if( !(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_M3U8_FILE) || (*flag & NS_M3U8_TAG_EXTINF) || (*flag & NS_M3U8_TAG_ALLOW_CACHE))
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.3, There MUST NOT be more than one Media Playlist tag, \"#EXT-X-ALLOW-CACHE\" "
                      "or EXT3MU is absent or EXTINF is absent or Media playlist's tag is coming in master file");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_ALLOW_CACHE;
    *flag |= NS_M3U8_TAG_TS_FILE;
  }
  else if(!strncmp(filled_line, "#EXTINF:", 8))
  {
    NSDL4_HLS(NULL, NULL, "NS_M3U8_TAG_EXTM3U = [%d], NS_M3U8_TAG_M3U8_FILE = [%d], NS_M3U8_TAG_TARGET_DURATION = [%d]",
                           !(*flag & NS_M3U8_TAG_EXTM3U), (*flag & NS_M3U8_TAG_M3U8_FILE), !(*flag & NS_M3U8_TAG_TARGET_DURATION));

    if(!(*flag & NS_M3U8_TAG_EXTM3U) || (*flag & NS_M3U8_TAG_M3U8_FILE) || !(*flag & NS_M3U8_TAG_TARGET_DURATION))
    {
      sprintf(errMsg, "HLS RFC 8216 - 4.3.3.1, if #EXTINF is found then \"#EXT-X-TARGETDURATION\" is required "
                      "or EXT3MU is absent or TARGET_DURATION is absent or Media playlist's tag is coming in master file");
      return NS_M3U8_ERROR;
    }
    *flag |= NS_M3U8_TAG_EXTINF;
    *flag |= NS_M3U8_TAG_TS_FILE;
    return NS_M3U8_EXTINF;
  }
  else 
  {
    NSDL4_HLS(NULL, NULL, "HLS RFC 8216 - 4.1, Commented  Line MUST be ignored");
  }
  return NS_M3U8_SUCCESS; 
}

/*---------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse m3u8 uri and all ts files 
 *
 * Input     : buffer        - Response of M3U8 
 *             len           - len of Response
 *             num_embd_urls - Will filled by number of embedded urls in provided buffer 
 *             bandwidth     - Provided bandwidth 
 *
 * Output    : Provide uri based on bandwitdh
 *
 * ---------------------------------------------------------------------------------------------------------
 * Eg: Response of M3U8 URI (Master file)
 * #EXTM3U
 * #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=514000,RESOLUTION=640x360
 * http://10.10.30.58:8007/macys/rendition.m3u8
 * #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1136000,RESOLUTION=960x540
 * http://10.10.30.58:8007/macys/rendition.m3u8
 *----------------------------------------------------------------------------------------------------------
 * Eg: Response of selected URI from master file (Media Playlist file)
 * #EXTM3U
 * #EXT-X-TARGETDURATION:10
 * #EXTINF:11,
 * 24953835001_4005726991001_s-1.ts
 * #EXTINF:11,
 * http://10.10.30.58/macys/24953835001_4005726991001_s-2.ts
 * #EXT-X-ENDLIST
 *-----------------------------------------------------------------------------------------------------------*/
static EmbdUrlsProp* parse_m3u8(char *buffer, size_t len, int *num_urls, int bandwidth, char *errMsg)
{
  char *p_read, *p_begin, *p_end;   //Read line by line of m3u8 file
  char bw_matched = 0;              //bandwidth matched flag
  char filled_line[MAX_M3U8_BUF_LEN + 1] = "";
  int i = 0, count = 0, max = 0, j = 0;
  int fill_buf_len = 0;
  int total = 0, ret = 0;
  char *ptr1 = NULL;
  int flag_m3u8 = 0;  
  EmbdUrlsProp *embd_url = NULL;
  m3u8_data *m3u8_attr = NULL;
  playlist_data *playlist_attr = NULL;

  NSDL1_HLS(NULL, NULL, "Method called, buffer = [%s], len = [%d], bandwidth = [%d]", buffer, len, bandwidth);

  *num_urls = 0; //Reset num urls
  p_begin = buffer;
  p_end = p_begin + len;

  do
  {
    read_m3u8_line(p_begin, &p_read, len, filled_line, &fill_buf_len); 
    p_begin = p_read;

    NSDL3_HLS(NULL, NULL, "line = [%s], fill_buf_len = [%d]", filled_line, fill_buf_len);

    ret = validate_m3u8_tags(filled_line, &flag_m3u8, errMsg);
    if(ret == NS_M3U8_ERROR)
    {
      NSDL3_HLS(NULL, NULL, "%s", errMsg);
      NSTL1(NULL, NULL, "%s", errMsg);
      return NULL;
    }
    if(ret == NS_M3U8_STREAM_INF)
    { 
      if(embd_url == NULL)
        MY_MALLOC_AND_MEMSET(embd_url, sizeof(EmbdUrlsProp), "embd_url", -1); 
      //Allocate memory to m3u8_data structure

      //Create Table Entry for Attributes 
      create_m3u8_table_entry(&count, &total, &max, (char **)&m3u8_attr, sizeof(m3u8_data));

      //Parse Attributes
      ret = parse_Attributes(filled_line + 18, &m3u8_attr[count], errMsg);

      if(ret == NS_M3U8_ERROR)
      {
        NSTL1(NULL, NULL, "%s", errMsg);
        NSDL3_HLS(NULL, NULL, "%s", errMsg);
        return NULL;
      }

      //Read URI
      read_m3u8_line(p_begin, &p_read, (p_end - p_begin), filled_line, &fill_buf_len);
      p_begin = p_read;

      if (filled_line[0] == '#' || filled_line[0] == '\n' || filled_line[0] == '\0' || filled_line[0] == ' ')
      {
        NSTL1(NULL, NULL, "Error: Media playlist uri must not starts with [%s].", filled_line);
        return NULL;
      }
      strncpy(m3u8_attr[count].uri, filled_line, MAX_URI_LEN);
      NSDL2_HLS(NULL, NULL, "URI = [%s]", m3u8_attr[count].uri);
      j++;
    }
    else if(ret == NS_M3U8_EXTINF)
    {

      create_m3u8_table_entry(&count, &total, &max, (char **)&playlist_attr, sizeof(playlist_data));

      ptr1 = &filled_line[8];               //len of #EXTINF:

      playlist_attr[count].media_seg_duration = atof(ptr1);

      NSDL2_HLS(NULL, NULL, "Media_seg_duration = [%f]", playlist_attr[count].media_seg_duration);

      //For URI - http://10.10.30.58/macys/24953835001_4005726991001_s-1.ts
      read_m3u8_line(p_begin, &p_read, p_end - p_begin, filled_line, &fill_buf_len);
      p_begin = p_read;

      if (filled_line[0] == '#' || filled_line[0] == '\n' || filled_line[0] == '\0' || filled_line[0] == ' ')
      {
        NSTL1(NULL, NULL, "Error: Media playlist uri must not starts with [%s].", filled_line);
        return NULL;
      }
      strncpy(playlist_attr[count].uri, filled_line, MAX_URI_LEN);
      NSDL2_HLS(NULL, NULL, "URI found = [%s]", playlist_attr[count].uri);
      j++;   //for counter of num_urls
    }
    else if (ret == NS_M3U8_ENDLIST)
    {
      *num_urls = j;
      NSDL2_HLS(NULL, NULL, "#EXT-X-ENDLIST tag recieved, hence if there is more playlist it will be ignored, emd_url = [%d]", *num_urls);
    }
  }while(p_begin < p_end);

  //Set Embedded Url Count, if not set yet.
  if((*num_urls) == 0)
  {
    *num_urls = j;
    NSDL2_HLS(NULL, NULL, "Setting num_urls = [%d]", *num_urls);
  }

  if(!(*num_urls))
  {
    NSDL2_HLS(NULL, NULL, "Error: No Embedded url is found.");
    NSTL1(NULL, NULL, "Error: No Embedded url is found");
    return NULL;
  }

  if(flag_m3u8 & NS_M3U8_TAG_M3U8_FILE)
  {
    NSDL3_HLS(NULL, NULL, "Processing NS_M3U8_TAG_M3U8_FILE");

    //Sorting data according to bandwidth 
    qsort(m3u8_attr, *num_urls, sizeof(m3u8_data), sort_m3u8_data);

    //Note:- 1. Exact bandwidth matched then select the same 
    //       2. No Exact bandwidth matched
    //         2.1 if provided bandwidth is less then all available bandwidth then select the minimum bandwidth.
    //         2.2 if provided bandwidth is greater then all available bandwidth then select maximum bandwidth.
    //         2.3 if provided bandwidth lies in between available bandwidths then select immediate lower to provided bandwidth.

    for(i = 0; i < *num_urls; i++)
    {
      if(m3u8_attr[i].bandwidth == bandwidth)
      {
        bw_matched = 1;
        NSDL3_HLS(NULL, NULL, "Bandwidth Selected = [%d]", m3u8_attr[i].bandwidth);
        break;
      }
      else if(m3u8_attr[i].bandwidth > bandwidth)
      {
        NSDL3_HLS(NULL, NULL, "Immediate Higher Bandwidth = [%d]", m3u8_attr[i].bandwidth);
        break;
      }
    }

    MY_MALLOC_AND_MEMSET(embd_url, sizeof(EmbdUrlsProp), "embd_url", -1); 
    MY_MALLOC_AND_MEMSET(embd_url->embd_url, MAX_URI_LEN + 1, "embd_url->embd_url", -1);
    if (bw_matched || i == 0)
    {
      //Case 1, 2.1
      strncpy(embd_url->embd_url, m3u8_attr[i].uri, MAX_URI_LEN);
      embd_url->embd_type = 0;
    }
    else
    {
      //Case 2.2, 2.3 
      strncpy(embd_url->embd_url, m3u8_attr[i-1].uri, MAX_URI_LEN);
      embd_url->embd_type = 0;
    }
    *num_urls = 1; //Note: resetting this to 1 because we have to select only one url based on given bandwith
    //if we do not reset this variable we will break while accessing rest urls as we just malloc only one.

    return embd_url;

  }

  if(flag_m3u8 & NS_M3U8_TAG_TS_FILE) 
  {
    NSDL3_HLS(NULL, NULL, "Processing NS_M3U8_TAG_TS_FILE");

    //Fill embd_url data structure and then return it 
    MY_MALLOC_AND_MEMSET(embd_url, (*num_urls) * sizeof(EmbdUrlsProp), "embd_url", -1); 
    for(i = 0; i < *num_urls; i++)
    {
      MY_MALLOC_AND_MEMSET(embd_url[i].embd_url, MAX_URI_LEN + 1, "embd_url->embd_url", -1);
      strncpy(embd_url[i].embd_url, playlist_attr[i].uri, MAX_URI_LEN) ;
      embd_url[i].embd_type = 0;  
      embd_url[i].duration = playlist_attr[i].media_seg_duration;
    }

    return embd_url;
  }
  return NULL;
}

/*---------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function is called from ns_auto_fetch_embd.c, this will fetch url of m3u8 type.
 *             Based on bandwidth this will select uri of video files 
 *
 * Input     : buffer        - Response of M3U8 
 *             len           - len of Response
 *             num_embd_urls - Will filled by number of embedded urls in provided buffer
 *             bandwidth     - Provided bandwidth 
 *
 * Output    : Provide uri based on bandwidth
 *-----------------------------------------------------------------------------------------------------------*/
EmbdUrlsProp *get_embd_m3u8_url(char *buffer, unsigned int len, int *num_embd_urls, char *errMsg, int bandwidth)
{
  *num_embd_urls = 0;
  EmbdUrlsProp *embd_url = NULL;

  NSDL2_HLS(NULL, NULL, "Method called, buffer = [%s], len = [%d], bandwidth = [%d]", buffer, len, bandwidth); 

  embd_url = parse_m3u8(buffer, len, num_embd_urls, bandwidth, errMsg);

  if(embd_url != NULL) 
    NSDL2_HLS(NULL, NULL, "URI = [%s]", embd_url->embd_url); 
  else
  {
    *num_embd_urls = 0;
    NSDL2_HLS(NULL, NULL, "embd_url is NULL");
  }

  return embd_url;
}
