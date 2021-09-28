#include <string.h>
#include <stdio.h>
#include "ns_js_events.h"

typedef struct _js_event_map_ {
    const char *ev_name;
    const char *ev_desc;
    }js_event_map_t;

/* ?? What if user created too Long func name */
#define MAX_JS_FUNC_LEN 64

typedef struct _ev_list_ {
    ns_type_js_event_t ev_type;
    void *node;
    char js_wrapper_func[MAX_JS_FUNC_LEN];
    }ev_list_t;

static js_event_map_t ev_map[] = {
    /* events applicable of body only - put at top */
    {"onload", "The page is loaded  "},
    {"onunload", "The user exits the page "},
    /* events applicable to any id */
    {"onclick", "Mouse clicks an object "},
    {"onselect", "Text is selected "},
    {"ondblclick", "Mouse double-clicks an object "},
    {"onerror", "An error occurs when loading a document or an image "},
    {"onfocus", "An element gets focus "},
    {"onkeydown", "A keyboard key is pressed "},
    {"onkeypress", "A keyboard key is pressed or held down "},
    {"onkeyup", "A keyboard key is released "},
    {"onmousedown", "A mouse button is pressed "},
    {"onmousemove", "The mouse is moved "},
    {"onmouseout", "The mouse is moved off an element "},
    {"onmouseover", "The mouse is moved over an element "},
    {"onmouseup", "A mouse button is released "},
    {"onchange", "The content of a field changes "},
    {"onresize", "A window or frame is resized "},
    {"onblur", "An element loses focus"}
    };

static ev_list_t ev_list[MAX_JS_EVENTS];
static int current_ev_ctr = 0;

/* So that we dont need to travel all events */
static int current_load_ev_ctr = -1;
static int current_unload_ev_ctr = -1;
static int cached_load_ev_id[MAX_JS_EVENTS];
static int cached_unload_ev_id[MAX_JS_EVENTS];

void init_js_events()
{
  current_ev_ctr = 0;
  current_load_ev_ctr = -1;
  current_unload_ev_ctr = -1;

}

const char * ev_get_name(int ev_type)
{
    return(ev_map[ev_type].ev_name);
}

int ev_get_id(const char *name)
{
  int i;

  for (i = 0; i < MAX_TYPE_JS_EVENTS; i++)
  {
    if (strcmp(name, ev_map[i].ev_name) == 0)
     return i;
  }  
    return -1;
}


void *ev_get_node(int index)
{
  return ev_list[index].node;
}

int ev_get_type(int index)
{
  return ev_list[index].ev_type;
}

char *ev_get_js_wrapper_func(int index)
{
  return ev_list[index].js_wrapper_func;
}


int ev_register(void *node, char *func_with_param, int ev_type, char *js_wrapper_script)
{
    int ev_id = -1;

    if (current_ev_ctr >= MAX_JS_EVENTS) {
        return -1;
    }
    /* Allocate Event slot */
    ev_id = current_ev_ctr;
    ++current_ev_ctr;
    ev_list[ev_id].ev_type = ev_type;
    ev_list[ev_id].node = node;

    /* Create a unique wrapper for the user func */
    snprintf(ev_list[ev_id].js_wrapper_func, MAX_JS_FUNC_LEN, "__ns_js_ev_%d__", ev_id); 

    /* Generate the script to invoke the user func via our function
       this is done to avoid parsing the paramters of user func */
    snprintf(js_wrapper_script, MAX_JS_FUNC_WRAPPER_SCRIPT_LEN,
                "function %s(){%s;};\n", ev_list[ev_id].js_wrapper_func, func_with_param);

    /* Cache the special event */
    switch (ev_type) {
        case onload:
            current_load_ev_ctr++;
            cached_load_ev_id[current_load_ev_ctr] = ev_id;
            break;
        case onunload:
            current_unload_ev_ctr++;
            cached_unload_ev_id[current_unload_ev_ctr] = ev_id;
            break;
    }
    return ev_id;
}

char * ev_get_func(int ev_id)
{
    return ev_list[ev_id].js_wrapper_func;
}

ns_type_js_event_t ev_get_event_type(int ev_id)
{
    return ev_list[ev_id].ev_type;
}

// Assumptionm is that there is only one onload. If many, last will be run
// Need to study more
char * ev_get_onload_event()
{
    if (current_load_ev_ctr == -1) return NULL;
    return ev_get_func(cached_load_ev_id[current_load_ev_ctr]);
}
/*
char * ev_get_onload_event(int *pev_id)
{
    int ev_id = *pev_id;
    if (ev_id > current_load_ev_ctr) return NULL;
    *pev_id +=1;
    return ev_get_func(cached_load_ev_id[ev_id]);
}
*/

char * ev_get_onunload_event(int *pev_id)
{
    int ev_id = *pev_id;
    if (ev_id > current_unload_ev_ctr) return NULL;
    *pev_id +=1;
    return ev_get_func(cached_unload_ev_id[ev_id]);
}
