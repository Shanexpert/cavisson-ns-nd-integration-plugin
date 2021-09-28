/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include "gtkmozembed.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream.h>
#include <unistd.h>

// mozilla specific headers
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "prenv.h"
#include "nkcache/nsICacheService.h"
#include "necko/nsNetCID.h"
#include "xpcom/nsCOMPtr.h"
#include "xpcom/nsIServiceManager.h"

static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

int url_file_fd = -1;
int url_detail_file_fd = -1;
int c_file_fd = -1;
int h_file_fd = -1;
int last_line_done = 0;
char cav_file_name[1024];
GtkWidget* g_save_window;

typedef struct _TestGtkBrowser {
  GtkWidget  *topLevelWindow;
  GtkWidget  *topLevelVBox;
  GtkWidget  *menuBar;
  GtkWidget  *fileMenuItem;
  GtkWidget  *fileMenu;
  GtkWidget  *fileOpenNewBrowser;
  GtkWidget  *fileStream;
  GtkWidget  *fileClose;
  GtkWidget  *fileQuit;
  GtkWidget  *toolbarHBox;
  GtkWidget  *toolbar;
  GtkWidget  *backButton;
  GtkWidget  *stopButton;
  GtkWidget  *forwardButton;
  GtkWidget  *reloadButton;
  GtkWidget  *urlEntry;
  GtkWidget  *mozEmbed;
  GtkWidget  *progressAreaHBox;
  GtkWidget  *progressBar;
  GtkWidget  *statusAlign;
  GtkWidget  *statusBar;
  const char *statusMessage;
  int         loadPercent;
  int         bytesLoaded;
  int         maxBytesLoaded;
  char       *tempMessage;
  gboolean menuBarOn;
  gboolean toolBarOn;
  gboolean locationBarOn;
  gboolean statusBarOn;

} TestGtkBrowser;

TestGtkBrowser  *g_browser;

typedef struct _GtkCommand {
  GtkWidget  *topLevelWindow;
  GtkWidget  *topLevelVBox;
  GtkWidget  *menuBar;
  GtkWidget  *fileMenuItem;
  GtkWidget  *fileMenu;
  GtkWidget  *fileSave;
  GtkWidget  *fileStartRecord;
  GtkWidget  *fileStopRecord;
  GtkWidget  *fileQuit;
  GtkWidget  *toolbarHBox;
  GtkWidget  *toolbar;
  GtkWidget  *startRecordButton;
  GtkWidget  *stopRecordButton;
  char* url_file_name;
} GtkCommand;

GtkCommand *g_command;
// the list of browser windows currently open
GList *browser_list = g_list_alloc();

static TestGtkBrowser *new_gtk_browser    (guint32 chromeMask, int isframe);
static void            set_browser_visibility (TestGtkBrowser *browser,
					       gboolean visibility);
static GtkCommand *new_gtk_command (void);
static void       set_command_visibility (GtkCommand* commands);

static int num_browsers = 0;

// callbacks from the UI
static void     back_clicked_cb    (GtkButton   *button, 
				    TestGtkBrowser *browser);
static void     stop_clicked_cb    (GtkButton   *button,
				    TestGtkBrowser *browser);
static void     forward_clicked_cb (GtkButton   *button,
				    TestGtkBrowser *browser);
static void     reload_clicked_cb  (GtkButton   *button,
				    TestGtkBrowser *browser);
static void     url_activate_cb    (GtkEditable *widget, 
				    TestGtkBrowser *browser);
static void     menu_open_new_cb   (GtkMenuItem *menuitem,
				    TestGtkBrowser *browser);
static void     menu_stream_cb     (GtkMenuItem *menuitem,
				    TestGtkBrowser *browser);
static void     menu_close_cb      (GtkMenuItem *menuitem,
				    TestGtkBrowser *browser);
static void     menu_quit_cb       (GtkMenuItem *menuitem,
				    TestGtkBrowser *browser);
static gboolean delete_cb          (GtkWidget *widget, GdkEventAny *event,
				    TestGtkBrowser *browser);
static void     destroy_cb         (GtkWidget *widget,
				    TestGtkBrowser *browser);

// callbacks from the widget
static void location_changed_cb  (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void title_changed_cb     (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void load_started_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void load_finished_cb     (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void net_state_change_cb  (GtkMozEmbed *embed, gint flags,
				  guint status, TestGtkBrowser *browser);
static void net_state_change_all_cb (GtkMozEmbed *embed, const char *uri,
				     gint flags, guint status,
				     TestGtkBrowser *browser);
static void progress_change_cb   (GtkMozEmbed *embed, gint cur, gint max,
				  TestGtkBrowser *browser);
static void progress_change_all_cb (GtkMozEmbed *embed, const char *uri,
				    gint cur, gint max,
				    TestGtkBrowser *browser);
static void link_message_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void js_status_cb         (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void new_window_cb        (GtkMozEmbed *embed,
				  GtkMozEmbed **retval, guint chromemask,
				  TestGtkBrowser *browser);
static void visibility_cb        (GtkMozEmbed *embed, 
				  gboolean visibility,
				  TestGtkBrowser *browser);
static void destroy_brsr_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser);
static gint open_uri_cb          (GtkMozEmbed *embed, const char *uri,
				  TestGtkBrowser *browser);
static void size_to_cb           (GtkMozEmbed *embed, gint width,
				  gint height, TestGtkBrowser *browser);
static gint dom_key_down_cb      (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
				  TestGtkBrowser *browser);
static gint dom_key_press_cb     (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
				  TestGtkBrowser *browser);
static gint dom_key_up_cb        (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_down_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_up_cb      (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_click_cb   (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_dbl_click_cb (GtkMozEmbed *embed, 
				  nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_over_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_out_cb     (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);

// callbacks from the singleton object
static void new_window_orphan_cb (GtkMozEmbedSingle *embed,
				  GtkMozEmbed **retval, guint chromemask,
				  gpointer data);

// some utility functions
static void update_status_bar_text  (TestGtkBrowser *browser);
static void update_temp_message     (TestGtkBrowser *browser,
				     const char *message);
static void update_nav_buttons      (TestGtkBrowser *browser);
  
// callbacks for the command frame
static void  menu_fileSave_cb   (GtkMenuItem *menuitem, GtkCommand *command);
static void  menu_fileStartRecord_cb  (GtkMenuItem  *menuitem, GtkCommand* command);
static void  menu_fileStopRecord_cb   (GtkMenuItem  *menuitem,  GtkCommand*  command);
static void  menu_fileQuit_cb   (GtkMenuItem  *menuitem,  GtkCommand*  command);
static void  startRecord_clicked_cb (GtkButton *button, GtkCommand *command);
static void  stopRecord_clicked_cb (GtkButton *button, GtkCommand *command);

nsCOMPtr<nsICacheService> CacheService = NULL;

int
main(int argc, char **argv)
{
  GtkWidget* window;
  GtkWidget* hpaned;
#ifdef NS_TRACE_MALLOC
  argc = NS_TraceMallocStartupArgs(argc, argv);
#endif

  gtk_set_locale();
  gtk_init(&argc, &argv);

  char *home_path;
  char *full_path;
  home_path = PR_GetEnv("HOME");
  if (!home_path) {
    fprintf(stderr, "Failed to get HOME\n");
    exit(1);
  }

  setenv("CAVISSON_TOGGLE", "0", 1);

  full_path = g_strdup_printf("%s/%s", home_path, ".TestGtkEmbed");
  
  gtk_moz_embed_set_profile_path(full_path, "TestGtkEmbed");

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title(GTK_WINDOW(window), "Cavisson Systems");

  gtk_widget_set_usize (window, 700, 500);
  /* Sets the border width of the window. */
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		    GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  //Create horizonal Pane
  //On left pane, put recording controls
  //On right put mozilla
  hpaned = gtk_hpaned_new();
  gtk_container_add(GTK_CONTAINER(window), hpaned);

  gtk_paned_set_handle_size (GTK_PANED(hpaned),
			     10);
  gtk_paned_set_gutter_size (GTK_PANED(hpaned),
			     15);

  gtk_widget_show(hpaned);

  GtkCommand* commands = new_gtk_command(); 
  g_command = commands;
  set_command_visibility(commands);
  gtk_paned_add1(GTK_PANED(hpaned), commands->topLevelWindow);

  TestGtkBrowser *browser = new_gtk_browser(GTK_MOZ_EMBED_FLAG_DEFAULTCHROME, 1);
  g_browser = browser;

  // set our minimum size
  gtk_widget_set_usize(browser->mozEmbed, 400, 400);
  
  set_browser_visibility(browser, FALSE);

  gtk_paned_add2(GTK_PANED(hpaned), browser->topLevelWindow);

  if (argc > 1)
    gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), argv[1]);

  // get the singleton object and hook up to its new window callback
  // so we can create orphaned windows.

  GtkMozEmbedSingle *single;

  single = gtk_moz_embed_single_get();
  if (!single) {
    fprintf(stderr, "Failed to get singleton embed object!\n");
    exit(1);
  }

  gtk_signal_connect(GTK_OBJECT(single), "new_window_orphan",
		     GTK_SIGNAL_FUNC(new_window_orphan_cb), NULL);

  //gtk_widget_set_sensitive(g_browser_urlEntry, FALSE);
  gtk_widget_show(window);

  gtk_main();
}

static GtkCommand *
new_gtk_command (void)
{
  GtkCommand *commands = 0;
  
  commands = g_new0(GtkCommand, 1);

  commands->topLevelWindow = gtk_frame_new("NetStorm/Capture");
  gtk_widget_set_usize(commands->topLevelWindow, 200, 400);

  commands->topLevelVBox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(commands->topLevelWindow),
		    commands->topLevelVBox);
  commands->menuBar = gtk_menu_bar_new();

  commands->fileMenuItem = gtk_menu_item_new_with_label("File");
  commands->fileMenu = gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(commands->fileMenuItem),
			     commands->fileMenu);

  commands->fileSave = 
    gtk_menu_item_new_with_label("Start New Session...");
  gtk_menu_append(GTK_MENU(commands->fileMenu),
		  commands->fileSave);
  
  commands->fileStartRecord = 
    gtk_menu_item_new_with_label("Start Recording");
  gtk_menu_append(GTK_MENU(commands->fileMenu),
		  commands->fileStartRecord);
  
  commands->fileStopRecord =
    gtk_menu_item_new_with_label("Stop Recording");
  gtk_menu_append(GTK_MENU(commands->fileMenu),
		  commands->fileStopRecord);

  commands->fileQuit =
    gtk_menu_item_new_with_label("Quit");
  gtk_menu_append(GTK_MENU(commands->fileMenu),
		  commands->fileQuit);
  
  // append it
  gtk_menu_bar_append(GTK_MENU_BAR(commands->menuBar), commands->fileMenuItem);

  // add it to the vbox
  gtk_box_pack_start(GTK_BOX(commands->topLevelVBox),
		     commands->menuBar,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  commands->toolbarHBox = gtk_hbox_new(FALSE, 0);
  // add that hbox to the vbox
  gtk_box_pack_start(GTK_BOX(commands->topLevelVBox), 
		     commands->toolbarHBox,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  // new horiz toolbar with buttons + icons
  commands->toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,
				     GTK_TOOLBAR_BOTH);
  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(commands->toolbarHBox), commands->toolbar,
		   FALSE, // expand
		   FALSE, // fill
		   0);    // padding
  commands->startRecordButton =
    gtk_toolbar_append_item(GTK_TOOLBAR(commands->toolbar),
			    "Start Recording",
			    "Start Recording",
			    "Start Recording",
			    0, // replace with icon
			    GTK_SIGNAL_FUNC(startRecord_clicked_cb),
			    commands);
  commands->stopRecordButton = 
    gtk_toolbar_append_item(GTK_TOOLBAR(commands->toolbar),
			    "Stop Recording",
			    "Stop Recording",
			    "Stop Recording",
			    0, // replace with icon
			    GTK_SIGNAL_FUNC(stopRecord_clicked_cb),
			    commands);

  // by default none of the buttons are marked as sensitive until a session name file is defined. 
  gtk_widget_set_sensitive(commands->startRecordButton, FALSE);
  gtk_widget_set_sensitive(commands->stopRecordButton, FALSE);
  //Make menu items unsensitive
  gtk_widget_set_sensitive(commands->fileStartRecord, FALSE);
  gtk_widget_set_sensitive(commands->fileStopRecord, FALSE);

  // catch the destruction of the toplevel window
  gtk_signal_connect(GTK_OBJECT(commands->topLevelWindow), "delete_event",
		     GTK_SIGNAL_FUNC(delete_cb), commands);

  gtk_signal_connect(GTK_OBJECT(commands->fileSave), "activate",
		     GTK_SIGNAL_FUNC(menu_fileSave_cb), commands);

  gtk_signal_connect(GTK_OBJECT(commands->fileStartRecord), "activate",
		     GTK_SIGNAL_FUNC(menu_fileStartRecord_cb), commands);

  gtk_signal_connect(GTK_OBJECT(commands->fileStopRecord), "activate",
		     GTK_SIGNAL_FUNC(menu_fileStopRecord_cb), commands);
  // quit the application
  gtk_signal_connect(GTK_OBJECT(commands->fileQuit), "activate",
		     GTK_SIGNAL_FUNC(menu_fileQuit_cb), commands);

  return commands;
}

static void
set_command_visibility (GtkCommand* commands)
{
  gtk_widget_show_all(commands->menuBar);
  gtk_widget_show_all(commands->toolbarHBox);
  gtk_widget_show(commands->topLevelVBox);
  gtk_widget_show(commands->topLevelWindow);
}


static TestGtkBrowser *
new_gtk_browser(guint32 chromeMask, int isframe)
{
  guint32         actualChromeMask = chromeMask;
  TestGtkBrowser *browser = 0;

  num_browsers++;

  browser = g_new0(TestGtkBrowser, 1);

  browser_list = g_list_prepend(browser_list, browser);

  browser->menuBarOn = FALSE;
  browser->toolBarOn = FALSE;
  browser->locationBarOn = FALSE;
  browser->statusBarOn = FALSE;

  g_print("new_gtk_browser\n");

  if (chromeMask == GTK_MOZ_EMBED_FLAG_DEFAULTCHROME)
    actualChromeMask = GTK_MOZ_EMBED_FLAG_ALLCHROME;

  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_MENUBARON)
  {
    browser->menuBarOn = FALSE;
    g_print("\tmenu bar\n");
  }
  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_TOOLBARON)
  {
    browser->toolBarOn = TRUE;
    g_print("\ttool bar\n");
  }
  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_LOCATIONBARON)
  {
    browser->locationBarOn = TRUE;
    g_print("\tlocation bar\n");
  }
  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_STATUSBARON)
  {
    browser->statusBarOn = TRUE;
    g_print("\tstatus bar\n");
  }

  // create our new toplevel window
  if (isframe)
    browser->topLevelWindow = gtk_frame_new(NULL);
  else
    browser->topLevelWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  // new vbox
  browser->topLevelVBox = gtk_vbox_new(FALSE, 0);
  // add it to the toplevel window
  gtk_container_add(GTK_CONTAINER(browser->topLevelWindow),
		    browser->topLevelVBox);
  // create our menu bar
  browser->menuBar = gtk_menu_bar_new();
  // create the file menu
  browser->fileMenuItem = gtk_menu_item_new_with_label("File");
  browser->fileMenu = gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(browser->fileMenuItem),
			     browser->fileMenu);

  browser->fileOpenNewBrowser = 
    gtk_menu_item_new_with_label("Open New Browser");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileOpenNewBrowser);
  
  browser->fileStream =
    gtk_menu_item_new_with_label("Test Stream");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileStream);

  browser->fileClose =
    gtk_menu_item_new_with_label("Close");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileClose);

  browser->fileQuit =
    gtk_menu_item_new_with_label("Quit");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileQuit);
  
  // append it
  gtk_menu_bar_append(GTK_MENU_BAR(browser->menuBar), browser->fileMenuItem);

  // add it to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox),
		     browser->menuBar,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  // create the hbox that will contain the toolbar and the url text entry bar
  browser->toolbarHBox = gtk_hbox_new(FALSE, 0);
  // add that hbox to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), 
		     browser->toolbarHBox,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  // new horiz toolbar with buttons + icons
  browser->toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,
				     GTK_TOOLBAR_BOTH);
  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->toolbarHBox), browser->toolbar,
		   FALSE, // expand
		   FALSE, // fill
		   0);    // padding
  // new back button
  browser->backButton =
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Back",
			    "Go Back",
			    "Go Back",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(back_clicked_cb),
			    browser);
  // new stop button
  browser->stopButton = 
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Stop",
			    "Stop",
			    "Stop",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(stop_clicked_cb),
			    browser);
  // new forward button
  browser->forwardButton =
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Forward",
			    "Forward",
			    "Forward",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(forward_clicked_cb),
			    browser);
  // new reload button
  browser->reloadButton = 
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Reload",
			    "Reload",
			    "Reload",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(reload_clicked_cb),
			    browser);
  // create the url text entry
  browser->urlEntry = gtk_entry_new();
  //g_browser_urlEntry = browser->urlEntry;
  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->toolbarHBox), browser->urlEntry,
		     TRUE, // expand
		     TRUE, // fill
		     0);    // padding
  // create our new gtk moz embed widget
  browser->mozEmbed = gtk_moz_embed_new();
  // add it to the toplevel vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), browser->mozEmbed,
		     TRUE, // expand
		     TRUE, // fill
		     0);   // padding
  // create the new hbox for the progress area
  browser->progressAreaHBox = gtk_hbox_new(FALSE, 0);
  // add it to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), browser->progressAreaHBox,
		     FALSE, // expand
		     FALSE, // fill
		     0);   // padding
  // create our new progress bar
  browser->progressBar = gtk_progress_bar_new();
  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->progressAreaHBox), browser->progressBar,
		     FALSE, // expand
		     FALSE, // fill
		     0); // padding
  
  // create our status area and the alignment object that will keep it
  // from expanding
  browser->statusAlign = gtk_alignment_new(0, 0, 1, 1);
  gtk_widget_set_usize(browser->statusAlign, 1, -1);
  // create the status bar
  browser->statusBar = gtk_statusbar_new();
  gtk_container_add(GTK_CONTAINER(browser->statusAlign), browser->statusBar);
  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->progressAreaHBox), browser->statusAlign,
		     TRUE, // expand
		     TRUE, // fill
		     0);   // padding
  // by default none of the buttons are marked as sensitive.
  gtk_widget_set_sensitive(browser->backButton, FALSE);
  gtk_widget_set_sensitive(browser->stopButton, FALSE);
  gtk_widget_set_sensitive(browser->forwardButton, FALSE);
  gtk_widget_set_sensitive(browser->reloadButton, FALSE);
  
  // catch the destruction of the toplevel window
  /*gtk_signal_connect(GTK_OBJECT(browser->topLevelWindow), "delete_event",
    GTK_SIGNAL_FUNC(delete_cb), browser);*/

  // hook up the activate signal to the right callback
  gtk_signal_connect(GTK_OBJECT(browser->urlEntry), "activate",
		     GTK_SIGNAL_FUNC(url_activate_cb), browser);

  // hook up to the open new browser activation
  gtk_signal_connect(GTK_OBJECT(browser->fileOpenNewBrowser), "activate",
		     GTK_SIGNAL_FUNC(menu_open_new_cb), browser);
  // hook up to the stream test
  gtk_signal_connect(GTK_OBJECT(browser->fileStream), "activate",
		     GTK_SIGNAL_FUNC(menu_stream_cb), browser);
  // close this window
  gtk_signal_connect(GTK_OBJECT(browser->fileClose), "activate",
		     GTK_SIGNAL_FUNC(menu_close_cb), browser);
  // quit the application
  gtk_signal_connect(GTK_OBJECT(browser->fileQuit), "activate",
		     GTK_SIGNAL_FUNC(menu_quit_cb), browser);

  // hook up the location change to update the urlEntry
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "location",
		     GTK_SIGNAL_FUNC(location_changed_cb), browser);
  // hook up the title change to update the window title
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "title",
		     GTK_SIGNAL_FUNC(title_changed_cb), browser);
  // hook up the start and stop signals
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_start",
		     GTK_SIGNAL_FUNC(load_started_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_stop",
		     GTK_SIGNAL_FUNC(load_finished_cb), browser);
  // hook up to the change in network status
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_state",
		     GTK_SIGNAL_FUNC(net_state_change_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_state_all",
		     GTK_SIGNAL_FUNC(net_state_change_all_cb), browser);
  // hookup to changes in progress
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "progress",
		     GTK_SIGNAL_FUNC(progress_change_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "progress_all",
		     GTK_SIGNAL_FUNC(progress_change_all_cb), browser);
  // hookup to changes in over-link message
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "link_message",
		     GTK_SIGNAL_FUNC(link_message_cb), browser);
  // hookup to changes in js status message
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "js_status",
		     GTK_SIGNAL_FUNC(js_status_cb), browser);
  // hookup to see whenever a new window is requested
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "new_window",
		     GTK_SIGNAL_FUNC(new_window_cb), browser);
  // hookup to any requested visibility changes
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "visibility",
		     GTK_SIGNAL_FUNC(visibility_cb), browser);
  // hookup to the signal that says that the browser requested to be
  // destroyed
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "destroy_browser",
		     GTK_SIGNAL_FUNC(destroy_brsr_cb), browser);
  // hookup to the signal that is called when someone clicks on a link
  // to load a new uri
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "open_uri",
		     GTK_SIGNAL_FUNC(open_uri_cb), browser);
  // this signal is emitted when there's a request to change the
  // containing browser window to a certain height, like with width
  // and height args for a window.open in javascript
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "size_to",
		     GTK_SIGNAL_FUNC(size_to_cb), browser);
  // key event signals
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_key_down",
		     GTK_SIGNAL_FUNC(dom_key_down_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_key_press",
		     GTK_SIGNAL_FUNC(dom_key_press_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_key_up",
		     GTK_SIGNAL_FUNC(dom_key_up_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_down",
		     GTK_SIGNAL_FUNC(dom_mouse_down_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_up",
		     GTK_SIGNAL_FUNC(dom_mouse_up_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_click",
		     GTK_SIGNAL_FUNC(dom_mouse_click_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_dbl_click",
		     GTK_SIGNAL_FUNC(dom_mouse_dbl_click_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_over",
		     GTK_SIGNAL_FUNC(dom_mouse_over_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_out",
		     GTK_SIGNAL_FUNC(dom_mouse_out_cb), browser);
  // hookup to when the window is destroyed
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "destroy",
		     GTK_SIGNAL_FUNC(destroy_cb), browser);
  
  // set the chrome type so it's stored in the object
  gtk_moz_embed_set_chrome_mask(GTK_MOZ_EMBED(browser->mozEmbed),
				actualChromeMask);

  return browser;
}

void
set_browser_visibility (TestGtkBrowser *browser, gboolean visibility)
{
  //if (!visibility)
  if ((getenv("CAVISSON_TOGGLE") && !strcmp(getenv("CAVISSON_TOGGLE"), "0")) || (!visibility))
  {
    gtk_widget_hide(browser->topLevelWindow);
    return;
  }
  
  if (browser->menuBarOn)
    gtk_widget_show_all(browser->menuBar);
  else
    gtk_widget_hide_all(browser->menuBar);

  // since they are on the same line here...
  if (browser->toolBarOn || browser->locationBarOn)
    gtk_widget_show_all(browser->toolbarHBox);
  else 
    gtk_widget_hide_all(browser->toolbarHBox);

  if (browser->statusBarOn)
    gtk_widget_show_all(browser->progressAreaHBox);
  else
    gtk_widget_hide_all(browser->progressAreaHBox);

  gtk_widget_show(browser->mozEmbed);
  gtk_widget_show(browser->topLevelVBox);
  gtk_widget_show(browser->topLevelWindow);
}

void
back_clicked_cb (GtkButton *button, TestGtkBrowser *browser)
{
  gtk_moz_embed_go_back(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
stop_clicked_cb (GtkButton *button, TestGtkBrowser *browser)
{
  g_print("stop_clicked_cb\n");
  gtk_moz_embed_stop_load(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
forward_clicked_cb (GtkButton *button, TestGtkBrowser *browser)
{
  g_print("forward_clicked_cb\n");
  gtk_moz_embed_go_forward(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
reload_clicked_cb  (GtkButton *button, TestGtkBrowser *browser)
{
  g_print("reload_clicked_cb\n");
  GdkModifierType state = (GdkModifierType)0;
  gint x, y;
  gdk_window_get_pointer(NULL, &x, &y, &state);
  
  gtk_moz_embed_reload(GTK_MOZ_EMBED(browser->mozEmbed),
		       (state & GDK_SHIFT_MASK) ?
		       GTK_MOZ_EMBED_FLAG_RELOADBYPASSCACHE : 
		       GTK_MOZ_EMBED_FLAG_RELOADNORMAL);
}

void 
stream_clicked_cb  (GtkButton   *button, TestGtkBrowser *browser)
{
  const char *data;
  const char *data2;
  data = "<html>Hi";
  data2 = " there</html>\n";
  g_print("stream_clicked_cb\n");
  gtk_moz_embed_open_stream(GTK_MOZ_EMBED(browser->mozEmbed),
			    "file://", "text/html");
  gtk_moz_embed_append_data(GTK_MOZ_EMBED(browser->mozEmbed),
			    data, strlen(data));
  gtk_moz_embed_append_data(GTK_MOZ_EMBED(browser->mozEmbed),
			    data2, strlen(data2));
  gtk_moz_embed_close_stream(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
url_activate_cb    (GtkEditable *widget, TestGtkBrowser *browser)
{
  gchar *text = gtk_editable_get_chars(widget, 0, -1);
  g_print("loading url %s\n", text);
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), text);
  g_free(text);
}

void
menu_open_new_cb   (GtkMenuItem *menuitem, TestGtkBrowser *browser)
{
  g_print("opening new browser.\n");
  TestGtkBrowser *newBrowser = 
    new_gtk_browser(GTK_MOZ_EMBED_FLAG_DEFAULTCHROME, 0);
  gtk_widget_set_usize(newBrowser->mozEmbed, 400, 400);
  set_browser_visibility(newBrowser, TRUE);
}

void
menu_stream_cb     (GtkMenuItem *menuitem, TestGtkBrowser *browser)
{
  g_print("menu_stream_cb\n");
  const char *data;
  const char *data2;
  data = "<html>Hi";
  data2 = " there</html>\n";
  g_print("stream_clicked_cb\n");
  gtk_moz_embed_open_stream(GTK_MOZ_EMBED(browser->mozEmbed),
			    "file://", "text/html");
  gtk_moz_embed_append_data(GTK_MOZ_EMBED(browser->mozEmbed),
			    data, strlen(data));
  gtk_moz_embed_append_data(GTK_MOZ_EMBED(browser->mozEmbed),
			    data2, strlen(data2));
  gtk_moz_embed_close_stream(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
menu_close_cb (GtkMenuItem *menuitem, TestGtkBrowser *browser)
{
  gtk_widget_destroy(browser->topLevelWindow);
}

void
menu_quit_cb (GtkMenuItem *menuitem, TestGtkBrowser *browser)
{
  TestGtkBrowser *tmpBrowser;
  GList *tmp_list = browser_list;
  tmpBrowser = (TestGtkBrowser *)tmp_list->data;
  while (tmpBrowser) {
    tmp_list = tmp_list->next;
    gtk_widget_destroy(tmpBrowser->topLevelWindow);
    tmpBrowser = (TestGtkBrowser *)tmp_list->data;
  }
}

gboolean
delete_cb(GtkWidget *widget, GdkEventAny *event, TestGtkBrowser *browser)
{
  g_print("delete_cb\n");
  gtk_widget_destroy(widget);
  return TRUE;
}

void
destroy_cb         (GtkWidget *widget, TestGtkBrowser *browser)
{
  GList *tmp_list;
  g_print("destroy_cb\n");
  num_browsers--;
  tmp_list = g_list_find(browser_list, browser);
  browser_list = g_list_remove_link(browser_list, tmp_list);
  if (browser->tempMessage)
    g_free(browser->tempMessage);
  if (num_browsers == 0) {
#if 0
    if (getenv("CAVISSON_NUM_EMBED")) {
      char page_end_buf[64];
      const char* last_page;
      sprintf(page_end_buf, "          NUM_EMBED=%s);\n", getenv("CAVISSON_NUM_EMBED"));

      if (write(url_file_fd, page_end_buf, strlen(page_end_buf)) != strlen(page_end_buf))
	cout << "CAVISSON: Error in writing to url file" << endl;
      
      if (write(url_file_fd, "        next_page = next_page_", strlen("        next_page = next_page_")) != strlen("        next_page = next_page_"))
	cout << "CAVISSON: Error in writing to url file" << endl;

      if ((last_page = getenv("CAVISSON_LAST_PAGE"))) {
	if (write(url_file_fd, last_page, strlen(last_page)) != strlen(last_page))
	  cout << "CAVISSON: Error in writing to url file" << endl;
      }

      if (write(url_file_fd, "();\n        break;\n", strlen("();\n        break;\n")) != strlen(";\n        break;\n"))
	cout << "CAVISSON: Error in writing to url file" << endl;

      if (write(url_file_fd, "\n      default:\n        next_page = -1;\n    }\n  }\n}\n", strlen("\n      default:\n        next_page = -1;\n    }\n  }\n}\n")) != strlen("\n      default:\n        next_page = -1;\n    }\n  }\n}\n"))
	cout << "CAVISSON: Error in writing to url file" << endl;
    }
    
    if (!last_line_done)
      write(c_file_fd, "\n\treturn -1;\n}\n", strlen("\n\treturn -1;\n}\n"));
#endif
    gtk_main_quit();
  }
}

void
location_changed_cb (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  char *newLocation;
  int   newPosition = 0;
  g_print("location_changed_cb\n");
  newLocation = gtk_moz_embed_get_location(embed);
  if (newLocation)
  {
    gtk_editable_delete_text(GTK_EDITABLE(browser->urlEntry), 0, -1);
    gtk_editable_insert_text(GTK_EDITABLE(browser->urlEntry),
			     newLocation, strlen(newLocation), &newPosition);
    g_free(newLocation);
  }
  else
    g_print("failed to get location!\n");
  // always make sure to clear the tempMessage.  it might have been
  // set from the link before a click and we wouldn't have gotten the
  // callback to unset it.
  update_temp_message(browser, 0);
  // update the nav buttons on a location change
  update_nav_buttons(browser);
}

void
title_changed_cb    (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  char *newTitle;
  g_print("title_changed_cb\n");
  newTitle = gtk_moz_embed_get_title(embed);
  /*  if (newTitle)
  {
    gtk_frame_set_title(GTK_FRAME(browser->topLevelWindow), newTitle);
    g_free(newTitle);
    }*/
  
}

void
load_started_cb     (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  g_print("load_started_cb\n");
  gtk_widget_set_sensitive(browser->stopButton, TRUE);
  gtk_widget_set_sensitive(browser->reloadButton, FALSE);
  browser->loadPercent = 0;
  browser->bytesLoaded = 0;
  browser->maxBytesLoaded = 0;
  update_status_bar_text(browser);
}

void
load_finished_cb    (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  g_print("load_finished_cb\n");
  gtk_widget_set_sensitive(browser->stopButton, FALSE);
  gtk_widget_set_sensitive(browser->reloadButton, TRUE);
  browser->loadPercent = 0;
  browser->bytesLoaded = 0;
  browser->maxBytesLoaded = 0;
  update_status_bar_text(browser);
  gtk_progress_set_percentage(GTK_PROGRESS(browser->progressBar), 0);
}


void
net_state_change_cb (GtkMozEmbed *embed, gint flags, guint status,
		     TestGtkBrowser *browser)
{
  g_print("net_state_change_cb %d\n", flags);
  if (flags & GTK_MOZ_EMBED_FLAG_IS_REQUEST) {
    if (flags & GTK_MOZ_EMBED_FLAG_REDIRECTING)
    browser->statusMessage = "Redirecting to site...";
    else if (flags & GTK_MOZ_EMBED_FLAG_TRANSFERRING)
    browser->statusMessage = "Transferring data from site...";
    else if (flags & GTK_MOZ_EMBED_FLAG_NEGOTIATING)
    browser->statusMessage = "Waiting for authorization...";
  }

  if (status == GTK_MOZ_EMBED_STATUS_FAILED_DNS)
    browser->statusMessage = "Site not found.";
  else if (status == GTK_MOZ_EMBED_STATUS_FAILED_CONNECT)
    browser->statusMessage = "Failed to connect to site.";
  else if (status == GTK_MOZ_EMBED_STATUS_FAILED_TIMEOUT)
    browser->statusMessage = "Failed due to connection timeout.";
  else if (status == GTK_MOZ_EMBED_STATUS_FAILED_USERCANCELED)
    browser->statusMessage = "User canceled connecting to site.";

  if (flags & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT) {
    if (flags & GTK_MOZ_EMBED_FLAG_START)
      browser->statusMessage = "Loading site...";
    else if (flags & GTK_MOZ_EMBED_FLAG_STOP)
      browser->statusMessage = "Done.";
  }

  update_status_bar_text(browser);
  
}

void net_state_change_all_cb (GtkMozEmbed *embed, const char *uri,
				     gint flags, guint status,
				     TestGtkBrowser *browser)
{
  //  g_print("net_state_change_all_cb %s %d %d\n", uri, flags, status);
}

void progress_change_cb   (GtkMozEmbed *embed, gint cur, gint max,
			   TestGtkBrowser *browser)
{
  g_print("progress_change_cb cur %d max %d\n", cur, max);

  // avoid those pesky divide by zero errors
  if (max < 1)
  {
    gtk_progress_set_activity_mode(GTK_PROGRESS(browser->progressBar), FALSE);
    browser->loadPercent = 0;
    browser->bytesLoaded = cur;
    browser->maxBytesLoaded = 0;
    update_status_bar_text(browser);
  }
  else
  {
    browser->bytesLoaded = cur;
    browser->maxBytesLoaded = max;
    if (cur > max)
      browser->loadPercent = 100;
    else
      browser->loadPercent = (cur * 100) / max;
    update_status_bar_text(browser);
    gtk_progress_set_percentage(GTK_PROGRESS(browser->progressBar), browser->loadPercent / 100.0);
  }
  
}

void progress_change_all_cb (GtkMozEmbed *embed, const char *uri,
			     gint cur, gint max,
			     TestGtkBrowser *browser)
{
  //g_print("progress_change_all_cb %s cur %d max %d\n", uri, cur, max);
}

void
link_message_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  char *message;
  g_print("link_message_cb\n");
  message = gtk_moz_embed_get_link_message(embed);
  if (message && (strlen(message) == 0))
    update_temp_message(browser, 0);
  else
    update_temp_message(browser, message);
  if (message)
    g_free(message);
}

void
js_status_cb (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
 char *message;
  g_print("js_status_cb\n");
  message = gtk_moz_embed_get_js_status(embed);
  if (message && (strlen(message) == 0))
    update_temp_message(browser, 0);
  else
    update_temp_message(browser, message);
  if (message)
    g_free(message);
}

void
new_window_cb (GtkMozEmbed *embed, GtkMozEmbed **newEmbed, guint chromemask, TestGtkBrowser *browser)
{
  g_print("new_window_cb\n");
  g_print("embed is %p chromemask is %d\n", (void *)embed, chromemask);
  TestGtkBrowser *newBrowser = new_gtk_browser(chromemask, 0);
  gtk_widget_set_usize(newBrowser->mozEmbed, 400, 400);
  *newEmbed = GTK_MOZ_EMBED(newBrowser->mozEmbed);
  g_print("new browser is %p\n", (void *)*newEmbed);
}

void
visibility_cb (GtkMozEmbed *embed, gboolean visibility, TestGtkBrowser *browser)
{
  g_print("visibility_cb %d\n", visibility);
  set_browser_visibility(browser, visibility);
}

void
destroy_brsr_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  g_print("destroy_brsr_cb\n");
  gtk_widget_destroy(browser->topLevelWindow);
}

gint
open_uri_cb          (GtkMozEmbed *embed, const char *uri, TestGtkBrowser *browser)
{
  g_print("open_uri_cb %s\n", uri);

  // interrupt this test load
  if (!strcmp(uri, "http://people.redhat.com/blizzard/monkeys.txt"))
    return TRUE;
  // don't interrupt anything
  return FALSE;
}

void
size_to_cb (GtkMozEmbed *embed, gint width, gint height,
	    TestGtkBrowser *browser)
{
  g_print("*** size_to_cb %d %d\n", width, height);
  gtk_widget_set_usize(browser->mozEmbed, width, height);
}

gint dom_key_down_cb      (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
			   TestGtkBrowser *browser)
{
  PRUint32 keyCode = 0;
  //  g_print("dom_key_down_cb\n");
  event->GetKeyCode(&keyCode);
  // g_print("key code is %d\n", keyCode);
  return NS_OK;
}

gint dom_key_press_cb     (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
			   TestGtkBrowser *browser)
{
  PRUint32 keyCode = 0;
  // g_print("dom_key_press_cb\n");
  event->GetCharCode(&keyCode);
  // g_print("char code is %d\n", keyCode);
  return NS_OK;
}

gint dom_key_up_cb        (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
			   TestGtkBrowser *browser)
{
  PRUint32 keyCode = 0;
  // g_print("dom_key_up_cb\n");
  event->GetKeyCode(&keyCode);
  // g_print("key code is %d\n", keyCode);
  return NS_OK;
}

gint dom_mouse_down_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //  g_print("dom_mouse_down_cb\n");
  return NS_OK;
 }

gint dom_mouse_up_cb      (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //  g_print("dom_mouse_up_cb\n");
  return NS_OK;
}

gint dom_mouse_click_cb   (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //  g_print("dom_mouse_click_cb\n");
  PRUint16 button;
  event->GetButton(&button);
  printf("button was %d\n", button);
  return NS_OK;
}

gint dom_mouse_dbl_click_cb (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			     TestGtkBrowser *browser)
{
  //  g_print("dom_mouse_dbl_click_cb\n");
  return NS_OK;
}

gint dom_mouse_over_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //g_print("dom_mouse_over_cb\n");
  return NS_OK;
}

gint dom_mouse_out_cb     (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //g_print("dom_mouse_out_cb\n");
  return NS_OK;
}

void new_window_orphan_cb (GtkMozEmbedSingle *embed,
			   GtkMozEmbed **retval, guint chromemask,
			   gpointer data)
{
  g_print("new_window_orphan_cb\n");
  g_print("chromemask is %d\n", chromemask);
  TestGtkBrowser *newBrowser = new_gtk_browser(chromemask, 0);
  *retval = GTK_MOZ_EMBED(newBrowser->mozEmbed);
  g_print("new browser is %p\n", (void *)*retval);
}

// utility functions

void
update_status_bar_text(TestGtkBrowser *browser)
{
  gchar message[256];
  
  gtk_statusbar_pop(GTK_STATUSBAR(browser->statusBar), 1);
  if (browser->tempMessage)
    gtk_statusbar_push(GTK_STATUSBAR(browser->statusBar), 1, browser->tempMessage);
  else
  {
    if (browser->loadPercent)
    {
      g_snprintf(message, 255, "%s (%d%% complete, %d bytes of %d loaded)", browser->statusMessage, browser->loadPercent, browser->bytesLoaded, browser->maxBytesLoaded);
    }
    else if (browser->bytesLoaded)
    {
      g_snprintf(message, 255, "%s (%d bytes loaded)", browser->statusMessage, browser->bytesLoaded);
    }
    else if (browser->statusMessage == NULL)
    {
      g_snprintf(message, 255, " ");
    }
    else
    {
      g_snprintf(message, 255, "%s", browser->statusMessage);
    }
    gtk_statusbar_push(GTK_STATUSBAR(browser->statusBar), 1, message);
  }
}

void
update_temp_message(TestGtkBrowser *browser, const char *message)
{
  if (browser->tempMessage)
    g_free(browser->tempMessage);
  if (message)
    browser->tempMessage = g_strdup(message);
  else
    browser->tempMessage = 0;
  // now that we've updated the temp message, redraw the status bar
  update_status_bar_text(browser);
}


void
update_nav_buttons      (TestGtkBrowser *browser)
{
  gboolean can_go_back;
  gboolean can_go_forward;
  can_go_back = gtk_moz_embed_can_go_back(GTK_MOZ_EMBED(browser->mozEmbed));
  can_go_forward = gtk_moz_embed_can_go_forward(GTK_MOZ_EMBED(browser->mozEmbed));
  if (can_go_back)
    gtk_widget_set_sensitive(browser->backButton, TRUE);
  else
    gtk_widget_set_sensitive(browser->backButton, FALSE);
  if (can_go_forward)
    gtk_widget_set_sensitive(browser->forwardButton, TRUE);
  else
    gtk_widget_set_sensitive(browser->forwardButton, FALSE);
 }


void notice_destroy( GtkWidget *widget,
                     GtkWidget *entry )
{
    gtk_widget_destroy(widget);
}

void show_notice( char *msg )
{
    GtkWidget *window;
    GtkWidget *vbox;
    //GtkWidget *hbox;
    GtkWidget *entry;
    GtkWidget *button;
    //GtkWidget *check;

    /* create a new window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_usize( GTK_WIDGET (window), 400, 100);
    gtk_window_set_title(GTK_WINDOW (window), "Session Name");
    gtk_signal_connect(GTK_OBJECT (window), "delete_event",
                       (GtkSignalFunc) gtk_exit, NULL);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);

    entry = gtk_entry_new_with_max_length (50);
    //gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(enter_callback), entry);
    gtk_entry_set_text (GTK_ENTRY (entry), msg);
    //gtk_entry_append_text (GTK_ENTRY (entry), " world");
    //gtk_entry_select_region (GTK_ENTRY (entry), 0, GTK_ENTRY(entry)->text_length);
    gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), entry, TRUE, TRUE, 0);
    gtk_widget_show (entry);

#if 0
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    gtk_widget_show (hbox);
                                  
    check = gtk_check_button_new_with_label("Editable");
    gtk_box_pack_start (GTK_BOX (hbox), check, TRUE, TRUE, 0);
    gtk_signal_connect (GTK_OBJECT(check), "toggled",
                        GTK_SIGNAL_FUNC(entry_toggle_editable), entry);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
    gtk_widget_show (check);
    
    check = gtk_check_button_new_with_label("Visible");
    gtk_box_pack_start (GTK_BOX (hbox), check, TRUE, TRUE, 0);
    gtk_signal_connect (GTK_OBJECT(check), "toggled",
                        GTK_SIGNAL_FUNC(entry_toggle_visibility), entry);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
    gtk_widget_show (check);
#endif
                                   
    button = gtk_button_new_with_label ("Close");
    gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                               GTK_SIGNAL_FUNC(notice_destroy),
                               GTK_OBJECT (window));
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_widget_grab_default (button);
    gtk_widget_show (button);
    
    gtk_widget_show(window);

}

//Callback after user enters a session name in the 'Save Session As' window displayed as 
// as result of File->Start New Session
void enter_save_callback( GtkWidget *widget,
                     GtkWidget *entry )
{
  gchar *entry_text;
  static char buf[10];
  //char* file_name;
  //int capture_file_length;
  int i, len, badname=0;
  char msg_buf[2048];
  char file_name[1024];
  static char wdir[512];
  
  //Get the name of the new session and do some validation checks on the session name
  entry_text = gtk_entry_get_text(GTK_ENTRY(entry));

  strcpy(cav_file_name, entry_text);
  
   len = strlen(cav_file_name);

   for (i=0; i<len;i++) {
	if (!(isalnum(int(cav_file_name[i])) || (cav_file_name[i] == '_'))) {
	    badname = 1;
	    break;
	}
   }
  
   //Destroy 'Save Session As' Window
   gtk_widget_destroy(g_save_window);

  //Get the Work directory
  if (getenv("NS_WORK_DIR"))
	strcpy(wdir, getenv("NS_WORK_DIR"));
  else 
	strcpy(wdir, ".");
   strcat(wdir, "/");
   strcat(wdir, cav_file_name);
   //If session Name is bad , display a note, else continue after confirmation
   if (badname) {
	sprintf(msg_buf, "Session Name Can only have alphanumeric characters or _, in its name");
	show_notice(msg_buf);
	return;
   } else {
 	if (mkdir(wdir, 0777) == 0) {
	    sprintf(msg_buf, "Creating Session Name '%s'", cav_file_name);
	    show_notice(msg_buf);
	} else {
	    sprintf(msg_buf, "Can not create Session Name '%s': %s", cav_file_name, strerror(errno));
	    show_notice(msg_buf);
	    return;
	}
   }
   setenv("NS_SCRIPT_NAME", cav_file_name, 1);
   //g_print("Entered File name is %s\n", entry_text);

  //capture_file_length = strlen(entry_text) + 10;
  //file_name = (char*)malloc(sizeof(char) * capture_file_length);
  sprintf(file_name, "%s/%s%s", wdir, entry_text, ".capture");
  if ((url_file_fd = open(file_name, O_CREAT| O_RDWR | O_TRUNC, 0666)) == -1) {
    sprintf(msg_buf, "Error in opening the capture file %s\n", entry_text);
    show_notice(msg_buf); return;
  } else {
    sprintf(file_name, "%s/%s%s", wdir, entry_text, ".detail");
    if ((url_detail_file_fd = open(file_name, O_CREAT| O_RDWR | O_TRUNC, 0666)) == -1) {
      printf(msg_buf, "Error in opening the detail capture file %s\n", file_name);
      show_notice(msg_buf); return;
    } else {
      sprintf(file_name, "%s/%s%s", wdir, entry_text, ".c");
      if ((c_file_fd = open(file_name, O_CREAT | O_RDWR | O_TRUNC, 0666)) == -1) {
	sprintf(msg_buf, "Error in opening the c file %s\n", file_name);
        show_notice(msg_buf); return;
      } else {
	sprintf(file_name, "%s/%s%s", wdir, entry_text, ".h");
	if ((h_file_fd = open(file_name, O_CREAT | O_RDWR | O_TRUNC, 0666)) == -1) {
	  sprintf(msg_buf, "Error in opening the h file %s\n", file_name);
          show_notice(msg_buf); return;
	} else {
	  write(c_file_fd, "#include <stdio.h>\n", strlen ("#include <stdio.h>\n"));
	  write(c_file_fd, "#include <stdlib.h>\n", strlen ("#include <stdlib.h>\n"));
	  write(c_file_fd, "#include <string.h>\n", strlen ("#include <string.h>\n"));
	  write(c_file_fd, "#include \"../../include/ns_string.h\"\n", strlen("#include \"../../include/ns_string.h\"\n"));
	  sprintf(file_name, "%s%s", entry_text, ".h");
	  write(c_file_fd, "#include \"", strlen("#include \""));
	  write(c_file_fd, file_name, strlen(file_name));
	  write(c_file_fd, "\"\n\n", strlen("\"\n\n"));

	  //free(file_name);
	  memset(buf, 0, 10);
	  sprintf(buf, "%d", url_file_fd);
	  if (setenv("CAVISSON_FD", buf, 1) == -1) {
	    sprintf(msg_buf, "Error in seting FD env. variable\n");
            show_notice(msg_buf); return;
	  }
	  memset(buf, 0, 10);
	  sprintf(buf, "%d", url_detail_file_fd);
	  if (setenv("CAVISSON_DETAIL_FD", buf, 1) == -1) {
	    sprintf(msg_buf, "Error in setting DETAIL_FD env. variable\n");
            show_notice(msg_buf); return;
	  }
	  memset(buf, 0, 10);
	  sprintf(buf, "%d", c_file_fd);
	  if (setenv("CAVISSON_C_FD", buf, 1) == -1) {
	    sprintf(msg_buf, "Error in setting C_FD env. variable\n");
            show_notice(msg_buf); return;
	  }
	  memset(buf, 0, 10);
	  sprintf(buf, "%d", h_file_fd);
	  if (setenv("CAVISSON_H_FD", buf, 1) == -1) {
	    sprintf(msg_buf, "Error in setting H_FD env. variable\n");
            show_notice(msg_buf); return;
	  }
	}
      }
    }
  }
  gtk_widget_set_sensitive(g_command->fileSave, FALSE);
  gtk_widget_set_sensitive(g_command->startRecordButton, TRUE);
}

//Call Back - after File->Start New Session is clicked
void
menu_fileSave_cb   (GtkMenuItem *menuitem, GtkCommand *command)
{
  GtkWidget* save_window;
  GtkWidget* entry;
  GtkWidget* vbox;
  GtkWidget* hbox;

  //Display a window or user to enter session name
  save_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  g_save_window = save_window;
  gtk_window_set_title(GTK_WINDOW(save_window), "Save Session As");

  gtk_widget_set_usize (save_window, 400, 50);
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (save_window), vbox);
  gtk_widget_show (vbox);

  /*gtk_signal_connect(GTK_OBJECT (save_window), "delete_event",
    (GtkSignalFunc) gtk_exit, NULL);*/

  entry = gtk_entry_new_with_max_length (50);
  //Call enter_save_call_back, when user enters return
  gtk_signal_connect(GTK_OBJECT(entry), "activate",
                       GTK_SIGNAL_FUNC(enter_save_callback), entry);
  
  gtk_box_pack_start (GTK_BOX (vbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  gtk_widget_show(save_window);

  //  gtk_main();

}

#define SESSION_LENGTH 4096
//Called from the callback - after Start Recording button is pressed
//void enter_session_callback( GtkWidget *widget, GtkWidget *entry )
void enter_session_callback( )
{
  gchar *entry_text;
  char session[SESSION_LENGTH];
  nsresult rv = NS_OK;

  //Enter the Session related headers in capture & C fie
  //if (!CacheService)
  {
    CacheService = do_CreateInstance(kCacheServiceCID, &rv);
    if (NS_FAILED(rv) || !CacheService) {
      printf("creating instance of cache service failed\n");
    }
  }

  //nsCOMPtr<nsICacheService> serv =do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  CacheService->EvictEntries(nsICache::STORE_ON_DISK);
  CacheService->EvictEntries(nsICache::STORE_IN_MEMORY);

  //entry_text = gtk_entry_get_text(GTK_ENTRY(entry));
  entry_text = cav_file_name;

  memset(session, 0, SESSION_LENGTH);
  
  sprintf(session, "main()\n{\n  int next_page;\n//Define Any NS Variables here. Do not remove or modify this line\n\n//End of NS Variable decalarations. Do not remove or modifify this line\n\n  next_page = init_%s();\n\n  while(next_page != -1) {\n    switch(next_page) {\n", entry_text);
  if (write(url_file_fd, session, strlen(session)) != (int) strlen(session)) {
    fprintf(stderr, "capture_urls: failed in writing to url file\n");
    exit(-1);
  }
  sprintf(session, "int init_%s(void) {\n", entry_text);

  if (write(c_file_fd, session, strlen(session)) != (int) strlen(session)) {
    fprintf(stderr, "capture_urls: failed in writing to c file\n");
    exit(-1);
  }
}

void 
menu_fileStartRecord_cb  (GtkMenuItem  *menuitem, GtkCommand* command)
{

}

void
menu_fileStopRecord_cb   (GtkMenuItem  *menuitem,  GtkCommand*  command)
{

}


void
menu_fileQuit_cb  (GtkMenuItem  *menuitem,  GtkCommand*  command)
{


}



//Call back - after Start Recording button is clicked
void
startRecord_clicked_cb (GtkButton *button, GtkCommand *command)
{
#if 0
  GtkWidget* session_window;
  GtkWidget* entry;
  GtkWidget* vbox;
  GtkWidget* hbox;

  session_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title(GTK_WINDOW(session_window), "Session File As");

  gtk_widget_set_usize (session_window, 200, 50);
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (session_window), vbox);
  gtk_widget_show (vbox);

  /*gtk_signal_connect(GTK_OBJECT (session_window), "delete_event",
    (GtkSignalFunc) gtk_exit, NULL);*/

  entry = gtk_entry_new_with_max_length (50);
  gtk_signal_connect(GTK_OBJECT(entry), "activate",
                       GTK_SIGNAL_FUNC(enter_session_callback), entry);
  
  gtk_box_pack_start (GTK_BOX (vbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  gtk_widget_show(session_window);
#endif
    
  enter_session_callback();
  if (getenv("CAVISSON_TOGGLE") && !strcmp(getenv("CAVISSON_TOGGLE"), "0")) {
    setenv("CAVISSON_TOGGLE", "1", 1);
    setenv("CAVISSON_SESS_NAME", cav_file_name, 1);
    gtk_widget_set_sensitive(command->startRecordButton, FALSE);
    gtk_widget_set_sensitive(command->stopRecordButton, TRUE);
    gtk_widget_set_sensitive(command->fileQuit, FALSE);
    //gtk_widget_set_sensitive(g_browser_urlEntry, TRUE);
    set_browser_visibility(g_browser, TRUE);
  }

  last_line_done = 0;
}


//Calback - After Stop Record button is pressed
void
stopRecord_clicked_cb (GtkButton *button, GtkCommand *command)
{
char cmd_buf[128];

  if (getenv("CAVISSON_TOGGLE") && !strcmp(getenv("CAVISSON_TOGGLE"), "1")) {
    setenv("CAVISSON_TOGGLE", "0", 1);
    gtk_widget_set_sensitive(command->stopRecordButton, FALSE);
    gtk_widget_set_sensitive(command->startRecordButton, FALSE);
    gtk_widget_set_sensitive(command->fileSave, TRUE);
    gtk_widget_set_sensitive(command->fileQuit, TRUE);
    //gtk_widget_set_sensitive(g_browser_urlEntry, FALSE);
    set_browser_visibility(g_browser, FALSE);
  }

    if (getenv("CAVISSON_NUM_EMBED")) {
      char page_end_buf[64];
      const char* last_page;
      sprintf(page_end_buf, "          NUM_EMBED=%s);\n", getenv("CAVISSON_NUM_EMBED"));
      unsetenv("CAVISSON_NUM_EMBED"); //Set the stage for next script recording

      if (write(url_file_fd, page_end_buf, strlen(page_end_buf)) != strlen(page_end_buf))
	cout << "CAVISSON: Error in writing to url file" << endl;
      
      if (write(url_file_fd, "        next_page = check_page_", strlen("        next_page = check_page_")) != strlen("        next_page = check_page_"))
	cout << "CAVISSON: Error in writing to url file" << endl;

      if ((last_page = getenv("CAVISSON_LAST_PAGE"))) {
	if (write(url_file_fd, last_page, strlen(last_page)) != strlen(last_page))
	  cout << "CAVISSON: Error in writing to url file" << endl;
      }

      if (write(url_file_fd, "();\n        break;\n", strlen("();\n        break;\n")) != strlen(";\n        break;\n"))
	cout << "CAVISSON: Error in writing to url file" << endl;

      if (write(url_file_fd, "\n      default:\n        next_page = -1;\n    }\n  }\n}\n", strlen("\n      default:\n        next_page = -1;\n    }\n  }\n}\n")) != strlen("\n      default:\n        next_page = -1;\n    }\n  }\n}\n"))
	cout << "CAVISSON: Error in writing to url file" << endl;
    }
    
  write(c_file_fd, "\n\treturn -1;\n}\n", strlen("\n\treturn -1;\n}\n"));
  last_line_done = 1;
  sprintf(cmd_buf, "nsi_capture_dump");
  system(cmd_buf);
}
