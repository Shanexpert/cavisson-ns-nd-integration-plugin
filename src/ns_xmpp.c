/************************************************************************************
 * Name      : ns_xmpp.c 
 * Purpose   : This file contains functions related to XMPP Protocol 
 * Author(s) : Atul Kumar Sharma/Devendar Jain
 * Date      : 14 June 2018 
 * Copyright : (c) Cavisson Systems
 * Modification History :
 ***********************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ntlm.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/hmac.h>

#include "url.h"
#include "util.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "netstorm.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_process_resp.h"
#include "ns_log_req_rep.h"
#include "ns_string.h"
#include "ns_url_req.h"
#include "ns_vuser_tasks.h"
#include "ns_http_script_parse.h"
#include "ns_page_dump.h"
#include "ns_vuser_thread.h" 
#include "nslb_encode_decode.h"
#include "nslb_util.h"
#include "ns_sock_com.h"
#include "ns_debug_trace.h"
#include "ns_trace_level.h"
#include "ns_group_data.h"
#include "ns_static_files.h"
#include "ns_dynamic_hosts.h"
#include "ns_xmpp.h"
#include "nslb_ssl_lib.h"

static __thread char g_xmpp_send_buffer[XMPP_SEND_BUF_SIZE + 1] = "";
static __thread char g_xmpp_read_buffer[XMPP_BUF_SIZE_XL + 1] = "";
static __thread int g_xmpp_send_buffer_len=0;
static __thread int g_xmpp_read_buffer_len=0;
static __thread SegTableEntry_Shr *xmpp_hdrs_seg_start = NULL;

//NormObjKey xmpp_group_norm_tbl;
//ns_bigbuf_t *g_xmpp_group_info = NULL;
//static int g_xmpp_group_info_total = 0;
//static int g_xmpp_group_info_max = 0;

typedef struct
{
  char name[15];
  char len;
  char quote;
}offset;

enum challange_option
{
  realm,
  nonce,
  charset,
  algorithm,
  qop,
  max_challange_fields
};

//realm="cataclysm.cx",nonce="OA6MG9tEQGm2hh",qop="auth",charset=utf-8,algorithm=md5-sess
static offset challange_option_str[] =
{
  {"realm",     5,	1},
  {"nonce",     5,	1},
  {"charset",   7,	0},
  {"algorithm", 9,	0},
  {"qop",       3,	1}
};

/*****************************************************************************
  Below is the finite state machine which will handle all state of xmpp STREAM init, TLS, SASL Negotiation
  and BIND 
Description : 

 ******************************************************************************/

enum xmpp_states
{
  XMPP_START_STREAM_INIT,
  XMPP_START_STREAM_DONE,
  XMPP_START_TLS_INIT,
  XMPP_START_SASL_INIT,
  XMPP_START_SASL_DONE,
  XMPP_BIND_INIT,
  XMPP_BIND_SESSION_INIT,
  XMPP_CONNECTED,
  XMPP_DISCONNECTED,
  XMPP_MAX_STATE
};

enum xmpp_inputs
{
  XMPP_START_STREAM,
  XMPP_START_TLS,
  XMPP_START_TLS_PROCEED,
  XMPP_START_SASL,
  XMPP_START_SASL_AUTH,
  XMPP_BIND,
  XMPP_BIND_SESSION,
  XMPP_MESSAGE,
  XMPP_NOTIFICATION,
  XMPP_SUCCESS,
  XMPP_GROUP_CHAT,
  XMPP_GROUP_NAME,
  XMPP_CREATE_GROUP,
  XMPP_ACCEPT_CONTACT,
  XMPP_IQ_RESULT,
  XMPP_IQ_RESULT_SUBSCRIBE,
  XMPP_IQ_SUBSCRIBED,
  XMPP_IQ_MUC_OWNER,
  XMPP_IQ_MUC_CONFIG,
  XMPP_ADD_MEMBER,
  XMPP_HTTP_UPLOAD,
  XMPP_UPLOAD_FILE_SLOT,
  XMPP_FAILURE,
  XMPP_MAX_INPUT
};

enum xmpp_actions
{
  ACT_START_STEAM,
  ACT_START_TLS,
  ACT_START_TLS_PROCEED,
  ACT_START_SASL,
  ACT_START_SASL_AUTH,
  ACT_DO_BIND,
  ACT_BIND_SESSION,
  ACT_BIND_SUCCESS,
  ACT_MESSAGE,
  ACT_NOTIFICATION,
  ACT_SUCCESS,
  ACT_GROUP_CHAT,
  ACT_GROUP_NAME,
  ACT_CREATE_GROUP,
  ACT_ACCEPT_CONTACT,
  ACT_IQ_RESULT,
  ACT_IQ_RESULT_SUBSCRIBE,
  ACT_IQ_SUBSCRIBED,
  ACT_IQ_MUC_OWNER,
  ACT_IQ_MUC_CONFIG,
  ACT_ADD_MEMBER,
  ACT_HTTP_FILE_SLOT,
  ACT_HTTP_FILE_UPLOAD,
  ACT_CLOSE,
  ACT_INVALID,
  ACT_ERROR,
  ACT_NONE,
  XMPP_MAX_ACTION
};

typedef struct xmpp_state_machine_object
{
  int action;
  int state;
}xmpp_state_machine_object;

xmpp_state_machine_object xmpp_state_machine[XMPP_MAX_STATE][XMPP_MAX_INPUT] = {

  /* State XMPP_START_STREAM_INIT*/
  {
    //Action           State                            Input 
    { ACT_START_STEAM, XMPP_START_STREAM_DONE },       /*XMPP_START_STREAM */ 
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_START_TLS*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_START_TLS_PROCEED*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_START_SASL*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_START_SASL_AUTH*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_BIND*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_BIND_SESSION*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_MESSAGE*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_NOTIFICATION*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_SUCEESS*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_GROUP_CHAT*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_GROUP_NAME*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_CREATE_GROUP*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_ACCEPT_CONTACT*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_IQ_RESULT*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_IQ_RESULT_SUBSCRIBE*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_IQ_SUBSCRIBE*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_IQ_MUC_OWNER*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_IQ_MUC_CONFIG*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_ADD_MEMBER*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_HTTP_UPLOAD*/
    { ACT_INVALID,     XMPP_DISCONNECTED },            /*XMPP_UPLOAD_FILE_SLOT*/
    { ACT_INVALID,     XMPP_DISCONNECTED }             /*XMPP_FAILURE*/
  },

  /* State XMPP_START_STREAM_DONE*/
  {
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_START_STREAM */ 
    { ACT_START_TLS,  XMPP_START_TLS_INIT    },      /*XMPP_START_TLS*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_START_TLS_PROCEED*/
    { ACT_START_SASL, XMPP_START_SASL_INIT   },      /*XMPP_START_SASL*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_START_SASL_AUTH*/
    { ACT_DO_BIND,    XMPP_BIND_INIT         },      /*XMPP_BIND*/
    { ACT_DO_BIND,    XMPP_BIND_SESSION_INIT },      /*XMPP_BIND_SESSION*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_MESSAGE*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_NOTIFICATION*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_SUCEESS*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_GROUP_CHAT*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_GROUP_NAME*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_CREATE_GROUP*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_ACCEPT_CONTACT*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_IQ_RESULT*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_IQ_RESULT_SUBSCRIBE*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_IQ_MUC_OWNER*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_IQ_MUC_CONFIG*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_ADD_MEMBER*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*  return -1;XMPP_HTTP_UPLOAD*/
    { ACT_CLOSE,      XMPP_DISCONNECTED      },      /*XMPP_UPLOAD_FILE_SLOT*/
    { ACT_ERROR,      XMPP_DISCONNECTED      }       /*XMPP_FAILURE*/
  },

  /* State XMPP_START_TLS_INIT*/
  {
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_START_STREAM */ 
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_START_TLS*/
    { ACT_START_TLS_PROCEED, XMPP_START_STREAM_INIT},  /*XMPP_START_TLS_PROCEED*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_START_SASL*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_START_SASL_AUTH*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_BIND*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_BIND_SESSION*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_MESSAGE*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_NOTIFICATION*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_SUCEESS*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_GROUP_CHAT*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_GROUP_NAME*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_CREATE_GROUP*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_ACCEPT_CONTACT*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_IQ_RESULT*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_IQ_RESULT_SUBSCRIBE*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_IQ_SUBSCRIBE*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_IQ_MUC_OWNER*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_IQ_MUC_CONFIG*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_ADD_MEMBER*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_HTTP_UPLOAD*/
    { ACT_CLOSE,             XMPP_DISCONNECTED     },  /*XMPP_UPLOAD_FILE_SLOT*/
    { ACT_ERROR,             XMPP_DISCONNECTED     }   /*XMPP_FAILURE*/
  },

  /* State XMPP_START_SASL_INIT*/
  {
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_START_STREAM */ 
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_START_TLS*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_START_TLS_PROCEED*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_START_SASL*/
    { ACT_START_SASL_AUTH, XMPP_START_SASL_DONE  },  /*XMPP_START_SASL_AUTH*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_BIND*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_BIND_SESSION*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_MESSAGE*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_NOTIFICATION*/
    { ACT_SUCCESS,         XMPP_START_STREAM_INIT},  /*XMPP_SUCEESS*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_GROUP_CHAT*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_GROUP_NAME*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_CREATE_GROUP*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_ACCEPT_CONTACT*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_IQ_RESULT*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_IQ_RESULT_SUBSCRIBE*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_IQ_SUBSCRIBE*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_IQ_MUC_OWNER*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_IQ_MUC_CONFIG*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_ADD_MEMBER*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_HTTP_UPLOAD*/
    { ACT_CLOSE,           XMPP_DISCONNECTED     },  /*XMPP_UPLOAD_FILE_SLOT*/
    { ACT_ERROR,           XMPP_DISCONNECTED     }   /*XMPP_FAILURE*/
  },

  /* State XMPP_START_SASL_DONE*/
  {
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_START_STREAM */ 
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_START_TLS*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_START_TLS_PROCEED*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_START_SASL*/
    { ACT_START_SASL_AUTH, XMPP_START_SASL_DONE  },  /*XMPP_START_SASL_AUTH*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_BIND*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_BIND_SESSION*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_MESSAGE*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_NOTIFICATION*/
    { ACT_SUCCESS, XMPP_START_STREAM_INIT  },        /*XMPP_SUCCESS*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_GROUP_CHAT*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_GROUP_NAME*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_CREATE_GROUP*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_ACCEPT_CONTACT*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_IQ_RESULT*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_IQ_RESULT_SUBSCRIBE*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_IQ_SUBSCRIBE*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_IQ_MUC_OWNER*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_IQ_MUC_CONFIG*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_ADD_MEMBER*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_HTTP_UPLOAD*/
    { ACT_CLOSE,   XMPP_DISCONNECTED       },        /*XMPP_UPLOAD_FILE_SLOT*/
    { ACT_ERROR,   XMPP_DISCONNECTED       }         /*XMPP_FAILURE*/
  },

  /* State XMPP_BIND_INIT*/
  {
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_STREAM */ 
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_TLS*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_TLS_PROCEED*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_SASL*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_SASL_AUTH*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_BIND*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_BIND_SESSION*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_MESSAGE*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_NOTIFICATION*/
    { ACT_BIND_SUCCESS, XMPP_CONNECTED     },        /*XMPP_SUCCESS*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_GROUP_CHAT*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_GROUP_NAME*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_CREATE_GROUP*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_ACCEPT_CONTACT*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_RESULT*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_RESULT_SUBSCRIBE*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_SUBSCRIBE*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_MUC_OWNER*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_MUC_CONFIG*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_ADD_MEMBER*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_HTTP_UPLOAD*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_UPLOAD_FILE_SLOT*/
    { ACT_ERROR,        XMPP_DISCONNECTED  }         /*XMPP_FAILURE*/
  },

  /* State XMPP_BIND_SESSION_INIT*/
  {
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_STREAM */ 
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_TLS*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_TLS_PROCEED*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_SASL*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_START_SASL_AUTH*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_BIND*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_BIND_SESSION*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_MESSAGE*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_NOTIFICATION*/
    { ACT_BIND_SESSION, XMPP_BIND_INIT     },        /*XMPP_SUCCESS*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_GROUP_CHAT*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_GROUP_NAME*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_CREATE_GROUP*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_ACCEPT_CONTACT*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_RESULT*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_RESULT_SUBSCRIBE*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_SUBSCRIBE*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_MUC_OWNER*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_IQ_MUC_CONFIG*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_ADD_MEMBER*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_HTTP_UPLOAD*/
    { ACT_CLOSE,        XMPP_DISCONNECTED  },        /*XMPP_UPLOAD_FILE_SLOT*/
    { ACT_ERROR,        XMPP_DISCONNECTED  }         /*XMPP_FAILURE*/
  },

  /*State XMPP_CONNECTED*/
  {
    { ACT_CLOSE,               XMPP_DISCONNECTED   },   /*XMPP_START_STREAM */ 
    { ACT_CLOSE,               XMPP_DISCONNECTED   },   /*XMPP_START_TLS*/
    { ACT_CLOSE,               XMPP_DISCONNECTED   },   /*XMPP_START_TLS_PROCEED*/
    { ACT_CLOSE,               XMPP_DISCONNECTED   },   /*XMPP_START_SASL*/
    { ACT_CLOSE,               XMPP_DISCONNECTED   },   /*XMPP_START_SASL_AUTH*/
    { ACT_CLOSE,               XMPP_DISCONNECTED   },   /*XMPP_BIND*/
    { ACT_CLOSE,               XMPP_DISCONNECTED   },   /*XMPP_BIND_SESSION*/
    { ACT_MESSAGE,             XMPP_CONNECTED      },   /*XMPP_MESSAGE*/
    { ACT_NOTIFICATION,        XMPP_CONNECTED      },   /*XMPP_NOTIFICATION*/
    { ACT_NONE,                XMPP_CONNECTED      },   /*XMPP_SUCCESS*/
    { ACT_GROUP_CHAT,          XMPP_CONNECTED      },   /*XMPP_GROUP_CHAT*/
    { ACT_GROUP_NAME,          XMPP_CONNECTED      },   /*XMPP_GROUP_NAME*/
    { ACT_CREATE_GROUP,        XMPP_CONNECTED      },   /*XMPP_CREATE_GROUP,*/
    { ACT_ACCEPT_CONTACT,      XMPP_CONNECTED      },   /*XMPP_ACCEPT_CONTACT*/
    { ACT_IQ_RESULT,           XMPP_CONNECTED      },   /*XMPP_IQ_RESULT*/
    { ACT_IQ_RESULT_SUBSCRIBE, XMPP_CONNECTED      },   /*XMPP_IQ_RESULT_SUBSCRIBE*/
    { ACT_IQ_SUBSCRIBED,       XMPP_CONNECTED      },   /*XMPP_IQ_SUBSCRIBE*/
    { ACT_IQ_MUC_OWNER,        XMPP_CONNECTED      },   /*XMPP_IQ_MUC_OWNER*/
    { ACT_IQ_MUC_CONFIG,       XMPP_CONNECTED      },   /*XMPP_IQ_MUC_CONFIG*/
    { ACT_ADD_MEMBER,          XMPP_CONNECTED      },   /*XMPP_ADD_MEMBER*/
    { ACT_HTTP_FILE_SLOT,      XMPP_CONNECTED      },   /*XMPP_HTTP_UPLOAD*/
    { ACT_HTTP_FILE_UPLOAD,    XMPP_CONNECTED      },   /*XMPP_UPLOAD_FILE_SLOT*/
    { ACT_ERROR,               XMPP_DISCONNECTED   }    /*XMPP_FAILURE*/
  }
};

static inline void xmpp_copy_item_id(char *buffer, char* item, int len)
{
  char *tmp1, *tmp2;
  NSDL2_XMPP(NULL, NULL, "Method Called, buffer = %s, item = %s", buffer, item);
  do
  {
    if((tmp1 = strstr(buffer, "<item")))
    {
      tmp1 += 6;
      if ((tmp2 = strstr(tmp1, "</item>")) ||( tmp2 = strstr(tmp1, "/>")))
      {
         buffer = tmp2+1;
         *tmp2='\0';
      }
      else
      {
        buffer = tmp2;
      }
      NSDL2_XMPP(NULL, NULL, "Found <item, tmp1 = %s", tmp1 - 6);
      if((tmp2 = strstr(tmp1, "name=")))
      { 
        tmp2 += 6;
        if((tmp2 = strcasestr(tmp2, item)))
        { 
          NSDL2_XMPP(NULL, NULL, "Found item = %s, buffer = %s", item, tmp2);
          if((tmp1 = strstr(tmp1, "jid=")))
          {
            tmp1 += 5;
            tmp2 = tmp1;
            while(*tmp2)
            {
              if(*tmp2 == 39 || *tmp2 == '"')
              {
                NSDL2_XMPP(NULL, NULL, "tmp2  = %s tmp1 = %s", tmp2, tmp1);
                g_xmpp_read_buffer_len =  tmp2 - tmp1;
                strncpy(g_xmpp_read_buffer,tmp1,g_xmpp_read_buffer_len);
                g_xmpp_read_buffer[g_xmpp_read_buffer_len] = '\0';
                NSDL2_XMPP(NULL, NULL, "g_xmpp_read_buffer = %s", g_xmpp_read_buffer);
                buffer = NULL; 
                break;
              }
              tmp2++;
            };
          }
        }
      }
    }
    if(!buffer)
      break;
  }while(tmp1);
}

static int get_values_from_segments(connection *cptr, StrEnt_Shr* seg_tab_ptr, char *buffer, int buf_size)
{
  int i;
  int ret;
  int filled_len=0;
  int avail_size;
  VUser *vptr = cptr->vptr;

  NSDL2_XMPP(vptr, cptr, "Method Called");
  // Get all segment values in a vector
  // Note that some segment may be parameterized
  if ((ret = insert_segments(vptr, cptr, seg_tab_ptr, &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
  {
     NSDL2_XMPP(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error in insert_segments()");
     if(ret == -2)
       return ret;
     return(-1);
  }
  for(i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    avail_size = buf_size - filled_len;
    if (g_scratch_io_vector.vector[i].iov_len < avail_size)
    {
      strncpy(buffer + filled_len, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len);
      filled_len += g_scratch_io_vector.vector[i].iov_len;
    }
    else
    {  
       strncpy(buffer + filled_len, g_scratch_io_vector.vector[i].iov_base, avail_size);
       filled_len += avail_size - 1;
       break;
    }
  }
  buffer[filled_len] = 0;
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
  NSDL2_XMPP(vptr, cptr, "segment value = [%s]", buffer);
  return  filled_len;
}
/*
int xmpp_insert_group_info(connection *cptr, char *group_jid, int group_jid_len)
{
  char group[XMPP_BUF_SIZE_XS + 1];
  int len;
  int norm_id;
  int is_new=0;
  static int xmpp_group_norm_tbl_init=0;
  IW_UNUSED(VUser *vptr = cptr->vptr);
  NSDL2_XMPP(vptr, cptr, "Method Called");
  if(!xmpp_group_norm_tbl_init)
  {
    nslb_init_norm_id_table_ex(&xmpp_group_norm_tbl, 1024);
    xmpp_group_norm_tbl_init = 1;
  }
  if(g_xmpp_group_info_total == g_xmpp_group_info_max)
  {
     g_xmpp_group_info_max += 16;
     MY_REALLOC(g_xmpp_group_info, g_xmpp_group_info_max * sizeof(ns_bigbuf_t),"g_xmpp_group_info",-1);
  }
  len = get_values_from_segments(cptr,&cptr->url_num->proto.xmpp.group, group, XMPP_BUF_SIZE_XS);
  NSDL2_XMPP(vptr, cptr, "segment value = [%s], len = %d", group, len);
  norm_id = nslb_get_or_set_norm_id(&xmpp_group_norm_tbl, group, len, &is_new);
  if(is_new)
  {
    NSDL2_XMPP(vptr, cptr, "Insert Group");
    g_xmpp_group_info[norm_id] = copy_into_big_buf(group_jid, group_jid_len);
    g_xmpp_group_info_total++;
  }
  return 0;
}

char* xmpp_get_group_info(char *name, int len)
{
  int norm_id;
  NSDL2_XMPP(NULL, NULL, "Method Called group name = %s, length = %d",name,len);
  norm_id = nslb_get_norm_id(&xmpp_group_norm_tbl, name, len);
  if(norm_id < 0)
  {
    //Invalid Group
    NSDL2_XMPP(NULL, NULL, "%s not found",name);
    return NULL;
  }
  return RETRIEVE_BUFFER_DATA(g_xmpp_group_info[norm_id]);
   
}
*/
//char *group_jid = xmpp_get_group_info(recipient, recipient_len);

static inline void xmpp_get_msg_attr(char *buffer, char *name, int len, char *outbuf, int outbuf_size)
{
  char *ptr2, *ptr;
  int data_size = 0;
  NSDL2_XMPP(NULL, NULL, "Method called buffer= '%s' name= '%s'",buffer,name);

  if((ptr = strstr(buffer, name)))
  {
    ptr += len + 1;
    ptr2 = ptr;
    while(*ptr2)
    {
      if(*ptr2 == 39 || *ptr2 == '"')
      { 
        data_size = ptr2 - ptr;
        break;
      }
      ptr2++;  
    };
  }
  if(data_size > outbuf_size)
    data_size = outbuf_size;
  strncpy(outbuf, ptr, data_size);
  outbuf[data_size]='\0';
  NSDL2_XMPP(NULL, NULL, "Out buffer = '%s'", outbuf);
}

int execute(connection *cptr, int input, u_ns_ts_t now)
{
  int state = cptr->proto_state;
  VUser *vptr = cptr->vptr;
  xmpp_state_machine_object *obj;
  NSDL2_XMPP(vptr,cptr, "Method called. input = %d, state = %d", input, state);
  
  if (input > XMPP_MAX_INPUT || state > XMPP_MAX_STATE) 
    return -1;

  obj = &xmpp_state_machine[state][input];
  cptr->proto_state = obj->state;
  vptr->xmpp->last_action = obj->action;
  switch(obj->action)
  {
    case ACT_START_STEAM:
      break;  
    case ACT_START_TLS:
      xmpp_start_tls(cptr, now);
      break;  
    case ACT_START_TLS_PROCEED:
      xmpp_tls_proceed(cptr, now);
      break;  
    case ACT_START_SASL:
      xmpp_sasl_auth(cptr, now);
      break;  
    case ACT_START_SASL_AUTH:
      xmpp_sasl_resp(cptr, now);
      break;  
    case ACT_DO_BIND:
      xmpp_do_bind(cptr, now);
      break;  
    case ACT_MESSAGE:
      xmpp_process_message(cptr, now);
      break;  
    //case ACT_HTTP_FILE_SLOT:
      //xmpp_send_upload_file_slot(cptr, now);
    //  break;
    case ACT_GROUP_NAME:
      if(g_xmpp_read_buffer_len)
      {
        if(cptr->url_num->proto.xmpp.action == NS_XMPP_JOIN_GROUP)
          vptr->xmpp_status = xmpp_join_group(cptr, now);
        else
          vptr->xmpp_status = xmpp_delete_group(cptr, now);
      }
      else
        vptr->xmpp_status = -1;
      do_xmpp_complete(vptr);
      break;
    //case ACT_CREATE_GROUP:
    //    xmpp_create_group(cptr, now);
    //  break;

    case ACT_HTTP_FILE_UPLOAD:
      xmpp_http_file_upload(cptr, now);
      break;
    case ACT_NOTIFICATION:
      //Increase Message Delivered Counters Here 
      XMPP_DATA_INTO_AVG(xmpp_msg_dlvrd);
      NSDL2_XMPP(vptr, cptr, "xmpp_msg_dlvrd = %d", xmpp_avgtime->xmpp_msg_dlvrd);
      break;
    case ACT_SUCCESS:
      xmpp_start_stream(cptr, now);
      break;
    case ACT_BIND_SESSION:
      xmpp_resource_bind(cptr);
      xmpp_start_session(cptr,now);
      break;
    case ACT_BIND_SUCCESS:
      xmpp_resource_bind(cptr);
      vptr->xmpp_status = xmpp_send_presence(cptr, now);
      do_xmpp_complete(vptr);
      break;
    case ACT_ACCEPT_CONTACT: 
      xmpp_accept_contact(cptr,now);
      break;
    case ACT_IQ_RESULT_SUBSCRIBE: 
      xmpp_send_subscribe(cptr,now); // Fall Through to next case to send query result
    case ACT_IQ_RESULT: 
      xmpp_send_result(cptr,now);
      break;
    case ACT_IQ_SUBSCRIBED: 
      xmpp_send_subscribed(cptr,now);
      xmpp_send_subscription(cptr,now);
      break;
   case ACT_IQ_MUC_OWNER:
      xmpp_send_muc_owner(cptr, now);
      break;
    case ACT_IQ_MUC_CONFIG:
      //vptr->xmpp_status = xmpp_send_muc_config(cptr, now);
      //do_xmpp_complete(vptr);
      xmpp_send_muc_config(cptr, now);
      break;
    case ACT_ADD_MEMBER:
      //xmpp_send_add_member(cptr, now);
      break;
    case ACT_ERROR: 
      vptr->xmpp_status = -1;
      do_xmpp_complete(vptr);
      break;     
    case ACT_CLOSE:
      // 1 = for close stream , 0 for not close connection
      vptr->xmpp_status = xmpp_close_disconnect(cptr, now, 1, 0);
      do_xmpp_complete(vptr);
      break;
    case ACT_INVALID:
      // 0 for not close stream 1 for close connection
      vptr->xmpp_status = xmpp_close_disconnect(cptr, now, 0, 1);
      do_xmpp_complete(vptr);
      break;
  
    case ACT_NONE: 
      NSDL2_XMPP(vptr, cptr, "Inside none case of Action.");
      if((cptr->url_num->proto.xmpp.action == NS_XMPP_ADD_CONTACT)
         ||(cptr->url_num->proto.xmpp.action == NS_XMPP_DELETE_CONTACT)
         ||(cptr->url_num->proto.xmpp.action == NS_XMPP_CREATE_GROUP)
         ||(cptr->url_num->proto.xmpp.action == NS_XMPP_DELETE_GROUP)
        )
      {
        vptr->xmpp_status = 0;
        do_xmpp_complete(vptr);
      }
 
      break;  
    default:
      NSDL2_XMPP(vptr, cptr, "Inside default case of Action.");
      return -1;
  }
  return 0;
}

static void xmpp_copy_partial_buffer(connection *cptr, char *buffer, int len)
{

  VUser *vptr = cptr->vptr;
  int req_size = vptr->xmpp->partial_buf_len + len;
  NSDL2_XMPP(vptr, cptr,"Method called.buffer = %s, len = %d, vptr->xmpp->partial_buf_len =%d", buffer, len, vptr->xmpp->partial_buf_len); 

  if(vptr->xmpp->partial_buf_size < req_size)
  {
    MY_REALLOC(vptr->xmpp->partial_buf, req_size + 1, "xmpp_partial_buf", -1);
    vptr->xmpp->partial_buf_size = req_size;
  }
  strncpy(&vptr->xmpp->partial_buf[vptr->xmpp->partial_buf_len], buffer,len);
  vptr->xmpp->partial_buf[req_size] = '\0';
  vptr->xmpp->partial_buf_len = req_size;
  NSDL2_XMPP(vptr, cptr,"partial: buffer = %s, len = %d", vptr->xmpp->partial_buf, vptr->xmpp->partial_buf_len); 
}

#define XMPP_PARSE_TAG(_buf, _tag, _size, _ret, _next)	\
{							\
  _ret = xmpp_parse_tag(_buf, _tag, _size, _next);	\
  if (_ret == -2)					\
  {							\
    xmpp_copy_partial_buffer(cptr, _buf, strlen(_buf));	\
    return -1;						\
  }							\
}

static int xmpp_parse_tag(char *buf, char *tag, int size, char **next)
{
  char *tmp = buf; 
  int multi_tag = 0;
  int found=0;
  NSDL2_XMPP(NULL, NULL,"Method called buf = %s, tag = %s, size = %d", buf, tag, size); 

  tmp += (size + 1); // 1 for '<'

  if(*tmp != ' ' && *tmp != '>')
  {
    return -1;
  }
  tmp++;
  while(*tmp)
  {
    if(*tmp == '<')
    {
      multi_tag = 1;
    }
    if(!multi_tag && !strncmp(tmp,"/>",2))
    {
      tmp += 2;
      if (*tmp)
      {
        *next =  tmp;
        *(tmp-1) = '\0';
      }
      found = 1;
      break;
    }
    if(multi_tag)
    {
      if(!strncmp(tmp,"</",2))
      { 
        tmp += 2;
        if (!strncmp(tmp,tag,size))
        {
          tmp += size;
          if(*tmp == '>')
          {
            tmp++;
            if(*tmp)
            {
              *next =  tmp;
              *(tmp-1) = '\0';
            }  
            found = 1;
            break;
          }
        }
      }
    }
    tmp++;    
  }
  if(!found) 
  {
    return -2;
  }
  return 0;
}

static int xmpp_parse_buffer(connection *cptr, char *buffer, char **next)
{

  char type = -1;
  int ret;
  char *ptr, *ptr2, *ptr3;
  char starttls=1;
  char process_again;
  char group[XMPP_BUF_SIZE_XS + 1];
  VUser *vptr = cptr->vptr;
  NSDL2_XMPP(vptr, cptr,"Method called"); 

  if(!buffer || !buffer[0])
    return -1;

  *next = NULL;
  g_xmpp_read_buffer_len = 0;
  g_xmpp_read_buffer[0] = '\0';
  do
  {
    process_again = 0; 
    
    if((ptr = strstr(buffer, "<stream:stream")))
    {
      type = XMPP_START_STREAM;
      *next = strstr(ptr, "<stream:features");
    }
    else if (!strncmp(buffer, "<stream:features", 16) || !strncmp(buffer, "<features", 9))
    {
      XMPP_PARSE_TAG(buffer,"stream:features", 15, ret, next);
      if(ret == -1)
        XMPP_PARSE_TAG(buffer,"features", 8, ret, next);
      if(!ret)
      {
        if(starttls && strstr(buffer, "<starttls"))
        {
          if(strstr(buffer, "required"))
          {
            if(!cptr->url_num->proto.xmpp.starttls)
              type =  XMPP_FAILURE; 
            else
              type = XMPP_START_TLS;
          }
          else
          {  
            if(cptr->url_num->proto.xmpp.starttls)
              type = XMPP_START_TLS;
            else
            {
              starttls=0;
              process_again = 1;
              continue;
            }
          }
        }
        else if((ptr = strstr(buffer, "<mechanisms")))
        {
          ptr += 11;
          type =  XMPP_START_SASL;
          if(strstr(ptr,"PLAIN"))
          {
            strcpy(g_xmpp_read_buffer, "PLAIN");
            g_xmpp_read_buffer_len = 5;
          }
          else if(strstr(ptr,"DIGEST-MD5"))
          {
            strcpy(g_xmpp_read_buffer, "DIGEST-MD5");
            g_xmpp_read_buffer_len = 10;
          }
          else if(strstr(ptr,"SCRAM-SHA-1"))
          {
            strcpy(g_xmpp_read_buffer, "SCRAM-SHA-1");
            g_xmpp_read_buffer_len = 11;
          }
          else
          {
            strcpy(g_xmpp_read_buffer, "PLAIN");
            g_xmpp_read_buffer_len = 5;
          }
        }
        else if((strstr(buffer, "<bind")))
        {
          if((ptr = strstr(buffer, "<jid"))) 
          {
            if((ptr = strchr(ptr, '>')))
            { 
              ptr++;
              if((ptr2 = strchr(ptr, '<')))
              {
                g_xmpp_read_buffer_len = ptr2 - ptr;
                g_xmpp_read_buffer[g_xmpp_read_buffer_len] = '\0';
              }
            }
          }
          type = XMPP_BIND;
          if((strstr(buffer, "<session")))
            type = XMPP_BIND_SESSION;
        }
        else
        {
          type = XMPP_FAILURE;
        }
      }
    }
    else if (!strncmp(buffer, "<proceed", 8))
    {
      type = XMPP_START_TLS_PROCEED;
    }
    else if (!strncmp(buffer, "<challenge", 10))
    {
      if((ptr = strchr(buffer, '>')))
      { 
        ptr++;
        if((ptr2 = strchr(ptr, '<')))
        {
          strncpy(g_xmpp_read_buffer,ptr,ptr2-ptr);
          g_xmpp_read_buffer_len = ptr2 - ptr;
          g_xmpp_read_buffer[g_xmpp_read_buffer_len] = '\0';
        }
      }
      type = XMPP_START_SASL_AUTH;
    }
    else if (!strncmp(buffer, "<message",8))
    {
      XMPP_PARSE_TAG(buffer,"message", 7, ret, next);
      if(!ret)
      {
        if((ptr = strstr(buffer,"type=" )))
        {
          ptr += 6;
          if(!strncmp(ptr,"error", 5)) 
          {
            type = XMPP_SUCCESS;
            break; 
          }
        }
        if ((ptr = strstr(buffer,"<invite" )))
        {
          type = XMPP_SUCCESS;
          break; 
        }
        type = XMPP_MESSAGE;
        if((ptr = strstr(buffer,"<request" )))
        {
          g_xmpp_read_buffer_len =  ptr - buffer;
          strncpy(g_xmpp_read_buffer,buffer, g_xmpp_read_buffer_len);
          g_xmpp_read_buffer[g_xmpp_read_buffer_len] = '\0';
        }
        else if((ptr = strstr(buffer,"<received" )))
        {
          type = XMPP_NOTIFICATION; 
        }
      } 
    }  
    else if (!strncmp(buffer, "<iq", 3))
    { 
      XMPP_PARSE_TAG(buffer,"iq", 2, ret, next);
      if(!ret)
      {
        if((ptr = strstr(buffer, "type=")))
        {
          ptr += 6; 
          if(!strncmp(ptr, "set",3))   
          {
            ptr +=5 ; //"set" + '"' + ' ';
            type = XMPP_IQ_RESULT;
            xmpp_get_msg_attr(buffer,"id=", 3, g_xmpp_read_buffer, XMPP_BUF_SIZE_XS);  
            g_xmpp_read_buffer_len = strlen(g_xmpp_read_buffer);
            if((ptr2 = strstr(ptr, "<query")))
            {
              ptr = ptr2 + 7;
              if((ptr2 = strstr(ptr, "roster")))
              {
                ptr = ptr2 + 8;
                if ((ptr2 = strstr(ptr, "ask")))
                  break;
                if ((ptr2 = strstr(ptr, "subscription")))
                {
                   ptr = ptr2 + 13;  
                   if ((ptr2 = strstr(ptr, "none")))
                   {
                    char tmp[XMPP_BUF_SIZE_M];
                    char tmp2[XMPP_BUF_SIZE_M];
                    strcpy(tmp, g_xmpp_read_buffer);
                    xmpp_get_msg_attr(buffer,"jid=", 4, tmp2, XMPP_BUF_SIZE_XS);  
                    g_xmpp_read_buffer_len = sprintf(g_xmpp_read_buffer,"<id='%s' from='%s'>",tmp ,tmp2); 
                    type = XMPP_IQ_RESULT_SUBSCRIBE; 
                   }  
                }   
              }
            }
          }
          else if (!strncmp(ptr, "result",6))
          {
            ptr +=  6;
            type = XMPP_SUCCESS;
            if((ptr2 = strstr(ptr, "<bind")))
            {
              ptr = ptr2 + 6;
              if((ptr = strstr(ptr, "<jid")))
              {
                if((ptr = strchr(ptr, '>')))
                { 
                  ptr++;
                  if((ptr2 = strchr(ptr, '<')))
                  {
                    //Copying full JID
                    g_xmpp_read_buffer_len = ptr2 - ptr;
                    strncpy(g_xmpp_read_buffer, ptr, g_xmpp_read_buffer_len);
                    g_xmpp_read_buffer[g_xmpp_read_buffer_len] = '\0';
                  }
                }
              }
            }
            else if ((ptr2 = strstr(ptr, "<slot")))
            {
              ptr = ptr2 + 5;  
              if((ptr2 = strstr(ptr, "<put url")))
              { 
                ptr2 += 10;
                if ((ptr3 = strchr(ptr2,'>')))
                {
                  ptr3 -= 2;
                  //Copying put url
                  g_xmpp_read_buffer_len = ptr3 - ptr2;
                  strncpy(g_xmpp_read_buffer, ptr2, g_xmpp_read_buffer_len);
                  g_xmpp_read_buffer[g_xmpp_read_buffer_len] = '\0';
                } 
              }
              if((ptr2 = strstr(ptr, "<get url")))
              { 
                ptr2 += 10;
                if((ptr3 = strchr(ptr2,'>')))  
                {
                  //Copying put url
                  ptr3 -= 2;
                  int len = ptr3 - ptr2;
                  if(len > vptr->xmpp->file_url_size) 
                  {
                     vptr->xmpp->file_url_size +=  len;
                     MY_REALLOC(vptr->xmpp->file_url, vptr->xmpp->file_url_size, "xmpp_file_url", -1)
                  }
                  strncpy(vptr->xmpp->file_url, ptr2, len);
                  vptr->xmpp->file_url[len] = '\0';
                } 
              }
              type = XMPP_UPLOAD_FILE_SLOT;
            }
            else if ((ptr2 = strstr(ptr, "<query")))
            {
              //Supporting HTTP File Upload type
              ptr = ptr2 + 6; //now buffer is pointing to value of iten jit
              if ((ptr2 = strstr(ptr, "muc#owner")))
              {
                type = XMPP_IQ_MUC_CONFIG;
              }
              else if((ptr2 = strstr(ptr, "roster")))
              {
                 xmpp_get_msg_attr(buffer,"id=", 3, g_xmpp_read_buffer, XMPP_BUF_SIZE_XS);
                 g_xmpp_read_buffer_len = strlen(g_xmpp_read_buffer);
                 type = XMPP_ACCEPT_CONTACT; 
              }
              else if((ptr2 = strstr(ptr, "disco#items")))
              {
                ptr = ptr2 + 12;
                /*
                if(cptr->url_num->proto.xmpp.file.seg_start)
                {
                  NSDL2_XMPP(vptr,cptr,"Inside File Upload processing Block");
                  xmpp_copy_item_id(ptr,"HTTP File Upload", 16);
                  if(g_xmpp_read_buffer_len)
                    type = XMPP_HTTP_UPLOAD;
                }
                else
                */ 
                if(cptr->url_num->proto.xmpp.group.seg_start)
                {
                  NSDL2_XMPP(vptr, cptr, "Inside group processing Block");
                  /*
                  xmpp_copy_item_id(ptr,"chatroom", 8);
                  if(g_xmpp_read_buffer_len)
                    type = XMPP_GROUP_CHAT;
                  else if (cptr->url_num->proto.xmpp.action == NS_XMPP_CREATE_GROUP)
                  {
                    xmpp_get_msg_attr(buffer,"from=", 5, g_xmpp_read_buffer, XMPP_BUF_SIZE_XS);
                    g_xmpp_read_buffer_len = strlen(g_xmpp_read_buffer);
                    type = XMPP_CREATE_GROUP;
                  }
                  else
                  */
                  {
                    int len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, group, XMPP_BUF_SIZE_XS);
                    xmpp_copy_item_id(ptr, group, len);
                    type = XMPP_GROUP_NAME;
                  }
                }
              }
            }
          }
          else if (!strncmp(ptr, "get", 3))
          {
            if ((ptr2 = strstr(ptr, "<ping")))
            {
              char tmp1[XMPP_BUF_SIZE_M];
              char tmp2[XMPP_BUF_SIZE_M];
              type = XMPP_IQ_RESULT;
              xmpp_get_msg_attr(buffer,"id=", 3, tmp1, XMPP_BUF_SIZE_XS);  
              xmpp_get_msg_attr(buffer,"from=", 5, tmp2, XMPP_BUF_SIZE_XS);  
              //we are filling buffer like this so that we can reuse xmpp_send_result() method 
              //buffer will be like "<id>' to='<to>" and will send as id='<id>' to='<to>'
              g_xmpp_read_buffer_len = sprintf(g_xmpp_read_buffer,"%s' to='%s", tmp1, tmp2); 
            }  
          }
          
          else if (!strncmp(ptr, "error", 5))
          {
             type = XMPP_FAILURE;
          }
        }
      }
    }  
    else if (!strncmp(buffer, "<presence", 9))
    {
      XMPP_PARSE_TAG(buffer,"presence", 8, ret, next);
      if(!ret)
      {
        NSDL4_XMPP(NULL , cptr, "buffer = %s", buffer);
        if((ptr = strstr(buffer, "type=")))
        {
          ptr +=  6;
          if(!strncmp(ptr,"subscribe",9))
          {
            xmpp_get_msg_attr(buffer,"from=", 5, g_xmpp_read_buffer, XMPP_BUF_SIZE_XS);  
            type = XMPP_IQ_SUBSCRIBED; 
          }
        }
        else if((ptr = strstr(buffer, "affiliation=")))
        {
          ptr += 13;
          if(!strncmp(ptr, "owner", 5))
          {
            if((cptr->url_num->proto.xmpp.action == NS_XMPP_CREATE_GROUP) && (vptr->xmpp->last_action != ACT_IQ_MUC_OWNER))
              type = XMPP_IQ_MUC_OWNER;
          }
        }
      }
    }
    else if (!strncmp(buffer, "<success", 8))
    {
      type = XMPP_SUCCESS; 
    }
    else if (!strncmp(buffer, "<failure", 8))
    {
      type = XMPP_FAILURE;
    }
    else if (!strncmp(buffer, "</stream:stream>", 16))
    {
      type = XMPP_FAILURE;
      cptr->proto_state = XMPP_DISCONNECTED;
    }
    else
    {
        //TODO Unhandled tag or uncomplete tag
    }
  } while(process_again);
  
  if(type == -1)
  {
    if (strstr(buffer,"<error"))
    {
      type = XMPP_FAILURE;
    }
  }
  NSDL2_XMPP(vptr ,cptr ,"type = %d", type);
  return type;
}

static inline void xmpp_build_start_stream(char *user, char* domain)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called user = %s, domain = %s", user, domain);
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
      "<?xml version='1.0'?>"
      "<stream:stream " 
      "from='%s@%s' "
      "to='%s' " 
      "version='1.0' "
      "xml:lang='en' "
      "xmlns='jabber:client' "
      "xmlns:stream='http://etherx.jabber.org/streams'>"
      , user ,domain, domain);
}

static inline void xmpp_build_item_discovery(char *from, char *id, char *to)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
               "<iq from='%s' "
               "id='%s' "
               "to='%s' "
               "type='get'>"
               "<query xmlns='http://jabber.org/protocol/disco#items'/>"
               "</iq>", 
               from, id, to);
}

static inline void xmpp_build_item_info(char *from, char *id, char *to)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
               "<iq from='%s' "
               "id='%s' "
               "to='%s' "
               "type='get'>"
               "<query xmlns='http://jabber.org/protocol/disco#info'/> "
               "</iq>", 
               from, id, to);
}

static inline void xmpp_build_presence_create_group(char *to ,char *nickname)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                      "<presence to='%s/%s'>"
                      "<priority>1</priority><c xmlns='http://jabber.org/protocol/caps' "
                      "node='http://pidgin.im/' hash='sha-1' ver='AcN1/PEN8nq7AHD+9jpxMV4U6YM=' "
                      "ext='voice-v1 camera-v1 video-v1'/>"
                      "<x xmlns='http://jabber.org/protocol/muc'/></presence>", 
                      to, nickname);
}

static inline void xmpp_build_join_group(char *from, char *id, char *to)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                          "<presence "
                          "from='%s' "
                          "id='%s' "
                          "to='%s'>",
                          from, id, to);

}

static inline void xmpp_build_delete_group(char *from, char *id, char *to)
{
  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                           "<iq from='%s' "
                           "id='%s' "
                           "to='%s' "
                           "type='set'>"
                           "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                           "<destroy/>"
                           "</query>"
                           "</iq>",
                           from, id, to);
                            
}

static inline void xmpp_build_add_contact(char *id, char *to, char *name, char *group)
{
  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  if(!group || !group[0])
    g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                 "<iq type='set' "
                 "id='%s'>"
                 "<query xmlns='jabber:iq:roster'>"
                 "<item jid='%s' name='%s'/>"
                 "</query>"
                 "</iq>",
                 id, to, name);
  else 
    g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                 "<iq type='set' "
                 "id='%s'>"
                 "<query xmlns='jabber:iq:roster'>"
                 "<item jid='%s' name='%s'>"
                 "<group>%s</group>"
                 "</item>"
                 "</query>"
                 "</iq>",
                 id, to, name, group);
}

static inline void xmpp_build_subscribe_contact(char *id, char *to, char *name)
{
  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                 "<iq type='set' "
                 "id='%s'>"
                 "<query xmlns='jabber:iq:roster'>"
                 "<item jid='%s' subscription='%s'/>"
                 "</query>"
                 "</iq>",
                 id, to, name);
}


static inline void xmpp_build_delete_contact(char *from, char *id, char *to)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                 "<iq from='%s' "
                 "id='%s' "
                 "type='set'>"
                 "<query xmlns='jabber:iq:roster' "
                 "<item jid='%s' subscription='remove'/>"
                 "</query>"
                 "</iq>",
                 from, id, to);
}

static inline void xmpp_build_accept_contact(char *from, char *id)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
               "<iq from='%s' "
               "id='%s' "
               "type='result'/>"
               "</iq>", 
               from, id);
}

static inline void xmpp_build_subscribe(char *to, char *type)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                    "<presence to='%s' "
                    "type='%s'/>",
                    to, type);
}

static inline void xmpp_build_result()
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
               "<iq type='result' "
               "id='%s'/>", 
               g_xmpp_read_buffer);
}
static inline void xmpp_build_iq_add_member(char *id, char *to )
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                           "<iq type='get' id='%s' "
                           "to='%s'>"
                           "<query xmlns='http://jabber.org/protocol/disco#info' "
                           "node='http://jabber.org/protocol/muc#traffic'/></iq>",
                           id, to);
}

static inline void xmpp_build_muc_owner(char *id, char *to)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                           "<iq type='get' id='%s' "
                            "to='%s'>"
                           "<query xmlns='http://jabber.org/protocol/muc#owner'/>"
                           "</iq>",
                           id, to);


}

static inline void xmpp_build_iq_muc_config(char *id, char *to, char *group, char *owner)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                           "<iq type='set' id='%s' to='%s'>"
                             "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                               "<x xmlns='jabber:x:data' type='submit'>"
                                  "<field var='FORM_TYPE'>"
                                    "<value>http://jabber.org/protocol/muc#roomconfig</value>"
                                  "</field>"
                                  "<field var='muc#roomconfig_roomname'><value>%s</value></field>"
                                  "<field var='muc#roomconfig_roomdesc'><value>%s</value></field>"
                                  "<field var='muc#roomconfig_changesubject'><value>0</value></field>"
                                  "<field var='muc#roomconfig_maxusers'><value>256</value></field>"
                                  "<field var='muc#roomconfig_presencebroadcast'>"
                                    "<value>moderator</value><value>participant</value><value>visitor</value>"
                                  "</field>"
                                  "<field var='muc#roomconfig_publicroom'><value>0</value></field>"
                                  "<field var='muc#roomconfig_persistentroom'><value>1</value></field>"
                                  "<field var='muc#roomconfig_moderatedroom'><value>0</value></field>"
                                  "<field var='muc#roomconfig_membersonly'><value>1</value></field>"
                           //       "<field var='muc#roomconfig_allowinvites'><value>0</value></field>"
                           //       "<field var='muc#roomconfig_passwordprotectedroom'><value>0</value></field>"
                           //       "<field var='muc#roomconfig_roomsecret'><value></value></field>"
                           //       "<field var='muc#roomconfig_whois'><value>anyone</value></field>"
                           //       "<field var='muc#roomconfig_allowpm'><value>anyone</value></field>"
                           //       "<field var='muc#roomconfig_enablelogging'><value>0</value></field>"
                           //       "<field var='x-muc#roomconfig_reservednick'><value>0</value></field>"
                           //       "<field var='x-muc#roomconfig_canchangenick'><value>1</value></field>"
                           //       "<field var='x-muc#roomconfig_registration'><value>1</value></field>"
                           //       "<field var='muc#roomconfig_roomadmins'/>"
                           //       "<field var='muc#roomconfig_roomowners'>"
                           //         "<value>%s</value>"
                           //       "</field>"
                               "</x>"
                             "</query>"
                           "</iq>",
                           id, to, group, group);


}
static inline void xmpp_build_message_add_member(char *to, char *member)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                          "<message to='%s'>"
                          "<x xmlns='http://jabber.org/protocol/muc#user'>"
                          "<invite to='%s'/>"
                          "</x></message>",
                           to, member);


}
static inline void xmpp_build_iq_delete_member(char *from, char *id, char *to, char *member)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
                          "<iq from='%s' "
                          "id='%s' "
                          "to='%s' "
                          "type='set'>"
                          "<query xmlns='http://jabber.org/protocol/muc#admin'>"
                          //"<item nick='%s' role='none'/>"
                          "<item affiliation='none' jid='%s'/>"
                          "</query>"
                          "</iq>",
                           from, id, to, member);
}

static inline void xmpp_build_iq_upload_file_slot(char *from, char *id, char *to, char *filename, int size, char *type)
{

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  g_xmpp_send_buffer_len = sprintf(g_xmpp_send_buffer,
        "<iq from='%s' "
        "id='%s' "
        "to='%s' "
        "type='get'>"
        "<request xmlns='urn:xmpp:http:upload:0' "
        "filename='%s' "
        "size='%d' "
        "content-type='%s'/>"
        "</iq>",
         from, id, to, filename, size, type);
}

static inline void gen_cnonce(unsigned char *cnonce)
{
   unsigned char buf[8]="";
   NSDL2_XMPP(NULL, NULL, "Method called");
   gen_nonce(buf, 8);           
   to64frombits(cnonce, buf, 8);
   cnonce[8] = '\0';
}

static inline void xmpp_build_sasl_auth(char *sasl_auth_type ,char *username, char *password)
{  
  unsigned char response[XMPP_BUF_SIZE_M] = "=";
  char payload[XMPP_BUF_SIZE_M];
  int payload_len;
  char cnonce[8 + 1]; 

  NSDL2_XMPP(NULL ,NULL ,"Method Called");
  /*our message is Base64(authzid,\0,authid,\0,password) */
  /* In case there is no authzid  that field can be empty */
  if(!strcmp(sasl_auth_type, "PLAIN"))
  {
    //making PLAIN init message
    payload_len = snprintf(payload, XMPP_BUF_SIZE_M, "%c%s%c%s",'\0', username,'\0',password); 
    nslb_encode_base64_ex((unsigned char *)payload, payload_len, response, XMPP_BUF_SIZE_M);
  }
  //In case of MD-5 we need to send "="
  else if (!strcmp(sasl_auth_type, "SCRAM-SHA-1"))
  {
    //making SCRAM-SHA-1 init message
    //n,,n=ram,r=a58287f886d56ef0f54042f7dd92ea1
    gen_cnonce((unsigned char *)cnonce);              /* 64-bit Client Nonce */
 
    NSDL2_XMPP(NULL ,NULL ,"cnonce = %s", cnonce);
    payload_len = snprintf(payload, XMPP_BUF_SIZE_M, "n,,n=%s,r=%s",username, cnonce); 
    nslb_encode_base64_ex((unsigned char *)payload, payload_len, response, XMPP_BUF_SIZE_M);
  }
  g_xmpp_send_buffer_len = snprintf(g_xmpp_send_buffer, XMPP_SEND_BUF_SIZE, 
                                    "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='%s'>%s</auth>",
                                     sasl_auth_type,response);
}

/*
response building steps :
  1  Create a string of the form "username:realm:password". Call this string X.
  2. Compute the 16 octet MD5 hash of X. Call the result Y.
  3. Create a string of the form "Y:nonce:cnonce:authzid". Call this string A1.
  4. Create a string of the form "AUTHENTICATE:digest-uri". Call this string A2.
  5. Compute the 32 hex digit MD5 hash of A1. Call the result HA1.
  6. Compute the 32 hex digit MD5 hash of A2. Call the result HA2.
  7. Create a string of the form "HA1:nonce:nc:cnonce:qop:HA2". Call this string KD.
  8. Compute the 32 hex digit MD5 hash of KD. Call the result Z.
*/

static inline void md5_to_hex_string(unsigned char *digest, char *hex)
{
  int i;
  for(i=0; i<MD5_DIGEST_LENGTH; i++)
    snprintf((char *)&hex[i*2], 3, "%02x", digest[i]);
}

static char* calculate_digest_response(char *username, char *realm, char *password, 
                   char *nonce, char* nc, char *cnonce, char *digest_uri, char *qop)
{
  char ha1[32+1]="";
  char ha2[32+1]="";
  static char response[32+1]="";
  unsigned char digest[16+1]=""; 

  NSDL2_XMPP(NULL, NULL, "Method called username=%s, realm=%s, password=%s, nonce=%s, nc=%s, cnonce=%s, digest_uri=%s, qop=%s",username,realm,password,nonce,nc,cnonce,digest_uri,qop);
  //Step 1
  MD5_CTX md5;
  MD5_Init(&md5);
  MD5_Update(&md5,username,strlen(username));
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,realm,strlen(realm));
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,password,strlen(password));
  MD5_Final(digest, &md5);

  //Step 2
  MD5_Init(&md5);
  MD5_Update(&md5,digest,16);
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,nonce,strlen(nonce));
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,cnonce,strlen(cnonce));
  MD5_Final(digest, &md5);
  md5_to_hex_string(digest, ha1);


  //Step 3
  MD5_Init(&md5);
  MD5_Update(&md5,"AUTHENTICATE",12);
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,digest_uri,strlen(digest_uri));
  MD5_Final(digest, &md5);
  md5_to_hex_string(digest, ha2);

  //Step 4
  MD5_Init(&md5);
  MD5_Update(&md5,ha1,32);
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,nonce, strlen(nonce));
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,nc,strlen(nc));
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,cnonce, strlen(cnonce));
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,qop,strlen(qop));
  MD5_Update(&md5,":",1);
  MD5_Update(&md5,ha2,32);
  MD5_Final(digest, &md5);
  md5_to_hex_string(digest, response);
 
  NSDL2_XMPP(NULL, NULL, "Response = %s", response);
  return response; 
}

void xor(char *out, char *in1, char *in2, int n) {
  int i;

  for (i = 0; i < n; i++)
    out[i] = in1[i] ^ in2[i];
}

/*
  Method Name     : calculate_scramsha1_response
  Purpose         : This Method will calculate challange responce for SCRAM-SHA-1
  Author          : Atul Sharma
  Date            : 22-october-2018
  Explanation
  RFC 5802         

  SaltedPassword  := Hi(Normalize(password), salt, i)
  ClientKey       := HMAC(SaltedPassword, "Client Key")
  StoredKey       := H(ClientKey)
  AuthMessage     := client-first-message-bare + "," +
                     server-first-message + "," +
                     client-final-message-without-proof
  ClientSignature := HMAC(StoredKey, AuthMessage)
  ClientProof     := ClientKey XOR ClientSignature
  ServerKey       := HMAC(SaltedPassword, "Server Key")
  ServerSignature := HMAC(ServerKey, AuthMessage)
*/

static int calculate_scramsha1_response(char *nonce, char *salt, char *challenge, int iteration, char *username, char *domain, char *password, char *result, int result_size)
{
  
  char authmessage[XMPP_BUF_SIZE_S + 1];
  char clientfinalmessagewithoutproof[XMPP_BUF_SIZE_XS];
  unsigned char SaltedPassword[SHA_DIGEST_LENGTH];
  unsigned char ClientKey[SHA_DIGEST_LENGTH];
  unsigned char StoredKey[SHA_DIGEST_LENGTH];
  unsigned char ClientSignature[SHA_DIGEST_LENGTH];
  char ClientProof[SHA_DIGEST_LENGTH];
  unsigned char clientproof_b64[50];
  char decode_salt[XMPP_BUF_SIZE_S + 1];
  char clientfirstmessagebare[ XMPP_BUF_SIZE_XS + 1];
  int saltlen = 0;
  int bare_len = 0;
  unsigned int resultlen = 0;
  int iter = 4096; /*RFC-5802 For the SCRAM-SHA-1/SCRAM-SHA-1-PLUS SASL mechanism, servers
                     SHOULD announce a hash iteration-count of at least 4096. */

  NSDL2_XMPP(NULL, NULL, "Method called, nonce = %s, salt = %s, iteration = %d, username = %s, domain = %s, password = %s", 
                                                                       nonce, salt, iteration, username, domain, password);

  snprintf(clientfinalmessagewithoutproof, XMPP_BUF_SIZE_XS, "c=biws,r=%s", nonce);


  //nslb_decode_base64_ex(unsigned char *input, int input_length, unsigned char *output, int output_size)
  saltlen = nslb_decode_base64_ex((unsigned char *)salt, strlen(salt), (unsigned char*) decode_salt, XMPP_BUF_SIZE_S);

  //Sample of clientfirstmessagebare and derverfirstmessage
  //char *clientfirstmessagebare = "n=ram,r=a58287f886d56ef0f54042f7dd92ea1";
  //char serverfirstmessage[] = "r=a58287f886d56ef0f54042f7dd92ea18c1d89c6-0f06-4925-a21f-641e9ae3101a,s=y7tyxwRaQEznTOj2RWMSV+JuLSJGwWOm,i=4096";

  //Server send client_nonce+server_nonce we can get client nonce by copying 8 bytes (8 bytes because we are 
  //generating only 8 byte nonce and after encode it inti base64 we truncate it to 8 byte see gen_cnonce method for details)
  bare_len = snprintf(clientfirstmessagebare, XMPP_BUF_SIZE_XS, "n=%s,r=", username);
  strncpy(clientfirstmessagebare + bare_len, nonce , CNONCE_LEN );
  clientfirstmessagebare[ bare_len + CNONCE_LEN] = '\0';

  NSDL2_XMPP(NULL, NULL, "clientfirstbasemessage  [%s]", clientfirstmessagebare );

 if (PKCS5_PBKDF2_HMAC_SHA1(password, strlen(password), (unsigned char *) decode_salt, saltlen, iter, SHA_DIGEST_LENGTH, SaltedPassword) != 1) 
  {
    NSTL1(NULL, NULL, "Error: Failed to generate PBKDF2\n");
    result = NULL;
    return -1;
  }

  /*  ClientKey       := HMAC(SaltedPassword, "Client Key") */
  HMAC(EVP_sha1(), SaltedPassword, SHA_DIGEST_LENGTH, (const unsigned char *) CLIENT_KEY, strlen(CLIENT_KEY), ClientKey, &resultlen);

  /* StoredKey       := H(ClientKey) */
  SHA1((const unsigned char *) ClientKey, SHA_DIGEST_LENGTH, StoredKey);

  /* ClientSignature := HMAC(StoredKey, authmessage) */
  snprintf(authmessage, XMPP_BUF_SIZE_S, "%s,%s,%s", clientfirstmessagebare, challenge, clientfinalmessagewithoutproof);
  NSDL2_XMPP(NULL, NULL, "authmessage = %s", authmessage);
  HMAC(EVP_sha1(), StoredKey, SHA_DIGEST_LENGTH, (const unsigned char *) authmessage, strlen(authmessage), ClientSignature, &resultlen);

  /* ClientProof     := ClientKey XOR ClientSignature */
  xor(ClientProof, (char *) ClientKey, (char *) ClientSignature, 20);

  to64frombits(clientproof_b64, (const unsigned char *) ClientProof, 20);

  resultlen = snprintf(result, result_size, "%s,p=%s", clientfinalmessagewithoutproof, clientproof_b64);

  NSDL2_XMPP(NULL, NULL, "Final result : %s\n", result);

  return resultlen;
}

/*
This method will handle DIGEST-MD5,SCRAM-SHA-1 Challange response

*/
static void xmpp_build_sasl_response(char *username, char *domain, char *password, char *auth_type)
{  
  char resp_str[XMPP_BUF_SIZE_M + 1];
  char response[XMPP_BUF_SIZE_M + 1];
  char challange[XMPP_BUF_SIZE_M + 1];
  char cnonce[XMPP_BUF_SIZE_XS + 1];
  char digest_uri[XMPP_BUF_SIZE_XS + 1];
  char *token[MAX_STATE_MODEL_TOKEN + 1];
  char challange_info[max_challange_fields][XMPP_BUF_SIZE_XS + 1];
  char *start_ptr;
  char *rsp_buf;
  char s_nonce[XMPP_BUF_SIZE_XS + 1];
  char salt[XMPP_BUF_SIZE_XS + 1];
  int i_val = -1;
  char *nc="00000001";
  int i = 0 , j, resp_len, num_tokens;
  int len;
  char resp_buf[XMPP_BUF_SIZE_S + 1];

  NSDL2_XMPP(NULL, NULL, "Method called");
  if(!strcmp(auth_type,"PLAIN"))
  {
    nslb_encode_base64_ex((unsigned char*)password, strlen(password), (unsigned char*)response, XMPP_BUF_SIZE_M);
  }
  else if(!strcmp(auth_type,"DIGEST-MD5")) 
  {
    nslb_decode_base64_ex((unsigned char *)g_xmpp_read_buffer, g_xmpp_read_buffer_len, (unsigned char*) response, XMPP_BUF_SIZE_M);

    if(!strncmp(response,"rspauth=",8))
    {
      //Decoded form will like 
      //rspauth=ea40f60335c427b5527b84dbabcdfffd
      response[0]='\0'; 
    } 
    else
    {
      //Decoded form will like 
      //realm="cataclysm.cx",nonce="OA6MG9tEQGm2hh",qop="auth",charset=utf-8,algorithm=md5-sess
      for(j = 0 ; j < max_challange_fields ; j++)
      { 
        challange_info[j][0] = '\0';
      }
      num_tokens = get_tokens(response, token, ",", MAX_STATE_MODEL_TOKEN);
      while( i < num_tokens )
      {
        NSDL3_XMPP(NULL, NULL, "token[%d]=%s", i, token[i]);
        for(j = 0; j < max_challange_fields ; j++)
        { 
          if(!strncmp(challange_option_str[j].name , token[i], challange_option_str[j].len))
          {
            
            start_ptr = token[i] + challange_option_str[j].len + 1 + challange_option_str[j].quote;
            len = snprintf(challange_info[j],XMPP_BUF_SIZE_XS,"%s",start_ptr);
            challange_info[j][len-challange_option_str[j].quote] = '\0';
            NSDL3_XMPP(NULL, NULL, "challange_option_str[%d]=%s challange_info[%d]=%s", 
                                                          j, challange_option_str[j].name, j, challange_info[j]);
            break;
          }
        } 
        i++;
      }
      //Validate challange info
    
      if(!challange_info[nonce][0])
      {
        NSDL2_XMPP(NULL,NULL, "nonce not fount in challange_info");
        return;
      }
    
      //if(!challange_info[realm][0]) 
      //  strcpy(challange_info[realm],domain);
  
      if(!challange_info[qop][0])
        strcpy(challange_info[qop],"auth"); 
    
      sprintf(digest_uri,"xmpp/%s", domain);
      gen_cnonce((unsigned char *)cnonce);
    
      rsp_buf = calculate_digest_response(username, challange_info[realm], password,
                                           challange_info[nonce], nc,cnonce, digest_uri,
                                           challange_info[qop]);
    
      resp_len = snprintf(resp_str, XMPP_BUF_SIZE_M,
                                      "username=\"%s\",realm=\"%s\",nonce=\"%s\",cnonce=\"%s\",nc=%s,"
                                      "qop=\"%s\",digest-uri=\"%s\",response=%s,charset=%s", 
                                      username, challange_info[realm], challange_info[nonce], cnonce, nc,
                                      challange_info[qop], digest_uri, rsp_buf, challange_info[charset]);
    
      resp_len = nslb_encode_base64_ex((unsigned char *)resp_str, resp_len, (unsigned char*)response, XMPP_BUF_SIZE_M);
      response[resp_len] = '\0';
    }
  }
  //Handle SCRAM-SHA-1 Challange Response here.
  else if(!strcmp(auth_type, "SCRAM-SHA-1"))
  {
    NSDL2_XMPP(NULL, NULL, "Proceeding with auth type SCRAM-SHA-1 ");

    nslb_decode_base64_ex((unsigned char *)g_xmpp_read_buffer, g_xmpp_read_buffer_len, (unsigned char*) response, XMPP_BUF_SIZE_M);
    //Decoded form will be like
    //r=a58287f886d56ef0f54042f7dd92ea18c1d89c6-0f06-4925-a21f-641e9ae3101a,s=y7tyxwRaQEznTOj2RWMSV+JuLSJGwWOm,i=4096
    strncpy(challange, response, strlen(response)); 
    num_tokens = get_tokens(challange, token, "," , MAX_STATE_MODEL_TOKEN);
    s_nonce[0] = '\0';
    salt[0] = '\0';
    while( i < num_tokens )
    {
     
      if(!strncmp("r=", token[i], 2))
      {
        snprintf(s_nonce, XMPP_BUF_SIZE_XS, "%s", token[i] + 2);
        NSDL3_XMPP(NULL, NULL, "SCRAM-SHA-1 nonce = %s", s_nonce); 
      }
      else if(!strncmp("s=", token[i], 2))
      {
        snprintf(salt, XMPP_BUF_SIZE_XS, "%s", token[i] + 2);
        NSDL3_XMPP(NULL, NULL, "SCRAM-SHA-1 salt = %s", salt); 
      }
      else if(!strncmp("i=", token[i], 2))
      {
        i_val = atoi(token[i]+2);
        NSDL3_XMPP(NULL, NULL, "SCRAM-SHA-1 i = %d", i_val); 
      }
      else
      {
        NSDL3_XMPP(NULL, NULL, "Unhandled challange response attribute = %s", token[i]); 
      }

      i++;
    }
    if( !s_nonce[0] || !salt[0] || (i_val == -1 ))
    {
      NSDL3_XMPP(NULL, NULL, "nonce, salt or iteration not found while process challenge response"); 
      return; 
    }
    
    resp_len = calculate_scramsha1_response(s_nonce, salt, response, i_val, username, domain, password, resp_buf, XMPP_BUF_SIZE_S);

    resp_len = nslb_encode_base64_ex((unsigned char *)resp_buf, resp_len, (unsigned char*)response, XMPP_BUF_SIZE_M);
    response[resp_len] = '\0';
  }
  else
  {
    // Not supported
    NSDL2_XMPP(NULL,NULL, "Unsupported SASL AUTH type found returning NULL");
    return ;
  }

  g_xmpp_send_buffer_len = snprintf(g_xmpp_send_buffer, XMPP_SEND_BUF_SIZE,
                                    "<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
                                    "%s"
                                    "</response>"
                                    ,response);
}

static inline void xmpp_build_bind(char *bind_id)
{  
  NSDL2_XMPP(NULL,NULL, "Method called");
  g_xmpp_send_buffer_len = snprintf(g_xmpp_send_buffer, XMPP_SEND_BUF_SIZE, 
                                    "<iq id='%s' type='set'>"
                                    "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'/>"
                                    "</iq>"
                                    , bind_id);
}

static inline void xmpp_build_session(char *session_id)
{  
  NSDL2_XMPP(NULL,NULL, "Method called");
  g_xmpp_send_buffer_len = snprintf(g_xmpp_send_buffer, XMPP_SEND_BUF_SIZE, 
                                    "<iq id='%s' type='set'>"
                                    "<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>"
                                    "</iq>"
                                    , session_id);
}

static inline void xmpp_build_message(char *type, char *from, char *id, char *to, char *text)
{
  NSDL2_XMPP(NULL,NULL, "Method called");
  g_xmpp_send_buffer_len = snprintf(g_xmpp_send_buffer, XMPP_SEND_BUF_SIZE, 
                                    "<message type='%s' "
                                    "from='%s' "
                                    "id='%s' "
                                    "to='%s'>"
                                    "<body>%s</body>" 
                                    "<request xmlns='urn:xmpp:receipts'/>"
                                    "</message>"
                                    ,type, from, id, to, text);

}

static inline void xmpp_build_receipt(char *from, char *msg_id, char *to, char *id)
{
  NSDL2_XMPP(NULL,NULL, "Method called");
  g_xmpp_send_buffer_len = snprintf(g_xmpp_send_buffer, XMPP_SEND_BUF_SIZE, 
                                    "<message "
                                    "from='%s' "
                                    "id='%s' "
                                    "to='%s'>"
                                    "<received xmlns='urn:xmpp:receipts' "
                                    "id='%s'/>"
                                    "</message>"
                                    ,from, msg_id, to, id);
}


static inline void xmpp_build_presence(char *from)
{
 
  NSDL2_XMPP(NULL,NULL, "Method called");
  g_xmpp_send_buffer_len = snprintf(g_xmpp_send_buffer, XMPP_SEND_BUF_SIZE,
                      "<presence from='%s'>"
                      "<show>xa</show>"
                      "<status>Available</status>"
                      "</presence>",
                       from);              
}

static inline void xmpp_build_presence_join_group(char *id, char *to, char *nickname)
{
 
  NSDL2_XMPP(NULL,NULL, "Method called");
  g_xmpp_send_buffer_len = snprintf(g_xmpp_send_buffer, XMPP_SEND_BUF_SIZE,
                           "<presence "
                           "id='%s' "
                           "to='%s/%s'>"
                           "<x xmlns='http://jabber.org/protocol/muc'/>"
                           "</presence>",
                           id, to, nickname );
}

/********************************************************************************************************/
/********************************************************************************************************/
void debug_log_res(connection *cptr, char *buf, int size) //TODO:HANDLE for log directories in case of partition and non partition case
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  NSDL2_XMPP(vptr,cptr, "Method called");

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_XMPP)))
    return;

  {
    char log_file[1024];
    int log_fd;

    // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
    //sprintf(log_file, "%s/logs/TR%d/url_rep_%d_%ld_%ld_%d_0.dat", g_ns_wdir, testidx, my_port_index, vptr->user_index, cur_vptr->sess_inst, vptr->page_instance);
  

   SAVE_REQ_REP_FILES
   sprintf(log_file, "%s/logs/%s/xmpp_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
    // Do not change the debug trace message as it is parsed by GUI
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
      NS_DT4(vptr, cptr, DM_L1, MM_XMPP, "Response is in file '%s'", log_file);

    if((log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging URL request\n");
    else
    {
      write(log_fd, buf, size);
      close(log_fd);
    }
  }
}

int xmpp_http_file_upload(connection *cptr, u_ns_ts_t now )
{
  char url_extract[MAX_LINE_LENGTH];
  char hostname[MAX_LINE_LENGTH];
  char file_path[MAX_LINE_LENGTH + 1];
  action_request_Shr *upload_url_num;
  VUser *vptr = cptr->vptr;
  int request_type;
  int port;
  int norm_id;
  char *file_name = NULL;
  int file_path_len;

  NSDL2_XMPP(vptr,cptr, "Method called");
  char *sess_name = get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/");
  file_name = get_file_name(vptr, cptr->url_num->proto.xmpp.file.seg_start);
  file_path_len = snprintf(file_path, MAX_LINE_LENGTH, "./scripts/%s/xmpp_files/%s", sess_name, file_name);
  norm_id = get_file_norm_id(file_path, file_path_len);
  if(norm_id < 0)
  { 
    //Error File not found in norm table
    NSDL2_XMPP(vptr, cptr, "File %s not found", file_path);
    return -1;
  }

  if(extract_hostname_and_request(g_xmpp_read_buffer, hostname, url_extract, &port, &request_type,
                               get_url_req_url(cptr), cptr->url_num->request_type) < 0) {
     NSTL1(vptr,cptr, "extract_hostname_and_request() failed");
     return -1;
  }
  if (port == -1) {
    if (request_type == HTTPS_REQUEST) { // https
      port = (443);
    } else // http
      port = (80);
  }
  NSDL2_XMPP(vptr, cptr, "Extracted URL (%s) from previous request line (%s), extracted host = %s, port = %d", 
              url_extract, g_xmpp_read_buffer, hostname, port);

  MY_MALLOC(upload_url_num, sizeof(action_request_Shr), "upload_url_num", -1);
  memcpy(upload_url_num, cptr->url_num, sizeof (action_request_Shr));

  if (hostname[0] != '\0') {
    unsigned short rec_server_port; //Sending Dummy
    int gserver_shr_idx;
    int hostname_len = find_host_name_length_without_port(hostname, &rec_server_port);
    if ((gserver_shr_idx = find_gserver_shr_idx(hostname, port, hostname_len)) == -1) 
    {
      /* Dynamic Host ---BEGIN */
      NSDL2_XMPP(vptr, cptr, "File Upload URL Type:cptr->url_num->proto.http.type = %d", cptr->url_num->proto.http.type);
      gserver_shr_idx = add_dynamic_hosts (vptr, hostname, port, MAIN_URL, 1, request_type, url_extract, vptr->sess_ptr->sess_name, vptr->cur_page->page_name, vptr->user_index, runprof_table_shr_mem[vptr->group_num].scen_group_name);
      if (gserver_shr_idx < 0)
      {
        NS_DT3(vptr, cptr, DM_L1, MM_XMPP, "File upload cannot be done due to host table full or host cannot be resolved");
        NSDL2_XMPP(vptr, cptr, "gserver_shr_idx = %d", gserver_shr_idx);
        // Free allocated memory
        FREE_AND_MAKE_NULL_EX (upload_url_num, sizeof (action_request_Shr), "Upload File", -1);
        FREE_CPTR_URL(cptr);
        NSDL2_XMPP(vptr, cptr, "File upload url failed hence aborting page");
        return -1;
      }
      /*Dynamic Host ---END */
    }
    upload_url_num->index.svr_ptr = &gserver_table_shr_mem[gserver_shr_idx];
  } else {
    upload_url_num->index.svr_ptr = cptr->url_num->index.svr_ptr;
  }
  FREE_CPTR_URL(cptr);

  int url_len = strlen(url_extract); 


  upload_url_num->proto.http.http_method = HTTP_METHOD_PUT;
  upload_url_num->proto.http.http_method_idx = HTTP_METHOD_PUT;
  upload_url_num->proto.http.http_version = 1; // HTTP/1.1
  upload_url_num->proto.http.header_flags |= NS_XMPP_UPLOAD_FILE;
  upload_url_num->proto.http.type = MAIN_URL;
  
  MY_MALLOC(upload_url_num->proto.http.redirected_url, url_len + 1,
          "upload_url_num->proto.http.redirected_url", -1);

  strcpy(upload_url_num->proto.http.redirected_url, url_extract); 
  upload_url_num->request_type = request_type;
  vptr->httpData->post_body = get_file_content(norm_id);
  vptr->httpData->post_body_len = get_file_size(norm_id);
  vptr->urls_left++;
  vptr->urls_awaited++;
  //content type
  char *content_type = get_file_content_type(norm_id);

  int content_type_len = strlen(content_type); 
  if (vptr->httpData->formenc_len < content_type_len )
  {
    vptr->httpData->formenc_len = content_type_len;
    MY_MALLOC(vptr->httpData->formencoding, vptr->httpData->formenc_len + 1, "vptr-httpData->formencoding", -1);
  }
  strcpy(vptr->httpData->formencoding, content_type);

  if(!xmpp_hdrs_seg_start)
  {
    MY_MALLOC(xmpp_hdrs_seg_start, sizeof(SegTableEntry_Shr), "Fill upload_url_num", -1);
    MY_MALLOC(xmpp_hdrs_seg_start->seg_ptr.str_ptr, sizeof(PointerTableEntry_Shr), "Fill upload_url_num", -1);
    xmpp_hdrs_seg_start->seg_ptr.str_ptr->big_buf_pointer = "\r\n";
    xmpp_hdrs_seg_start->seg_ptr.str_ptr->size = 2;
    xmpp_hdrs_seg_start->type = STR;
  }
  upload_url_num->proto.http.hdrs.seg_start = xmpp_hdrs_seg_start;
  upload_url_num->proto.http.hdrs.num_entries = 1;
 
  if(LOG_LEVEL_FOR_DRILL_DOWN_REPORT)
  { 
    NSDL2_XMPP(NULL, cptr, "Redirected URL = %s", upload_url_num->proto.http.redirected_url);
    upload_url_num->proto.http.url_index = url_hash_get_url_idx_for_dynamic_urls((u_ns_char_t *)upload_url_num->proto.http.redirected_url, 
                                                             url_len, vptr->cur_page->page_id, 0, 0, vptr->cur_page->page_name);
    NSDL3_XMPP(vptr, NULL, "req_url->url_id = %d", upload_url_num->proto.http.url_index);
  }

  if (!try_url_on_any_con (vptr, upload_url_num, now, NS_HONOR_REQUEST)) {
    NSDL2_XMPP(vptr, cptr, "File upload url failed hence aborting page");
    return -1;
  }
  return 0;
}

int xmpp_ssl_write (connection *cptr, u_ns_ts_t now)
{
  int i;
  char *ptr_ssl_buff;
  int bytes_left_to_send = cptr->bytes_left_to_send;
  VUser *vptr = cptr->vptr;

  NSDL2_XMPP(vptr, cptr, "Method called");
 
  copy_request_into_buffer(cptr, g_xmpp_send_buffer_len, &g_req_rep_io_vector);
  ptr_ssl_buff = cptr->free_array + cptr->tcp_bytes_sent;
  bytes_left_to_send = cptr->bytes_left_to_send;

  NSDL2_XMPP(vptr, cptr,"tcp_bytes_sent = %d, bytes_left_to_send = %d", cptr->tcp_bytes_sent, cptr->bytes_left_to_send);

  ERR_clear_error();
  i = SSL_write(cptr->ssl, ptr_ssl_buff, bytes_left_to_send);
  switch (SSL_get_error(cptr->ssl, i)) 
  {
    case SSL_ERROR_NONE:
      XMPP_DATA_INTO_AVG_THROUGHPUT(xmpp_send_bytes, i); 
      NSDL2_XMPP(vptr, cptr, "bytes_sent = %d, xmpp_send_bytes = %d", i, xmpp_avgtime->xmpp_send_bytes);
      cptr->tcp_bytes_sent += i;
      average_time->tx_bytes += i;
      if (i >= bytes_left_to_send) 
      {
        cptr->bytes_left_to_send -= i;
        //all sent
//#ifdef NS_DEBUG_ON
//        debug_log_xmpp_req(cptr, ptr_ssl_buff, i, 1, 1);
//#endif
        break;
      } 
      else
      {
        cptr->bytes_left_to_send -= i;
//#ifdef NS_DEBUG_ON
        //debug_log_xmpp_req(cptr, ptr_ssl_buff, i, 0, 1);
//#endif
      }
    case SSL_ERROR_WANT_WRITE:
      NSDL3_HTTP(NULL, cptr, "SSL_ERROR_WANT_WRITE occurred", cptr->bytes_left_to_send);
      cptr->conn_state = CNST_SSL_WRITING;
      return -2;
    case SSL_ERROR_WANT_READ:
      NSDL3_HTTP(NULL, cptr, "SSL_ERROR_WANT_READ occurred", cptr->bytes_left_to_send);
      cptr->conn_state = CNST_SSL_WRITING;
      return -2;
    case SSL_ERROR_ZERO_RETURN:
      NSDL3_HTTP(NULL, cptr, "SSL_Write ERROR occurred");
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return -1;
    case SSL_ERROR_SSL:
      ERR_print_errors_fp(stderr);
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return -1;
    default:
      NSDL3_HTTP(NULL, cptr, "SSL_Write ERROR %d",errno);
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return -1;
  }

  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  on_request_write_done (cptr);
  return 0;
}

int xmpp_write(connection *cptr, u_ns_ts_t now)
{
  int bytes_sent;
  VUser *vptr = cptr->vptr;

  NSDL2_XMPP(vptr, cptr, "Method called. sending , fd = %d", cptr->conn_fd);
  if ((bytes_sent = writev(cptr->conn_fd, g_req_rep_io_vector.vector, g_req_rep_io_vector.cur_idx)) < 0)
  {
    NSDL2_XMPP(vptr, cptr, "Sending XMPP Request Failed. Error = %s, fd = %d", nslb_strerror(errno), cptr->conn_fd);
    retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
    return -1;
  }

  if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
  {
    retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    return -1;
  }

  XMPP_DATA_INTO_AVG_THROUGHPUT(xmpp_send_bytes, bytes_sent); 
  NSDL2_XMPP(vptr, cptr, "bytes_sent = %d, xmpp_send_bytes = %d", bytes_sent, xmpp_avgtime->xmpp_send_bytes);
  NSDL2_XMPP(vptr, cptr, "bytes_sent = %d, g_xmpp_send_buffer_len= %d", bytes_sent, g_xmpp_send_buffer_len);
  if (bytes_sent < g_xmpp_send_buffer_len )
  {
    handle_incomplete_write(cptr, &g_req_rep_io_vector, g_req_rep_io_vector.cur_idx, g_xmpp_send_buffer_len, bytes_sent);
    return -2;
  }
  on_request_write_done (cptr); 
  return 0; 
}

int xmpp_send(connection *cptr, u_ns_ts_t now )
{
  
  int ret;
  NSDL2_XMPP(NULL, cptr,"Method Called cptr = %p, SENDING Buffer = %s", cptr, g_xmpp_send_buffer);
  char ssl = cptr->request_type == XMPPS_REQUEST ? 1:0;

  NS_FILL_IOVEC(g_req_rep_io_vector, g_xmpp_send_buffer, g_xmpp_send_buffer_len);

  if(ssl)
    ret  = xmpp_ssl_write(cptr, now);
  else
    ret = xmpp_write(cptr, now);

  g_xmpp_send_buffer[0]='\0';
  g_xmpp_send_buffer_len = 0;
  return ret;
}

static inline void handle_bad_read (connection *cptr, u_ns_ts_t now)
{
  NSDL2_IMAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  retry_connection(cptr, now, NS_REQUEST_BAD_HDR);
}

//called from ns_child.c 
int xmpp_read(connection *cptr, u_ns_ts_t now)
{
  char *next, *buffer;
  char *err_buff;
  int bytes_read, input;
  char buf[XMPP_BUF_SIZE_XXL + 1];   
  char ssl = cptr->request_type == XMPPS_REQUEST ? 1:0;
  g_xmpp_read_buffer_len = 0;
  g_xmpp_read_buffer[0]='\0'; 
  VUser *vptr = cptr->vptr;
  int err;
  
  NSDL2_XMPP(vptr, cptr,"Method Called cptr = %p", cptr);
  while(1)
  {
    if(ssl)
    {
      bytes_read = SSL_read(cptr->ssl, buf, XMPP_BUF_SIZE_XXL);
 
      if (bytes_read <= 0) {
        err = SSL_get_error(cptr->ssl, bytes_read);
        switch (err) {
          case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
            handle_server_close (cptr, now);
            return -1;
          case SSL_ERROR_WANT_READ:
            return -1;
            /* It can but isn't supposed to happen */
          case SSL_ERROR_WANT_WRITE:
            NSDL2_XMPP(vptr, cptr, "SSL_read error: SSL_ERROR_WANT_WRITE");
            handle_bad_read (cptr, now);
            return -1 ;
          case SSL_ERROR_SYSCALL: //Some I/O error occurred
            if (errno == EAGAIN) // no more data available, return (it is like SSL_ERROR_WANT_READ)
            {
              NSDL1_SSL(NULL, cptr, "XMPP SSL_read: No more data available, return");
              return -1;
            }
 
            if (errno == EINTR)
            {
              NSDL3_XMPP(NULL, cptr, "XMPP SSL_read interrupted. Continuing...");
              //continue;
              //As in case of XMPP we are not reading data in loop.
            }
            /* FALLTHRU */
          case SSL_ERROR_SSL: //A failure in the SSL library occurred, usually a protocol error
            /* FALLTHRU */
          default:
            err_buff = ERR_error_string(err, NULL);
            NSTL1(NULL, NULL, "SSl library error %s ", err_buff);
            //ERR_print_errors_fp(ssl_logs);
            if ((bytes_read == 0) && (!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.ssl_clean_close_only))
              handle_server_close (cptr, now);
            else
              handle_bad_read (cptr, now);
            return -1;
        }
      }
    }
    else
    {
      NSDL1_XMPP(NULL, cptr, "vptr->xmpp->partial_buf_len = %d", vptr->xmpp->partial_buf_len);
      if(vptr->xmpp->partial_buf_len)
      {
        if (vptr->xmpp->partial_buf_len < XMPP_BUF_SIZE_XXL)
          strncpy(buf, vptr->xmpp->partial_buf, vptr->xmpp->partial_buf_len);
        //TODO if vptr->xmpp->partial_buf_len >= XMPP_BUF_SIZE_XXL
      }
      bytes_read = read( cptr->conn_fd, &buf[vptr->xmpp->partial_buf_len], (XMPP_BUF_SIZE_XXL - vptr->xmpp->partial_buf_len));
 
      if ( bytes_read < 0 ) {
        if (errno == EAGAIN) // no more data available, return (it is like SSL_ERROR_WANT_READ)
        {
          NSDL1_XMPP(NULL, cptr, "XMPP read: No more data available, return");
          return -1;
        } else {
          handle_bad_read (cptr, now);
          return -1;
        }
      } else if (bytes_read == 0) {
        handle_server_close (cptr, now);
        return -1;
      }
    }
    XMPP_DATA_INTO_AVG_THROUGHPUT(xmpp_rcvd_bytes, bytes_read); 
    NSDL2_XMPP(vptr, cptr, "bytes_read = %d, xmpp_rcvd_bytes = %d", bytes_read, xmpp_avgtime->xmpp_rcvd_bytes);
    buf[vptr->xmpp->partial_buf_len + bytes_read] = '\0';
    //Reset the xmpp->partial_buf_len to zero as we have copied the same into buf 
    vptr->xmpp->partial_buf_len = 0;
    NSDL3_XMPP(NULL, cptr, "XMPP RECIV:  Read %d bytes.MSG = %s", bytes_read, buf);
    /*Process Received XMPP XML*/
    buffer = buf;
    do{
      input  = xmpp_parse_buffer(cptr, buffer, &next);
      if((input >= XMPP_START_STREAM) && (input < XMPP_MAX_INPUT))
      { 
        if (execute(cptr, input, now) < 0)
          return -1;
      }
      buffer = next;
      NSDL4_XMPP(NULL, cptr, "message to process next = %s", next); 
    }while(buffer);
  }
  return 0;
}

int xmpp_start_stream(connection *cptr, u_ns_ts_t now )
{
  VUser *vptr = cptr->vptr;

  NSDL2_XMPP(vptr, cptr, "Method Called cptr = %p", cptr);
  cptr->proto_state = XMPP_START_STREAM_INIT;

  char user[XMPP_BUF_SIZE_XS + 1];
  char domain[XMPP_BUF_SIZE_XS + 1];

  //Malloc vptr->xmpp if not malloced 
  if(!vptr->xmpp)
  {
    MY_MALLOC_AND_MEMSET(vptr->xmpp, sizeof(nsXmppInfo),"nsXmppInfo",-1);
    vptr->flags |= NS_XMPP_ENABLE;
  }
   //TODO no need to call get_value_from_segment again and again. make a global variable and fill it.
  get_values_from_segments(cptr,&cptr->url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_XS);
  get_values_from_segments(cptr,&cptr->url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS);

  NSDL2_XMPP(vptr, cptr,"user = %s, domain= %s", user, domain);

  xmpp_build_start_stream(user, domain);
  return xmpp_send(cptr, now);
}

int xmpp_start_tls(connection *cptr, u_ns_ts_t now)
{
  
  IW_UNUSED(VUser *vptr = cptr->vptr);
  NSDL2_XMPP(vptr, cptr, "Method Called cptr = %p", cptr);
  strcpy(g_xmpp_send_buffer, "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
  g_xmpp_send_buffer_len = 51; 
  return xmpp_send(cptr, now);
}

int xmpp_tls_proceed(connection *cptr, u_ns_ts_t now)
{
  IW_UNUSED(VUser *vptr = cptr->vptr);
  NSDL2_XMPP(vptr, cptr, "Method Called cptr = %p", cptr);
  cptr->request_type = XMPPS_REQUEST; 
  handle_connect(cptr, now, 0);
  return xmpp_start_stream(cptr, now);
}

int xmpp_sasl_auth(connection *cptr, u_ns_ts_t now)
{
  char password[XMPP_BUF_SIZE_XS];
  char user[XMPP_BUF_SIZE_XS];
  char auth_type[XMPP_BUF_SIZE_XS];
  char *auth_type_ptr;
  int auth_type_len;
  VUser *vptr = cptr->vptr;
  NSDL2_XMPP(vptr, cptr, "Method Called");
  get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_XS);
  get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.password, password, XMPP_BUF_SIZE_XS);
  if(cptr->url_num->proto.xmpp.sasl_auth_type.seg_start)
  {
    auth_type_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.sasl_auth_type, auth_type, XMPP_BUF_SIZE_XS);
    auth_type_ptr = auth_type;
  }
  else
  {
    auth_type_ptr = g_xmpp_read_buffer;
    auth_type_len = g_xmpp_read_buffer_len;
  }
  //Store sasl-auth-type into file_url to use into challange response api in case of DIGEST-MD5
  if(strcmp(auth_type_ptr,"PLAIN"))
  {
    if(vptr->xmpp->file_url_size < auth_type_len)
    {
      MY_REALLOC(vptr->xmpp->file_url, auth_type_len + 1, "SaslAuthType", -1);
    }
    //reusing vptr->xmpp->file_url 
    strcpy(vptr->xmpp->file_url,auth_type_ptr);
  }
  xmpp_build_sasl_auth(auth_type_ptr, user, password);
  return xmpp_send(cptr, now);
}


int xmpp_sasl_resp(connection *cptr, u_ns_ts_t now)
{
  char user[XMPP_BUF_SIZE_XS], password[XMPP_BUF_SIZE_XS];
  char domain[XMPP_BUF_SIZE_XS];
  VUser *vptr = cptr->vptr;
  NSDL2_XMPP(vptr, cptr, "Method Called");
  get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_XS);
  get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.password, password, XMPP_BUF_SIZE_XS);
  get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS);

  //reusing vptr->xmpp->file_url 
  xmpp_build_sasl_response(user, domain, password, vptr->xmpp->file_url);
  return xmpp_send(cptr, now);
}

int xmpp_do_bind(connection *cptr, u_ns_ts_t now)
{
  char msg_id[XMPP_BUF_SIZE_XS + 1]="";
  VUser *vptr = cptr->vptr;
  NSDL2_XMPP(vptr, cptr, "Method Called");
  snprintf(msg_id, XMPP_BUF_SIZE_XS, "bind_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  xmpp_build_bind(msg_id);
  return xmpp_send(cptr, now);
}

int xmpp_start_session(connection *cptr, u_ns_ts_t now)
{
  char msg_id[XMPP_BUF_SIZE_XS + 1]="";
  VUser *vptr = cptr->vptr;
  NSDL2_XMPP(vptr, cptr, "Method Called");
  snprintf(msg_id, XMPP_BUF_SIZE_XS, "session_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  xmpp_build_session(msg_id);
  return xmpp_send(cptr, now);
}


int xmpp_resource_bind(connection *cptr)
{
  VUser *vptr = cptr->vptr;
  NSDL2_XMPP(vptr, cptr, "Method Called");
  if(g_xmpp_read_buffer_len > vptr->xmpp->uid_size)
  {
    MY_REALLOC(vptr->xmpp->uid,g_xmpp_read_buffer_len,"Allocate Memory for XMPP UID",-1);
    vptr->xmpp->uid_size = g_xmpp_read_buffer_len;
  }
  if(g_xmpp_read_buffer_len)
  {
    //Storing Full Jabber Id on vptr to further use
    strcpy(vptr->xmpp->uid, g_xmpp_read_buffer);
    vptr->xmpp->first_page_url =  vptr->first_page_url;
    vptr->xmpp_cptr = cptr;
  }
  return 0;
}

int xmpp_join_group(connection *cptr, u_ns_ts_t now)
{
  char presence_id[XMPP_BUF_SIZE_XS + 1];
  char nickname[XMPP_BUF_SIZE_XS + 1];
  VUser *vptr = cptr->vptr;
  NSDL2_XMPP(vptr, cptr, "Method Called");
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;
  get_values_from_segments(cptr,&url_num->proto.xmpp.user, nickname, XMPP_BUF_SIZE_XS);
  snprintf(presence_id, XMPP_BUF_SIZE_XS, "presence_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  xmpp_build_presence_join_group(presence_id, g_xmpp_read_buffer, nickname);
  return  xmpp_send(cptr, now);
}

int xmpp_send_presence(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  NSDL2_XMPP(vptr, cptr, "Method Called");
  xmpp_build_presence(vptr->xmpp->uid);
  return xmpp_send(cptr, now);
}

int xmpp_close_stream(connection *cptr, u_ns_ts_t now)
{
  IW_UNUSED(VUser *vptr = cptr->vptr);
  NSDL2_XMPP(vptr, cptr, "Method Called");
  strcpy(g_xmpp_send_buffer, "</stream:stream>");
  g_xmpp_send_buffer_len = 16;
  return xmpp_send(cptr, now);
}

int xmpp_send_message(connection *cptr, u_ns_ts_t now)
{
  char message[XMPP_BUF_SIZE_XL + 1];
  char msg_id[XMPP_BUF_SIZE_XS + 1];
  char recipient[XMPP_BUF_SIZE_XS + 1];
  char service[XMPP_BUF_SIZE_XS + 1];
  char domain[XMPP_BUF_SIZE_XS + 1];
  char *type;
  int recipient_len = 0;
  int ret;
  VUser *vptr = cptr->vptr;
  NSDL2_XMPP(vptr, cptr, "Method Called cptr = %p", cptr);

  action_request_Shr* url_num =  vptr->xmpp->first_page_url;
   
  if(cptr->url_num->proto.xmpp.user.seg_start)
  {
    char domain[XMPP_BUF_SIZE_XS + 1];
    if((recipient_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.user, recipient, XMPP_BUF_SIZE_M)) < 0 )
      return -1;
    get_values_from_segments(cptr,&url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS);
    sprintf(recipient, "%s@%s", recipient, domain);
    type = "chat";
  }  
  else if(cptr->url_num->proto.xmpp.group.seg_start)
  {
    if((recipient_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, recipient, XMPP_BUF_SIZE_M)) < 0 )
      return -1;

    get_values_from_segments(cptr,&url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS);
    if(url_num->proto.xmpp.group.seg_start)
      get_values_from_segments(cptr, &url_num->proto.xmpp.group, service, XMPP_BUF_SIZE_XS);
    else
    {
      strcpy(service, "conference");
    }
    snprintf(&recipient[recipient_len], XMPP_BUF_SIZE_XS, "@%s.%s", service[0]?service:"conference", domain);
    type = "groupchat";
  }
  else
  {
    return -1; // Should not reach here
  }
  if(get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.message, message, XMPP_BUF_SIZE_XL) < 0) 
    return -1;
  
  snprintf(msg_id, XMPP_BUF_SIZE_XS, "message_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  
  xmpp_build_message(type, vptr->xmpp->uid, msg_id, recipient, message);
  ret  = xmpp_send(cptr, now);
  //Increase Message Sent Counters Here 
  if ( ret == -1)
  {
    XMPP_DATA_INTO_AVG(xmpp_msg_send_failed);
    NSDL2_XMPP(vptr, cptr, "xmpp_msg_send_failed = %d", xmpp_avgtime->xmpp_msg_send_failed);
  }
  else                  //Considering partial as success
  {
    XMPP_DATA_INTO_AVG(xmpp_msg_sent);
    NSDL2_XMPP(vptr, cptr, "xmpp_msg_sent = %d", xmpp_avgtime->xmpp_msg_sent);
  }
  return ret;
}

static int xmpp_send_url(connection *cptr, u_ns_ts_t now)
{
  char msg_id[XMPP_BUF_SIZE_XS + 1];
  char recipient[XMPP_BUF_SIZE_XS + 1];
  char domain[XMPP_BUF_SIZE_XS + 1];
  int recipient_len = 0;
  char *type;
  int ret;
  VUser *vptr = cptr->vptr;

  NSDL2_XMPP(vptr, cptr, "Method Called cptr = %p", cptr);
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;

  if(cptr->url_num->proto.xmpp.user.seg_start)
  {
    if((recipient_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.user, recipient, XMPP_BUF_SIZE_M)) < 0 )
      return -1;
    //Atul: Access domain form first_page url bcs current api does not contains domain.
    get_values_from_segments(cptr,&url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS);
    sprintf(recipient, "%s@%s", recipient, domain);
    type = "chat";
  }
  else if(cptr->url_num->proto.xmpp.group.seg_start)
  {
    char service[XMPP_BUF_SIZE_XS + 1];
    if((recipient_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, recipient, XMPP_BUF_SIZE_M)) < 0 )
      return -1;

    get_values_from_segments(cptr,&url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS);
    if(url_num->proto.xmpp.group.seg_start)
      get_values_from_segments(cptr, &url_num->proto.xmpp.group, service, XMPP_BUF_SIZE_XS);
    else
    {
      strcpy(service, "conference");
    }
    snprintf(&recipient[recipient_len], XMPP_BUF_SIZE_XS, "@%s.%s", service[0]?service:"conference", domain);
    type = "groupchat";
  }
  else
  {
    return -1; // Should not reach here
  }
  snprintf(msg_id, XMPP_BUF_SIZE_XS, "url_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  
  xmpp_build_message(type, vptr->xmpp->uid, msg_id, recipient, vptr->xmpp->file_url); 
 
  ret  = xmpp_send(cptr, now);
  //Increase Message Sent Counters Here 
  if ( ret == -1)
  {
    XMPP_DATA_INTO_AVG(xmpp_msg_send_failed);
    NSDL2_XMPP(vptr, cptr, "xmpp_msg_send_failed = %d", xmpp_avgtime->xmpp_msg_send_failed);
  }
  else                  //Considering partial as success
  {
    XMPP_DATA_INTO_AVG(xmpp_msg_sent);
    NSDL2_XMPP(vptr, cptr, "xmpp_msg_sent = %d", xmpp_avgtime->xmpp_msg_sent);
  }
  return ret;
}

//int xmpp_item_discovery(connection *cptr, u_ns_ts_t now, char *to)
int xmpp_item_discovery(connection *cptr, u_ns_ts_t now)
{
  char id[XMPP_BUF_SIZE_XS + 1];
  char domain[XMPP_BUF_SIZE_XS + 1];
  char service[XMPP_BUF_SIZE_XS + 1]=""; //Must
  int service_len = 0;
  VUser *vptr = cptr->vptr;
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;

  NSDL2_XMPP(vptr, cptr, "Method Called, cptr = %p", cptr);
  snprintf(id, XMPP_BUF_SIZE_XS, "item_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);

  if(url_num->proto.xmpp.group.seg_start)
    service_len = get_values_from_segments(cptr,&url_num->proto.xmpp.group, service, XMPP_BUF_SIZE_XS);
  else
  {
    strcpy(service,"conference"); 
    service_len = 10;
  }
  get_values_from_segments(cptr,&url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS);
  snprintf(&service[service_len], XMPP_BUF_SIZE_XS, ".%s",domain); //'.' is intentionally
  xmpp_build_item_discovery(vptr->xmpp->uid, id, service);
  return xmpp_send(cptr, now);
}

int xmpp_send_upload_file_slot(connection *cptr, u_ns_ts_t now)
{
  char file_id[XMPP_BUF_SIZE_XS + 1];
  char service[XMPP_BUF_SIZE_XS + 1];
  char domain[XMPP_BUF_SIZE_XS + 1];
  char file_path[MAX_LINE_LENGTH + 1];
  //char file[XMPP_BUF_SIZE_XS + 1];
  char *type;
  char *file_name;
  int size;
  int service_len;
  char *sess_name;
  int file_path_len;
  int norm_id;
  action_request_Shr* url_num;
  VUser *vptr = cptr->vptr;

  NSDL2_XMPP(vptr, cptr, "Method Called, cptr = %p", cptr);

  sess_name = get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/");
  file_name = get_file_name(vptr, cptr->url_num->proto.xmpp.file.seg_start);
  file_path_len = snprintf(file_path, MAX_LINE_LENGTH, "./scripts/%s/xmpp_files/%s", sess_name, file_name);    
  norm_id = get_file_norm_id(file_path, file_path_len);
  if(norm_id < 0)
  {
    //Error File not found in norm table
    NSDL2_XMPP(vptr, cptr, "File %s not found", file_path);
    return -1;
  }
  size = get_file_size(norm_id);
  type = get_file_content_type(norm_id); 

  snprintf(file_id, XMPP_BUF_SIZE_XS, "file_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);

  /*Get the Service Name from Login API*/ 
  url_num =  vptr->xmpp->first_page_url;
  if(url_num->proto.xmpp.file.seg_start)
    service_len = get_values_from_segments(cptr,&url_num->proto.xmpp.file, service, XMPP_BUF_SIZE_XS);
  else
  {
    strcpy(service,"httpfileupload"); 
    service_len = 14;
  }
  get_values_from_segments(cptr,&url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS);
  snprintf(&service[service_len], XMPP_BUF_SIZE_XS, ".%s",domain); //'.' is intentionally

  xmpp_build_iq_upload_file_slot(vptr->xmpp->uid, file_id, service, file_name, size, type);
  return xmpp_send(cptr, now);
  
}

int xmpp_process_message(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char to[XMPP_BUF_SIZE_M + 1];
  char id[XMPP_BUF_SIZE_XS + 1];
  char msg_id[XMPP_BUF_SIZE_XS + 1];
 
  NSDL2_XMPP(vptr, cptr, "Method called");
  //Increase Message Received Counters Here 
  XMPP_DATA_INTO_AVG(xmpp_msg_rcvd);
  NSDL2_XMPP(vptr, cptr, "xmpp_msg_rcvd = %d", xmpp_avgtime->xmpp_msg_rcvd);
  if (g_xmpp_read_buffer_len)
  {
    xmpp_get_msg_attr(g_xmpp_read_buffer,"from=", 5, to, XMPP_BUF_SIZE_M);
    //xmpp_get_msg_attr(g_xmpp_read_buffer,"to=", 3, from, XMPP_BUF_SIZE_M);
    xmpp_get_msg_attr(g_xmpp_read_buffer,"id=", 3, id, XMPP_BUF_SIZE_XS);
    snprintf(msg_id, XMPP_BUF_SIZE_XS, "receipt_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
    xmpp_build_receipt(vptr->xmpp->uid, msg_id, to, id);
    return xmpp_send(cptr,now) ;
  } 
  return 0;
}

int xmpp_create_group(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char group[XMPP_BUF_SIZE_XS + 1];
  char user[XMPP_BUF_SIZE_XS + 1];
  char domain[XMPP_BUF_SIZE_XS + 1];
  char service[XMPP_BUF_SIZE_XS + 1]="";
  char recipient[XMPP_BUF_SIZE_XS + 1];

  NSDL2_XMPP(vptr, cptr, "Method called");
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;
 
  if(get_values_from_segments(cptr, &url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if(get_values_from_segments(cptr, &url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if((get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, group, XMPP_BUF_SIZE_M)) < 0 )
    return -1;
  
  if(url_num->proto.xmpp.group.seg_start)
    get_values_from_segments(cptr,&url_num->proto.xmpp.group, service, XMPP_BUF_SIZE_XS);

  snprintf(recipient, XMPP_BUF_SIZE_XS, "%s@%s.%s", group, service[0]?service:"conference", domain);

  //xmpp_insert_group_info(cptr, recipient, recipient_len);
  xmpp_build_presence_create_group(recipient, user);
  return xmpp_send(cptr, now);
}

int xmpp_delete_group(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char domain[XMPP_BUF_SIZE_XS + 1];
  char user[XMPP_BUF_SIZE_XS + 1];
  char group[XMPP_BUF_SIZE_XS + 1];
  char id[XMPP_BUF_SIZE_XS + 1];
  char recipient[XMPP_BUF_SIZE_M + 1];
  int  group_len = 0;
  char service[XMPP_BUF_SIZE_XS + 1]="";
  NSDL2_XMPP(vptr, cptr, "Method called");

  action_request_Shr* url_num =  vptr->xmpp->first_page_url;
  if(get_values_from_segments(cptr, &url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if(get_values_from_segments(cptr, &url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if((group_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, group, XMPP_BUF_SIZE_M)) < 0 )
    return -1;

  if(url_num->proto.xmpp.group.seg_start)
    get_values_from_segments(cptr,&url_num->proto.xmpp.group, service, XMPP_BUF_SIZE_XS);

  snprintf(recipient, XMPP_BUF_SIZE_XS, "%s@%s.%s", group, service[0]?service:"conference", domain);

  snprintf(id, XMPP_BUF_SIZE_XS, "delete_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  xmpp_build_delete_group(vptr->xmpp->uid, id, recipient);
  return xmpp_send(cptr, now);
}

int xmpp_delete_contact(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char domain[XMPP_BUF_SIZE_XS + 1];
  char to[XMPP_BUF_SIZE_M + 1];
  char id[XMPP_BUF_SIZE_XS + 1];
  char user[XMPP_BUF_SIZE_XS + 1];

  NSDL2_XMPP(vptr, cptr, "Method called");
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;

  if(get_values_from_segments(cptr,&url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if((get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_M)) < 0 )
    return -1;

  sprintf(to,"%s@%s",user,domain);
  xmpp_build_delete_contact(vptr->xmpp->uid, id, to);
  snprintf(id, XMPP_BUF_SIZE_XS, "unsubscribe_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  xmpp_build_subscribe_contact(id, to,"remove");
  return xmpp_send(cptr, now);
}

int xmpp_add_contact(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char domain[XMPP_BUF_SIZE_XS + 1];
  char user[XMPP_BUF_SIZE_XS + 1];
  char group[XMPP_BUF_SIZE_XS + 1] = ""; //Must initialize 
  char to[XMPP_BUF_SIZE_M + 1];
  char id[XMPP_BUF_SIZE_XS + 1];

  NSDL2_XMPP(vptr, cptr, "Method called");
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;
  if(get_values_from_segments(cptr,&url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS) < 0)
    return -1;


  if(get_values_from_segments(cptr,&cptr->url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if(cptr->url_num->proto.xmpp.group.seg_start)
    if((get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, group, XMPP_BUF_SIZE_M)) < 0 )
      return -1;

  sprintf(to,"%s@%s",user,domain);
  snprintf(id, XMPP_BUF_SIZE_XS, "contact_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  xmpp_build_add_contact(id, to, user, group);
  return xmpp_send(cptr, now);
}

int xmpp_accept_contact(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char id[XMPP_BUF_SIZE_XS + 1];

  NSDL2_XMPP(vptr, cptr, "Method called");
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;
  if (!url_num->proto.xmpp.accept_contact)
    return 0;
  
  snprintf(id, XMPP_BUF_SIZE_XS, "auth_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  xmpp_build_accept_contact(vptr->xmpp->uid, id);
  return xmpp_send(cptr, now);
}

int xmpp_send_subscribe(connection *cptr, u_ns_ts_t now)
{
  IW_UNUSED(VUser *vptr = cptr->vptr);
  char to[XMPP_BUF_SIZE_M + 1];
  char id[XMPP_BUF_SIZE_M + 1];
  NSDL2_XMPP(vptr, cptr, "Method called");

  xmpp_get_msg_attr(g_xmpp_read_buffer,"from=", 5, to, XMPP_BUF_SIZE_M);
  xmpp_get_msg_attr(g_xmpp_read_buffer,"id=", 3, id, XMPP_BUF_SIZE_M);
  strcpy(g_xmpp_read_buffer,id);
  g_xmpp_read_buffer_len = strlen(id);
  xmpp_build_subscribe(to,"subscribe");
  return xmpp_send(cptr, now);
}
int xmpp_send_subscribed(connection *cptr, u_ns_ts_t now)
{
  IW_UNUSED(VUser *vptr = cptr->vptr);
  NSDL2_XMPP(vptr, cptr, "Method called");
  xmpp_build_subscribe(g_xmpp_read_buffer,"subscribed");
  return xmpp_send(cptr, now);
}
int xmpp_send_subscription(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char id[XMPP_BUF_SIZE_XS + 1];
  NSDL2_XMPP(vptr, cptr, "Method called");
  snprintf(id, XMPP_BUF_SIZE_XS, "subscribe_ns%dn%du%dp%ds", child_idx, vptr->user_index, vptr->page_instance, vptr->sess_inst);
  xmpp_build_subscribe_contact(id, g_xmpp_read_buffer,"from");
  return xmpp_send(cptr, now);
}

int xmpp_send_result(connection *cptr, u_ns_ts_t now)
{
  IW_UNUSED(VUser *vptr = cptr->vptr);
  NSDL2_XMPP(vptr, cptr, "Method called");
  xmpp_build_result();
  return xmpp_send(cptr, now);
}

int xmpp_send_muc_owner(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char id[XMPP_BUF_SIZE_XS + 1];
  char group[XMPP_BUF_SIZE_XS + 1];
  char recipient[XMPP_BUF_SIZE_XS + 1];
  char service[XMPP_BUF_SIZE_XS + 1];
  char domain[XMPP_BUF_SIZE_XS + 1];
  int group_len;
 
  NSDL2_XMPP(vptr, cptr, "Method called");
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;

  if(get_values_from_segments(cptr, &url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if((group_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, group, XMPP_BUF_SIZE_XS)) < 0)
    return -1;

  if(url_num->proto.xmpp.group.seg_start)
    get_values_from_segments(cptr,&url_num->proto.xmpp.group, service, XMPP_BUF_SIZE_XS);
  else
  {
    strcpy(service, "conference"); 
  }
  snprintf(recipient, XMPP_BUF_SIZE_XS, "%s@%s.%s", group, service[0]?service:"conference", domain);

  snprintf(id, XMPP_BUF_SIZE_XS, "muc_owner_query%dn%du%dp", child_idx, vptr->user_index, vptr->next_pg_id);
  xmpp_build_muc_owner(id, recipient);
  return xmpp_send(cptr, now);
}

int xmpp_send_muc_config(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char id[XMPP_BUF_SIZE_XS + 1];
  char group[XMPP_BUF_SIZE_XS + 1];
  char owner[XMPP_BUF_SIZE_XS + 1];
  char user[XMPP_BUF_SIZE_XS + 1];
  char domain[XMPP_BUF_SIZE_XS + 1];
  char recipient[XMPP_BUF_SIZE_XS + 1];
  char service[XMPP_BUF_SIZE_XS + 1];
  int group_len; 

  NSDL2_XMPP(vptr, cptr, "Method called");
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;

  if(get_values_from_segments(cptr, &url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if(get_values_from_segments(cptr, &url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS) < 0)
    return -1;


  if((group_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, group, XMPP_BUF_SIZE_XS)) < 0)
    return -1;

  if(url_num->proto.xmpp.group.seg_start)
    get_values_from_segments(cptr,&url_num->proto.xmpp.group, service, XMPP_BUF_SIZE_XS);
  else
  {
    strcpy(service, "conference"); 
  }
  snprintf(recipient, XMPP_BUF_SIZE_XS, "%s@%s.%s", group, service[0]?service:"conference", domain);


  snprintf(owner, XMPP_BUF_SIZE_XS, "%s@%s", user,domain);

  snprintf(id, XMPP_BUF_SIZE_XS, "muc_config_query%dn%du%dp", child_idx, vptr->user_index, vptr->next_pg_id);
  xmpp_build_iq_muc_config(id, recipient, group, owner);
  return xmpp_send(cptr, now);
}

int xmpp_add_member(connection *cptr, u_ns_ts_t now)
{

  VUser *vptr = cptr->vptr;
  char domain[XMPP_BUF_SIZE_XS + 1];
  char group[XMPP_BUF_SIZE_XS + 1];
  char user[XMPP_BUF_SIZE_XS + 1];
  char member[XMPP_BUF_SIZE_M + 1];
  int  group_len;
  char service[XMPP_BUF_SIZE_XS + 1]=""; 
  char recipient[XMPP_BUF_SIZE_XS + 1];
  NSDL2_XMPP(vptr, cptr, "Method called");
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;

  if(get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if(get_values_from_segments(cptr, &url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if((group_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, group, XMPP_BUF_SIZE_M)) < 0 )
    return -1;

  if(url_num->proto.xmpp.group.seg_start)
    get_values_from_segments(cptr,&url_num->proto.xmpp.group, service, XMPP_BUF_SIZE_XS);
  else
  {
    strcpy(service,"conference"); 
  }
  snprintf(recipient, XMPP_BUF_SIZE_XS, "%s@%s.%s", group, service[0]?service:"conference", domain);
  
  sprintf(member,"%s@%s",user, domain);
  xmpp_build_message_add_member(recipient, member);
  return xmpp_send(cptr, now);

}

int xmpp_delete_member(connection *cptr, u_ns_ts_t now)
{

  VUser *vptr = cptr->vptr;
  char id[XMPP_BUF_SIZE_XS + 1];
  char user[XMPP_BUF_SIZE_XS + 1];
  char group[XMPP_BUF_SIZE_XS + 1];
  char member[XMPP_BUF_SIZE_XS + 1];
  char domain[XMPP_BUF_SIZE_XS + 1];
  char recipient[XMPP_BUF_SIZE_XS + 1];
  char service[XMPP_BUF_SIZE_XS + 1];
  int group_len;

  NSDL2_XMPP(vptr, cptr, "Method called");
  action_request_Shr* url_num =  vptr->xmpp->first_page_url;

  if(get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.user, user, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if(get_values_from_segments(cptr, &url_num->proto.xmpp.domain, domain, XMPP_BUF_SIZE_XS) < 0)
    return -1;

  if((group_len = get_values_from_segments(cptr, &cptr->url_num->proto.xmpp.group, group, XMPP_BUF_SIZE_M)) < 0 )
    return -1;

  if(url_num->proto.xmpp.group.seg_start)
    get_values_from_segments(cptr,&url_num->proto.xmpp.group, service, XMPP_BUF_SIZE_XS);
  else
  {
    strcpy(service,"conference"); 
  }
  snprintf(recipient, XMPP_BUF_SIZE_XS, "%s@%s.%s", group, service[0]?service:"conference", domain);
  

  snprintf(id, XMPP_BUF_SIZE_XS, "delete_member%dn%du%dp", child_idx, vptr->user_index, vptr->next_pg_id);
  sprintf(member,"%s@%s",user, domain);
  xmpp_build_iq_delete_member(vptr->xmpp->uid, id, recipient, member);
  return xmpp_send(cptr, now);

}

int xmpp_close_disconnect(connection *cptr, u_ns_ts_t now, int close_stream, int close_connection)
{
  VUser *vptr = cptr->vptr;
  int ret = 0;
  NSDL2_XMPP(vptr, cptr, "Method called url_awaited  = %d, close_stream = %d, close_connection = %d",
                                       vptr->urls_awaited, close_stream, close_connection);

  if(vptr->urls_awaited)
    vptr->urls_awaited--;

  if(close_stream)
  {
    cptr->url_num = vptr->xmpp->first_page_url;
    ret = xmpp_close_stream(cptr,now);
    if (ret == -2)
      return ret;  
  }
  if(close_connection)
  {
    Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);
  }
  else
  {
    cptr->conn_state = CNST_REUSE_CON; 
    vptr->last_cptr = cptr;
  }
  vptr->xmpp_cptr = NULL;
  return ret;
}

//This method will use to send xmpp message 
//Called from ns_vuser_tasks.c
int nsi_xmpp_send(VUser *vptr, u_ns_ts_t now)
{
  int ret = -1;
  connection *cptr = vptr->xmpp_cptr;
  NSDL2_XMPP(vptr, cptr, "Method called");

  if(cptr == NULL) 
  { 
    NSTL2(NULL, NULL, "Error: cptr is NULL");  
    NSDL2_XMPP(NULL, NULL, "Error: cptr is NULL"); 
    return -1; 
  } 
   
  if(cptr->conn_fd < 0) 
  { 
    NSTL2(vptr, cptr, "XMPP: cptr->conn_fd is -1"); 
    NSDL2_XMPP(vptr, cptr, "XMPP: cptr->conn_fd is -1"); 
    return -1;  
  }
 
  if(cptr->proto_state != XMPP_CONNECTED)
  {
    NSTL2(vptr, cptr, "XMPP: cptr->proto_state is %d",cptr->proto_state); 
    NSDL2_XMPP(vptr, cptr, "XMPP: cptr->proto_state is %d",cptr->proto_state); 
    return -1;
  }

  cptr->url_num = &request_table_shr_mem[vptr->next_pg_id];
  NSDL2_XMPP(vptr, cptr, "cptr->url_num->proto.xmpp.action = %p, vptr->next_pg_id = %d", cptr->url_num->proto.xmpp.action, vptr->next_pg_id);

  switch(cptr->url_num->proto.xmpp.action)
  {
    case NS_XMPP_SEND_MESSAGE: 
      if(cptr->url_num->proto.xmpp.message.seg_start)
        ret = xmpp_send_message(cptr, now);
      if(cptr->url_num->proto.xmpp.file.seg_start)
      {
        ret = xmpp_send_upload_file_slot(cptr, now);
        //ret = xmpp_item_discovery(cptr, now);
        if(!ret)
           ret = -2; // Partial Complete
      }  
    break;
   
    case NS_XMPP_ADD_CONTACT:
      ret = xmpp_add_contact(cptr,now); 
      if(!ret)
         ret = -2; // Partial Complete
    break;

    case NS_XMPP_DELETE_CONTACT:
      ret = xmpp_delete_contact(cptr,now); 
      if(!ret)
         ret = -2; // Partial Complete
    break;

    case NS_XMPP_CREATE_GROUP:
      ret = xmpp_create_group(cptr, now);
      if(!ret)
         ret = -2; // Partial Complete
    break;

    case NS_XMPP_DELETE_GROUP:
      ret = xmpp_delete_group(cptr, now);
      if(!ret)
         ret = -2; // Partial Complete
    break;

    case NS_XMPP_JOIN_GROUP:
      ret = xmpp_item_discovery(cptr, now);
      if(!ret)
         ret = -2; // Partial Complete
    break;

    case NS_XMPP_LEAVE_GROUP:
     //TODO
     // ret = xmpp_leave_group(cptr, now);
    break;

    case NS_XMPP_ADD_MEMBER:
      ret = xmpp_add_member(cptr, now); 
    break;

    case NS_XMPP_DELETE_MEMBER:
      ret = xmpp_delete_member(cptr, now); 
    break;
  }
  return ret; 
}

int nsi_xmpp_logout(VUser *vptr, u_ns_ts_t now)
{
  connection *cptr = vptr->xmpp_cptr;
  NSDL2_XMPP(vptr, cptr, "Method called");

  if(cptr == NULL)
  {
    NSTL2(NULL, NULL, "Error: cptr is NULL");  
    NSDL2_XMPP(NULL, NULL, "Error: cptr is NULL");
    return -1;
  }
   
  if(cptr->conn_fd < 0)
  {
    NSTL2(vptr, cptr, "XMPP: cptr->conn_fd is -1"); 
    NSDL2_XMPP(vptr, cptr, "XMPP: cptr->conn_fd is -1");
    return -1;
  }

  if(cptr->proto_state != XMPP_CONNECTED)
  {
    NSTL2(vptr, cptr, "XMPP: cptr->proto_state is %d",cptr->proto_state); 
    NSDL2_XMPP(vptr, cptr, "XMPP: cptr->proto_state is %d",cptr->proto_state); 
    return -1;
  }

  return xmpp_close_disconnect(cptr, now, 1, 1);
}

//Called from file page.c  from handle_page_complete()
//file is put at server now get url will send to user/group
int xmpp_file_upload_complete(VUser *vptr, u_ns_ts_t now, int status)
{
  connection *cptr = vptr->xmpp_cptr;
  //int action = cptr->url_num->proto.xmpp.action;
  NSDL2_XMPP(vptr, cptr, "Method called");
 
  if(status == NS_REQUEST_OK)
  {
    vptr->xmpp_status = xmpp_send_url(cptr, now);
  }
  else
  {
    NSDL2_XMPP(NULL,NULL, "File upload failed");
    NSTL2(NULL,NULL, "File upload failed");
    vptr->xmpp_status = -1;
  }
  return do_xmpp_complete(vptr);
}

int xmpp_update_login_stats(VUser *vptr)
{
  int status = vptr->xmpp_status;
  NSDL2_XMPP(NULL,NULL, "At Method, status =  %d", status);

  XMPP_DATA_INTO_AVG(xmpp_login_completed);

  if(status == NS_REQUEST_OK)
  {
    XMPP_DATA_INTO_AVG(xmpp_login_succ);
  }
  else 
  {
    XMPP_DATA_INTO_AVG(xmpp_login_failed);
  }
  return 0;
}

int do_xmpp_complete(VUser *vptr)
{
  connection *cptr = vptr->xmpp_cptr;
  NSDL2_XMPP(vptr, NULL, "Method called");
  if(vptr->vuser_state == NS_VUSER_THINKING) 
  {
    if(cptr->proto_state == XMPP_CONNECTED)
    {
      NSDL2_XMPP(NULL, NULL, "XMPP User is in thinking state");
      return 0; 
    }
    VUSER_THINKING_TO_ACTIVE(vptr); //changing the state of vuser thinking to active
  }

  if(vptr->xmpp_status == -2) 
  {
    vptr->xmpp_status = NS_REQUEST_OK;
    NSDL2_XMPP(NULL, NULL, "XMPP partial done");
  }
  else if(vptr->xmpp_status == -1) 
  {
    vptr->xmpp_status = NS_REQUEST_ERRMISC; 
    NSDL2_XMPP(NULL, NULL, "XMPP failed");
    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      switch_to_vuser_ctx(vptr, "XMPP Failed");
    else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
      send_msg_nvm_to_vutd(vptr, NS_API_XMPP_SEND_REP, -1);
  }
  else
  {
    vptr->xmpp_status = NS_REQUEST_OK;
    NSDL2_XMPP(NULL, NULL, "XMPP success");
    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      switch_to_vuser_ctx(vptr, "XMPP Success");
    else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
      send_msg_nvm_to_vutd(vptr, NS_API_XMPP_SEND_REP, 0);    
  }

  return vptr->xmpp_status;
}
