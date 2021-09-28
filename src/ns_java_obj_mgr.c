#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<strings.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include <sys/un.h>
#include <errno.h>

#include "ns_script_parse.h"
#include "util.h"
#include "ns_child_thread_util.h"
#include "ns_msg_com_util.h"

#define LOOPBACK_IP "127.0.0.1"
#define CON_TIMEOUT 120 
#define REQ_OPCODE  1
#define RESP_OPCODE 2


#define COPY_BYTES(ptr, data, len, total_len)\
              bcopy(data, ptr, len);\
              ptr += len;\
              total_len += len


/*#define COPY_INTEGER(ptr, bytes, len)\
               char temp[4];\
               int b32 = (bytes >> 24) & 0x000000FF;\
               int b24 = (bytes >> 16) & 0x000000FF;\
               int b16 = (bytes >> 8) & 0x000000FF;\
               int b8 = bytes & 0x000000FF;\
               temp[0] = b32; temp[1] = b24; temp[2] = b16, temp[3] = b8;\
               bcopy(temp, ptr, sizeof(bytes)); \
               *len += sizeof(bytes);\
               ptr += sizeof(bytes);\
               bytes = 0;
*/
//char *send_buff = NULL;
//int total_len = 0;
static Msg_com_con mccptr;

void init_java_obj_mgr_con(VUser *vptr, int port){
  NSDL2_PARSING(NULL, NULL, "Method called port = [%d]", port);

  int fd = -1; 
  char err_msg[2048];

  
 // memset(mccptr, 0, sizeof(Msg_com_con));

  fd = nslb_tcp_client_ex(LOOPBACK_IP, port, CON_TIMEOUT, err_msg); 

  if(fd == -1){
    fprintf(stderr, "Failed to make connection with java object manager at port %d, Error = %s\n", port, err_msg);
    end_test_run();
  }else{
    NSDL2_PARSING(NULL, NULL, "Method called , connection fd = [%d]", fd);
    mccptr.fd = fd;
  }
}

int create_java_obj_msg(int src_type, char *src_ptr, char *out, int *len, int *out_len, int opcode){

   NSDL2_PARSING(NULL, NULL, "Method called , opcode = [%d] len = [%d]", opcode, *len);

   char *buff_ptr = out;
   int msg_len = *len + 8;
   int total_len = 0;
   int tmp_len = *len;
   
   //MY_MALLOC(send_buff, len + 8/* msg size(4 bytes)+opcode(4 bytes) */ + 4/*data size */+ 1, "malloc'd for java obj mgr", -1);   
  
   msg_len = htonl(msg_len); 
   opcode = htonl(opcode); 
   tmp_len = htonl(tmp_len);
 
   COPY_BYTES(buff_ptr, &msg_len, 4, total_len); 
   COPY_BYTES(buff_ptr, &opcode, 4, total_len); 
   COPY_BYTES(buff_ptr, &tmp_len, 4, total_len); 
   COPY_BYTES(buff_ptr, src_ptr, *len, total_len); 

   NSDL2_PARSING(NULL, NULL, "total request len=[%d]", total_len);
   *out_len = total_len;
   return total_len;
}

int send_java_obj_mgr_data(char* data, int len, int stop){

  int written_bytes = 0;
  int total_bytes = 0;
  char *ptr = data;
  NSDL2_PARSING(NULL, NULL, "Method called  len = [%d], mccptr.fd = %d", len, mccptr.fd);

  while(total_bytes != len){ 

    if((written_bytes = write(mccptr.fd, ptr, len - total_bytes)) < 0){
       fprintf(stderr, "Failed write complete request to java object manager jvm fd=[%d], written_bytes = [%d], total_len = [%d],  , Error = %s\n", mccptr.fd, written_bytes, len, nslb_strerror(errno));
       if(stop)
       { 
	 end_test_run();
       }
       else 
         return -1;
    }
 
    ptr += written_bytes; 
    total_bytes += written_bytes;
            
    NSDL2_PARSING(NULL, NULL, "written_bytes = [%d] total_bytes = [%d]", written_bytes, total_bytes);
  }
    NSDL2_PARSING(NULL, NULL, "total_written bytes = [%d]", total_bytes);
  return 0;
}

int read_java_obj_msg(char *read_buffer, int *content_size, int stop){

#ifdef ENABLE_SSL
  /* See the comment below */ // size changed from 32768 to 65536 for optimization : Anuj 28/11/07
  char buf[65536 + 1];    /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#else
  char buf[4096 + 1];     /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#endif
  int bytes_handled = 0;
  int bytes_read = 0;
  int read_bytes = 0;
  //int *size_ptr = NULL;
  int size_done = 0;
  int req_len = 0;
  int size;

  NSDL2_PARSING(NULL, NULL, "Method called, mccptr.fd = %d", mccptr.fd);

  while (1) {
    if ( do_throttle )
      read_bytes = THROTTLE / 2;
    else
      read_bytes = sizeof(buf) - 1;

    NSDL2_PARSING(NULL, NULL, "read_bytes = %d ", read_bytes);
    bytes_read = read(mccptr.fd, buf, read_bytes);

    NSDL2_PARSING(NULL, NULL, "bytes_read = %d ", bytes_read);

    if ( bytes_read < 0 ) {
      if (errno == EAGAIN) {
        continue;
      } else {
        NSDL2_PARSING(NULL, NULL, "error occured = [%s]", nslb_strerror(errno));
        return -1;
      }
    } else if (bytes_read == 0) {
      close(mccptr.fd);
      return -1;
    }


    memcpy(read_buffer + bytes_handled, buf, bytes_read);

    bytes_handled += bytes_read; 
    NSDL2_PARSING(NULL, NULL, "bytes_handled = %d", bytes_handled);
      
    if(bytes_read >= 4 && !size_done){
      //size_ptr = (int*)buf;
      size = *((int*)buf);
      size = ntohl(size);
      read_bytes = size - bytes_read;
      req_len = 4 + size;
      size_done = 1;
      NSDL2_PARSING(NULL, NULL, "size = [%d] req_len = [%d]", size, req_len);
    }


    if( bytes_handled == req_len)
     break;
  }//end of while 
   
  memmove(read_buffer, (read_buffer+12), (req_len-12));
  NSDL4_HTTP(NULL, NULL, "Before encoding xml buffer = %p", read_buffer);
  NSDL2_PARSING(NULL, NULL, "ENDDDD");
  *content_size = (bytes_handled - 12);
  return 0;
   
}
