/******************************************************************
 * Name    : ns_batch_jobs.h
 * Author  : 
 * Purpose : This file contains declaration of macros, structure,
             and methods.
 * Note:
 * Modification History:
 * 30/05/2012 - Initial Version
*****************************************************************/
#ifndef _NS_BATCH_JOBS_H
#define _NS_BATCH_JOBS_H

extern void kw_set_batch_job_group(char *batch_group_name, int num, int runtime_flag, char *err_msg);
extern int parse_job_batch(char *buf, int runtime_flag, char *err_msg, char *delimiter, JSON_info *json_info_ptr);
#endif
