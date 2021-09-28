#ifndef NS_H2_FRAMES_H
#define NS_H2_FRAMES_H


#define DATA_FRAME            0x0
#define HEADER_FRAME          0x1
#define PRIORITY_FRAME        0x2 
#define RESET_FRAME           0x3
#define SETTINGS_FRAME        0x4
#define PUSH_PROMISE          0x5
#define PING_FRAME            0x6
#define GOAWAY_FRAME          0x7
#define WINDOW_UPDATE_FRAME   0x8
#define CONTINUATION_FRAME    0x9
#define INVALID_FRAME         0xd // Invalid frame

#define NO_ERROR              0x0
#define PROTOCOL_ERROR        0x1
#define INTERNAL_ERROR        0x2
#define FLOW_CONTROL_ERROR    0x3
#define SETTINGS_TIMEOUT      0x4
#define FRAME_SIZE_ERROR      0x6
#define REFUSED_STREAM        0x7
#define CANCEL_STREAM         0x8
#define COMPRESSION_ERROR     0x9
#define CONNECT_ERROR         0xa
#define HTTP_1_1_REQUIRED     0xd


//flags 

#define FLAG_NONE   0x00
#define END_STREAM  0x01
#define END_HEADER  0x04
#define PADDING     0x08
#define PRIORITY    0x20 

// Settings Frame IDENTIFIER 
 
#define SETTINGS_HEADER_TABLE_SIZE           0x1 
#define SETTINGS_ENABLE_PUSH                 0x2
#define SETTINGS_MAX_CONCURRENT_STREAMS      0x3
#define SETTINGS_INITIAL_WINDOW_SIZE         0x4 
#define SETTINGS_MAX_FRAME_SIZE              0x5  
#define SETTINGS_MAX_HEADER_LIST_SIZE        0x6

//Frame Length 

#define FRAME_LENGTH 3 
#define UINT_LENGTH  4
#define UCHAR_LENGTH  1 

#define SETTINGS_FRAME_LEN 24 
#define WINDOW_UPDATE_LEN 4
#define RESET_FRAME_LEN 4
				
/*bug 84661 : added HEADER_FRAME_SIZE MACRO*/
#define HEADER_FRAME_SIZE 9
// Flag bits 
#ifndef Settings
#define Settings
#define SETTINGS_ACK_SET   0 
#endif
#define END_STREAM_SET     0
#define END_HEADER_SET     2
#define PADDING_SET        3
#define PRIORITY_SET       5 

#define PING_FRAME_LENGTH  8

#define NS_HTTP2_INITIAL_FRAME_SIZE 16384
#define PARTIAL_CONT_BUF_INIT_SIZE NS_HTTP2_INITIAL_FRAME_SIZE
#define HASH_ARRAY_INIT_SIZE 256
#define PROMISE_BUF_DELTA 30 // Max thirty resources will be pushed 
#define NUM_HEADERS_IN_PROMISE_BUF 50
#define NUM_HEADERS_IN_PROMISE_BUF_DELTA 10
#define MAX_HEADER_NAME_LEN 40
#define MAX_HEADER_VALUE_LEN 300
#define SERVER_PUSH_DATA_DELTA 1048576
/* Macro for checking whether bit is set or not */
#define CHECK_FLAG(val, bit_no) (((val) >> (bit_no)) & 1)

/* NISHI :: FRame_header pool for data frame */
#define INIT_DFRAME_BUFFER   2048
#define INIT_WINDOW_UPDATE_SIZE 1073741824
#define INIT_STREAM_WINDOW_UPDATE_SIZE 1073741824   
#define MAX_WINDOW_UPDATE_SIZE 2147483647
#define NS_HTTP2_WINDOW_UPDATE_FRAME_LEN 64

typedef struct Http2SettingsFrames{

 unsigned int settings_header_table_size;
 unsigned int settings_enable_push; 
 unsigned int settings_max_concurrent_streams;
 unsigned int settings_map_size;
 unsigned int settings_initial_window_size;
 unsigned int settings_max_frame_size;
 unsigned int settings_max_header_list_size;

}Http2SettingsFrames;

typedef struct {
  unsigned char fr_header[9];
  struct data_frame_hdr* next;
} data_frame_hdr;

extern data_frame_hdr *frame_hdr;
extern data_frame_hdr* get_free_data_frame();
 

// MACRO FOR MEMCPY
#define MY_MEMCPY(destination, source, len)  \
{                                            \
  int count = 0;                             \
  char *dest = (char *)destination;          \
  char *src = (char *)source;                \
  while(count != len)                        \
  {                                          \
    *dest++ = *src++;                        \
    count++;                                 \
  }                                          \
}

#endif
