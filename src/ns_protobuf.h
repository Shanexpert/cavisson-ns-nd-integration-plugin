#ifndef NS_PROTOBUF_H
#define NS_PROTOBUF_H

#define NS_PB_MAX_ERROR_MSG_LEN         4096
#define NS_PB_MAX_PARAM_LEN             2048

//Script Parsing
#define NS_PROTBUF_REQ_FILE             0
#define NS_PROTBUF_RESP_FILE            1
#define NS_PROTOBUF_REQ_MESSAGE_TYPE    3
#define NS_PROTOBUF_RESP_MESSAGE_TYPE   4

#define NS_DECODED_BUFFER               4*1024*1024
#define NS_ENCODED_BUFFER               65000
typedef struct ProtobufUrlAttr
{
  //Request
  void *req_message;
  ns_bigbuf_t req_pb_file; 
  ns_bigbuf_t req_pb_msg_type; 
  short grpc_comp_type;
  //Response
  void *resp_message;
  ns_bigbuf_t resp_pb_file;
  ns_bigbuf_t resp_pb_msg_type; 
}ProtobufUrlAttr;

typedef struct ProtobufUrlAttr_Shr
{
  //Request
  void *req_message;
  short grpc_comp_type;
  //ns_bigbuf_t req_pb_file; 
  //ns_bigbuf_t req_pb_msg_type; 

  //Response
  void *resp_message;
  //ns_bigbuf_t resp_pb_file;
  //ns_bigbuf_t resp_pb_msg_type; 
}ProtobufUrlAttr_Shr;



extern int ns_protobuf_parse_urlAttr(char *attr_name, char *attr_val, char *flow_file, int url_idx,
                                     int sess_idx, int script_ln_no, int pb_attr_flag);

extern void *ns_create_protobuf_msgobj(char *proto_file, char *msg_type, char *script);
//extern int ns_protobuf_segment_line(int url_idx, StrEnt* segtable, char* line, int noparam_flag,
//                                    int line_number, int sess_idx, char *fname, char *err_msg, int err_msg_len);

extern int read_xml_file(char *xml_fname, char **in_xml, long *in_xml_len);
extern int parse_protobuf_encoded_segbuf(void *message, unsigned char *segbuf_ptr, unsigned char *out_buf, unsigned int obuf_size);
#endif
