/********************************************************************
* Name: ns_cpu_affinity.h
* Purpose: This files is having the prototypes of functions, used in the CPU_AFFINITY feature of NetStorm
* Author: Anuj Dhiman
* Intial version date:     28/03/08
* Last modification date:
********************************************************************/

#ifndef _NS_CPU_AFFINITY_H
#define _NS_CPU_AFFINITY_H

extern int parse_cpu_affinity(char *line_buf);
extern int validate_cpu_affinity(void);
extern int set_cpu_affinity(int index, pid_t child_pid);

#endif

