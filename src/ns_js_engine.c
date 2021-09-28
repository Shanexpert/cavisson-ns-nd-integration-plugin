/******************************************************************
 * Name                 : ns_js_engine.c 
 * Purpose              : Main engine of Java Script 
 * Note                 :
 * Initial Version      : Sun Feb 20 13:01:16 IST 2011
 * Modification History :
 ******************************************************************/

#include <regex.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "decomp.h"

#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_sock_list.h"
#include "ns_msg_com_util.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "amf.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "ns_sock_com.h"
#include "netstorm_rmi.h"
#include "ns_child_msg_com.h"
#include "ns_log.h"
#include "ns_log_req_rep.h"
#include "ns_ssl.h"
#include "ns_wan_env.h"
#include "ns_url_req.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_auto_redirect.h"
#include "ns_replay_access_logs.h"
#include "ns_vuser.h"
#include "ns_schedule_phases_parse.h"
#include "ns_gdf.h"
#include "ns_schedule_pause_and_resume.h"
#include "ns_page.h"
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include "nslb_util.h"
#include "comp_decomp/nslb_comp_decomp.h"
#include "ns_dns.h"
#include "ns_event_log.h"
#include "ns_keep_alive.h"
#include "ns_event_id.h"
#include "ns_http_process_resp.h"
#include "ns_http_pipelining.h"
#include "ns_js.h"
#include "ns_http_cache.h"
#include "ns_http_cache_reporting.h"
#include "ns_http_cache_store.h"
#include "ns_js_events.h"
#include "ns_auto_fetch_embd.h"
#include "ns_auto_cookie.h"
#include "ns_click_script_parse.h"
#include "ns_click_script.h"
#include "nslb_cav_conf.h"

static char *js_url_resp_buff = NULL;
static int js_url_resp_size = 0;

/* JS error logging and event log */
static connection *cptr_for_js_error;
static int js_id; // Counter
static int embedded_js_id; // Counter
static int included_js_id; // Counter
static char *embedded_js_url; // Embedded URL

#define JS_NEWSTRING_COPYN(js_context, elm, elm_len) { \
    str = JS_NewStringCopyN(js_context, elm, elm_len); \
    if(str == NULL)                                   \
    {                                                 \
      NS_EL_3_ATTR(EID_JS_NEW_STRING_COPY, vptr->user_index,           \
                                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR, \
                                  vptr->sess_ptr->sess_name,                \
                                  vptr->cur_page->page_name,                \
                                  (char*)__FUNCTION__,                      \
                                   "Error in copy JS new string. Element = %s, element length = %d", elm, elm_len);\
      return -1;                                      \
    }                                                 \
  }


/* The class of the global object. */
static JSClass global_class = {
      "global", JSCLASS_GLOBAL_FLAGS,
      JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
      JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
      JSCLASS_NO_OPTIONAL_MEMBERS
};

#ifdef NS_DEBUG_ON
/******************************************************************************
 * ns_js_debug_callback()
 *   This method is called from __ns_js_debug_print from any js file (ns_bootstrap.js)
 *   It prints the string in debug log passed in __ns_js_debug_print() from JS
 * *******************************************************************************/
static JSBool ns_js_debug_callback(JSContext *cx, JSObject *g, uintN argc, jsval *argv, jsval *rval)
{
  const char *native_str;

  JSString *str = JS_ValueToString(cx, argv[0]);

  native_str = JS_GetStringBytes(str);

  NSDL2_JAVA_SCRIPT(NULL, NULL, "JS DBG LOG: '%s'", native_str?native_str:"NULL");

  *rval= STRING_TO_JSVAL ("\0");
  return JS_TRUE;

}

/* List of mapping JS functions with corresponding native c function.
 * This is used in JS_DefineFunctions() which is called in
 * process_buffer_in_js() after initializing JS (creating JS context, gloabl
 * object and init JS Classes
 */
static JSFunctionSpec myjs_global_functions[] = {
    {"__ns_js_debug_print_callback", ns_js_debug_callback, 1},
    {0}
};

#endif


static void *create_dom_from_html_stream(connection *cptr, char *stream, int stream_len) {

  VUser *vptr = cptr->vptr;
  htmlParserCtxtPtr ctxt;
  xmlDocPtr doc;
  //char *page_buffer = html_str;
  char *page_buffer = stream;

  NSDL2_JAVA_SCRIPT(NULL, cptr, "Method called, stream_len = %d, stream = [%*.*s]",
                          stream_len, stream_len, stream_len, stream);

  ctxt = htmlCreateMemoryParserCtxt(page_buffer, stream_len);
  if (ctxt == NULL) {
    NS_EL_2_ATTR(EID_JS_XML_ERROR, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  "XML Error: Failed to create parser context using htmlCreateMemoryParserCtxt()!");
    return NULL;
  }

  // Step2 - //Parse a Chunk of memory
  int ret;  
  if((ret = htmlParseChunk(ctxt, page_buffer, stream_len, 1)) != 0) {
     /* We can ignore it for now as we have not issue here: Need to Check
     NS_EL_2_ATTR(EID_JS_XML_ERROR, vptr->user_index,
                                   vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                   vptr->sess_ptr->sess_name,
                                   vptr->cur_page->page_name,
                                   "XML Error: Failed to parse chunk. Error = %d", ret);
    */
    //return NULL; 
  }
  // Step3 - Free all the memory used by a parser context. However the parsed document in ctxt->myDoc is not freed.
  // Step4 - Get the root element of the document
  doc = ctxt->myDoc;
  htmlFreeParserCtxt(ctxt);

  if (doc == NULL ) {
    NS_EL_2_ATTR(EID_JS_XML_ERROR, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  "XML Error: XML buffer not parsed successfully.");
    return NULL;
  }

  return (void *)doc;
}


//Free doc and parser ctxt
static void js_close_parser(void * html_doc) {

  xmlDocPtr doc = (xmlDocPtr) html_doc; 

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method called, doc = %p", doc);
  if(doc ==NULL)
    return;

  xmlCleanupParser();  //To cleanup function for the XML library. 
  NSDL2_JAVA_SCRIPT(NULL, NULL, "After xmlCleanupParser(), doc = %p", doc);

  xmlMemoryDump();     //Dump in-extenso the memory blocks allocated to the file .memorylist
  NSDL2_JAVA_SCRIPT(NULL, NULL, "After xmlMempryDump(), doc = %p", doc);

  xmlFreeDoc(doc);     //To free up all the structures used by a document, tree included
  NSDL2_JAVA_SCRIPT(NULL, NULL, "After xmlFreeDoc(doc), doc = %p", doc);
  
}


void inline free_dom_and_js_ctx(VUser *vptr)
{

  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, vptr=%p", vptr);
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, closing parser and context destroy started at %lu", get_ms_stamp());

 if (vptr->httpData->js_context != NULL)
  {
     
    NSDL4_JAVA_SCRIPT(vptr, NULL, "Destroying JS context: vptr->httpData->js_context = %p", vptr->httpData->js_context);
    JS_DestroyContext(vptr->httpData->js_context);
    vptr->httpData->js_context = NULL;
  }

  if (vptr->httpData->ptr_html_doc != NULL)
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "Closing DOM Tree: vptr->httpData->ptr_html_doc = %p", vptr->httpData->ptr_html_doc);
    js_close_parser(vptr->httpData->ptr_html_doc);
    vptr->httpData->ptr_html_doc = NULL;
  }

  vptr->httpData->global = NULL;
}

static void inline free_javascript_data_in_vptr(VUser *vptr)
{

  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, vptr=%p", vptr);

  free_dom_and_js_ctx(vptr);

  NSDL4_JAVA_SCRIPT(vptr, NULL, "Freeing malloc'ed url string: vptr->httpData->clicked_url = %p", vptr->httpData->clicked_url);
  FREE_AND_MAKE_NULL(vptr->httpData->clicked_url, "vptr->httpData->clicked_url", 0);

  vptr->httpData->clicked_url_len = -1;
  vptr->httpData->clickaction_id = -1;
  vptr->httpData->server_port = -1;

  NSDL4_JAVA_SCRIPT(vptr, NULL, "Freeing malloc'ed url string: vptr->httpData->server_hostname = %p", 
                                   vptr->httpData->server_hostname);

  FREE_AND_MAKE_NULL(vptr->httpData->server_hostname, "vptr->httpData->server_hostname", 0);
  vptr->httpData->server_hostname_len = -1;

  NSDL4_JAVA_SCRIPT(vptr, NULL, "Freeing malloc'ed url string: vptr->httpData->post_body = %p", 
                                   vptr->httpData->post_body);

  FREE_AND_MAKE_NULL(vptr->httpData->post_body, "vptr->httpData->post_body", 0);

  vptr->httpData->request_type = -1;
  vptr->httpData->post_body_len = -1;
  vptr->httpData->http_method = -1;

  FREE_AND_MAKE_NULL(vptr->httpData->formencoding, "vptr->httpData->formencoding", 0);
  vptr->httpData->formenc_len = -1;

}

inline static void ns_check_length (char *element, int *elem_len)
{
  NSDL1_JAVA_SCRIPT(NULL, NULL, "Method called");

  if(element)
    *elem_len = strlen(element);
  else
    *elem_len = 0;
  NSDL4_JAVA_SCRIPT(NULL, NULL, "element length = %d", *elem_len);
}

/* List of wrapper js functions created dynamically for functions in html tag
 * eg - <img id="test" onclick="func()" src=abc.gif /> */
static char function_script[MAX_JS_FUNC_WRAPPER_SCRIPT_LEN];

/* Get the root from the xml-doc */
inline xmlNode * html_parse_get_root(void *doc) {

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called");
  return xmlDocGetRootElement((xmlDocPtr)doc);
}

/* Execute Inline Script in the JS context */
int ns_js_feed_inline_script(void *c, void *g, char *script_str) {
JSContext *js_context = (JSContext *)c;
JSObject  *global = (JSObject *)g;
jsval rval;
  VUser *vptr = cptr_for_js_error->vptr;

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called with script: %s", script_str);
  
  included_js_id++;
  js_id = included_js_id;
 
  
  if (!JS_EvaluateScript(js_context, global, script_str, strlen(script_str),
                     __FILE__, __LINE__, &rval))
  {
    // NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error in running inline java script code. Script: %s", script_str);
    NS_EL_3_ATTR(EID_JS_EVALUATE_SCRIPT, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  (char*)__FUNCTION__,
                                 "Error in running inline java script code. Script: %s", script_str);

    NSDL2_JAVA_SCRIPT(NULL, NULL, "Error in running inline java script code. Script: %s", script_str);
    return -1;
  }

  return 0;
}

#ifdef NS_DEBUG_ON
static inline void  ns_js_set_debug_flag_on(JSContext *js_context, JSObject *global)
{

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called");
  jsval rval;
  VUser *vptr = cptr_for_js_error->vptr;

  if (!JS_CallFunctionName(js_context, global, "__ns_js_set_debug_flag_on", 0, &rval, &rval)) {
    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  (char*)__FUNCTION__,
                                  "Error Setting the debug flag in Javascript");

    NSDL2_JAVA_SCRIPT(NULL, NULL, "Error Setting the debug flag in Javascript");
  }
}
#endif

/* Add element to DOM tree in the JS context */
int ns_js_set_user_agent(void *c, void *g, char *ua) {
  JSContext *js_context = (JSContext *)c;
  JSObject  *global = (JSObject *)g;
  jsval rval;
  jsval argv[1];
  JSString *str;
  int elm_len = 0;
  VUser *vptr = cptr_for_js_error->vptr;

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, ua = '%s'", ua?ua:"NULL");

  ns_check_length(ua, &elm_len);
  //str = JS_NewStringCopyN(js_context, id, elm_len);
  JS_NEWSTRING_COPYN(js_context, ua, elm_len);
  argv[0] = STRING_TO_JSVAL(str);

  if (!JS_CallFunctionName(js_context, global, "__ns_js_set_user_agent_cglue", 1, argv, &rval)) {
    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  (char*)__FUNCTION__,
                                  "Error Setting up User Agent JS, UA = '%s'", ua?ua:"NULL");

    NSDL2_JAVA_SCRIPT(NULL, NULL, "Error Setting up User Agent JS, UA = '%s'", ua?ua:"NULL");

    return -1;
  } else {

    NSDL2_JAVA_SCRIPT(NULL, NULL, "User Agent set in JS, UA = '%s'", ua?ua:"NULL");
  }

  return 0;
}


/* Add element to DOM tree in the JS context */
int ns_js_feed_element(void *c, void *g, char *id, char *value) {
JSContext *js_context = (JSContext *)c;
JSObject  *global = (JSObject *)g;
jsval rval;
jsval argv[2];
JSString *str;
 int elm_len = 0;
  VUser *vptr = cptr_for_js_error->vptr;

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, Id = %s, Value = %s", id, value);

  ns_check_length(id, &elm_len);
  //str = JS_NewStringCopyN(js_context, id, elm_len);
  JS_NEWSTRING_COPYN(js_context, id, elm_len);
  argv[0] = STRING_TO_JSVAL(str);
  ns_check_length(value, &elm_len);
 // str = JS_NewStringCopyN(js_context, value, elm_len);
  JS_NEWSTRING_COPYN(js_context, value, elm_len);
  argv[1] = STRING_TO_JSVAL(str);
  if (!JS_CallFunctionName(js_context, global, "__ns_js_add_element_cglue", 2, argv, &rval)) {
   // NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error Setting up element in JS, ID = %s, Value = %s", id, value);
    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  (char*)__FUNCTION__,
                                  "Error Setting up element in JS, ID = %s, Value = %s", id, value);

    NSDL2_JAVA_SCRIPT(NULL, NULL, "Error Setting up element in JS, ID = %s, Value = %s", id, value);
    return -1;
  } else {
//    NSDL2_JAVA_SCRIPT(NULL, NULL, "DOM Added Element ID %s Value %s\n", id, value);
    NSDL2_JAVA_SCRIPT(NULL, NULL, "DOM Added Element ID %s", id);
  }

  return 0;
}

/* Add form element to DOM tree in the JS context */
int ns_js_feed_form_element(void *c, void *g, char *id, char *name, char *action, char *method, char *value) {
JSContext *js_context = (JSContext *)c;
JSObject  *global = (JSObject *)g;
jsval rval;
jsval argv[5];
JSString *str;
int elm_len = 0;
  VUser *vptr = cptr_for_js_error->vptr;

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, id = %p '%s', "
                                    "name = %p, '%s', "
                                    "action = %p, '%s', "
                                    "method = %p, '%s', "
                                    "value = %p, '%s'", 
                                    id, id?id:"null",
                                    name, name?name:"null",
                                    action, action?action:"null",
                                    method, method?method:"null",
                                    value, value?value:"null");

  ns_check_length(id, &elm_len);
  JS_NEWSTRING_COPYN(js_context, id, elm_len);
  argv[0] = STRING_TO_JSVAL(str);

  ns_check_length(name, &elm_len);
  JS_NEWSTRING_COPYN(js_context, name, elm_len);
  argv[1] = STRING_TO_JSVAL(str);

  ns_check_length(action, &elm_len);
  JS_NEWSTRING_COPYN(js_context, action, elm_len);
  argv[2] = STRING_TO_JSVAL(str);

  ns_check_length(method, &elm_len);
  JS_NEWSTRING_COPYN(js_context, method, elm_len);
  argv[3] = STRING_TO_JSVAL(str);

  ns_check_length(value, &elm_len);
  JS_NEWSTRING_COPYN(js_context, value, elm_len);
  argv[4] = STRING_TO_JSVAL(str);

  if (!JS_CallFunctionName(js_context, global, "__ns_js_add_form_element_cglue", 5, argv, &rval)) {

      NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  (char*)__FUNCTION__,
                                  "Error Setting up form element in JS, id = %p, '%s', "
                                  "name = %p, '%s', "
                                  "action = %p, '%s', "
                                  "method = %p, '%s', "
                                  "value = %p, '%s'", 
                                  id, id?id:"null",
                                  name, name?name:"null",
                                  action, action?action:"null",
                                  method, method?method:"null",
                                  value, value?value:"null");

    NSDL4_JAVA_SCRIPT(NULL, NULL, "Error Setting up form element in JS, id = %p, '%s', "
                                  "name = %p, '%s', "
                                  "action = %p, '%s', "
                                  "method = %p, '%s', "
                                  "value = %p, '%s'", 
                                  id, id?id:"null",
                                  name, name?name:"null",
                                  action, action?action:"null",
                                  method, method?method:"null",
                                  value, value?value:"null");

    return -1;

  } else {

    NSDL4_JAVA_SCRIPT(NULL, NULL, "DOM Added Form Element JS, id = %p, '%s', "
                                  "name = %p, '%s', "
                                  "action = %p, '%s', "
                                  "method = %p, '%s', "
                                  "value = %p, '%s'", 
                                  id, id?id:"null",
                                  name, name?name:"null",
                                  action, action?action:"null",
                                  method, method?method:"null",
                                  value, value?value:"null");

  }

  return 0;
}


/* ns_js_feed_form_input_element() - Adds form input element of DOM tree in the JS 
 * Takes care of 
 *  - 'input' tag type elements, (checked argument is for radio group, checbox and select option) 
 *  - 'text' tag tpe elements ('TEXTAREA' in ns_text_area()) and 
 *  - 'select' tag type elements ('SELECT' in ns_list(); 'checked' argument is selected attribute
 *                                 and multiple flag is 1 if 'multiple' attribute is defined in tag 
 **/
int ns_js_feed_form_input_element(void *c, void *g, xmlNode *form_node_ptr, char *id, char *name, char *type, char *value, char *content, int checked, int multiple) {

JSContext *js_context = (JSContext *)c;
JSObject  *global = (JSObject *)g;
jsval rval;
jsval argv[8];
JSString *str;

int elm_len = 0;
  VUser *vptr = cptr_for_js_error->vptr;
  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, "
                     "form_node_ptr = %p, "
                     "id = %p, '%s', "
                     "name = %p, '%s', "
                     "type = %p, '%s', "
                     "value = %p, '%s'"
                     "content = %p, '%s'"
                     "checked = %d, multiple = %d",
                     form_node_ptr,
                     id, id==NULL?"null":id,
                     name, name==NULL?"null":name,
                     type, type==NULL?"null":type,
                     value, value==NULL?"null":value,
                     content, content==NULL?"null":content, 
                     checked, multiple);


/* pointer typcast to long and stored in local var. then long type cast to double. 
 * This is done as gcc compiler was giving error when pointer was being type cast to double */
  long tmp = (long)form_node_ptr;
  JS_NewNumberValue(js_context, (jsdouble) tmp, &argv[0]);
  
  ns_check_length(id, &elm_len);
  JS_NEWSTRING_COPYN(js_context, id, elm_len);
  argv[1] = STRING_TO_JSVAL(str);

  ns_check_length(name, &elm_len);
  JS_NEWSTRING_COPYN(js_context, name, elm_len);
  argv[2] = STRING_TO_JSVAL(str);

  ns_check_length(type, &elm_len);
  JS_NEWSTRING_COPYN(js_context, type, elm_len);
  argv[3] = STRING_TO_JSVAL(str);

  ns_check_length(value, &elm_len);
  JS_NEWSTRING_COPYN(js_context, value, elm_len);
  argv[4] = STRING_TO_JSVAL(str);

  ns_check_length(content, &elm_len);
  JS_NEWSTRING_COPYN(js_context, content, elm_len);
  argv[5] = STRING_TO_JSVAL(str);

  JS_NewNumberValue(js_context, (jsdouble) checked, &argv[6]);

  JS_NewNumberValue(js_context, (jsdouble) multiple, &argv[7]);

  if (!JS_CallFunctionName(js_context, global, "__ns_js_add_form_input_element_cglue", 8, argv, &rval)) {

    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                       vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                       vptr->sess_ptr->sess_name,
                       vptr->cur_page->page_name,
                       (char*)__FUNCTION__,
                       "Error Setting up form input element in JS, "
                       "form_node_ptr = %p, "
                       "id = %p, '%s', "
                       "name = %p, '%s', "
                       "type = %p, '%s', "
                       "value = %p, '%s'"
                       "content = %p, '%s'"
                       "checked = %d",
                       form_node_ptr,
                       id, id==NULL?"null":id,
                       name, name==NULL?"null":name,
                       type, type==NULL?"null":type,
                       value, value==NULL?"null":value,
                       content, content==NULL?"null":content, 
                       checked);

    NSDL2_JAVA_SCRIPT(NULL, NULL, "Error Setting up form input element in JS, "
                       "form_node_ptr = %p, "
                       "id = %p, '%s', "
                       "name = %p, '%s', "
                       "type = %p, '%s', "
                       "value = %p, '%s'"
                       "content = %p, '%s'"
                       "checked = %d",
                       form_node_ptr,
                       id, id==NULL?"null":id,
                       name, name==NULL?"null":name,
                       type, type==NULL?"null":type,
                       value, value==NULL?"null":value,
                       content, content==NULL?"null":content,
                       checked);


    return -1;
  } else {
    NSDL2_JAVA_SCRIPT(NULL, NULL, "Added Form Input Element in JS, "
                       "form_node_ptr = %p, "
                       "id = %p, '%s', "
                       "name = %p, '%s', "
                       "type = %p, '%s', "
                       "value = %p, '%s'"
                       "content = %p, '%s'"
                       "checked = %d",
                       form_node_ptr,
                       id, id==NULL?"null":id,
                       name, name==NULL?"null":name,
                       type, type==NULL?"null":type,
                       value, value==NULL?"null":value,
                       content, content==NULL?"null":content,
                       checked);


  }

  return 0;
}

/* Get element from  DOM tree in the JS context */
char * ns_js_get_element(void *c, void *g, char *id) {
  JSContext *js_context = (JSContext *)c;
  JSObject  *global = (JSObject *)g;
  jsval rval;
  jsval argv[1];
  JSBool ok;
  JSString *str;
  int elm_len = 0;

  VUser *vptr = cptr_for_js_error->vptr;
  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, Id = %s", id);

  ns_check_length(id, &elm_len);
  str = JS_NewStringCopyN(js_context, id, elm_len);
  if(str == NULL)                                   
  {                                                 
    NS_EL_3_ATTR(EID_JS_NEW_STRING_COPY, vptr->user_index,           
                                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR, 
                                  vptr->sess_ptr->sess_name,                
                                  vptr->cur_page->page_name,                
                                  (char*)__FUNCTION__,                      
                                   "Error in copy JS new string. Element = %s, element length = %d", id, elm_len);
      return NULL;                                      
  }                                                 
  argv[0] = STRING_TO_JSVAL(str);
  ok = JS_CallFunctionName(js_context, global, "__ns_js_get_element_cglue", 1, argv, &rval); 
  if (!ok) {
    //  NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error Getting element from JS");
      NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  (char*)__FUNCTION__,
                                  "Error Getting element from JS");
      NSDL2_JAVA_SCRIPT(NULL, NULL, "Error Getting element from JS");
      return NULL;
  }
  str = JS_ValueToString(js_context, rval);
  NSDL2_JAVA_SCRIPT(NULL, NULL, "GET ID %s Value is: %s\n",id, JS_GetStringBytes(str));
  return JS_GetStringBytes(str);
}

static char *js_cache_get_resp_buffer(connection *cptr, char *url, int *url_resp_len) {

  VUser *vptr = cptr->vptr;
  CacheTable_t *cacheptr = NULL;
  CacheTable_t *master_cacheptr = NULL;
  CacheTable_t *prev_cache_link = NULL;
  CacheTable_t *prev_master_cache_link = NULL;
  int ihashIndex;
  unsigned int ihashValue;
  int body_offset;
  int compression_type;

  u_ns_ts_t lol_search_url_start;
  u_ns_4B_t lol_search_url_time;
  u_ns_4B_t cache_search_url_time_min = 0xFFFFFFFF;
  u_ns_4B_t cache_search_url_time_max = 0;
  uncomp_cur_len = 0;

  NSDL2_JAVA_SCRIPT(NULL, cptr, "Method Called, url = %s", url);

  lol_search_url_start = get_ms_stamp();

  // Check if URL is in the user cache table. If not then we cannot do any processing
  //cacheptr = cache_url_found(vptr, (unsigned char *)url, strlen(url),
   //                          &prev_cache_link, &ihashIndex, &ihashValue, HTTP_METHOD_GET, USER_NODE);

  cacheptr = cache_url_found(vptr, (unsigned char *)url, strlen(url),
                             &prev_cache_link, &ihashIndex, &ihashValue, cptr->url_num->proto.http.http_method, USER_NODE);

  lol_search_url_time = (u_ns_4B_t)(get_ms_stamp() - lol_search_url_start);
  
  /*We are calculating timing but not including in ourr grafhs 
 *  bcoz doing this we have to adjust url_search also.*/
  if(lol_search_url_time < cache_search_url_time_min)
    cache_search_url_time_min = lol_search_url_time;
  if(lol_search_url_time > cache_search_url_time_max)
    cache_search_url_time_max = lol_search_url_time;

   NSDL1_JAVA_SCRIPT(vptr, NULL, "Time taken to search URL in Cache for JS = %'.3f seconds, min = %'.3f, max = %'.3f", (double )lol_search_url_time/1000.0, (double )cache_search_url_time_min/1000.0, (double )cache_search_url_time_max/1000.0);
 
  if(cacheptr == NULL) {
    NS_EL_2_ATTR(EID_JS_URL_NOT_IN_CACHE, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  "Url [%s] is not found in the cache table.", url);
    return NULL;
  }
 
  unsigned int blen;
  int i;
  int complete_buffers;
  int incomplete_buf_size;
  struct copy_buffer *buffer;
  char *copy_cursor;

  // This URL should not be main as it is  included in the main page as JS file
  // If Master mode, then get repsonse from master cache table
  if(runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode == 1 && cptr->url_num->proto.http.type != MAIN_URL){
    NSDL2_CACHE(vptr, cptr, "About to call get_cache_node() for master node");
    if(cacheptr->ihashMasterIdx == -1){ // Safety check
      NS_EL_2_ATTR(EID_JS_URL_NOT_IN_CACHE, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  "Master table hash index [%d] is not valid for Url [%s] ", 
                                  cacheptr->ihashMasterIdx, cacheptr->url);
      return NULL;
    }

    // else
    master_cacheptr = get_cache_node(cacheptr->ihashMasterIdx, cptr->url_num->proto.http.http_method, &prev_master_cache_link, vptr->httpData->master_cacheTable, cacheptr->url, cacheptr->url_len, cacheptr->ihashValue);

    if(master_cacheptr != NULL){
      NSDL2_CACHE(vptr, cptr, "URL [%s] found in master table, master_cacheptr->resp_buf_head = %p, master_cacheptr->resp_len = %d", master_cacheptr->url, master_cacheptr->resp_buf_head, master_cacheptr->resp_len);
      buffer = master_cacheptr->resp_buf_head; 
      blen = master_cacheptr->resp_len;
      body_offset = master_cacheptr->body_offset;
      compression_type = master_cacheptr->compression_type;
    }else{
      NSDL2_CACHE(vptr, cptr, "URL [%s] not found in master table", cacheptr->url);
      NS_EL_2_ATTR(EID_JS_URL_NOT_IN_CACHE, vptr->user_index,
                                vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                vptr->sess_ptr->sess_name,
                                vptr->cur_page->page_name,
                                "Url [%s] is not found in the master table.", cacheptr->url);
      return NULL;
    }
  } 
  else
  {
    // Get from user table node
    buffer = cacheptr->resp_buf_head; 
    blen = cacheptr->resp_len;
    body_offset = cacheptr->body_offset;
    compression_type = cacheptr->compression_type;
  }

  NSDL2_JAVA_SCRIPT(NULL, cptr, "compression_type = %d", cacheptr->compression_type);
  NSDL2_JAVA_SCRIPT(NULL, cptr, "resp_len = %d", cacheptr->resp_len);

  if(buffer == NULL){
    NS_EL_2_ATTR(EID_JS_URL_NOT_IN_CACHE, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  "Url [%s] content is 0 in the cache table.", url);
    NSDL2_JAVA_SCRIPT(NULL, cptr, "Returning... Since buffer size is 0");
    return NULL;
  }

  complete_buffers = blen / COPY_BUFFER_LENGTH;
  incomplete_buf_size = blen % COPY_BUFFER_LENGTH;

  if( (js_url_resp_buff == NULL) || (blen > js_url_resp_size)) {
    MY_REALLOC_EX(js_url_resp_buff, blen + 1, js_url_resp_size + 1, "js_url_resp_buff", -1);
    js_url_resp_size = blen; // does not include null temination
  }

  copy_cursor = js_url_resp_buff;

  for (i = 0; i < complete_buffers; i++) {
    if (buffer && buffer->buffer) {
      memcpy(copy_cursor, buffer->buffer, COPY_BUFFER_LENGTH);
      copy_cursor += COPY_BUFFER_LENGTH;
      buffer = buffer->next;
    } else {
      break;
    }
  }

  if (incomplete_buf_size) {
    if (buffer && buffer->buffer) {
      memcpy(copy_cursor, buffer->buffer, incomplete_buf_size);
    } else {
      return NULL;
    }
  }

  js_url_resp_buff[js_url_resp_size] = 0;

  // If this url was cached for JS processing only, then delete it from cache
  if(cacheptr->cache_flags & NS_CACHE_ENTRY_NOT_FOR_CACHE) {
    if(cacheptr->ihashMasterIdx != -1){  // This was taken from master table
    int free_emd; // Not needed but we need to pass
      NSDL3_CACHE(NULL, cptr, "Going to delete url in master table");
      cache_delete_entry_frm_master_tbl(vptr, master_cacheptr, prev_master_cache_link, cacheptr->ihashMasterIdx, &free_emd, 1);
    }
    cache_delete_entry_from_user_tbl(cptr->vptr, (unsigned char *)url, EMBEDDED_URL, cacheptr, prev_cache_link, ihashIndex);
  }
  else {
    NSDL2_CACHE(vptr, NULL, "Entry is for cache can not delete");
  }

  if (compression_type) {
    char err[1024 + 1];
    char *inp_js_buffer_body = js_url_resp_buff + body_offset;
    int body_size = blen - body_offset;

  VUser *vptr = cptr_for_js_error->vptr;
    NSDL2_JAVA_SCRIPT(NULL, cptr, "Decompressing, body_offset = %d, "
                           "resp_len = %d, body_size = %d",
                           body_offset, blen,
                           body_size,
                           compression_type);
    //if (ns_decomp_do_new (inp_js_buffer_body, body_size, compression_type, err)) 
    if (nslb_decompress(inp_js_buffer_body, body_size, &uncomp_buf, (size_t *)&uncomp_max_len, (size_t *)&uncomp_cur_len, compression_type,
          err, 1024)) {
      NS_EL_2_ATTR(EID_JS_DECOMPRESS, vptr->user_index,
                                     vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                     vptr->sess_ptr->sess_name,
                                     vptr->cur_page->page_name,
                                     "Error in decompressing URL contents. URL = %s, Error = %s",
                                     url, err);
      *url_resp_len  = blen ;
      return js_url_resp_buff;
    }
    *url_resp_len  = uncomp_cur_len;
    return uncomp_buf;
  } else {
    *url_resp_len  = blen;
    return js_url_resp_buff; 
  }
}

// This method is for execution of java script included in main page using src tag
static void extract_java_script_and_evaluate(connection *cptr, char *embd_url, void *ctx, void *glob){

  VUser *vptr = cptr->vptr;
  int emb_request_type;
  char emb_hostname[512] = "\0";
  char emb_request_line[MAX_LINE_LENGTH];  // Is it enough
  int request_type;
  char hostname[512] = "\0";
  char request_line[MAX_LINE_LENGTH];  // Is it enough
  char new_url[MAX_LINE_LENGTH];  // Is it enough
  char *url_resp = NULL; 
  int url_resp_len;


  NSDL2_JAVA_SCRIPT(NULL, cptr, "Method Called, embd_url = %s, page_main_url = %s",
                          embd_url, vptr->httpData->page_main_url);

  embedded_js_id++;
  js_id = embedded_js_id;
  
  if (RET_PARSE_NOK == parse_url(embd_url, "/?#", &emb_request_type, emb_hostname, emb_request_line)) {
      if(!strcmp(embd_url, ""))
        embd_url = "NA";
      NS_EL_3_ATTR(EID_JS_INVALID_URL, vptr->user_index,
                                    vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                    vptr->sess_ptr->sess_name,
                                    vptr->cur_page->page_name,
                                    embd_url,
                                    "Invalid included JavaScript URL");
     return;
  }

  NSDL2_JAVA_SCRIPT(NULL, cptr, "emb_request_type = %d, emb_hostname = [%s], emb_request_line = [%s]",
                          emb_request_type, emb_hostname, emb_request_line);

  if(emb_request_type != -1) { // URL is fully qualified URL (e.g. http://abc.com/index.html
    url_resp = js_cache_get_resp_buffer(cptr, embd_url, &url_resp_len);
    embedded_js_url = embd_url;
  } else {
  // For relative URL, we need to get http[s]://host[:port] from main url
    if (RET_PARSE_NOK == parse_url(vptr->httpData->page_main_url, "/?#", &request_type, hostname, request_line)) {
      NS_EL_3_ATTR(EID_JS_INVALID_URL , vptr->user_index,
                                    vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                    vptr->sess_ptr->sess_name,
                                    vptr->cur_page->page_name,
                                    vptr->httpData->page_main_url,
                                    "Invalid main URL.");
     return;
    }
    NSDL2_JAVA_SCRIPT(NULL, cptr, "request_type = %d, hostname = [%s], request_line = [%s]",
                            request_type, hostname, request_line);
    emb_request_type = request_type;

    if (emb_hostname[0] != '\0') {
    /* This is the case when embedded url was in the format '//<hostname>/<url_path>'
 *     In this case the parse_url() would have returned request_type as -1 but
 *     the emb_hostname would have been returned. So we should used the hostname from 
 *     embedded url only instead of parent url */
      strcpy(hostname, emb_hostname);
    }

    if(emb_request_line[0] == '/') { // URL is absolute URL
      sprintf(new_url, "%s://%s%s", request_type == HTTP_REQUEST?"http":"https", hostname, emb_request_line);
    } else { // URL is relative URL
      make_absolute_from_relative_url(emb_request_line, request_line, emb_request_line);
      sprintf(new_url, "%s://%s%s", request_type == HTTP_REQUEST?"http":"https",
                                        hostname, emb_request_line);
    }
    NSDL2_JAVA_SCRIPT(NULL, cptr, "new_url = [%s]", new_url);
    url_resp = js_cache_get_resp_buffer(cptr, new_url, &url_resp_len);
    embedded_js_url = new_url;
  }

  NSDL2_JAVA_SCRIPT(NULL, cptr, "url_resp_len = %d, url_resp = [%s]",
                          url_resp_len, url_resp);
  JSBool ok;
  jsval rval;
  if(url_resp) {
    ok = JS_EvaluateScript(ctx, glob, url_resp, url_resp_len,
                       __FILE__, __LINE__, &rval);
    if (!ok) {
       // This must be an event
       NSDL2_JAVA_SCRIPT(vptr, NULL, "Error in Evaluating embedded script. Embedded URL = [%s]", embedded_js_url);
       NS_EL_3_ATTR(EID_JS_EVALUATE_SCRIPT, vptr->user_index,
                                     vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                     vptr->sess_ptr->sess_name,
                                     vptr->cur_page->page_name,
                                     (char*)__FUNCTION__,
                                     "Error in Evaluating embedded script. Embedded URL = [%s]", embedded_js_url);
       embedded_js_url = "NA";
       return;
    }
  }
  embedded_js_url = "NA";
}

/* returns ascii value of non white character found in a string.
 * Otherwise returns 0 */
static int str_has_non_white_content(char * str)
{
  if (NULL == str) return 0;

  for(;*str; str++)
    if (!isspace(*str))
      return *str;

  return 0;
}

static int _html_parse_feed_elements(connection *cptr, xmlNode *root, void *ctx, void *gobj, xmlNode *form_node, xmlNode *select_node) {

  xmlNode *cur_node = NULL;
  xmlChar *embd_obj; 
  xmlChar *cur_node_content; 
  int i;
  int id;
  int start_event;
  int subtree_created = 0;
  VUser *vptr = (VUser *) cptr->vptr;

  NSDL2_JAVA_SCRIPT(NULL, cptr, "Method Called");

  for (cur_node = (xmlNode *)root; cur_node; cur_node = cur_node->next) {
      NSDL2_JAVA_SCRIPT(NULL, cptr, "Current node name = %s, address = %p", cur_node->name, cur_node);

    if (cur_node->type == XML_ELEMENT_NODE) {

      /* >>>> Script Tag : Takes care of inline as well as embedded scripts */
      if (xmlStrcmp((const xmlChar *)cur_node->name, (const xmlChar *)"script") == 0) {
         NSDL2_JAVA_SCRIPT(NULL, cptr, "Got Script tag");

         if((embd_obj = xmlGetProp(cur_node, (xmlChar *)"src"))) 
         {
           /* script tag is used to include an embedded js file */
           NSDL2_JAVA_SCRIPT(NULL, cptr, "Got embedded script, embd_obj url = '%p', '%s'", embd_obj, embd_obj);

           extract_java_script_and_evaluate(cptr, (char*)embd_obj, ctx, gobj); 
         }

         /* Inline script code will feed JS DOM tree for a new node of type SCRIPT
          * This covers following cases:
          * CASE 1: (Inline Script)
          * eg - <script type="txt/javascript"> x=2; </script>
          * CASE 2: (Inline script along with embedded js file)
          * eg - <script type="txt/javascript" src="js_file.js"> x=2; </script> */
         cur_node_content = xmlNodeGetContent(cur_node);
         if(cur_node_content) {
           if (str_has_non_white_content((char*)cur_node_content)) {
             NSDL2_JAVA_SCRIPT(NULL, cptr, "embedded_js_url = %s", embedded_js_url);
             ns_js_feed_inline_script(ctx, gobj, (char *)cur_node_content);
/****************** document.write() BEGIN **********/
             jsval rval;
             
             JS_CallFunctionName(ctx, gobj, "__ns_js_get_write_buffer_cglue", 0, &rval, &rval);
             JSString *str = JS_ValueToString(ctx, rval);
             char *write_buf = (char *) JS_GetStringBytes(str);
    
             NSDL2_JAVA_SCRIPT(NULL, cptr, "document.write returned: '%s'", write_buf);

             if(write_buf && write_buf[0] != '\0' && strcmp(write_buf, "__CAV_NULL"))
             {
               xmlDocPtr subtree_doc = xmlRecoverMemory(write_buf, strlen(write_buf));

               if(subtree_doc){
                 xmlNode *subtree_root = xmlDocGetRootElement((xmlDocPtr)subtree_doc);

                 if (subtree_root){
                   subtree_created = 1;
                   xmlReplaceNode(cur_node, subtree_root);
                   cur_node = subtree_root;
                   xmlSetTreeDoc(cur_node, (xmlDocPtr)vptr->httpData->ptr_html_doc);
                   /* TODO: check if subtree_doc needs be freed */

                 } /* subtree_root */
               } /* subtree_doc */
             }
/****************** document.write() END **********/
           }
           free(cur_node_content);
         }
         FREE_XML_CHAR(embd_obj);
      } 

      else 

      /* >>>> Form Tag */
      if (xmlStrcmp((const xmlChar *)cur_node->name, (const xmlChar *)"form") == 0) {
        NSDL2_JAVA_SCRIPT(NULL, cptr, "Node containing HTML form, cur_node = %p", cur_node);

        /* Save the form node pointer as it will be passed in the recursive
         * calls to this function so that any children elements viz., input, textarea,
         * select etc. tags will have reference of their container for. This pointer
         * will be saved in JavaScript as it would be needed for the purpose of setting  
         * the values while processing ns_edit_field(), ns_text_area(), ns_check_box(),
         * ns_list(), ns_radio_group() etc. click API's.
         */
        form_node = cur_node; 

        xmlChar *form_id      = xmlGetProp(cur_node, (xmlChar *)"id"); 
        xmlChar *form_name    = xmlGetProp(cur_node, (xmlChar *)"name"); 
        xmlChar *form_action  = xmlGetProp(cur_node, (xmlChar *)"action"); 
        xmlChar *form_method  = xmlGetProp(cur_node, (xmlChar *)"method");
        
        cur_node_content = xmlNodeGetContent(cur_node);

        if (str_has_non_white_content((char*)cur_node_content)) {

          ns_js_feed_form_element(ctx, gobj, (char*)form_id, 
                                  (char*)form_name, (char*)form_action, 
                                  (char*)form_method, (char *)cur_node_content);

        }

        /*
        We can change to following if need to change from cglue to executing inline script
        snprintf(js_script, MAX_JS_FUNC_WRAPPER_SCRIPT_LEN (2048), "document.__add_form(\"%s\", \"%s\", \"%s\", \"%s\", \"%s\");", 
            form_id, form_name, form_action, form_method, (char*)xmlNodeGetContent(cur_node));
        ns_js_feed_inline_script(ctx, gobj, js_script);
        Note: when I tried this I got error in the last argument bcoz construction of argument was spliting the string in seperate lines, 
        which was not executed by JS engine successfully.
        And if we make change in this direction then we need to do same with other ns_js_feed functions
        */

        FREE_XML_CHAR(form_id);
        FREE_XML_CHAR(form_name);
        FREE_XML_CHAR(form_action);
        FREE_XML_CHAR(form_method);
        FREE_XML_CHAR(cur_node_content);

      } 

      else 

      /* >>>> INPUT tag */
      if (xmlStrcmp((const xmlChar *)cur_node->name, (const xmlChar *)"input") == 0) { 
        NSDL2_JAVA_SCRIPT(NULL, cptr, "Node containing HTML form with Input");

        int checked = 0;
        xmlChar *input_id    = xmlGetProp(cur_node, (xmlChar *)"id"); 
        xmlChar *input_name  = xmlGetProp(cur_node, (xmlChar *)"name"); 
        xmlChar *input_type  = xmlGetProp(cur_node, (xmlChar *)"type"); 
        xmlChar *input_value = xmlGetProp(cur_node, (xmlChar *)"value");
        xmlChar *input_checked = xmlGetProp(cur_node, (xmlChar *)"checked");/*In case of radio group and checkbox*/
        if(input_checked) checked = 1;
        FREE_XML_CHAR(input_checked);


        NSDL2_JAVA_SCRIPT(NULL, cptr, "form_node = %p, "
                                      "id = %p '%s', "
                                      "name = %p, '%s', ", 
                                      "type = %p '%s', "
                                      "value = %p, '%s', ", 
                                      "checked = %d", 
                                      form_node, 
                                      input_id, input_id==NULL?"null":(char *)input_id,
                                      input_name, input_name==NULL?"null":(char *)input_name,
                                      input_type, input_type==NULL?"null":(char *)input_type,
                                      input_value, input_value==NULL?"null":(char *)input_value,
                                      checked);

        /* Input element is child of form */
        ns_js_feed_form_input_element(ctx, gobj, form_node, 
                                      (char*)input_id, (char*)input_name,
                                      (char*)input_type, (char*)input_value, NULL /*content*/, checked, 0);


        FREE_XML_CHAR(input_id);
        FREE_XML_CHAR(input_name);
        FREE_XML_CHAR(input_type);
        FREE_XML_CHAR(input_value);
      
      } 

      else 

      /* >>>> TEXTAREA tag */
      if (xmlStrcasecmp((const xmlChar *)cur_node->name, (const xmlChar *)"textarea") == 0) { 
        NSDL2_JAVA_SCRIPT(NULL, cptr, "Node containing HTML TEXTAREA");
 
        xmlChar *input_id    = xmlGetProp(cur_node, (xmlChar *)"id"); 
        xmlChar *input_name  = xmlGetProp(cur_node, (xmlChar *)"name"); 
        xmlChar *input_content = xmlNodeGetContent(cur_node);
 
 
        NSDL2_JAVA_SCRIPT(NULL, cptr, "form_node = %p, "
                                      "id = %p '%s', "
                                      "name = %p, '%s', ", 
                                      "type = 'TEXTAREA', "
                                      "content = %p, '%s', ", 
                                      form_node, 
                                      input_id, input_id==NULL?"null":(char *)input_id,
                                      input_name, input_name==NULL?"null":(char *)input_name,
                                      input_content, input_content==NULL?"null":(char *)input_content);

        /* Input element is child of form */
        ns_js_feed_form_input_element(ctx, gobj, form_node, 
                                      (char*)input_id, (char*)input_name,
                                      /*(char*)input_type*/ "TEXTAREA", NULL /*value */, 
                                      (char*)input_content, 0, 0);

        /* Assuming that in ns_text_area(), the att[TYPE] will always be in capitals (TEXTAREA) 
         * On another note, in case of text tag, the content is to be URI encoded and used as value 
         * in name=value in the query string. The cglue method in JS that constructs the Query string
         * takes care of this logic in case of text area */


        FREE_XML_CHAR(input_id);
        FREE_XML_CHAR(input_name);
        FREE_XML_CHAR(input_content);
        
      } 

      else 

      /* >>>> SELECT tag */
      if (!xmlStrcasecmp((const xmlChar *)cur_node->name, (const xmlChar *)"select")) { 

        NSDL2_JAVA_SCRIPT(NULL, cptr, "Node containing HTML SELECT");

        select_node = cur_node; /* Save the node pointer */
        xmlChar *input_id    = xmlGetProp(cur_node, (xmlChar *)"id"); 
        xmlChar *input_name  = xmlGetProp(cur_node, (xmlChar *)"name"); 
 
        NSDL2_JAVA_SCRIPT(NULL, cptr, "form_node = %p, "
                                      "id = %p '%s', "
                                      "name = %p, '%s', ", 
                                      "type = 'SELECT', ",
                                      form_node, 
                                      input_id, input_id==NULL?"null":(char *)input_id,
                                      input_name, input_name==NULL?"null":(char *)input_name);

        /* Input element is child of form */
/*        ns_js_feed_form_input_element(ctx, gobj, form_node, 
                                      (char*)input_id, (char*)input_name,
                                      "SELECT", NULL, NULL, 0);*/
        /* In case of select tag, only the node pointer is saved. Only the options
         * coming under option tag will be saved in the JS repository. */

        FREE_XML_CHAR(input_id);
        FREE_XML_CHAR(input_name);
        
      } 

      else 

      /* >>>> OPTION tag */
      if (!xmlStrcasecmp((const xmlChar *)cur_node->name, (const xmlChar *)"option")) { 

        NSDL2_JAVA_SCRIPT(NULL, cptr, "Node containing HTML OPTION of SELECT list");

        if(select_node)
        {
 
          xmlChar *input_id      = xmlGetProp(select_node, (xmlChar *)"id"); 
          xmlChar *input_name    = xmlGetProp(select_node, (xmlChar *)"name"); 
          xmlChar *flag_multiple = xmlGetProp(select_node, (xmlChar *)"multiple"); 
          xmlChar *input_value   = xmlGetProp(cur_node, (xmlChar *)"value"); 
          xmlChar *input_content = xmlNodeGetContent(cur_node); 
          xmlChar *flag_selected = xmlGetProp(cur_node, (xmlChar *)"selected"); 
  
          NSDL2_JAVA_SCRIPT(NULL, cptr, "SELECT OPTION: form_node = %p, select_node = %p, "
                                        "id = %p '%s', "
                                        "name = %p, '%s', ", 
                                        "type = 'SELECT', ",
                                        form_node, select_node, 
                                        input_id, input_id==NULL?"null":(char *)input_id,
                                        input_name, input_name==NULL?"null":(char *)input_name);
 
            NSDL4_JAVA_SCRIPT(NULL, cptr, "SELECT OPTION: setting first option as default");
            ns_js_feed_form_input_element(ctx, gobj, form_node, 
                                        (char*)input_id, (char*)input_name,
                                        "SELECT", (char *)input_value, (char *)input_content, 
                                         flag_selected?1:0, flag_multiple?1:0);
 
          /* Assuming that in ns_list(), the att[TYPE] will always be in capitals (SELECT) 
           * Another observation is that in case of select options, if both value and content 
           * are present, value is used for appending in Query string (name=value). 
           * If value property is absent, content is used In JS, the cglue method for constructing 
           * the query string takes care of this logic  
           */
 
          FREE_XML_CHAR(input_id);
          FREE_XML_CHAR(input_name);
          FREE_XML_CHAR(input_value);
          FREE_XML_CHAR(input_content);
          FREE_XML_CHAR(flag_selected);
          FREE_XML_CHAR(flag_multiple);
        }
        
      } 

      else 

      /* >>>> Any other tag not handled above */
      if((embd_obj = xmlGetProp(cur_node, (xmlChar *)"id")) != NULL) { 
        /* Node having an identifier, used by Javascript to get and set html nodes.
         * eg - <span id="timetag">none</span>
         * javascript can access the span using getElementByID("timetag");
         * javascript can change the span using setElementByID("timetag)=<new value>; */
        cur_node_content = xmlNodeGetContent(cur_node);
        if(cur_node_content) {
          ns_js_feed_element(ctx, gobj, (char*)embd_obj, (char *)cur_node_content);
          free(cur_node_content);
        }
        FREE_XML_CHAR(embd_obj);
      }

      else

      /* >>>> A Tag (anchor) or MAP tag */
      if ((xmlStrcmp((const xmlChar *)cur_node->name, (const xmlChar *)"a")  == 0) ||
          (xmlStrcmp((const xmlChar *)cur_node->name, (const xmlChar *)"area")== 0)) 
      {
        char *anchor_href = (char *) xmlGetProp(cur_node, (xmlChar *)"href");
        if ((anchor_href != NULL) && (!strncmp(anchor_href, "javascript:", 11))){
          char *tmp = anchor_href+11;
          id = ev_register((void *)cur_node, tmp, onclick, function_script);
          if (id < 0) {
            /* Warn that we are out event objects */
            NSDL2_JAVA_SCRIPT(NULL, cptr, "Event Registration failed because reached max limit");
          } else {
            ns_js_feed_inline_script(ctx, gobj, function_script);
          }
        }
      }

      /* >>>> BODY tag */
      if (xmlStrcmp((const xmlChar *)cur_node->name, (const xmlChar *)"body") == 0) {
        /* for body node we need to register onload and onunload events along with others */
        start_event = START_BODY_JS_EVENTS;
      } else {
        start_event = START_GENERIC_JS_EVENTS; /* Skip onload and unload */
      }

      /* Now register all the events for current node, if present, e.g. onclick, onchange etc. */
      for (i=start_event; i< MAX_TYPE_JS_EVENTS; ++i) {
        if((embd_obj = xmlGetProp(cur_node, (xmlChar *)ev_get_name(i))) != NULL)
        {
          /* Register the function and run the wrapper script */
          /* ASSUMING MONOLITHIC CODE - using global varaible without lock - beware!! */
          id = ev_register((void *)cur_node, (char *)embd_obj, i, function_script);
          if (id < 0) {
            /* Warn that we are out event objects */
            NSDL2_JAVA_SCRIPT(NULL, cptr, "Event Registration failed because reached max limit");
          } else {
            ns_js_feed_inline_script(ctx, gobj, function_script);
          }
          /* TODO: Some how make the event ID available and reverse correlation */
          FREE_XML_CHAR(embd_obj);
        }
      }
    } /* XML_ELEMENT_NODE check */
/****************** document.write() BEGIN **********/
    if(subtree_created) /*when document.write created new node, process replaced node instead of child*/
    {
      subtree_created = 0;
      _html_parse_feed_elements(cptr, cur_node, ctx, gobj, form_node, select_node);
    }else
/****************** document.write() END **********/
      _html_parse_feed_elements(cptr, cur_node->xmlChildrenNode, ctx, gobj, form_node, select_node);
    NSDL2_JAVA_SCRIPT(NULL, cptr, "Reached for loop");

  } /* End of for loop */
  NSDL2_JAVA_SCRIPT(NULL, cptr, "Call completed");

  return 0;
}

/* Wrapper to take care of doc vs node */
int html_parse_feed_elements(connection *cptr, void *doc, void *ctx, void *gobj) {
  NSDL2_JAVA_SCRIPT(NULL, cptr, "Method Called");

  int ret = _html_parse_feed_elements(cptr, html_parse_get_root(doc), ctx, gobj, NULL, NULL);

#if NS_DEBUG_ON
  jsval rval;

  if(ctx && gobj)
  {
    JS_CallFunctionName(ctx, gobj, "__ns_js_debug_print_all_input_elements_cglue", 0, &rval, &rval);
    JSString *str = JS_ValueToString((JSContext *)ctx, rval);
    
    NSDL2_JAVA_SCRIPT(NULL, cptr, "Input Elements: %s", JS_GetStringBytes(str));
  }
#endif

  return ret;
}

static void _html_parse_sync_elements(xmlNode *root, NS_JS_GET_FUNC get, void *ctx, void *gobj) {
  xmlNode *cur_node = NULL;
  xmlChar *embd_obj;
  char *value;

  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called");

  for (cur_node = root; cur_node; cur_node = cur_node->next) {

    if (cur_node->type == XML_ELEMENT_NODE) {

        value = NULL;

        if((embd_obj = xmlGetProp(cur_node, (xmlChar *)"id")) != NULL) {

            value = (char*)get(ctx, gobj, (char*)embd_obj);

            if (value != NULL) {

              NSDL4_JAVA_SCRIPT(NULL, NULL, "cur_node = %p, "
                                            "embd_obj = %p, %s, "
                                            "value = %p, %s", 
                                            cur_node, 
                                            embd_obj, embd_obj, 
                                            value, value);

/*              xmlNodeSetContent(cur_node, (xmlChar *)"\0");
              if(value)
                xmlNodeAddContent(cur_node, BAD_CAST ((xmlChar*)value));
*/


              
            }
            free(embd_obj);
        }
    }
    _html_parse_sync_elements(cur_node->xmlChildrenNode, get, ctx, gobj);
  }
}

void html_parse_sync_elements(void *doc, NS_JS_GET_FUNC get, void *ctx, void *gobj) {
  NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called");
  return _html_parse_sync_elements(html_parse_get_root(doc), get, ctx, gobj);
}

xmlChar *html_parse_print_tree(void *doc, VUser *vptr, int *len) {
  xmlChar *hstr;
  FILE *debug_fp;

  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method Called");

  xmlDocDumpFormatMemory((xmlDocPtr)doc, &hstr, len, 1);

  if ((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4)  && 
       (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP)) {
    char log_file[1024];
    sprintf(log_file, "%s/logs/TR%d/url_rep_js_body_%hd_%u_%u_%d_0_%d_%u_%u_0.dat", 
            g_ns_wdir, testidx, child_idx, vptr->user_index, vptr->sess_inst,
            vptr->page_instance, vptr->group_num,
            GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));


    debug_fp = fopen(log_file, "w");
    if(debug_fp) {
      fprintf(debug_fp, "%s\n", hstr);
      fclose(debug_fp);
    } else {
      fprintf(stdout, "%s\n", hstr);
    }
  }

  return hstr;
  /*We will free it in do data  processing*/
  /*xmlFree(hstr);*/
}

void reportError(JSContext *cx, const char *message, JSErrorReport *report) {

  // Event Log 
  VUser *vptr = cptr_for_js_error->vptr;
  char embedded_js_id_lol[128];

  NSDL2_JAVA_SCRIPT(NULL, cptr_for_js_error, "Method Called");

  sprintf(embedded_js_id_lol, "%d", js_id);
 
  NS_EL_5_ATTR(EID_JS_ERROR, vptr->user_index,
                                vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                vptr->sess_ptr->sess_name,
                                vptr->cur_page->page_name,
                                vptr->httpData->page_main_url,
                                embedded_js_id_lol,
                                embedded_js_url,
                                "Error reported by JavaScript engine. Page Instance: %d\n"
                                "Error: [%s]\n"
                                "At Line: %d\n"
                                "At Text: [%s]\n"
                                "In Text: [%s]\n",
                                vptr->page_instance,
                                message,
                                report->lineno,
                                report->tokenptr,
                                report->linebuf);
}   

static void init_js_error_logging(connection *cptr, JSContext *js_context) {

  cptr_for_js_error = cptr;
  js_id = -1;
  embedded_js_id = -1;
  included_js_id = -1;
  //embedded_js_url = NULL;
  /*we are doing NA as sprintf does not print NULL 
   * and we are not getting nothing in event log for emdded_js_url*/
  embedded_js_url = "NA";
  JS_SetErrorReporter(js_context, reportError);
}



inline char *get_form_data_from_js(VUser *vptr, xmlNode *form_node_ptr, int form_encoding_type)
{ 
  jsval rval;
  jsval argv[2];
  JSBool ok;
  JSContext *js_context = vptr->httpData->js_context;
  JSObject *global = vptr->httpData->global;
 

  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called");

/* pointer typcast to long and stored in local var. then long type cast to double. 
 * This is done as gcc compiler was giving error when pointer was being type cast to double */
  long tmp = (long)form_node_ptr;
  JS_NewNumberValue(js_context, (jsdouble) tmp, &argv[0]);

  JS_NewNumberValue(js_context, (jsdouble) form_encoding_type, &argv[1]);

  ok = JS_CallFunctionName(js_context, global, "__ns_js_get_form_data_cglue", 2, argv, &rval); 
  if (!ok) {
    NSDL2_JAVA_SCRIPT(vptr, NULL, "Error getting the form submit data from JS");


    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                  vptr->sess_ptr->sess_name,
                  vptr->cur_page->page_name,
                  (char*)__FUNCTION__,
                 "Error getting the form submit data from JS");
   return NULL;
  
  } else {

    JSString *str = JS_ValueToString(js_context, rval);

    NSDL4_JAVA_SCRIPT(vptr, NULL, "Form Submit Data read from JS = '%s'", str?JS_GetStringBytes(str):"NULL");
    return JS_GetStringBytes(str); 
  }
}


inline void set_form_input_element_in_js(VUser *vptr, xmlNode *form_node_ptr, char *id, char *name, char *type, char *value, char *content)
{ 
  jsval rval;
  JSBool ok;
  JSContext *js_context = vptr->httpData->js_context;
  JSObject *global = vptr->httpData->global;
  jsval argv[6];
  JSString *str;
  int elm_len;

   NSDL2_JAVA_SCRIPT(NULL, NULL, "Method Called, "
                     "form_node_ptr = %p, "
                     "id = %p, '%s', "
                     "name = %p, '%s', "
                     "type = %p, '%s', "
                     "value = %p, '%s'"
                     "content = %p, '%s'",
                     form_node_ptr,
                     id, id==NULL?"null":id,
                     name, name==NULL?"null":name,
                     type, type==NULL?"null":type,
                     value, value==NULL?"null":value,
                     content, content==NULL?"null":content);

/* pointer typcast to long and stored in local var. then long type cast to double. 
 * This is done as gcc compiler was giving error when pointer was being type cast to double */
  long tmp = (long)form_node_ptr;
  JS_NewNumberValue(js_context, (jsdouble) tmp, &argv[0]);

  ns_check_length(id, &elm_len);
  str = JS_NewStringCopyN(js_context, id, elm_len); 
  argv[1] = STRING_TO_JSVAL(str);

  ns_check_length(name, &elm_len);
  str = JS_NewStringCopyN(js_context, name, elm_len); 
  argv[2] = STRING_TO_JSVAL(str);

  ns_check_length(type, &elm_len);
  str = JS_NewStringCopyN(js_context, type, elm_len); 
  argv[3] = STRING_TO_JSVAL(str);

  ns_check_length(value, &elm_len);
  str = JS_NewStringCopyN(js_context, value, elm_len); 
  argv[4] = STRING_TO_JSVAL(str);

  ns_check_length(content, &elm_len);
  str = JS_NewStringCopyN(js_context, content, elm_len); 
  argv[5] = STRING_TO_JSVAL(str);


  ok = JS_CallFunctionName(js_context, global, "__ns_js_set_input_element_cglue", 6, argv, &rval); 
  if (!ok) {
    NSDL2_JAVA_SCRIPT(vptr, NULL, "Error setting form input element");
    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                  vptr->sess_ptr->sess_name,
                  vptr->cur_page->page_name,
                  (char*)__FUNCTION__,
                 "Error setting form input element");
  
  }
}

char* process_buffer_in_js(connection *cptr, char *resp_buf,
                         int resp_buf_len, int *dom_resp_len) {

  VUser *vptr = cptr->vptr;
  JSContext *js_context;
  JSObject  *global;
  JSBool ok;
  jsval rval;
  jsval argv[1];
  JSString *str;
  //int ev = 0;
  char *source = NULL;
  int elm_len = 0;

  int ret;
  char *dom_resp = NULL;

  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, js_mode = %d, vptr->httpData->js_rt = %p, "
                         "resp_buf = %d, resp_buf = [%s] "
                         "request_type = %d, url_awaited = %d",
                          runprof_table_shr_mem[vptr->group_num].gset.js_mode,
                          vptr->httpData->js_rt, resp_buf_len, resp_buf,
                          cptr->url_num->request_type,
                          vptr->urls_awaited);
 
  /*Create DOM */
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, Dom creation started at %lu", get_ms_stamp());

  vptr->httpData->ptr_html_doc = create_dom_from_html_stream(cptr, resp_buf, resp_buf_len);
  if (!vptr->httpData->ptr_html_doc) {
      NS_EL_2_ATTR(EID_JS_DOM, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  "Error in creating DOM");

      NSDL2_JAVA_SCRIPT(NULL, cptr, "Error in creating DOM,"
                             " resp_buf_len = %d, resp_buf  = [%s]",
                             resp_buf_len, resp_buf);

    free_javascript_data_in_vptr(vptr);
    return NULL;
  }


  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, Dom creation completed at %lu, vptr->httpData->ptr_html_doc = '%p'", get_ms_stamp(), vptr->httpData->ptr_html_doc);
#ifdef NS_DEBUG
  dom_resp = (char*)html_parse_print_tree(vptr->httpData->ptr_html_doc, vptr, dom_resp_len);
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Tree after creating DOM = %s", dom_resp);
  XML_FREE_JS_DOM_BUFFER(cptr, dom_resp);
#endif
  
  init_js_events();

  /* Create a context. */
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, new context creation started at %lu", get_ms_stamp());
  js_context = JS_NewContext(vptr->httpData->js_rt, 
                global_settings->js_stack_size>0?global_settings->js_stack_size:CONTEXT_STACK_CHUNK_SIZE);
  vptr->httpData->js_context = js_context; // Save to vptr
  if (js_context == NULL) {
    NS_EL_2_ATTR(EID_JS_ENGINE, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  "Unable to create new JavaScript context.");

    free_javascript_data_in_vptr(vptr);
    return NULL;
  }
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, new context creation completed at %lu", get_ms_stamp());
  NSDL4_JAVA_SCRIPT(vptr, NULL, "New JS Context created; js_context=%p, stack_size=%d", js_context, 
             global_settings->js_stack_size>0?global_settings->js_stack_size:CONTEXT_STACK_CHUNK_SIZE);
 
  init_js_error_logging(cptr, js_context);
  //JS_SetErrorReporter(js_context, reportError);

  /* Create the global object. */
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, new object and standard class creation started at %lu", get_ms_stamp());
  global = JS_NewObject(js_context, &global_class, NULL, NULL);
  vptr->httpData->global = global; // Save to vptr
  if (global == NULL) {
    NS_EL_2_ATTR(EID_JS_ENGINE, vptr->user_index,
                                   vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                   vptr->sess_ptr->sess_name,
                                   vptr->cur_page->page_name,
                                   "Unable to create new JavaScript Object.");
     free_javascript_data_in_vptr(vptr);
     return NULL;
  }

  /* Populate the global object with the standard globals,
   * like Object and Array. */
  if (!JS_InitStandardClasses(js_context, global)) {
     NS_EL_2_ATTR(EID_JS_ENGINE, vptr->user_index,
                                   vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                   vptr->sess_ptr->sess_name,
                                   vptr->cur_page->page_name,
                                   "Error in JS_InitStandardClasses()");
     free_javascript_data_in_vptr(vptr);
     return NULL;
  }

  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, new object and standard class creation completed at %lu", get_ms_stamp());

/************ debug callback begin ********/
#ifdef NS_DEBUG_ON
  if (!JS_DefineFunctions(js_context, global, myjs_global_functions))
  {
     NS_EL_2_ATTR(EID_JS_ENGINE, vptr->user_index,
                                 vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                 vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 "Unable to create JS debug callback.");
  }
#endif
/************ debug callback end ********/

  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, bootstrap evaluation started at %lu", get_ms_stamp());
//  ok = JS_EvaluateScript(js_context, global, bootstrap_js_buffer, strlen(bootstrap_js_buffer),
  ok = JS_EvaluateScript(js_context, global, bootstrap_js_buffer, bootstrap_js_buffer_len,
                             __FILE__, __LINE__, &rval);

  if (!ok) {
      NS_EL_3_ATTR(EID_JS_EVALUATE_SCRIPT, vptr->user_index,
                                    vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                    vptr->sess_ptr->sess_name,
                                    vptr->cur_page->page_name,
                                    (char*)__FUNCTION__,
                                    "Error in evaluating boot strap script.");
     free_javascript_data_in_vptr(vptr);
     return NULL;
  }
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, bootstrap evaluation completed at %lu", get_ms_stamp());


#ifdef NS_DEBUG_ON
  /* set debug flag on in Javascript */
  ns_js_set_debug_flag_on(js_context, global);
#endif

  if(vptr->browser->UA)
   ns_js_set_user_agent(js_context, global, vptr->browser->UA);

  //  Cookie Stuff
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, cookie setup started at %lu", get_ms_stamp());
  char *cookie_buf = get_all_cookies(vptr, NULL, -1);
  if(cookie_buf) {
    str = JS_NewStringCopyN(js_context, cookie_buf, strlen(cookie_buf)); // Optimize strlen here
    //JS_NEWSTRING_COPYN(js_context, cookie_buf, strlen(cookie_buf));
    argv[0] = STRING_TO_JSVAL(str);
    ok = JS_CallFunctionName(js_context, global, "__ns_js_set_cookie", 1, argv, &rval); 
    if (!ok) {
      //  NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error Setting up Cookie");
        NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                  vptr->sess_ptr->sess_name,
                                  vptr->cur_page->page_name,
                                  (char*)__FUNCTION__,
                                  "Error Setting up Cookie");

        NSDL2_JAVA_SCRIPT(NULL, NULL, "Error Setting up Cookie");
     
     free_javascript_data_in_vptr(vptr);
     return NULL;
    }
    NSDL2_JAVA_SCRIPT(NULL, NULL, "Cookie changed");
  }
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, cookie setup completed at %lu", get_ms_stamp());

  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, html_parse_feed_elements started at %lu", get_ms_stamp());
  ret = html_parse_feed_elements(cptr, vptr->httpData->ptr_html_doc, (void *)js_context, (void *)global);
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, html_parse_feed_elements completed at %lu", get_ms_stamp());
  if (ret<0) {
      NSDL2_JAVA_SCRIPT(vptr, NULL, "Error  HTML feed to JS engine ");

     free_javascript_data_in_vptr(vptr);
     return NULL;
  }
   
  {
  VUser *vptr = cptr_for_js_error->vptr;
  NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, events started at %lu", get_ms_stamp());

  /***************** SET THE windlow.location ****************/
  NSDL4_JAVA_SCRIPT(vptr, NULL, "before __ns_js_set_window_location_href_cglue; "
                                "vptr->httpData->page_main_url = %p, '%s'",
                                vptr->httpData->page_main_url, 
                                vptr->httpData->page_main_url?vptr->httpData->page_main_url:"null");


  ns_check_length(vptr->httpData->page_main_url, &elm_len);
  str = JS_NewStringCopyN(js_context, vptr->httpData->page_main_url, elm_len);
  argv[0] = STRING_TO_JSVAL(str);

  ok = JS_CallFunctionName(js_context, global, "__ns_js_set_window_location_href_cglue", 1, argv, &rval); 

  if (!ok) {
    NSDL2_JAVA_SCRIPT(vptr, NULL, "Error setting the window location");
    
    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                  vptr->sess_ptr->sess_name,
                  vptr->cur_page->page_name,
                  (char*)__FUNCTION__,
                 "Error setting the window location");
  }
  else
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "__ns_js_set_window_location_href_cglue: "
                                  "Set the window location successfully; "
                                  "vptr->httpData->page_main_url = %p, '%s'",
                                   vptr->httpData->page_main_url, 
                                   vptr->httpData->page_main_url?vptr->httpData->page_main_url:"null");

  }
  

 
  /******************** EMIT THE ONLOAD EVENT *****************/
  if((source = ev_get_onload_event())) {

    NSDL2_JAVA_SCRIPT(vptr, NULL, "Calling JS onload function - %s", source);

    ok = JS_CallFunctionName(js_context, global, source, 0, &rval, &rval); 

    if (!ok) {
      NSDL2_JAVA_SCRIPT(vptr, NULL, "Error Emiting OnLoad event");

      NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                                vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                vptr->sess_ptr->sess_name,
                                vptr->cur_page->page_name,
                                (char*)__FUNCTION__,
                               "Error Emiting OnLoad event");
    }else {

      NSDL4_JAVA_SCRIPT(vptr, NULL, "OnLoad event emitted successfully");
    }
  }

#if 0
    /* ONLOAD function stuff */
    ok = JS_CallFunctionName(js_context, global, ev_get_func(0), 0, &rval, &rval); 
    if (!ok) {
      NSDL2_HTTP(vptr, NULL, "Error While executing ONLOAD");
      NSEL_MAJ(NULL, cptr, ERROR_ID, ERROR_ATTR, "Error  While executing ONLOAD");
      JS_DestroyContext(js_context);
      return NULL;
    }
#endif

    NSDL2_JAVA_SCRIPT(vptr, NULL, "Calling JS window.onload function - %s", "__ns_js_emit_onload_event_cglue");
    ok = JS_CallFunctionName(js_context, global, "__ns_js_emit_onload_event_cglue", 0, &rval, &rval); 
    if (!ok) {
        NSDL2_JAVA_SCRIPT(vptr, NULL, "Error Emiting OnLoad event cglue");
        NSEL_MAJ(NULL, cptr, ERROR_ID, ERROR_ATTR, "Error Emiting OnLoad event cglue");
        //JS_DestroyContext(js_context);
        //return NULL;
    }
    NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, events completed at %lu", get_ms_stamp());

    /******************** EMIT THE RANDOM EVENT *****************/
    /*    ok = JS_CallFunctionName(cx, global, ev_get_func(1), 0, &rval, &rval); 
        if (!ok) {
            printf("Error Emiting ev-0 event\n");
            return -1;
        }*/

    /******************** SYNC DOM TREE from JS ***************/
    NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, html_parse_sync_elements started at %lu", get_ms_stamp());
    html_parse_sync_elements(vptr->httpData->ptr_html_doc, ns_js_get_element, (void *)js_context, (void *)global);
    NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, html_parse_sync_elements completed at %lu", get_ms_stamp());
    /*if (ret<0) {
      NSDL2_HTTP(vptr, NULL, "Error from JS engine while synchronizing DOM Tree from JS");
      return NULL;
    }*/

    /******************** Print DOM TREE ***************/
    NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, html_parse_print_tree started at %lu", get_ms_stamp());
    dom_resp = (char*)html_parse_print_tree(vptr->httpData->ptr_html_doc, vptr, dom_resp_len);
    NSDL1_JAVA_SCRIPT(vptr, NULL, "Time, html_parse_print_tree completed at %lu", get_ms_stamp());

    /******************** EMIT THE UNLOAD EVENT *****************/
    //ev = 0;
    source = NULL;
#if 0
    while((source = ev_get_onunload_event(&ev))) {
      ok = JS_CallFunctionName(js_context, global, source, 0, &rval, &rval); 
      if (!ok) {
        NSDL2_HTTP(vptr, NULL, "Error Emiting OnUnLoad event");
        JS_DestroyContext(js_context);
        return NULL;
      }
    }
#endif

  
  }


  NSDL4_JAVA_SCRIPT(vptr, NULL, "global_settings->protocol_enabled & CLICKSCRIPT_PROTOCOL_ENABLED = %d", global_settings->protocol_enabled & CLICKSCRIPT_PROTOCOL_ENABLED);

  /* Free the DOM tree only if Click and Script is disabled */
  if ((global_settings->protocol_enabled & CLICKSCRIPT_PROTOCOL_ENABLED) == 0){

    NSDL1_JAVA_SCRIPT(vptr, NULL, "Closing parser. Freeing vptr->httpData->ptr_html_doc pointer ('%p')", vptr->httpData->ptr_html_doc);

    free_dom_and_js_ctx(vptr);

  } else {

    /* Save some important data on vptr->httpData that will be used later while processing click actions*/
    vptr->httpData->server_port  = cptr->url_num->index.svr_ptr->server_port;

    int n = strlen(cptr->url_num->index.svr_ptr->server_hostname);
    MY_MALLOC(vptr->httpData->server_hostname, n+1, "vptr->httpData->server_hostname", 0);

    strncpy(vptr->httpData->server_hostname, cptr->url_num->index.svr_ptr->server_hostname, n);
    vptr->httpData->server_hostname[n] = '\0';
    vptr->httpData->server_hostname_len = n; /* bug 3358 wells core dump */

    vptr->httpData->request_type = cptr->url_num->request_type;

    NSDL4_JAVA_SCRIPT(vptr, NULL, "Saved data on vptr; "
                "vptr->httpData->server_port = %d "
                "vptr->httpData->server_hostname = '%s' "
                "vptr->httpData->request_type = %d ", 
                 vptr->httpData->server_port, 
                 vptr->httpData->server_hostname,
                 vptr->httpData->request_type);
  }

  return dom_resp;
}

/* This method is called from file: ns_vuser.c method: user_cleanup() */
inline void free_javascript_data(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, vptr=%p at %u", vptr, now);

  free_javascript_data_in_vptr(vptr);
}

inline void destroy_js_runtime_per_user(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, vptr=%p, vptr->httpData->js_rt=%p,  at time=%u", 
                                vptr, (vptr && vptr->httpData)?vptr->httpData->js_rt:0, now);

  if(global_settings->js_runtime_mem_mode == NS_JS_RUNTIME_MEMORY_MODE_PER_VUSER)
  {
    NSDL2_JAVA_SCRIPT(vptr, NULL, "JS RT memory mode is per user, destroying the rt = %p", (vptr && vptr->httpData)?vptr->httpData->js_rt:0);
    JS_DestroyRuntime((JSRuntime *)vptr->httpData->js_rt);
    vptr->httpData->js_rt = NULL;
  }
  else
    NSDL2_JAVA_SCRIPT(vptr, NULL, "JS RT memory mode is per NVM, not destroying the rt = %p", (vptr && vptr->httpData)?vptr->httpData->js_rt:0);
}

void init_javascript_data(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, vptr=%p at %u", vptr, now);

  vptr->httpData->js_context = NULL;
  vptr->httpData->ptr_html_doc = NULL;
  vptr->httpData->global = NULL;
  vptr->httpData->clicked_url = NULL;
  vptr->httpData->clicked_url_len = 0;
  vptr->httpData->clickaction_id = -1;
  vptr->httpData->server_port = -1;
  vptr->httpData->server_hostname = NULL;
  vptr->httpData->server_hostname_len = -1;
  vptr->httpData->request_type = -1;
  vptr->httpData->http_method = -1;
  vptr->httpData->post_body = NULL;
  vptr->httpData->post_body_len = -1;
  vptr->httpData->formencoding = NULL;
  vptr->httpData->formenc_len = -1;

  if(global_settings->js_enabled)
  {
    switch (global_settings->js_runtime_mem_mode)
    {
      case NS_JS_RUNTIME_MEMORY_MODE_PER_NVM:
        NSDL4_JAVA_SCRIPT(NULL, NULL, "Saving global pointer to JS RT ns_js_rt=%p on vptr", ns_js_rt);
        vptr->httpData->js_rt = (void *)ns_js_rt;
        break;

      case NS_JS_RUNTIME_MEMORY_MODE_PER_VUSER:
        NSDL4_JAVA_SCRIPT(NULL, NULL, "JS RT mem mode is per user, creating new JS RunTime");
        vptr->httpData->js_rt = (void *)JS_NewRuntime(global_settings->js_runtime_mem);
        NSDL4_JAVA_SCRIPT(NULL, NULL, "JS_NewRuntime() returned %p, saved to vptr->httpData->js_rt",  
                                      vptr->httpData->js_rt);
        if(vptr->httpData->js_rt == NULL)
        {
          /* log event */
          NSDL2_JAVA_SCRIPT(NULL, NULL, "Unable to allocate  memory for Java Run Time.");
          NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                  vptr->sess_ptr->sess_name,
                  vptr->cur_page->page_name,
                  (char*)__FUNCTION__,
                  "Unable to allocate  memory for Java Run Time.");
        }
        break;

    }/* End of switch */

  } /* End of js_enabled */
}

char *get_window_location_href(VUser *vptr, JSContext *js_context, JSObject *global)
{

  jsval rval;
  JSBool ok;

  NSDL4_JAVA_SCRIPT(vptr, NULL, "Method called, vptr = %p, js_context = %p, global = %p", vptr, js_context, global);

  ok = JS_CallFunctionName(js_context, global, "__ns_js_get_window_location_href_cglue", 0, &rval, &rval);

  if (!ok) {

    NSDL2_JAVA_SCRIPT(vptr, NULL, "Error getting window location href");

    NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
                  vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                  vptr->sess_ptr->sess_name,
                  vptr->cur_page->page_name,
                  (char*)__FUNCTION__,
                 "Error getting window location href");

    return NULL;
  
  } else {

    JSString *str = JS_ValueToString(js_context, rval);

    NSDL4_JAVA_SCRIPT(vptr, NULL, "window location href read from JS, ok = %d, str = %p", ok, str);

    char *tmpstr = JS_GetStringBytes(str);
    NSDL4_JAVA_SCRIPT(vptr, NULL, "Window location href = '%s'", tmpstr?tmpstr:"NULL");

    return tmpstr;
  }

}

void set_x_y_coordinates_for_form_image(VUser *vptr, char *img_name, xmlNode *form_node, int xval, int yval)
{
  char name[512] = "\0";
  char x_str[10] = "\0";
  char y_str[10] = "\0";

  NSDL4_JAVA_SCRIPT(NULL, NULL, "Method Called, Setting %s.x=%d, "
                                "and %s.y=%d in form",
                                (char *)img_name, xval,
                                (char *)img_name, yval);

  sprintf(name, "%s.x", (char *) img_name);
  sprintf(x_str, "%d", xval);
  sprintf(y_str, "%d", yval);

  ns_js_feed_form_input_element(vptr->httpData->js_context, vptr->httpData->global, 
                                form_node, NULL /*id*/, name, "image" /*type*/, 
                                x_str /*value*/, NULL /* content */, 0, 0);

  sprintf(name, "%s.y", (char *) img_name);
  ns_js_feed_form_input_element(vptr->httpData->js_context, vptr->httpData->global, 
                                form_node, NULL /*id*/, name, "image" /*type*/, 
                                y_str /*value*/, NULL /* content */, 0, 0);


}

void search_and_emit_event(VUser *vptr, JSContext *c, JSObject *g,  xmlNode *node_clicked, char *eventname)
{
  jsval rval;
  JSBool ok;
  int j;

  NSDL2_JAVA_SCRIPT(vptr, NULL, "Method called, node_clicked = %p, eventname = %p, '%s'", node_clicked, eventname, eventname);

  for(j = 0; j< MAX_JS_EVENTS; j++) {
    /* Search if onclick event is registered in ev_list[] for this node. */
    if(node_clicked == (xmlNode *) ev_get_node(j) && ev_get_id(eventname) == ev_get_type(j))
      break;

  } /* End of for loop */

  if(j < MAX_JS_EVENTS) /* onclick event must be found in ev_list[], j is index into this array */
  {
    NSDL4_JAVA_SCRIPT(vptr, NULL, "found registerd event for onclick, \
       j = %d, ev_list[j].node = %p, \n\
       ev_list[j].type = %d, \n\
       ev_list[j].js_wrapper_func = '%s'",
       j, ev_get_node(j), 
       ev_get_type(j), 
       ev_get_js_wrapper_func(j));


    /* Now emit the event */
    ok = JS_CallFunctionName(c, g, ev_get_js_wrapper_func(j), 0, &rval, &rval);
    if (!ok) {
      NSDL2_JAVA_SCRIPT(vptr, NULL, "Error Emiting click event");

      NS_EL_3_ATTR(EID_JS_CALL_FUNCTION_FAILED, vptr->user_index,
         vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
         vptr->sess_ptr->sess_name,
         vptr->cur_page->page_name,
         (char*)__FUNCTION__,
         "Error Emiting click event");

    } else {

      NSDL2_JAVA_SCRIPT(vptr, NULL, "Successfully emitted onclick event %s" 
         "xmlNode = %p, event index in ev_list[] = %d", 
         ev_get_js_wrapper_func(j), ev_get_node(j), j);
    }
  }
}


/* END OF FILE */
