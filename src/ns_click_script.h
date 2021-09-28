#ifndef NS_CLICK_SCRIPT_H
#define NS_CLICK_SCRIPT_H

/* Flag passed as an argument to methods that traverse 
 * the DOM tree recursively */
#define FIRST_TIME 1 

/* Bit flags passed to recursive method for searching 
 * a clicked element in DOM */
#define ATTR_TAG     0x0001
#define ATTR_ID      0x0002
#define ATTR_NAME    0x0004
#define ATTR_TYPE    0x0008
#define ATTR_VALUE   0x0010
#define ATTR_CONTENT 0x0020
#define ATTR_ALT     0x0040
#define ATTR_SHAPE   0x0080
#define ATTR_COORDS  0x0100
#define ATTR_TITLE   0x0200

/* Convenience macros */
#define ATTR_MATCH_DEFAULT ATTR_TAG|ATTR_ID|ATTR_NAME|ATTR_TYPE|ATTR_TITLE
#define ATTR_MATCH_IMAGE_ALT ATTR_ID|ATTR_NAME|ATTR_ALT

/*Indices to nodes_list array passed to search_clicked_element_in_dom()*/
#define ROOT_NODE_INDEX    0 /* Passed to recursive search method */
#define CLICKED_NODE_INDEX 1 /* Returned from recursive search method */
#define FORM_NODE_INDEX    2 /* Container form returned from recursive search
                              * method and passed to next call of the method */
#define SELECT_NODE_INDEX  3 /* Container select returned from recursive search 
                              * method and passed to next call of the method */
#define ANCHOR_NODE_INDEX  4 /* Container anchor node returned from recursive search
                              * method and passed to next call of the method */

/* In case of Form method="post", encoding type macros */
#define FORM_POST_ENC_TYPE_APPLICATION_X_WWW_FORM_URL_ENCODED 0 
#define FORM_POST_ENC_TYPE_TEXT_PLAIN 1
#define FORM_POST_ENC_TYPE_MULTIPART_FORM_DATA 2

extern void copy_click_actions_table_to_shr(void);
extern inline void set_server(connection *cptr);
inline void extract_and_set_url_params(connection *cptr, char *url, unsigned short *p_url_len);
extern int search_clicked_element_in_dom(char **att, int first_time, int attributes_to_be_matched, void **nodes_list);
extern int  handle_click_actions(VUser *vptr, int page_id, int ca_idx);
extern int read_attributes_array_from_ca_table(VUser *vptr, char **att, ClickActionTableEntry_Shr *ca);
extern int get_full_element(VUser *vptr, const StrEnt_Shr* seg_tab_ptr, char *elm_buf, int *elm_size);
#endif
