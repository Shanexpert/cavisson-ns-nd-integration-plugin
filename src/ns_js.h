/******************************************************************
 * Name                 : ns_js.h 
 * Purpose              : Header file of Java Script Engine 
 * Note                 :
 * Initial Version      : Sun Feb 20 13:01:16 IST 2011
 * Modification History :
 ******************************************************************/

#ifndef __NS_JS_H__
#define __NS_JS_H__

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libxml/HTMLparser.h>

#define XP_UNIX
#include "jsapi.h"

/*
typedef struct VuserJSFields {
   JSContext *js_context;
   JSObject  *js_object;
}
*/
#define FREE_XML_CHAR(ptr) { \
             if(ptr) {              \
               NSDL4_JAVA_SCRIPT(NULL, NULL, "Freeing XML ptr = %p", ptr); \
               xmlFree((xmlChar *)ptr);       \
               ptr = NULL;         \
             }                      \
          }



#define XML_FREE_JS_DOM_BUFFER(cptr, ptr) { \
           if(ptr) {              \
             NSDL4_JAVA_SCRIPT(NULL, cptr, "Freeing JS response buffer, ptr = %p", ptr); \
             xmlFree(ptr);       \
           }                      \
        }

#define TYPE_ELEMENT 0
#define TYPE_SCRIPT 1

#define CONTEXT_STACK_CHUNK_SIZE        4096

#define NS_JS_DISABLE                   0
#define NS_JS_DO_NOT_CHECK_POINT        1
#define NS_JS_DO_CHECK_POINT            2

#define NS_JS_RUNTIME_MEMORY_MODE_PER_NVM 0
#define NS_JS_RUNTIME_MEMORY_MODE_PER_VUSER 1

extern JSRuntime *ns_js_rt;  // Runtime for JS
extern int kw_set_g_java_script(char *buf, GroupSettings *, char *err_msg, int runtime_flag);
extern void kw_set_java_script_runtime_mem(char *buf);
extern int js_init();

extern char* process_buffer_in_js(connection *cptr, char *resp_buffer, int resp_buf_len, int *dom_resp_len);

typedef int (*NS_JS_FEED_FUNC) (void *c, void *g, int type, char *id, char *value);
typedef char * (*NS_JS_GET_FUNC) (void *c, void *g, char *id);

extern int html_parse_feed_elements(connection*, void *, void *, void *);
extern void html_parse_sync_elements(void *, NS_JS_GET_FUNC, void *, void *);
extern xmlChar* html_parse_print_tree(void *doc, VUser *vptr, int *len);
extern char *bootstrap_js_buffer;
extern int bootstrap_js_buffer_len;
extern inline xmlNode * html_parse_get_root(void *doc);
extern inline void free_dom_and_js_ctx(VUser *vptr);
extern inline void free_javascript_data(VUser *vptr, u_ns_ts_t now);
extern inline void init_javascript_data(VUser *vptr, u_ns_ts_t now);
extern inline void destroy_js_runtime_per_user(VUser *vptr, u_ns_ts_t now);

extern int ns_js_feed_form_input_element(void *c, void *g, xmlNode *form_node_ptr, char *id, char *name, char *type, char *value, char *content, int checked, int multiple); 
extern inline void set_form_input_element_in_js(VUser *vptr, xmlNode *form_node_ptr, char *id, char *name, char *type, char *value, char *content);
extern inline char *get_form_data_from_js(VUser *vptr, xmlNode *form_node_ptr, int form_encoding_type);
extern char *get_window_location_href(VUser *vptr, JSContext *js_context, JSObject *global);
extern void set_x_y_coordinates_for_form_image(VUser *vptr, char *img_name, xmlNode *form_node, int xval, int yval);
void search_and_emit_event(VUser *vptr, JSContext *c, JSObject *g,  xmlNode *node_clicked, char *eventname);
#endif
