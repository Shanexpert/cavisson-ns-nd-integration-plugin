#ifndef __JS_EVENT_INCLUDED__
#define __JS_EVENT_INCLUDED__

typedef enum _ns_type_js_event_ {
    onload,   /* Body event - keep at top */
    onunload, /* Body event - keep at top */
    /* general events */
    onclick,
    onselect,
    ondblclick,
    onerror,
    onfocus,
    onkeydown,
    onkeypress,
    onkeyup,
    onmousedown,
    onmousemove,
    onmouseout,
    onmouseover,
    onmouseup,
    onchange,
    onresize,
    onblur,
    MAX_TYPE_JS_EVENTS
    } ns_type_js_event_t;

#define START_GENERIC_JS_EVENTS 2
#define START_BODY_JS_EVENTS    0

/* How many events system will cater for */
#define MAX_JS_EVENTS 100
#define MAX_JS_FUNC_WRAPPER_SCRIPT_LEN 2048

extern const char * ev_get_name(int ev_type);
extern int ev_register(void *node, char *func_with_param, int ev_type, char *js_wrapper_script);
extern char * ev_get_func(int ev_id);
extern ns_type_js_event_t ev_get_event_type(int ev_id);
// extern char * ev_get_onload_event(int *pev_id);
extern char * ev_get_onload_event();
extern char * ev_get_onunload_event(int *pev_id);

extern int ev_get_id(const char *name);
extern void *ev_get_node(int index);
extern int ev_get_type(int index);
extern char *ev_get_js_wrapper_func(int index);

extern void init_js_events();

#endif
