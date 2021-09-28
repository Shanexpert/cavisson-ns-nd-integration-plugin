/******************************************************************
 * Name    : ns_goal_based_run.c
 * Purpose : This file contains methods related to goal based execution
 * Note    :
 * Author  : Anuj
 * Intial version date: May 21, 08
 * Last modification date: May 21, 08
 *****************************************************************/

#ifndef NS_GOAL_BASED_RUN_H
#define NS_GOAL_BASED_RUN_H

#define SLA_RESULT_OK                 0
#define SLA_RESULT_INCONSISTENT       1
#define SLA_RESULT_CANT_LOAD_ENOUGH   2
#define SLA_RESULT_SERVER_OVERLOAD    3
#define SLA_RESULT_NA                 4
#define SLA_RESULT_NF                 5
#define SLA_RESULT_LAST_RUN           98
#define SLA_RESULT_CONTINUE           99

extern int run_length;                // defined in ns_parent.c
extern int *run_variable;             // defined in ns_parent.c
extern int run_num;                   // For printing the sample number in the summary.html

extern int check_proc_run_mode(avgtime *end_results, cavgtime *c_end_results); // called from ns_parent.c 
extern char *show_run_type(int type); // For printing the phase name in the summary.html
extern int is_rampdown_in_progress();

#endif
//End of file

