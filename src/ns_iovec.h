#ifndef _NS_IOVEC_H
#define _NS_IOVEC_H

/* Structure 'NS_IO_VECTOR' contains all important data about IO vector */
typedef struct
{
  struct iovec *vector;     /* Array of allocated vectors.*/
  int *flags;             /* flags: this member will provide information about freeing the vector, 
                               size of this array must be equal to vector_size,
                               This has 1 to 1 mapping with 'vector' */
  int tot_size;             /* tot_size: provide total size of vectors or total number of vectors */
  int cur_idx;
}NSIOVector;

#define NS_INIT_IO_VECTOR_SIZE           10000 
#define NS_DELTA_IOVECTOR_SIZE           1000
#define NS_MAX_IO_VECTOR_SIZE            100000

#define NS_IOVEC_FREE_FLAG              	0x00000001 
#define NS_IOVEC_BODY_FLAG              	0x00000002
#define NS_IOVEC_HTTP2_FRAME_FLAG             	0x00000004

#define NS_FILL_IOVEC(ns_iovec, ptr, len)\
{\
  NSDL4_HTTP(NULL, NULL, "Filling Vector[%d] Len = %d, Value = %*.*s%s", (ns_iovec).cur_idx, len, len<1024?len:1024, len<1024?len:1024, ptr, len>1024?"...":"");\
  (ns_iovec).vector[(ns_iovec).cur_idx].iov_base = ptr;\
  (ns_iovec).vector[(ns_iovec).cur_idx].iov_len = len;\
  (ns_iovec).cur_idx++;\
}

#define NS_FILL_IOVEC_IDX(ns_iovec, ptr, len, idx)\
{\
  NSDL4_HTTP(NULL, NULL, "Filling Vector[%d] = %*.*s%s", idx, (len>1024?len:1024), (len>1024?len:1024), ptr, (len>1024?"...":""));\
  (ns_iovec).vector[(idx)].iov_base = ptr;\
  (ns_iovec).vector[(idx)].iov_len = len;\
}

#define NS_FILL_IOVEC_AND_MARK_FREE(ns_iovec, ptr, len)\
{\
  (ns_iovec).flags[(ns_iovec).cur_idx] |= NS_IOVEC_FREE_FLAG;\
  NS_FILL_IOVEC(ns_iovec, ptr, len);\
}

#define NS_FILL_IOVEC_AND_MARK_HTTP2_HEADER_IDX(ns_iovec, ptr, len, idx)\
{\
  (ns_iovec).flags[idx] |= NS_IOVEC_HTTP2_FRAME_FLAG;\
  NS_FILL_IOVEC_IDX(ns_iovec, ptr, len, idx);\
}

#define NS_FILL_IOVEC_AND_MARK_BODY(ns_iovec, ptr, len)\
{\
  (ns_iovec).flags[(ns_iovec).cur_idx] |= NS_IOVEC_BODY_FLAG;\
  NS_FILL_IOVEC(ns_iovec, ptr, len);\
}

#define NS_FILL_IOVEC_AND_MARK_FREE_BODY(ns_iovec, ptr,len)\
{\
  (ns_iovec).flags[(ns_iovec).cur_idx] |= (NS_IOVEC_FREE_FLAG|NS_IOVEC_BODY_FLAG);\
  NS_FILL_IOVEC(ns_iovec,ptr, len);\
}

#define NS_FILL_IOVEC_AND_MARK_FREE_IDX(ns_iovec, ptr, len, idx)\
{\
  (ns_iovec).flags[idx] |= NS_IOVEC_FREE_FLAG;\
  NS_FILL_IOVEC_IDX(ns_iovec, ptr, len, idx);\
}

#define NS_IS_IOVEC_BODY(ns_iovec, idx) ((ns_iovec).flags[idx] & NS_IOVEC_BODY_FLAG)

#define NS_IS_IOVEC_FREE(ns_iovec, idx) ((ns_iovec).flags[idx] & NS_IOVEC_FREE_FLAG)

#define NS_IS_HTTP2_FRAME(ns_iovec, idx) ((ns_iovec).flags[idx] & NS_IOVEC_HTTP2_FRAME_FLAG)

#define NS_FREE_IOVEC(ns_iovec, idx)\
{\
  if(NS_IS_IOVEC_FREE(ns_iovec, idx))\
  {\
    FREE_AND_MAKE_NULL((ns_iovec).vector[(idx)].iov_base, "ns_iovec[idx].iov_base", (idx));\
    (ns_iovec).vector[(idx)].iov_len = 0;\
    (ns_iovec).flags[(idx)] = 0;\
  }\
  else if(NS_IS_HTTP2_FRAME(ns_iovec, idx))\
  {\
    release_frame((data_frame_hdr *)((ns_iovec).vector[(idx)].iov_base));\
    (ns_iovec).flags[(idx)] = 0;\
  }\
}

#define NS_CHK_FILL_IOVEC_AND_MARK_FREE(ns_iovec, ptr, len, free_flag)\
{\
  if(free_flag)\
  {\
    NS_FILL_IOVEC_AND_MARK_FREE(ns_iovec, ptr, len);\
  }\
  else\
  {\
    NS_FILL_IOVEC(ns_iovec, ptr, len);\
  }\
}

#define NS_CHK_FILL_IOVEC_AND_MARK_FREE_BODY(ns_iovec, ptr, len, free_flag)\
{\
  if(free_flag)\
  {\
    NS_FILL_IOVEC_AND_MARK_FREE_BODY(ns_iovec, ptr, len);\
  }\
  else\
  {\
    NS_FILL_IOVEC_AND_MARK_BODY(ns_iovec, ptr, len);\
  }\
}

#define NS_CHK_AND_GROW_IOVEC(vptr, ns_iovec, tot_body_segs)\
{\
  int ret;\
  if((ns_iovec).cur_idx + tot_body_segs + io_vector_delta_size >= (ns_iovec).tot_size)\
    if((ret = grow_io_vector(vptr, &ns_iovec, tot_body_segs)) < 0)\
      return ret;\
}

#define NS_REALLOC_IOVEC(ns_iovec, req_size)\
{\
  MY_REALLOC_AND_MEMSET((ns_iovec).vector, ((req_size) * sizeof(struct iovec)), (((ns_iovec).tot_size) * sizeof(struct iovec)) ,"Reallocate io vector", 0);\
  MY_REALLOC_AND_MEMSET((ns_iovec).flags, ((req_size) * sizeof(int)), (((ns_iovec).tot_size) * sizeof(int)) ,"Reallocate flags", 0);\
  (ns_iovec).tot_size = req_size; \
}

#define NS_MALLOC_IOVEC(ns_iovec, req_size)\
{\
  MY_MALLOC_AND_MEMSET((ns_iovec).vector, (req_size * sizeof(struct iovec)), "Allocate io vector", 0);\
  MY_MALLOC_AND_MEMSET((ns_iovec).flags, (req_size * sizeof(int)), "Allocate io flag", 0);\
  (ns_iovec).tot_size = req_size; \
  (ns_iovec).cur_idx = 0;\
}

#define NS_RESET_IOVEC(ns_iovec)\
{\
  memset((ns_iovec).flags, 0, ((ns_iovec).cur_idx * sizeof(int)));\
  (ns_iovec).cur_idx = 0;\
}

#define NS_FREE_RESET_IOVEC(ns_iovec)\
{\
  for(int i = 0; i < (ns_iovec).cur_idx; i++)\
  {\
    NS_FREE_IOVEC(ns_iovec, i);\
  }\
  NS_RESET_IOVEC(ns_iovec);\
}

#define NS_GET_IOVEC_CUR_IDX(ns_iovec) (ns_iovec).cur_idx

#define NS_INC_IOVEC_CUR_IDX(ns_iovec) (ns_iovec).cur_idx++

/*bug 52092 macro defined to reduced inx by 1 */
#define NS_DEC_IOVEC_CUR_IDX(ns_iovec) (ns_iovec).cur_idx--

#define NS_GET_IOVEC_LEN(ns_data_iovec, index) (ns_data_iovec).vector[index].iov_len 
#define NS_GET_IOVEC_VAL(ns_data_iovec, index) (ns_data_iovec).vector[index].iov_base

extern char *null_iovec;

#define NULL_IOVEC_LEN 0 

#ifndef _NS_INIT_IOVEC_VARS
#define _NS_INIT_IOVEC_VARS
int io_vector_init_size;
int io_vector_delta_size;
int io_vector_max_size;
NSIOVector g_req_rep_io_vector;
NSIOVector g_scratch_io_vector;
#else
extern int io_vector_init_size;
extern int io_vector_delta_size;
extern int io_vector_max_size;
extern NSIOVector g_req_rep_io_vector;
extern NSIOVector g_scratch_io_vector;
#endif

struct VUser;
void init_io_vector();
int grow_io_vector(struct VUser *vptr, NSIOVector *ns_iovec, int vec_needed);

#endif
