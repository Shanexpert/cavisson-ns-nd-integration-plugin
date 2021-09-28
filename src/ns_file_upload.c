/******************************************************************************************************
 * Name                :  ns_file_upload.c
 * Purpose             :  Send content of a file on a server over HTTP protocol
 * Author              :  Devendar Jain
 * Intial version date :  23/07/2020
 * Last modification date:
*******************************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "nslb_alloc.h"

#include "ns_file_upload.h"
#include "ns_rbu.h"

NetstormAlert *g_ns_file_upload;

void ns_init_file_upload(int thread_pool_init_size, int thread_pool_max_size, int alert_queue_init_size, int alert_queue_max_size)
{
  NSDL2_HTTP(NULL, NULL, "Method called, thread_pool_init_size = %d, thread_pool_max_size = %d", thread_pool_init_size, thread_pool_max_size);
  if(!g_ns_file_upload)
    g_ns_file_upload = nslb_alert_init(thread_pool_init_size, thread_pool_max_size, alert_queue_init_size, alert_queue_max_size);

  return;
}

int ns_config_file_upload(char *server_ip, unsigned short server_port, char protocol, char *url,
                          unsigned short max_conn_retry, unsigned short retry_timer, int trace_fd)
{
  NSDL2_HTTP(NULL, NULL, "Method called, server_ip = %s, server_port = %d, protocol = %d, url = %s", server_ip, server_port, protocol, url);
  if(!g_ns_file_upload)
  {
    NSTL1(NULL, NULL, "File upload feature is not initialised");
    return -1;
  }

  if(!server_ip || !server_ip[0] || !url || !url[0])
  {
    NSTL1(NULL, NULL, "File upload configuration is invalid");
    return -1;
  }

  if(nslb_alert_config(g_ns_file_upload, server_ip, server_port, protocol, url, max_conn_retry, retry_timer, 0, -1/*trace_fd*/) < 0)
  {
    NSTL1(NULL, NULL, "Failed to configure file upload setting, Err: %s", nslb_get_error());
    return -1;
  }

  return 0;
}

int ns_file_upload_ex(char *file_name, char* content, char* content_type, unsigned int size)
{
  char hdr[1024 + 1];
  int ret = 0;
  
  NSDL2_HTTP(NULL, NULL, "Method called, file_name = %s, size = %d, content type = %s", file_name, size, content_type);

  sprintf(hdr, "Content-Type: %s\r\nX-Cav-Target: HPD\r\nX-Cav-File-Upload: %s", content_type, file_name);

  if((ret = nslb_alert_send(g_ns_file_upload, HTTP_METHOD_POST, hdr, content, size)) < 0)
  {
    NSTL1(NULL, NULL, "Failed to upload file = %s, Error: %s", file_name, nslb_get_error());
  }

  return ret;
}

int ns_file_upload(char *file_name, char* content, unsigned int size)
{
  return ns_file_upload_ex(file_name, content, NS_CONTENT_TYPE_TEXT, size); 
}

int ns_image_upload(char *file_name, char* content, unsigned int size)
{
  return ns_file_upload_ex(file_name, content, NS_CONTENT_TYPE_IMAGE, size); 
}

static char filter_str[1024];

static int filter_file(const struct dirent *a)
{
  char *ptr = (char*)a->d_name;

  if(strstr(ptr, filter_str))
    return 1;
  else
    return 0;
}

static int my_alpha_sort(const struct dirent **aa, const struct dirent **bb)
{
  int data1 , data2;
  NSDL1_RBU(NULL, NULL, "first string = %s, second string = %s", (*aa)->d_name, (*bb)->d_name);
  data1 = get_clip_id((char*)(*aa)->d_name);
  data2 = get_clip_id((char*)(*bb)->d_name);
  return(data1 - data2);
}

int ns_upload_clips(char *source_path, char *dest_path, char *filter)
{
  int i;

  NSDL3_RBU(NULL, NULL, "Method called, source_path = %s, filter = %s", source_path, filter);

  int num_snap_shots = 0; 
  struct dirent **snap_shot_dirent;
  char clip_name[512 + 1] = "";
  char cavtest_file_name[512 + 1] = "";

  sprintf(filter_str, "video_clip_%s", filter);
  num_snap_shots = scandir(source_path, &snap_shot_dirent, filter_file, my_alpha_sort);
  NSDL1_RBU(NULL, NULL, "Method Called, source path of snap_shot = %s, num_snap_shots = %d", source_path, num_snap_shots);

  if(num_snap_shots < 0 )
  {
    NSTL1_OUT(NULL, NULL, "Error: Failed to open snap_shots dir");
    return -1;
  }

  /* declare a variables */
  int fd;
  char *buffer = NULL;
  long numbytes = 0;
  int init_size = 1024;
 
  if(num_snap_shots)
  {
    NSDL3_RBU(NULL, NULL, "num_snap_shots = %d", num_snap_shots);
  
    /* grab sufficient memory for the buffer to hold the text */
    NSLB_MALLOC(buffer, 1024 + 1, "local buffer", -1, NULL);

    struct stat info;

    for(i = 0; i < num_snap_shots; i++)
    {
      NSDL4_RBU(NULL, NULL, "snap_shot_dirent[%d]->d_name  = %s.", i, snap_shot_dirent[i]->d_name);

      snprintf(clip_name, 512, "%s%s", source_path, snap_shot_dirent[i]->d_name);

      /* open an existing file for reading */
      if(stat(clip_name, &info) == 0)
      {
        /* Get the number of bytes */
        numbytes = info.st_size; 
        if(numbytes > init_size)
        {
          /* grab sufficient memory for the buffer to hold the text */
          NSLB_REALLOC(buffer, numbytes + 1, "local buffer", -1, NULL);
          init_size = numbytes;
        }

        fd = open(clip_name, O_RDONLY|O_CLOEXEC);
        if(read(fd, buffer, numbytes) > 0)
        {
          /*Upload file */
          snprintf(cavtest_file_name, 512, "%s/%s", dest_path, snap_shot_dirent[i]->d_name);
          
          ns_image_upload(cavtest_file_name, buffer, numbytes);
        }
        else
        {
          NSTL1(NULL, NULL, "Failed to read, clip_name = %s", clip_name);
        }
        close(fd); 
      }
    }

    /* free the memory we used for the buffer */
    free(buffer);
  }
  return 0;
}
