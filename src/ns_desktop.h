#ifndef NS_DESKTOP_H
#define NS_DESKTOP_H


#define DESKTOP_SUCCESS		0
#define DESKTOP_ERROR		-1

#define MOUSE_MOVE_ABSOLUTE	0
#define MOUSE_MOVE_RELATIVE	1

#define MOUSE_LEFT_CLICK	0
#define MOUSE_LEFT_BUTTON_DOWN	1
#define MOUSE_LEFT_BUTTON_UP	2
#define MOUSE_RIGHT_CLICK	3
#define MOUSE_RIGHT_BUTTON_DOWN	4
#define MOSUE_RIGHT_BUTTON_UP	5


#define KEY_PRESS_AND_RELEASE	0
#define KEY_DOWN		1
#define KEY_UP			2

typedef struct KeyApiCount
{
  int key;
  int down;
  int up;
  int type;
  int sync;
}KeyApiCount;

typedef struct MouseApiCount
{
  int down;
  int up;
  int click;
  int double_click;
  int mouse_move;
  int mouse_drag;
}MouseApiCount;


int ns_desktop_open();
int ns_desktop_close();
int ns_desktop_key_type(int type, char *input);
int ns_desktop_wait_sync(int msec);
int ns_desktop_mouse_move(int type, int mouseX, int mouseY);
int ns_desktop_mouse_click(int type);
int ns_desktop_mouse_double_click(int button_type);

int ns_parse_mouse_apis_ex(char *line, FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx);
int ns_parse_key_apis_ex(char *line, FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx);
int ns_parse_key_apis(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type);
int ns_parse_mouse_apis(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type);
int ns_parse_sync_api(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type);
#endif
