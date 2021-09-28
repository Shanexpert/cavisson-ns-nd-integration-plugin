/**********************************************************************
 * File Name            : ns_h2_process_resp.c
 * Author(s)            : Sanjana Joshi 
 * Date                 : 19 Dec 2015
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Parsing & Processing HTT2 Header 
                          Received in Response
                         
 * Modification History :
 * Author(s)            :
 * Date                 :
 *Change Description/Location>
 **********************************************************************/

#include "ns_h2_header_files.h"
#include "comp_decomp/nslb_comp_decomp.h"

void process_response_headers(connection *cptr, nghttp2_nv nv , u_ns_ts_t now){

  char buffer[16 + 1];
  int consumed_bytes = 0;
  CacheTable_t *cacheptr;
  VUser *vptr = cptr->vptr;
  NSDL2_HTTP2(NULL, NULL, "Method Called");

  // We will save headers . This is required for search paremeter support 
  if (((vptr->cur_page->save_headers & SEARCH_IN_HEADER)) && (cptr->url_num->proto.http.type == MAIN_URL)) {
    NSDL2_HTTP(NULL, cptr, "Going to save headers");
    save_header(vptr, (char *)nv.name, nv.namelen);
    save_header(vptr, ": ", 2);
    save_header(vptr, (char *)nv.value, nv.valuelen);
  }

  if(((VUser *)(cptr->vptr))->flags & NS_CACHING_ON){
    cacheptr = (CacheTable_t*)cptr->cptr_data->cache_data;
    if (cacheptr)
     cacheptr->cache_flags |=  NS_CACHE_ENTRY_IS_CACHABLE;
  }

  if (!strncmp((char *)nv.name, ":status", 7)){
    NSDL3_HTTP2(NULL, NULL, "Received status in header");
    if(nv.valuelen > 16){
      NSDL2_HTTP2(NULL, NULL, "got more than 16 lenght value in status, it should not happen");
      return;
    }
    strncpy(buffer, (char *)nv.value, nv.valuelen);
    buffer[nv.valuelen] = '\0'; 
    cptr->req_code = atoi(buffer);
    NSDL2_HTTP2(NULL, NULL, "cptr->req_code = %d", cptr->req_code);
  }/*bug 93672: gRPC status handling*/ 
  else if (!strncmp((char *)nv.name, "grpc-status", nv.namelen)){
    strncpy(buffer, (char *)nv.value, nv.valuelen);
    buffer[nv.valuelen] = '\0'; 
    ((VUser*)cptr->vptr)->page_status = atoi(buffer);
    NSDL2_HTTP2(NULL, NULL, " vptr->page_status = %d", ((VUser*)cptr->vptr)->page_status);
    NSTL1(NULL, NULL, " GRPC:  grpc-status = %d", ((VUser*)cptr->vptr)->page_status);
    if(((VUser*)cptr->vptr)->page_status)
      ((VUser*)cptr->vptr)->page_status = NS_REQUEST_ERRMISC;
    NSDL2_HTTP2(NULL, NULL, "now , vptr->page_status = %d", ((VUser*)cptr->vptr)->page_status);
  }else if (!strncmp((char *)nv.name, "grpc-message", nv.namelen)){
    NSTL1(NULL, NULL, " GRPC: grpc-message=%*.*s ", nv.valuelen, nv.valuelen, nv.value);
  } else if (!strncmp((char *)nv.name, "location", 8)) {
    int existing_value_length= 0; 
    int total_value_length = 0;
    NSDL3_HTTP2(NULL, NULL, "Received location in header");
    // Check whether cptr->location_url is empty or not 
    if (cptr->location_url)
      existing_value_length = strlen(cptr->location_url);
    else
      existing_value_length = 0;
    total_value_length = existing_value_length + nv.valuelen;
    if (!(total_value_length)) {
      NSDL2_HTTP2(NULL, NULL, "Warning, getting a NULL value for Redirect LOCATION");
      return;
    }
    // Copy location to cptr 
    MY_REALLOC_EX(cptr->location_url, total_value_length + 1, existing_value_length + 1, "cptr->location_url", -1);
    bcopy(nv.value, cptr->location_url + existing_value_length, nv.valuelen);
    cptr->location_url[total_value_length] = '\0';
    
    // Check request codes   
    if (cptr->req_code == 301 || cptr->req_code == 302 || cptr->req_code == 303 || cptr->req_code == 307 || cptr->req_code == 308
       || runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.save_loc_hdr_on_all_rsp_code) {
      NSDL3_HTTP2(NULL, cptr, "Got LOCATION header (Location = %s). Request URL = %s",
                            cptr->location_url, get_url_req_url(cptr));
    } else {
      // TODO - WARING EVent Log
      fprintf(stderr, "Warning: Got LOCATION header (Location = %s) and response code is not 301, 302, 303, 307 or 308. Request URL = %s\n",
                       cptr->location_url, get_url_req_url(cptr));
      FREE_LOCATION_URL(cptr, 0) // Force Free
    }
  } else if (!strncmp((char *)nv.name, "content-length", 14)) { // Content length header received
    char ptr[64] = "";
    int content_len = 0;
    strncpy(ptr, (char *)(nv.value), nv.valuelen);
    ptr[nv.valuelen] ='\0';
    if (ptr != NULL)
    {
      content_len = atoi(ptr);
    } 
    NSDL2_HTTP2(NULL, NULL, "Received content-length header");
    cptr->content_length = content_len;
  } else if (!strncmp((char *)nv.name, "content-type", 12)) { // Content type received 
    NSDL2_HTTP2(NULL, NULL, "Received content-type header");
    if (!strncmp((char *)nv.value, "application/x-amf", 17)) { // Check for AMF Header
      NSDL2_HTTP2(NULL, NULL, "Received amf");
      cptr->flags |= NS_CPTR_FLAGS_AMF;
    } else if (!(strncmp((char *)nv.value, "x-application/hessian", 21 )) || !(strncmp((char *)nv.value, "application/x-hessian", 21))) {//Hessian Header
      NSDL2_HTTP2(NULL, NULL, "Received hessian");
      cptr->flags |= NS_CPTR_FLAGS_HESSIAN;
    } else if (!strncmp((char *)nv.value, "application/octet-stream", 24)) {
      NSDL2_HTTP2(NULL, NULL, "Received application/octet-stream");
      if (runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED)
      {
        cptr->flags |= NS_CPTR_FLAGS_SAVE_HTTP_RESP_BODY;
      }
      if (global_settings->use_java_obj_mgr)
        cptr->flags |= NS_CPTR_FLAGS_JAVA_OBJ;
    }else if (!(strncmp((char *)nv.value, "application/grpc", 16)))
     {
        if((16 == nv.valuelen) || (!strncmp((char*)nv.value + 16, "+proto", 6)))
          cptr->flags |= NS_CPTR_CONTENT_TYPE_GRPC_PROTO; 
        else if(!strncmp((char*)nv.value + 16, "+json", 5)) 
          cptr->flags |= NS_CPTR_CONTENT_TYPE_GRPC_JSON;
        else
          cptr->flags |= NS_CPTR_CONTENT_TYPE_GRPC_CUSTOM;        
    } 
  } 
  else if ( (!strncmp((char *)nv.name, "content-encoding", 16)) ){
    NSDL3_HTTP2(NULL, NULL, "Received content-encoding header");
   /*When per-RPC compression configuration isn't present for a message, the
     channel compression configuration MUST be used.
    Otherwise grpc-encoding MUST be used*/ 
   if(!(cptr->flags & NS_CPTR_GRPC_ENCODING)) {
    // Check if value gzip.
     if (!strncmp((char*)nv.value, "gzip", 4)) {
       cptr->compression_type = NSLB_COMP_GZIP;
      } else if (!strncmp((char*)nv.value, "deflate", 7)) {
         cptr->compression_type = NSLB_COMP_DEFLATE;
       } else if (!strncmp((char*)nv.value, "br", 2)) {
        cptr->compression_type = NSLB_COMP_BROTLI;
      }
    }
  }else if ((!strncmp((char *)nv.name, "grpc-encoding", 13))){
    NSDL3_HTTP2(NULL, NULL, "Received  grpc-encoding header");
    cptr->flags |= NS_CPTR_GRPC_ENCODING;
    // Check if value gzip.
    if (!strncmp((char*)nv.value, "gzip", 4)) {
      cptr->compression_type = NSLB_COMP_GZIP;
    } else if (!strncmp((char*)nv.value, "deflate", 7)) {
      cptr->compression_type = NSLB_COMP_DEFLATE;
    } else if (!strncmp((char*)nv.value, "br", 2)) {
      cptr->compression_type = NSLB_COMP_BROTLI;
    }
  } else if (!strncmp((char *)nv.name, "cache-control", 13)) {
    NSDL2_HTTP2(NULL, NULL, "cache-control Header received");
    http2_cache_save_header(cptr, (char *)nv.value, nv.valuelen + 1, CACHE_CONTROL);
  } else if (!strncmp((char *)nv.name, "date", 4)) {
    NSDL2_HTTP2(NULL, NULL, "date Header Received");
    http2_cache_save_header(cptr, (char *)nv.value, nv.valuelen + 1, CACHE_DATE);
  } else if (!strncmp((char *)nv.name, "etag", 4)) {
    NSDL3_HTTP2(NULL, NULL, "etag Header Received");
    http2_cache_save_header(cptr, (char *)nv.value, nv.valuelen + 1, CACHE_ETAG);
  } else if (!strncmp((char *)nv.name, "age", 3 )) {
    NSDL3_HTTP2(NULL, NULL, "age Header Received");
    http2_cache_save_header(cptr, (char *)nv.value, nv.valuelen + 1, CACHE_AGE);
  } else if (!strncmp((char *)nv.name, "last-modified", 13)) {
    NSDL3_HTTP2(NULL, NULL, "last-modified Header Received");
    http2_cache_save_header(cptr, (char *)nv.value, nv.valuelen + 1, CACHE_LMD); 
  } else if (!strncmp((char *)nv.name, "expires", 7)) {
    NSDL3_HTTP2(NULL, NULL, "expires Header Received"); 
    http2_cache_save_header(cptr, (char *)nv.value, nv.valuelen + 1, CACHE_EXPIRES);
  } else if (!strncmp((char *)nv.name, "pragma", 6)) {
    NSDL2_HTTP2(NULL, NULL, "pragma Header received");
    http2_cache_save_header(cptr, (char *)nv.value, nv.valuelen + 1, CACHE_PRAGMA);
  } else if (!strncmp((char *)nv.name, "transfer-encoding", 17)) {
    NSDL3_HTTP2(NULL, NULL, "transfer-encoding Header received");
    if (cptr->req_code != 204)
    {
      cptr->chunked_state = CHK_SIZE;
      cptr->content_length = 0;
    } 
    else 
    NSDL1_HTTP(NULL, cptr, "Got Transfer chunked encoding with status code %d, ignoring chunked header.", cptr->req_code);
  } else if (!strncmp((char *)nv.name, "set-cookie", 10)) {
    NSDL3_HTTP2(NULL, NULL, "set-cookie Header received");
    /*On passing nv.valuelen +1 , cookie header was not getting set properly bug 40831, domain_len was incremented by 1 in case
     example - set-cookie: shippingCountry=IN; path=/; domain=.macys.com domain_len was set to 11 instead of 10 in get_cookie_attribute_value
    */
    if(cptr->url)
    save_auto_cookie((char *)nv.value, nv.valuelen , cptr, 1);
  } else if (!strncmp((char *)nv.name, "www-authenticate", 16)) {
    NSDL3_HTTP2(NULL, NULL, "www-autheticate Header Received");
    if (cptr->req_code != 401) 
      NSDL2_AUTH(vptr, cptr, "Ignoring 'www-authenticate' Header as response code is not 401");
    else if (cptr->proto_state == ST_AUTH_HANDSHAKE_FAILURE) 
       NSDL2_AUTH(vptr, cptr, "Ignoring 'www-authentcate' Header as there was Authenticate handshake failed");
    else 
    {
      cptr->flags |= NS_CPTR_AUTH_HDR_RCVD; 
      parse_authenticate_hdr(cptr, (char *)nv.value, nv.valuelen + 1, &consumed_bytes, now);
    }
  } else if (!strncmp((char *)nv.name, "proxy-authenticate", 18)) {
    NSDL3_HTTP2(NULL, NULL, "proxy-autheticate Header Received");
     if (cptr->req_code != 407)
      NSDL2_AUTH(vptr, cptr, "Ignoring 'proxy-authenticate' Header as response code is not 401");
    else if (cptr->proto_state == ST_AUTH_HANDSHAKE_FAILURE)
       NSDL2_AUTH(vptr, cptr, "Ignoring 'proxy-authentcate' Header as there was Authenticate handshake failed");
    else
      parse_authenticate_hdr(cptr, (char *)nv.value, nv.valuelen + 1, &consumed_bytes, now);
      cptr->flags |= NS_CPTR_FLAGS_CON_PROXY_AUTH;
  } else if ((!strncmp((char *)nv.name, "x-cache", 7)) || (!strncmp((char *)nv.name, "x-cache-remote", 14)) || 
             (!strncmp((char *)nv.name, "cf-cache-status", 15))) {
    NSDL3_HTTP2(NULL, NULL, "x-cache Header Received");
    if (!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.enable_network_cache_stats)
    { 
      NSDL3_HTTP2(NULL, NULL, "Ignoring Network cache Header as newtwork cache is not enable");
      return ;
    }  
    if (!cptr->cptr_data)
    {
      MY_MALLOC(cptr->cptr_data, sizeof(cptr_data_t), "cptr data", -1);
      memset(cptr->cptr_data, 0, sizeof(cptr_data_t));
    }
    nw_cache_stats_headers_parse_set_value((char *)nv.value, nv.valuelen, cptr, &(cptr->cptr_data->nw_cache_state)); 
 
  } else if (!strncmp((char *)nv.name, "x-check-cacheable", 17)) {
    NSDL3_HTTP2(NULL, NULL, "x-check-cacheable Header Received");
    if (!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.enable_network_cache_stats)
    {
      NSDL2_REPORTING(vptr, cptr, "Ignoring Network cache Header as newtwork cache is not enable");
    }
    if (!cptr->cptr_data)
    {
      MY_MALLOC(cptr->cptr_data, sizeof(cptr_data_t), "cptr data", -1);
      memset(cptr->cptr_data, 0, sizeof(cptr_data_t));
    }

    nw_cache_stats_chk_cacheable_header_parse_set_val((char *)nv.value, nv.valuelen, cptr);
    
  } else if (!strncmp((char *)nv.name, "connection", 7)) {
    NSDL3_HTTP2(NULL, NULL, "connection Header Received");
    if (!strncmp((char *)nv.value, "close", 5)) {
      cptr->connection_type = NKA;
    }
  } else if (!strncmp((char *)nv.name, "link", 4)) {
    int existing_value_length = 0; 
    int total_value_length = 0;
    NSDL3_HTTP2(NULL, NULL, "link Header Received");
   
    if (cptr->link_hdr_val)
      existing_value_length = strlen (cptr->link_hdr_val);
    else
      existing_value_length = 0;
    total_value_length = nv.valuelen + existing_value_length;
    if (!(total_value_length)) {
      fprintf(stderr, "Warning, getting a NULL value for Link header\n");
    }
    
    MY_REALLOC_EX(cptr->link_hdr_val, total_value_length + 1, existing_value_length + 1, "cptr->link_hdr_val", -1);
    bcopy(nv.value, cptr->link_hdr_val + existing_value_length, nv.valuelen);
    cptr->link_hdr_val[total_value_length] = '\0';
 } else if (!strncmp((char *)nv.name, "x-dynatrace", 11)) {
   NSDL3_HTTP2(NULL, NULL, "connection Header Received");
   if (!cptr->x_dynaTrace_data)
   {
     MY_MALLOC(cptr->x_dynaTrace_data, sizeof(x_dynaTrace_data_t), "x_dynaTrace_data", -1);
     memset(cptr->x_dynaTrace_data, 0, sizeof(x_dynaTrace_data_t));
   }
   x_dynaTrace_data_t *x_dynaTrace_data = cptr->x_dynaTrace_data;

   if (!(nv.valuelen + x_dynaTrace_data->use_len)) {
     fprintf(stderr, "Warning, getting a NULL value for X-Dynatrace header.\n");
     return; 
   }
   int prev_length = x_dynaTrace_data->len;
   x_dynaTrace_data->use_len += nv.valuelen;
   if (x_dynaTrace_data->use_len >= x_dynaTrace_data->len)
   {
     x_dynaTrace_data->len = x_dynaTrace_data->use_len + 1;
     MY_REALLOC_EX(x_dynaTrace_data->buffer, x_dynaTrace_data->len, prev_length, "x_dynaTrace_data->buffer", -1);
   }

   bcopy(nv.value, x_dynaTrace_data->buffer + prev_length, nv.valuelen);
   x_dynaTrace_data->buffer[x_dynaTrace_data->use_len] = '\0';
   NSDL2_HTTP2(NULL, cptr, "Final X-Dynatrace hader value (%s) got, Setting header state to HDST_CR",
                        cptr->x_dynaTrace_data->buffer);
 }
    
}
