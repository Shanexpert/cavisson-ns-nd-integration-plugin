#ifndef _NS_CLICK_SCRIPT_PARSE_H_
#define _NS_CLICK_SCRIPT_PARSE_H_

extern int parse_ns_browser     (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_link        (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_button      (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_edit_field  (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_check_box   (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_radio_group (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_list        (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_form        (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_map_area    (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_submit_image(FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_js_dialog   (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_text_area   (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_span        (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_scroll      (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_element     (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_mouse_hover (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_mouse_out   (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_key_event   (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_get_num_domelement (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_browse_file (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_js_checkpoint (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_execute_js  (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);
extern int parse_ns_mouse_move  (FILE *clickscript_fp, FILE *outfp, char *flow_file, char *flow_outfile, int sess_idx);

extern int free_attributes_array (char **att);


extern char *attribute_name[];
#ifdef NS_DEBUG_ON
extern char *attributes2str(char **att, char *msg);
#endif
#endif

