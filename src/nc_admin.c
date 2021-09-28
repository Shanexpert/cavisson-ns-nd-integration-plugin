#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include "nc_admin.h"
#include "nslb_util.h"
#include "nslb_netcloud_util.h"
#define NS_EXIT_VAR
#include "ns_exit.h"
#include "ns_error_msg.h"

char operation[64] = "validate";
char controller_wdir[128] = {0};
char gen_file_name[256]; 
char raw[64] = {0};
char err_msg[1024] = {0};
char controller_ip[128] = {0};   //need global access
int operation_flag = 0, controller_wdir_flag = 0, gen_file_name_flag = 0, raw_flag = 0;
extern generatorList *gen_detail;
extern int tot_gen_entries;
FILE *fp = NULL;

static void display_help_and_exit(char *err)
{
  fprintf(stderr, "Error: %s\n", err);
  fprintf(stderr, "Usage: ncadmin –o <operation> -c <Controller:Wdir> -f <generator list file name> -r <raw or pretty format>\n");
  fprintf(stderr, "Added Options:\n");
  fprintf(stderr,   "-o <operations> are:\n");
  fprintf(stderr,      "validate   - All enteries are in correct format(like generator name, work and IP were unique).\n");
  fprintf(stderr,      "check      - It will check connectivity through nsu_server_admin, build_version, DNS config, Disk space etc.\n");
  fprintf(stderr,      "show       - It will show each allocation corresponding passed controller.\n");
  fprintf(stderr,                   "By default: shows generators of current controller");
  fprintf(stderr,                   "For example: .\n");
  fprintf(stderr,      "remove     - \n");
  fprintf(stderr,      "getTRlogs  - \n");
  fprintf(stderr,      "rmTRlogs   - \n");
  fprintf(stderr,   "-c <Controller working directory>:\n");
  fprintf(stderr,   "    It shows data according to passed Controller work.");
  fprintf(stderr,   "    By default - It shows current  Controller data. For all controllers, it would be pass like 'ALL'");
  fprintf(stderr,   "-f <Generator list file name>:\n");
  fprintf(stderr,   "   It is mendatory argument to passed by user, This file contain all generator related information.\n");
  exit(-1);
}

void get_ip_from_cav_conf_file(char *controller_ip)
{
  FILE *fp_conf = NULL;
  char read_buf[1024], temp[1024], *fields[5];
  char cav_file[256];
  int ret;
  
  sprintf(cav_file, "/home/cavisson/etc/cav_%s.conf", basename(controller_wdir));
  //Open config file
  if((fp_conf = fopen(cav_file, "r")) == NULL)
  {
    NCATDL("ERROR: File %s does not exists, hence opening '/home/cavisson/etc/cav.conf' file", cav_file);
    if ((fp_conf = fopen("/home/cavisson/etc/cav.conf", "r")) == NULL)
    {
      fprintf(stderr, CAV_ERR_1000006, cav_file, errno, nslb_strerror(errno));
      NCATDL("ERROR: in opening file /home/cavisson/etc/cav.conf");
      perror("fopen");
      exit (1);
    }
  }

  // read interface name for client/server and IP address of server
  while (fgets(read_buf, 1024, fp_conf)) 
  {
    if (!strncmp(read_buf, "NSAdminIP", 9))
      sscanf(read_buf, "%s %s", temp, controller_ip);
  }
  //controller ip is in CIDR notation
  ret = get_tokens(controller_ip, fields, "/", 2);
  strcpy(controller_ip, fields[0]);
  NCATDL("Method called, Controller Ip found = %s.", controller_ip);
  if(ret != 2)
    fprintf(stderr, "Error: Error in Ip format");
  fclose(fp_conf);
}

void show_generator_allocation_as_per_controller_work(char *ctrl_work)
{
  int i, found = 0, ret;
  //fprintf(stderr, "Jagat: ctrl_work = %s", ctrl_work);
  NCATDL("Method called, ctrl_work = %s", ctrl_work);
  ret = nslb_get_gen_for_given_controller(ctrl_work, gen_file_name, err_msg, 1);
  if(ret != 0) {
      fprintf(stderr, "%s", err_msg);
      NCATDL("err_msg = %s", err_msg);
      exit(-1);
  }
  NCATDL("tot_gen_entries = %d, controller_ip = %s.", tot_gen_entries, controller_ip);
  for(i = 0; i < tot_gen_entries; i++)
  {
    if(!strcmp(ctrl_work, "ALL"))
    {
      fprintf(stdout, "%s|%s|%s|%s|%s|%s|%s|%s\n", gen_detail[i].gen_name, gen_detail[i].gen_ip, gen_detail[i].cmon_port, gen_detail[i].gen_location, gen_detail[i].gen_work, gen_detail[i].gen_type, gen_detail[i].team, gen_detail[i].comments);
      NCATDL("%s|%s|%s|%s|%s|%s|%s|%s", gen_detail[i].gen_name, gen_detail[i].gen_ip, gen_detail[i].cmon_port, gen_detail[i].gen_location, gen_detail[i].gen_work, gen_detail[i].gen_type, gen_detail[i].team, gen_detail[i].comments);
      found++;
    }
    else if((!strcmp(controller_ip, gen_detail[i].controller_ip)) && (!strcmp(ctrl_work, gen_detail[i].controller_work))) 
    {
      fprintf(stdout, "%s|%s|%s|%s|%s|%s|%s|%s\n", gen_detail[i].gen_name, gen_detail[i].gen_ip, gen_detail[i].cmon_port, gen_detail[i].gen_location, gen_detail[i].gen_work, gen_detail[i].gen_type, gen_detail[i].team, gen_detail[i].comments);
      NCATDL("%s|%s|%s|%s|%s|%s|%s|%s", gen_detail[i].gen_name, gen_detail[i].gen_ip, gen_detail[i].cmon_port, gen_detail[i].gen_location, gen_detail[i].gen_work, gen_detail[i].gen_type, gen_detail[i].team, gen_detail[i].comments);
      found++;
    } else {
      continue;
    }
  }
  if(tot_gen_entries == 0)
    fprintf(stderr, CAV_ERR_1014028, basename(ctrl_work), gen_file_name);
  else if(found == 0)
  {
    NCATDL("No Generator entry found for Controller '%s' in file '%s'", basename(ctrl_work), gen_file_name);
    if(!strcmp(basename(ctrl_work), "work"))
      fprintf(stderr, CAV_ERR_1014029, basename(ctrl_work), controller_ip, ctrl_work);
    else
      fprintf(stderr, CAV_ERR_1014027, basename(ctrl_work), controller_ip, basename(ctrl_work), ctrl_work);
  }
}

void get_ns_wdir_env(char *controller_wdir)
{       
  char *ptr;
  ptr = getenv("NS_WDIR");

  if(!ptr)
  {
    fprintf(stderr, "Error: Unable to get NS_WDIR.\n");
    NCATDL("Error: Unable to get NS_WDIR.");
    exit(-1);
  }
  else
   strcpy(controller_wdir, ptr);
  
  NCATDL("Method called, get_ns_wdir_env(): controller_wdir = %s.", controller_wdir);
}

int main(int argc, char** argv) 
{
  char ch;
  int ret;

  //TODO: check how many argc is mendatory or passed
  
  NCATDL("Method called, No.of arguments = %d", argc);
  //set the default values.
  get_ns_wdir_env(controller_wdir);

  sprintf(gen_file_name, "%s/etc/.netcloud/generators.dat", controller_wdir); 
  
  struct option longopts[] = {
                                {"operation", 1, NULL, 'o'},
                                {"controller_wdir", 1, NULL, 'c'},
                                {"gen_file_name", 1, NULL, 'f'},
                                {"raw", 1, NULL, 'r'},
                                {0 , 0, 0, 0}
                             };

  while((ch = getopt_long(argc, argv, "o:c:f:r:", longopts, NULL)) != -1)
  {
    switch (ch) {
      case 'o':
        strcpy(operation, optarg);
        operation_flag = 1;
        break;
      case 'c':
        strcpy(controller_wdir, optarg);
        controller_wdir_flag = 1;
        break;
      case 'f':
        strcpy(gen_file_name, optarg);
        gen_file_name_flag = 1;
        break;
      case 'r':
        strcpy(raw, optarg);
        raw_flag = 1;
        break;
      case '?':
      default:
        display_help_and_exit("Invalid arguments");
    }
  }

  NCATDL("Opt arguments, operation = %s, controller_wdir = %s, gen_file_name = %s, raw = %s.", operation, controller_wdir, gen_file_name, raw);
  get_ip_from_cav_conf_file(controller_ip);

  if(operation_flag)
  {
    if(!strncmp(operation, "check", 5)) {
    } else if(!strncmp(operation, "show", 4)) {
      show_generator_allocation_as_per_controller_work(controller_wdir);
    } else if(!strncmp(operation, "remove", 5)) {
    } else if(!strncmp(operation, "getTRlogs", 9)) {
    } else if(!strncmp(operation, "rmTRlogs", 8)) {
    } else if(!strncmp(operation, "validate", 8)) {
      ret = nslb_get_gen_for_given_controller(controller_wdir, gen_file_name, err_msg, 1);
      if(ret != 0) {
        fprintf(stderr, "%s", err_msg);
        NCATDL("%s", err_msg);
        exit(-1);
      }
      ret = nslb_validate_gen_are_unique_as_per_gen_name(controller_wdir, controller_ip, err_msg, 1);
      if(ret != 0) {
        fprintf(stderr, "%s", err_msg);
        NCATDL("%s", err_msg);
        exit(-1);
      }
    } else {
      display_help_and_exit("Invalid value passed for -o (--operation) option");
    }
  } else {
    ret = nslb_get_gen_for_given_controller(controller_wdir, gen_file_name, err_msg, 1);
    if(ret != 0) {
      fprintf(stderr, "%s", err_msg);
      NCATDL("%s", err_msg);
      exit(-1);
    }
    ret = nslb_validate_gen_are_unique_as_per_gen_name(controller_wdir, controller_ip, err_msg, 1);
    if(ret != 0) {
      fprintf(stderr, "%s", err_msg);
      NCATDL("%s", err_msg);
      exit(-1);
    }
  }
  return 0;
}
