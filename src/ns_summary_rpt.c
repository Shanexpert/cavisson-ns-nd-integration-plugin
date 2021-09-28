/****************************************************************************************************************
* Name: ns_summary_rpt.c 
* Purpose: For genrating the summary.html 
* Author: Anuj Dhiman 
* Intial version date: 04/09/08 
* Last modification date

****************************************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "ns_log.h"
#include "wait_forever.h"
#include "ns_goal_based_run.h"
#include "ns_summary_rpt.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_global_dat.h"
#include "ns_gdf.h"
#include "ns_trace_level.h"
#include "ns_exit.h"

static FILE *srfp_html = NULL;
static int flag = 0, table = 2;             //table is used to expand(drop down) table ,each time a new table required & before using this variable we have used 1 table.
static char *table_row[2]={"\"tableRowOdd\"", "\"tableRowEven\""};

// Mthd for creating the file summary.html
static void open_srfp_html()
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  char buf[600];    
  sprintf(buf, "logs/TR%d/summary.html", testidx);
  close_srfp_html();
  if ((srfp_html = fopen(buf, "w")) == NULL) 
  {
    NSTL1(NULL, NULL, "Error in creating the summary.html file\n");
    perror("netstorm");
    NS_EXIT(1, "Error in creating the summary.html file");
  }
}

// Mthd for writing in to the file summary.html
static void add_line_in_srfp_html(char *format, ...)
{
  va_list ap;
  int amt_written = 0;
  char buffer[10001];

  memset (buffer, 0, sizeof(buffer));
  va_start(ap, format);
  //amt_written = vsprintf(buffer, format, ap);
  amt_written = vsnprintf(buffer, 10000, format, ap);
  va_end(ap);

  buffer[amt_written] = 0;
  fprintf(srfp_html, "%s", buffer);
  //NSTL1_OUT(NULL, NULL, "%s", buffer);
}

static void add_http_status_code_header()
{
  char hdr[] = "<table width=\"100%\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\" >"
    "  <tr>"
    "    <td align=\"center\"><br />"
    "      <br />"
    "<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" width=98% class=\"table\" align=\"center\"  max-width=1200px>"
    "        <tr>"
    "          <td  align=\"center\"><a href=\"#\"><img class=\"imageUpDown\" src=\"/netstorm/images/arrow_d.png\"  name=\"img9\" width=\"30\" height=\"21\" border=\"0\" onClick=\"showMe('table09', 'img9')\" /></a></td>"
    "          <td width=\"970\" align=\"left\"  class=\"tableTitle\">HTTP Status codes  </td>"
    "        </tr>"
    "        <tr>"
    "          <td colspan=\"2\" align=\"center\" valign=\"top\">"
    "            <table id=\"table09\" width = \"100%\" border=\"0\"  align = \"center\" cellpadding = \"0\" cellspacing = \"0\"class=\"table-border\" display:; >"
    "              "
    "              <tr  class=\"tableHeader\" align=\"center\">"
    "                <td height=\"20\"  style=text-align:center><b>HTTP Status codes</b></td>"
    "                <td><b>Count</b></td>"
    "                <td  ><b>Percent</b></td>"
    "              </tr>";
  fprintf(srfp_html, "%s", hdr);
}

static void add_http_status_code_tail()
{

  char tail[] = "            </table>"
    "          </td>"
    "        </tr>"
    "      </table>"
    "    </td>"
    "  </tr>"
    "</table>";

  fprintf(srfp_html, "%s", tail);
}

static void add_http_status_code_line(cavgtime *cavg)
{
  int i;
  char line[] = "              <tr align=\"center\" class=\"tableRowOdd\"  >"
    "                <td height=\"18\" valign = \"middle\" >%s</td>"
    "                <td valign = \"middle\" > %lu </td>"
    "                <td valign = \"middle\" > %.2lf </td>"
    "              </tr>";

  unsigned int total = 0;
  char *error_code_list[] = {
			"100 Continue", "101 Switching Protocols", "200 OK",
			"201 Created", "202 Accepted", "203 Non-Authoritative Information",
			"204 No Content", "205 Reset Content", "206 Partial Content",
			"300 Multiple Choices", "301 Moved Permanently", "302 Found",
			"303 See Other", "304 Not Modified", "305 Use Proxy",
			"306 Switch Proxy", "307 Temporary Redirect", "400 Bad Request",
			"401 Unauthorized", "402 Payment Required", "403 Forbidden",
			"404 Not Found", "405 Method Not Allowed", "406 Not Acceptable",
			"407 Proxy Authentication Required", "408 Request Timeout", "409 Conflict",
			"410 Gone", "411 Length Required", "412 Precondition Failed",
			"413 Request Entity Too Large", "414 Request-URI Too Long", "415 Unsupported Media Type",
			"416 Requested Range Not Satisfiable", "417 Expectation Failed", "500 Internal Server Error",
			"501 Not Implemented", "502 Bad Gateway", "503 Service Unavailable",
			"504 Gateway Timeout", "505 HTTP Version Not Supported"
	                };
 
  for (i = 0; i < total_http_status_codes; i++)
    total += cavg->cum_http_status_codes[i];

  i = 0;
  if (total == 0) total = 1;    /* This is possible if all urls are conn fail for example. */  

  for(i = 0; i < total_http_status_codes; i++) {
    fprintf(srfp_html, line, error_code_list[i], cavg->cum_http_status_codes[i],
				     ((cavg->cum_http_status_codes[i] / (double)total) * 100.0));
  }
}

void log_http_status_codes_to_summary_report(cavgtime *cavg)
{
  add_http_status_code_header();
  add_http_status_code_line(cavg);
  add_http_status_code_tail();
}

// This mthd is to open the fp for summery_html and writes the title in this file 
void open_and_add_title_srfp_html()
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  char html_header[10000];
  open_srfp_html();  
  //to get help icon uncomment following line & comment the next   
 
  //sprintf(html_header, "<html>\n<head>\n<link rel=\"stylesheet\" href=\" /netstorm/css/Netstorm.css\" type=\"text/css\" /link>\n<title>Test Runs</title>\n\n</head>\n<body>\n<form id=\"form1\" name=\"form1\" method=\"post\" action=\"\">\n\n<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" id=\"header\">\n  <tr><td width=\"95%%\" align=\"left\" valign=\"top\"><img src=\"/netstorm/images/logo.png\"/></td>\n<td><img src=\"/netstorm/images/completeHelp.gif\"/></td>\n</tr>\n<tr><td align=\"center\" colspan=\"2\">\n<table width=\"98%%\" cellpadding=\"0\" cellspacing=\"0\">\n<tr ><td colspan=\"2\" align=\"left\" class=\"screenTitle\">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Summary Report - Test Run Number : %d</td></tr></table></td></tr></table>", testidx);
  
  sprintf(html_header, "<html>\n<head>\n<script >\n var img1 = \"/netstorm/images/arrow_r.png\";\n var img2 = \"/netstorm/images/arrow_d.png\";\n var vis1 = \"none\";\n var vis2 = \"block\";\n function showMe (it, c_img)\n {\n if (document[c_img].src.indexOf(img2)!= -1)\n {\n document.getElementById(it).style.display = vis1;\n                document[c_img].src = img1;\n }\n else\n {\n document.getElementById(it).style.display = vis2;\n document[c_img].src = img2;\n }\n }\n </script>\n<link rel=\"stylesheet\" href=\" /netstorm/css/Netstorm.css\" type=\"text/css\" /link>\n<title>Test Runs</title>\n\n</head>\n<body>\n<form id=\"form1\" name=\"form1\" method=\"post\" action=\"\">\n\n<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\" id=\"header\">\n  <tr><td width=\"95%%\" align=\"left\" valign=\"top\"><img src=\"/netstorm/images/logo.png\"/></td>\n</tr>\n<tr><td align=\"center\" colspan=\"2\">\n<table width=\"98%%\" cellpadding=\"0\" cellspacing=\"0\">\n<tr ><td colspan=\"2\" align=\"left\" class=\"screenTitle\">Summary Report - Test Run Number : %d</td></tr></table></td></tr></table>", testidx);

   add_line_in_srfp_html (html_header);
}

void add_version_header_srfp_html(char *version_buf, char *g_testrun, int testidx, char *g_test_start_time)
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  char version_header[10000];

  sprintf(version_header,"<table  cellpadding=\"0\" cellspacing=\"0\" id=\"text\">\n<tr><td align=\"center\">\n<br />\n\n<table cellpadding=\"0\" cellspacing=\"0\" class=\"table\" width=\"98%%\" max-width=1200px\">\n<tr><td  align=\"center\"><a href=\"#\"><img class=\"imageUpDown\" src=\"/netstorm/images/arrow_d.png\" onClick=\"showMe('table01', 'img1')\" name=\"img1\" width=\"21\" height=\"21\" border=\"0\" /></a></td>\n<td width=\"970\" align=\"left\"  class=\"tableTitle\">Test Information </td></tr>\n<tr><td colspan=\"2\" align=\"center\">\n<table  cellspacing=\"0\" cellpadding=\"0\" id=\"table01\" style=\"width:100%%; max-width:1200px; display:;\">\n<tr> <td width=\"49%%\" >&nbsp;</td> <td width=\"2%%\">&nbsp;</td> <td width=\"49%%\">&nbsp;</td> </tr>\n<tr>\n<td align=\"right\" valign=\"top\">\n<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\">\n<tr class=\"tableRowWithHeight\">\n<td  align=\"right\" class=\"fieldLableWithBottomBorder\"><b>NetStorm </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">%s</td>\n</tr>\n\n<tr class=\"tableRowWithHeight\">\n<td  align=\"right\" class=\"fieldLableWithBottomBorder\">Test Case Name </td>\n<td  class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">%s</td>\n</tr>\n<tr class=\"tableRowWithHeight\">\n<td  align=\"right\" class=\"fieldLableWithBottomBorder\">Test Run Number </td>\n<td  class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">%d</td>\n</tr>\n<tr class=\"tableRowWithHeight\">\n<td  align=\"right\" class=\"fieldLableWithBottomBorder\">Test Started At </td>\n<td  class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">%s</td>\n</tr>\n</table></td>\n<td>&nbsp;</td>", version_buf, g_testrun, testidx, g_test_start_time);
  add_line_in_srfp_html(version_header);
}


void add_test_configuration_srfp_html(TestCaseType_Shr *testcase_shr_mem)
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  char configuration_buf[10000];
  // char temp_buf[1024] = "\0";
  
  sprintf(configuration_buf, "<td align=\"left\" valign=\"top\">\n<table width=\"95%%\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\" >\n<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\">Test Configuration</td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td></tr>\n\n");
  add_line_in_srfp_html(configuration_buf);

  switch (testcase_shr_mem->mode)
  {
    case TC_FIX_CONCURRENT_USERS:
      add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Fix Concurrent Users</td> </tr>\n");
    break;
   
    case TC_FIX_USER_RATE:
      if(global_settings->replay_mode == 0)
        add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Fix Session Rate</td> </tr>\n");
      else
        add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Replay Access Logs</td> </tr>\n");
    break;

    case TC_MIXED_MODE:
      add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Mixed Mode</td> </tr>\n");
    break;
  
    case TC_FIX_PAGE_RATE:
      add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Fix Page Rate</td> </tr>\n");
    break;

    case TC_FIX_TX_RATE:
      add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Fix Transaction Users</td> </tr>\n");
    break;

    case TC_MEET_SLA:
      add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Meet SLA</td>\n</tr>\n");
    break;
   
    case TC_MEET_SERVER_LOAD:
      add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Meet Serever Load</td>\n</tr>\n");
break;

    case TC_FIX_MEAN_USERS:
      add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Fix Mean Users</td> </tr>\n");
      //add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Test Mode </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">Fix Mean Users</td> </tr>\n<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Number of Users </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">%d per minute</td>\n</tr>\n", testcase_shr_mem->target_rate);
    break;
  }

  add_line_in_srfp_html ("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\"><b>Target Completion </b></td>\n<td class=\"fieldLableWithBottomBorder\">&nbsp;</td>\n<td class=\"fieldValueWithBottomBorder\">%s</td>\n</tr>\n", target_completion_time);

  if (global_settings->num_dirs)
    add_line_in_srfp_html("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\">Using SpecWeb file set with %d directories</td></tr>\n", global_settings->num_dirs);
  else
    add_line_in_srfp_html("<tr class=\"tableRowWithHeight\">\n<td align=\"right\" class=\"fieldLableWithBottomBorder\">Using %d URL's</td>\n", total_request_entries);

   add_line_in_srfp_html("</table> </td> <tr>\n<td>&nbsp;</td>\n<td>&nbsp;</td><td>&nbsp;</td>\n</tr>\n</table></td></tr>\n</table></td></tr>\n</table>\n<br />\n");
}

// create the table for object_summary, prints the heading of object_summary_table
void add_object_summary_table_start_srfp_html()
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  char table_heading_buf[10000];  
  
  if (global_settings->show_initiated)
    sprintf(table_heading_buf, "\n<table cellpadding=\"0\" cellspacing=\"0\" class=\"table\" width=\"98%%\"  align=\"center\" max-width=1200px>\n <tr>\n <td align=\"center\"><a href=\"#\"><img src=\"/netstorm/images/arrow_d.png\" onClick=\"showMe('table0%d', 'img%d')\" name=\"img%d\" width=\"21\" height=\"21\" border=\"0\" /></a></td>\n <td width=\"970\" align=\"left\" class=\"tableTitle\">Object Report (All time in seconds) </td>\n </tr>\n <tr>\n <td colspan=\"2\" align=\"center\" valign=\"top\">\n <table width =\" 100%%\" border=\"0\"  align =\" center\" cellpadding =\" 0\" cellspacing =\" 0\" id=\"table0%d\">\n<tr align=\"center\" class=\"tableHeader\">\n <td height=\"20\">Object Name</td>\n <td>Minimum</td>\n <td>Average</td>\n <td>Maximum</td>\n<td>Initiated</td>\n<td>Completed</td>\n <td>Successful</td>\n <td>Errors</td>\n </tr>\n\n",table, table, table, table);
  else
    sprintf(table_heading_buf, "\n<table cellpadding=\"0\" cellspacing=\"0\" class=\"table\" width=\"98%%\" align=\"center\" max-width=1200px>\n <tr>\n <td align=\"center\"><a href=\"#\"><img src=\"/netstorm/images/arrow_d.png\" onClick=\"showMe('table0%d', 'img%d')\" name=\"img%d\" width=\"21\" height=\"21\" border=\"0\" /></a></td>\n <td width=\"970\" align=\"left\" class=\"tableTitle\">Object Report (All time in seconds)</td>\n </tr>\n <tr>\n <td colspan=\"2\" align=\"center\" valign=\"top\">\n <table width =\" 100%%\" border=\"0\"  align =\" center\" cellpadding =\" 0\" cellspacing =\" 0\" id=\"table0%d\">\n<tr align=\"center\" class=\"tableHeader\">\n <td height=\"20\">Object Name</td>\n <td>Minimum</td>\n <td>Average</td>\n <td>Maximum</td>\n <td>Completed</td>\n <td>Successful</td>\n <td>Errors</td>\n </tr>\n\n",table, table, table, table);

  add_line_in_srfp_html(table_heading_buf); 
  table++;
}

// prints the row for particular object type (URL, Page, Transaction and Session)
void add_object_summary_table_row_srfp_html(int obj_type, double min_time, double avg_time, double max_time, u_ns_8B_t num_initiated, u_ns_8B_t num_completed, u_ns_8B_t num_succ)
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  // This is for printing the Report Type in the summary.html
  char *html_name;
  if (obj_type == URL_REPORT)  html_name="URL";
  else if (obj_type == PAGE_REPORT) html_name="Page";
  else if (obj_type == TX_REPORT)   html_name="Transaction";
  else if (obj_type == SESS_REPORT) html_name="Session";
  else if (obj_type == SMTP_REPORT) html_name="Smtp";
  else if (obj_type == POP3_REPORT) html_name="Pop3";
  else if (obj_type == FTP_REPORT) html_name="Ftp";
  else if (obj_type == DNS_REPORT) html_name="Dns";
  //else 
  //{
   // error and return;
  //}

  if (global_settings->show_initiated)
    add_line_in_srfp_html("<tr align=\"center\" class=%s>\n <td height=\"18\" valign =\" middle\" >%s</td>\n <td valign =\" middle\" > %6.3f </td>\n <td valign =\" middle\" > %6.3f </td>\n <td valign =\" middle\" > %6.3f </td>\n <td valign =\" middle\" > %llu </td>\n <td valign =\" middle\" > %llu </td>\n <td valign =\" middle\" > %llu</td>\n <td valign =\" middle\" > %lu</td>\n </tr>\n\n", table_row[flag++ % 2], html_name, min_time, avg_time, max_time, num_initiated, num_completed, num_succ, (num_completed-num_succ));
  else
    add_line_in_srfp_html("<tr align=\"center\" class=%s >\n <td height=\"18\" valign =\" middle\" >%s</td>\n <td valign =\" middle\" > %6.3f </td>\n <td valign =\" middle\" > %6.3f </td>\n <td valign =\" middle\" > %6.3f </td>\n <td valign =\" middle\" > %llu </td>\n <td valign =\" middle\" > %llu</td>\n <td valign =\" middle\" > %lu</td>\n </tr>\n\n", table_row[flag++ % 2], html_name, min_time, avg_time, max_time, num_completed, num_succ, (num_completed-num_succ));
}

// End the object_summary_table
void add_object_summary_table_end_srfp_html()
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  add_line_in_srfp_html("</table>\n <br />\n</td>\n </tr>\n </table>\n <br />\n\n\n");
}

void add_vuser_info_srfp_html(int avg_users, char *sessrate, int cur_vusers_active, int cur_vusers_thinking, int cur_vusers_waiting, int cur_vusers_cleanup, int cur_vusers_in_sp ,int cur_vusers_blocked, int cur_vusers_paused)
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");

  add_line_in_srfp_html("<table  align =\" center\" width =\" 95%%\" cellspacing =\" 0\" cellpadding =\" 0\" class=\"tableWithoutBorder\">\n <tr align=\"left\" >\n <td class=\"tableRow\">&nbsp <span class=\"fieldLable\">Vusers:</span> <span class=\"fieldValue\"> Avg.  %d,%s Current: Active %d, Thinking %d, Waiting(Between Sessions) %d, Idling %d, SyncPoint %d, Blocked %d, Paused %d</span></td>\n </tr>\n", avg_users, sessrate, cur_vusers_active, cur_vusers_thinking, cur_vusers_waiting, cur_vusers_cleanup, cur_vusers_in_sp, cur_vusers_blocked, cur_vusers_paused);

}

void add_other_info_srfp_html(u_ns_8B_t hit_initited_rate, u_ns_8B_t hit_tot_rate, u_ns_8B_t hit_succ_rate, double tcp_rx, double tcp_tx, char *tbuffer, int num_connections, u_ns_8B_t con_made_rate, u_ns_8B_t con_break_rate, double ssl_new, double ssl_reuse_attempted, double ssl_reused)
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");

  if (global_settings->show_initiated)
    add_line_in_srfp_html("\n<tr align=\"left\" >\n <td class=\"tableRow\">&nbsp;<span class=\"fieldLable\"> HTTP hit rate (per sec)</b>:</span> <span class=\"fieldValue\">Initiated %llu, Completed %llu, Success %llu</span></td> </tr>\n <tr align=\"left\" > <td class=\"tableRow\">&nbsp;<span class=\"fieldLable\"> Throughput (Kbits/sec)</b>:</span> <span class=\"fieldValue\"> TCP Rx/Tx: %6.3f/%6.3f%s</span></td>\n </tr>\n <tr align=\"left\" >\n <td class=\"tableRow\">&nbsp;<span class=\"fieldLable\"> TCP Connections</b>:</span> <span class=\"fieldValue\">Current Connections %d, Conns open/Sec %llu, conns close/Sec %llu</span></td>\n </tr>\n <tr align=\"left\" >\n <td class=\"tableRow\">&nbsp;<span class=\"fieldLable\"> SSL Sessions</b>:</span> <span class=\"fieldValue\"> New Sessions/Sec  %6.3f, Reuse Requested/Sec %6.3f, Reused/Sec %6.3f</span></td>\n </tr></table> \n<br />\n\n", hit_initited_rate, hit_tot_rate, hit_succ_rate, tcp_rx, tcp_tx, tbuffer, num_connections, con_made_rate, con_break_rate, ssl_new, ssl_reuse_attempted, ssl_reused);
  else
    add_line_in_srfp_html("\n<tr align=\"left\" >\n <td class=\"tableRow\">&nbsp;<span class=\"fieldLable\"> HTTP hit rate (per sec)</b>:</span> <span class=\"fieldValue\">Total %llu, Success %llu</span></td> </tr>\n <tr align=\"left\" > <td class=\"tableRow\">&nbsp;<span class=\"fieldLable\"> Throughput (Kbits/sec)</b>:</span> <span class=\"fieldValue\">TCP Rx/Tx: %6.3f/%6.3f%s</span></td>\n </tr>\n <tr align=\"left\" >\n <td class=\"tableRow\">&nbsp;<span class=\"fieldLable\"> TCP Connections</b>:</span> <span class=\"fieldValue\">Current Connections %d, Conns open/Sec %llu, conns close/Sec %llu</span></td>\n </tr>\n <tr align=\"left\" >\n <td class=\"tableRow\">&nbsp;<span class=\"fieldLable\"> SSL Sessions</b>:</span> <span class=\"fieldValue\">New Sessions/Sec  %6.3f, Reuse Requested/Sec %6.3f, Reused/Sec %6.3f</span></td>\n </tr></table> \n<br />\n\n", hit_tot_rate, hit_succ_rate, tcp_rx, tcp_tx, tbuffer, num_connections, con_made_rate, con_break_rate, ssl_new, ssl_reuse_attempted, ssl_reused);
}

void add_tx_info_table_start_srfp_html()
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  if ( global_settings->show_initiated == 1 ) 
    add_line_in_srfp_html("<table cellpadding=\"0\" cellspacing=\"0\" class=\"table\" width=\"98%%\" align=\"center\" max-width=1200px>\n <tr>\n <td align=\"center\"><a href=\"#\"><img class=\"ImageUpDown\" src=\"/netstorm/images/arrow_d.png\" onClick=\"showMe('table0%d', 'img%d')\" name=\"img%d\" width=\"21\" height=\"21\" border=\"0\"/></a></td>\n <td width=\"970\" align=\"left\" class=\"tableTitle\">Detail Transaction Report (All time in seconds))</td>\n </tr>\n <tr>\n <td colspan=\"2\" align=\"center\" valign=\"top\"> <table width =\" 100%%\" border=\"0\"  align =\" center\" cellpadding =\" 0\" cellspacing =\" 0\" id=\"table0%d\">\n<tr align=\"center\"  class=\"tableHeader\">\n <td height=\"20\"> Transaction Name</td>\n <td>Min</td>\n <td>Avg</td>\n <td>Max</td>\n <td>Stddev</td>\n <td>Initiated</td>\n <td>Completed</td>\n <td>Success</td>\n <td>Error</td>\n </tr>\n\n",table, table, table, table);

  if ( global_settings->show_initiated == 0 ) 
    add_line_in_srfp_html("<table cellpadding=\"0\" cellspacing=\"0\" class=\"table\" width=\"98%%\" align=\"center\" max-width=1200px>\n <tr>\n <td align=\"center\"><a href=\"#\"><img class=\"ImageUpDown\" src=\"/netstorm/images/arrow_d.png\" onClick=\"showMe('table0%d', 'img%d')\" name=\"img%d\" width=\"21\" height=\"21\" border=\"0\"/></a></td>\n <td width=\"970\" align=\"left\" class=\"tableTitle\">Detail Transaction Report (All time in seconds)</td>\n </tr>\n <tr>\n <td colspan=\"2\" align=\"center\" valign=\"top\"> <table width =\" 100%\" border=\"0\"  align =\" center\" cellpadding =\" 0\" cellspacing =\" 0\" id=\"table0%d\">\n<tr align=\"center\"  class=\"tableHeader\">\n <td height=\"20\"> Transaction Name</td>\n <td>Min</td>\n <td>Avg</td>\n <td>Max</td>\n <td>Stddev</td>\n <td>Completed</td>\n <td>Success</td>\n <td>Error</td>\n </tr>\n\n",table, table, table, table);
  
    table++;
}

void add_tx_info_table_row_srfp_html(int is_initiated, char *tx_name, float min_time, float avg_time, float max_time, float std_dev, u_ns_8B_t num_initiated, u_ns_8B_t num_completed, u_ns_8B_t num_succ)

{
  
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  if (is_initiated == 1)
    add_line_in_srfp_html("<tr align=\"center\" class=\"tableRowOdd\"  >\n <td valign =\" middle\" >%s</td>\n <td valign =\" middle\" >%6.3f</td>\n <td valign =\" middle\" >%6.3f</td>\n <td valign =\" middle\" >%6.3f</td>\n <td valign =\" middle\" >%6.3f</td>\n <td valign =\" middle\" >%llu</td>\n <td valign =\" middle\" >%llu</td>\n <td valign =\" middle\" >%llu</td>\n <td valign =\" middle\" >%llu</td>\n </tr>\n", tx_name, min_time, avg_time, max_time, std_dev, num_initiated, num_completed, num_succ, (num_completed - num_succ));

  if (is_initiated == 0)
    add_line_in_srfp_html("<tr align=\"center\" class=\"tableRowOdd\"  >\n <td valign =\" middle\" >%s</td>\n <td valign =\" middle\" >%6.3f</td>\n <td valign =\" middle\" >%6.3f</td>\n <td valign =\" middle\" >%6.3f</td>\n <td valign =\" middle\" >%6.3f</td>\n <td valign =\" middle\" >%llu</td>\n <td valign =\" middle\" >%llu</td>\n <td valign =\" middle\" >%llu</td>\n</tr>\n",tx_name, min_time, avg_time, max_time, std_dev, num_completed, num_succ, (num_completed - num_succ));
}

void add_tx_info_table_end_srfp_html()
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  add_line_in_srfp_html("</table>\n </td>\n </tr>\n </table>\n <br />\n");
}

// creates the percentile_report_table
void add_obj_percentile_report_table_start_srfp_html(char *mode_buf)
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");

  add_line_in_srfp_html("<table cellpadding=\"0\" cellspacing=\"0\" class=\"table\" width=98%% align=\"center\"  max-width=1200px>\n <tr>\n <td align=\"center\"><a href=\"#\"> <img class=\"ImageUpDown\" src=\"/netstorm/images/arrow_d.png\" onClick=\"showMe('table0%d', 'img%d')\" name=\"img%d\" width=\"21\" height=\"21\" border=\"0\"/> </a></td>\n <td width=\"970\" align=\"left\" class=\"tableTitle\">%s Percentile Report</td>\n </tr>\n <tr>\n <td colspan=\"2\" align=\"center\" valign=\"top\">\n <table width =\" 100%\" border=\"0\"  align =\" center\" cellpadding =\" 0\" cellspacing =\" 0\" id=\"table0%d\" >\n <tr align=\"center\"  class=\"tableHeader\">\n <td height=\"20\"> Response Time Window (RTW)</td>\n <td >Number of Responses in RTW</td>\n <td>Pct of Responses in RTW</td>\n <td  >Pct of Responses upto upper bound of RTW</td>\n </tr>\n\n", table, table, table, mode_buf ,table);
  
  ++table;

}

void add_obj_percentile_report_table_row_srfp_html(float start_time, float end_time, unsigned int num_responses, float pct, float upper_pct)
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  //char table_row[2][14]={"\"tableRowOdd\"", "\"tableRowEven\""};
  add_line_in_srfp_html("<tr align=\"center\" class=%s >\n <td valign =\" middle\" >%6.2f-%6.2f secs</td>\n <td valign =\" middle\" >%9lu</td>\n <td valign =\" middle\" >%6.2f%%</td>\n <td valign =\" middle\" >%6.2f%%</td>\n </tr>\n\n",table_row[flag++ % 2], start_time, end_time, num_responses, pct, upper_pct); 
  //add_line_in_srfp_html("<tr align=\"center\" class=\"tableRowOdd\"  >\n <td valign =\" middle\" >%6.2f-%6.2f secs:</td>\n <td valign =\" middle\" >%9lu</td>\n <td valign =\" middle\" >%6.2f%%</td>\n <td valign =\" middle\" >%6.2f%%</td>\n </tr>\n\n",start_time, end_time, num_responses, pct, upper_pct); 
}

void add_obj_percentile_report_table_last_row_srfp_html(float start_time, unsigned int num_responses, float pct, float upper_pct)
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  add_line_in_srfp_html("<tr align=\"center\" class=\"tableRowEven\"  >\n <td valign =\" middle\" >%6.2f-higher secs</td>\n <td valign =\" middle\" >%17lu</td>\n <td valign =\" middle\" >%6.2f%%</td>\n <td valign =\" middle\" >%6.2f%%</td>\n </tr>\n", start_time, num_responses, pct, upper_pct);
}

void add_obj_percentile_report_table_end_srfp_html()
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  add_line_in_srfp_html("</table>\n<br/> </td>\n </tr>\n </table>\n <br />\n\n");
}

void add_obj_median_time_info_srfp_html(float data0, float data1, float data2, float data3, float data4)
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  add_line_in_srfp_html("<table  align =\" center\" width =\" 95%%\" cellspacing =\" 0\" cellpadding =\" 0\" class=\"tableWithoutBorder\">\n <tr align=\"left\" >\n <td class=\"tableRow\">&nbsp <span class=\"fieldLable\"> median-time: </span> <span class=\"fieldValue\"> %6.3f sec, 80%%:  %6.3f sec, 90%%:  %6.3f sec, 95%%:  %6.3f sec, 99%%:  %6.3f sec </span></td>\n </tr>\n</table>\n <br />\n", data0, data1, data2, data3, data4);

}

void add_close_button_with_footer()
{ 
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  add_line_in_srfp_html("<table width = \"100%%\"><br><br><tr><td align = center>\n<input type=Button class=button style=\"width:60;\" value=Close name=Close onclick = \"window.close();\">\n</td></tr></table>\n <table width=\"100%%\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\">\n <br>\n <br>\n <tr>\n <td background=\"/netstorm/images/bg_footer.png\"><img src=\"/netstorm/images/bg_footer.png\" /></td>\n </tr>\n </table>\n </form>\n </body>\n </html>\n");
}

void close_srfp_html()
{
  if (srfp_html) 
  {
    NSTL1(NULL, NULL, "Closing srfp_html");
    fclose(srfp_html);
  }
  srfp_html = NULL; 
}

void end_and_close_srfp_html()
{
  NSDL1_REPORTING(NULL, NULL, "Method called.");
  char end_buffer[10000];
  sprintf(end_buffer, "</table>\n</form>\n\n</body>\n</html>");
  add_line_in_srfp_html(end_buffer);
  //close_srfp_html();
}

// For testing only 
/*
int main ()

{
  open_summary_html();
  add_header_in_srfp_html();
  add_middle_part_in_srfp_html();
  return 0;
}
*/

// End of file

