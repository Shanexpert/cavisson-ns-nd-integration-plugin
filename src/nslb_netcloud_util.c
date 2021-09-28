#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <libgen.h>
#include <time.h>
#include <pwd.h>
#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <regex.h>
#include <dirent.h>

#include "nslb_util.h"
#include "nslb_netcloud_util.h"
#include "nslb_cav_conf.h"
#include "nslb_log.h"
#include "nslb_alloc.h"

#include <sys/file.h>
#include <grp.h>
#include <math.h>
#include "ns_error_msg.h"
#include "ns_exit.h"
#define FAILURE_RETURN -1
#define SUCCESS_RETURN 0
#define DELTA_GENERATOR_ENTRIES 128
#define MAX_GENERATOR_ENTRIES 255

generatorList *gen_detail;                     //no need to make thread bcoz it is calling from parent and another src tool
UsedGeneratorList *used_gen_list = NULL;       //no need to make thread bcoz it is calling from parent and another src tool
int tot_gen_entries;                           //no need to make thread bcoz it is calling from parent and another src tool
int max_gen_entries;                           //no need to make thread bcoz it is calling from parent and another src tool
int used_generator_entries = 0;                //no need to make thread bcoz it is calling from parent 
int max_used_gen_entries = 0;
FILE *t_debug_fp = NULL;                       //no need to make thread bcoz it is calling from  nslb_netcloud_util.c

void open_file_ncat_fd (FILE **fp) 
{ 
  char debug_file[128];
  
  sprintf(debug_file, "/tmp/nc_admin_tool_%d.log", getpid());
  //delele the log file before creating it
  unlink(debug_file);
  *fp = fopen(debug_file, "a+"); 
 
  if(*fp == NULL)
  {
    fprintf(stderr, "Error in opening file = %s\n", debug_file);
    exit(-1);
  }
}

void ncat_debug_logs(char *filename, int line, char *fname, char *format, ...)
{
  va_list ap;
  char buffer[MAX_DEBUG_LOG_BUF_SIZE + 1] = "\0";
  int amt_written = 0, amt_written1=0;

  static  int flag = 0;   //no need to make thread specific because it is calling from only nslb_netcloud_util.c 

  if(flag == 0) 
  {
    open_file_ncat_fd(&t_debug_fp);
    flag++;
  }

  amt_written1 = sprintf(buffer, "%d|%s|%s|", line, filename, fname);

  va_start(ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_DEBUG_LOG_BUF_SIZE - amt_written1, format, ap);

  va_end(ap);

  buffer[MAX_DEBUG_LOG_BUF_SIZE] = 0;

  if (amt_written < 0) {
    amt_written = strlen(buffer) - amt_written1;
  }

  if (amt_written > (MAX_DEBUG_LOG_BUF_SIZE - amt_written1)) {
    amt_written = (MAX_DEBUG_LOG_BUF_SIZE - amt_written1);
  }

  if(t_debug_fp != NULL)
    fprintf(t_debug_fp, "%s\n", buffer);
  else
    fprintf(stderr, "%s\n", buffer);
}

//validating duplicate generator entries in scenario file
int add_generator_to_list(char *generator_name, char *err_msg)
{
  int i;

  NSLBDL2_MISC("Method called, generator_name = %s", generator_name);

  if(used_generator_entries == MAX_GENERATOR_ENTRIES)
  {
    NS_EXIT(-1, CAV_ERR_1014001);
  }

  if(used_generator_entries == max_used_gen_entries)
  {
    max_used_gen_entries += DELTA_GENERATOR_ENTRIES;
    NSLB_REALLOC_AND_MEMSET(used_gen_list, (max_used_gen_entries * sizeof(UsedGeneratorList)), (used_generator_entries * sizeof(UsedGeneratorList)), "used_gen_list malloc", -1, NULL);
  }

  for(i = 0; i < used_generator_entries; i++)
  {
    if(!strcmp(used_gen_list[i].generator_name, generator_name))
    {
      sprintf(err_msg, CAV_ERR_1011242, used_gen_list[i].generator_name);
      return FAILURE_RETURN;
    }
  }
  strcpy(used_gen_list[used_generator_entries++].generator_name, generator_name);
  return SUCCESS_RETURN;
}

int nslb_get_controller_ip_and_gen_list_file(char *controller_ip, char *scenario_file, char *gen_file, char *g_ns_wdir, int testidx, char *tr_or_partition, char *err_msg, int *default_gen_file)
{
  char buf[MAX_LENGTH_OF_LINE] = {0};
  char line[2 * MAX_LENGTH_OF_LINE] = {0};
  char keyword[MAX_LENGTH_OF_LINE] = {0};
  char *ptr = NULL, *line_ptr = NULL;
  FILE *fp = NULL;
  int num, ret;
  struct stat s;

  NSLBDL1_MISC("Method nslb_get_controller_ip_and_gen_list_file() called: scenario_file = %s, product type = %s", scenario_file, g_cavinfo.config);

  /*if(g_cavinfo == NULL)
    nslb_init_cav(); */
  if((fp = fopen(scenario_file, "r")) == NULL) {
    //sprintf(err_msg, "scenario_file: Error opening file %s", scenario_file);
    sprintf(err_msg, CAV_ERR_1000006, scenario_file, errno, nslb_strerror(errno));
    return FAILURE_RETURN;
  }
  if ((!strcmp (g_cavinfo.config, "NS>NO")) || (!strcmp (g_cavinfo.config, "NS")) || (!strcmp (g_cavinfo.config, "NS+NO"))
     || (!strcmp (g_cavinfo.config, "NC")) || (!strcmp (g_cavinfo.config, "NDE")) || (!strcmp (g_cavinfo.config, "SM")) ) 
  {
    strcpy(controller_ip, g_cavinfo.NSAdminIP);
  }
  else
  {
    sprintf(err_msg, CAV_ERR_1000015, g_cavinfo.config);
    //sprintf(err_msg, "Error: Product type of machine (%s) is not valid, please ensure that product type should be anyone of "
     //                "NS>NO|NS+NO|NS|NC|NDE|SM in /home/cavisson/etc/cav.conf", g_cavinfo.config);
    return FAILURE_RETURN;
  }

  //Get generator file from scenario
  while (fgets(line, 2048, fp) != NULL)
  {
    line[strlen(line)-1] = '\0';
    line_ptr = line;
    
    if(line_ptr[0] == '\0' || line_ptr[0] == '#')
      continue;

    if((ptr = strchr(line_ptr, '\n')) != NULL)
      *ptr = '\0';

    if((num = sscanf(line_ptr, "%s %s", keyword, buf)) < 2) {
      fprintf(stderr, "\nreading sceanario keywords(): At least two fields required <%s>\n", line_ptr);
      continue;
    }
    else if (strcasecmp(keyword, "NS_GENERATOR_FILE") == 0) {
      strcpy(gen_file, buf);
    } else if (strcasecmp(keyword, "NS_GENERATOR") == 0) {
      if((ret = add_generator_to_list(buf, err_msg)) != SUCCESS_RETURN)
        return FAILURE_RETURN;
    }
  }

  if (!used_generator_entries)
    return SUCCESS_RETURN;

  //Here to check if Generator File not found in scenario, then we set default (/etc/.netcloud/generators.dat) file.
  if(gen_file[0] == '\0') {
    sprintf(gen_file, "%s/etc/.netcloud/generators.dat", g_ns_wdir);
    *default_gen_file = 1;
    if(stat(gen_file, &s) != 0) {
      sprintf(err_msg, CAV_ERR_1014002, g_ns_wdir);
      //sprintf(err_msg, "Error: NS_GENERATOR_FILE not found in scenario, And Default Generator file [%s] also doesn't exist, Hence exiting..\n", gen_file);
      return FAILURE_RETURN;
    }
  }
  NSLBDL3_MISC("controller_ip = %s, gen_file = %s", controller_ip, gen_file);
  return SUCCESS_RETURN; 
}

int nslb_create_generator_list(int* row_num, char *err_msg)
{
  NSLBDL1_MISC("Method nslb_create_generator_list called(): row_num = %d, tot_gen_entries = %d, max_gen_entries = %d", 
                       *row_num, tot_gen_entries, max_gen_entries);
  if (tot_gen_entries == max_gen_entries) {
    gen_detail = (generatorList *)realloc(gen_detail, (max_gen_entries + DELTA_GENERATOR_ENTRIES) * sizeof(generatorList));
    if (!gen_detail) {
	//TODO
      sprintf(err_msg, "nslb_create_generator_list(): Error allocating more memory for gen_detail\n");
      return FAILURE_RETURN;
    } else{
        max_gen_entries += DELTA_GENERATOR_ENTRIES;
      }
  }

  *row_num = tot_gen_entries++;
  NSLBDL1_MISC("row_num = %d, tot_gen_entries = %d", *row_num, tot_gen_entries);
  return SUCCESS_RETURN;
}


//This function is to get the generators for given controller(Combination of ctrl_work, ctrl_ip)
int nslb_get_gen_for_given_controller(char *ctrl_work, char *gen_file, char *err_msg, int is_run_with_tool)
{
  char line[1024] = {0};
  char *field[20] = {0}, *ptr = NULL;
  int total_flds, rnum = 0, line_num = 0;
  FILE *fp = NULL;

  NCLBDL(is_run_with_tool, "Method nslb_get_gen_for_given_controller() Called: ctrl_work = %s, gen_file = %s, tot_gen_entries = %d, "
                       "max_gen_entries = %d", ctrl_work, gen_file, tot_gen_entries, max_gen_entries);

  //Read generator list file 
  if ((fp = fopen(gen_file, "r")) == NULL)
  {
    sprintf(err_msg, CAV_ERR_1000006, gen_file, errno, nslb_strerror(errno));
    //sprintf(err_msg, "Error in opening %s file.\n", gen_file);
    return FAILURE_RETURN;
  }

  //Read file line by line from generartor list file pointer.
  while (fgets(line, 1024, fp) != NULL)
  {
    char *line_ptr = line;
    CLEAR_WHITE_SPACE(line_ptr);
    memmove(line, line_ptr, strlen(line_ptr));
    //Ignoring generator header and blank or commented lines
    if(line[0] == '\n' || line[0] == '#') {
      line_num++;
      continue;
    }
    if((ptr = strchr(line, '\n')) != NULL) //Replace newline with NULL char in each line before saving fields in structure
      *ptr = '\0';

    total_flds = get_tokens_ex(line, field, "|", 20);

    //validation for New generator file format - 
    //#GeneratorName|IP|CaMonAgentPort|Location|Work|Type|ControllerIp|ControllerName|ControllerWORK|Team|NameServer|Comments
    if(total_flds != 20) {
      sprintf(err_msg, CAV_ERR_1014003, g_ns_wdir);
      //sprintf(err_msg, "Generator list file =  %s format is not correct.", gen_file);
      return FAILURE_RETURN;
    }

    //Ignoring generator header - if it is without #, so we can check with GeneratorName and IP word.
    if ((!strcmp(field[0], "GeneratorName")) || (!strcmp(field[1], "IP"))) {
      line_num++;
      continue;
    }
    
    //checking whether generator name contains '.' character or not
    if(strchr(field[0], '.') != NULL) {
      sprintf(err_msg, CAV_ERR_1014004, field[0]);
      //sprintf(err_msg, "Error: Generator Name (%s) is containing dot (.), generator name can not have dot(.)\n", field[0]);
      return FAILURE_RETURN;
    }

    //Validation check for CavMonAgent Port
    if (ns_is_numeric(field[2]) == 0) {
      sprintf(err_msg, CAV_ERR_1014005, field[0]);
      //sprintf(err_msg, "Error:In generator file CaMonAgentPort given as \"%s\" at line \"%s\", "
      //                        "CavMonAgentPort can have only integer value.\n", field[2], line);
      return FAILURE_RETURN;
    }

    if(nslb_create_generator_list(&rnum, err_msg) == FAILURE_RETURN)
    {
      //sprintf(err_msg, "Error in creating new generator list struct\n");
      return FAILURE_RETURN;
    }
    //added 4 more fields for gui to get generator old file format
    strcpy(gen_detail[rnum].gen_name, field[0]);
    strcpy(gen_detail[rnum].gen_ip, field[1]);
    strcpy(gen_detail[rnum].cmon_port, field[2]);  
    strcpy(gen_detail[rnum].gen_location, field[3]);   
    strcpy(gen_detail[rnum].gen_work, field[4]);
    strcpy(gen_detail[rnum].gen_type, field[5]);           
    strcpy(gen_detail[rnum].controller_ip, field[6]);
    strcpy(gen_detail[rnum].controller_name, field[7]);
    strcpy(gen_detail[rnum].controller_work, field[8]);
    strcpy(gen_detail[rnum].team, field[9]);
    strcpy(gen_detail[rnum].comments, field[19]);          
    NCLBDL(is_run_with_tool, "rnum = %d, gen_name = %s, gen_ip = %s, gen_work = %s, controller_ip = %s, controller_name = %s, "
                             "controller_work = %s, team = %s", rnum, gen_detail[rnum].gen_name, gen_detail[rnum].gen_ip, 
                              gen_detail[rnum].gen_work, gen_detail[rnum].controller_ip, gen_detail[rnum].controller_name,
                              gen_detail[rnum].controller_work, gen_detail[rnum].team); 
  }

  NCLBDL(is_run_with_tool, "nslb_get_gen_for_given_controller() end successfully..");
  return SUCCESS_RETURN;
}

int nslb_validate_gen_are_unique_as_per_gen_name(char *cntrl_work, char *cntrl_ip, char *err_msg, int is_run_with_tool)
{
  int i, j, num_of_gen_alloc = 0;
  char gen_first_line_buff[MAX_LENGTH_OF_LINE + 1] = {0};
  char gen_sec_line_buff[MAX_LENGTH_OF_LINE + 1] = {0};
  char ctrl_first_line_buff[MAX_LENGTH_OF_LINE + 1] = {0};
  char ctrl_sec_line_buff[MAX_LENGTH_OF_LINE + 1] = {0};

  NCLBDL(is_run_with_tool, "Method nslb_validate_gen_are_unique_as_per_gen_name() called: tot_gen_entries = %d, cntrl_work = %s, cntrl_ip = %s", 
                       tot_gen_entries, cntrl_work, cntrl_ip);
 
  for(i = 0; i < tot_gen_entries; i++)
  {
    for(j = 0; j < used_generator_entries; j++)
    {
      //First check first line is controller ip and controller work is same or not otherwise is continue
      if((!strcmp(used_gen_list[j].generator_name, gen_detail[i].gen_name)) && (!strcmp(cntrl_ip, gen_detail[i].controller_ip))
          && (!strcmp(cntrl_work, gen_detail[i].controller_work))) {
        num_of_gen_alloc++;
        used_gen_list[j].used_gen_status = 1;
        NSLBDL2_MISC("i = %d, j = %d, used_gen_list_generator_name = %s, gen_detail_gen_name = %s, cntrl_ip = %s, "
                     "gen_detail_controller_ip = %s, cntrl_work = %s, controller_work = %s, num_of_gen_alloc = %d", i, j,
                     used_gen_list[j].generator_name, gen_detail[i].gen_name, cntrl_ip, gen_detail[i].controller_ip,
                     cntrl_work, gen_detail[i].controller_work, num_of_gen_alloc);
        break;
      }
      else
      {
        if((!strcmp(used_gen_list[j].generator_name, gen_detail[i].gen_name)) && (used_gen_list[j].used_gen_status != 1)) {
          used_gen_list[j].used_gen_status = 2;
        }
      }
    }

    //First check first line is controller ip and controller work is same or not otherwise is continue
    if(is_run_with_tool) {
      if((strcmp(cntrl_ip, gen_detail[i].controller_ip) != 0) && (strcmp(cntrl_work, gen_detail[i].controller_work) != 0))
        continue;
    }
    
    //get same controller ip and controller work
    //fetch gen ip and gen work
    //compare this gen ip and gen work for all enteries and it would be unique
    sprintf(gen_first_line_buff, "%s_%s", gen_detail[i].gen_ip, gen_detail[i].gen_work);
    sprintf(ctrl_first_line_buff, "%s_%s", gen_detail[i].controller_ip, gen_detail[i].controller_work);
    NCLBDL(is_run_with_tool, "i = %d, gen_first_line_buff = %s, ctrl_first_line_buff = %s", i, gen_first_line_buff, ctrl_first_line_buff);

    for(j = 0; j < tot_gen_entries; j++)
    { 
      if(i == j) {
        continue;
      }
      sprintf(gen_sec_line_buff, "%s_%s", gen_detail[j].gen_ip, gen_detail[j].gen_work);
      sprintf(ctrl_sec_line_buff, "%s_%s", gen_detail[j].controller_ip, gen_detail[j].controller_work);
  
      NCLBDL(is_run_with_tool, "i = %d, j = %d, gen_first_line_buff = %s, gen_sec_line_buff = %s, ctrl_first_line_buff = %s, ctrl_sec_line_buff = %s "
                   "gen_detail[i].team = %s, gen_detail[j].team = %s", i, j, gen_first_line_buff, gen_sec_line_buff, ctrl_first_line_buff, 
                    ctrl_sec_line_buff, gen_detail[i].team, gen_detail[j].team); 

      if(!strcmp(gen_first_line_buff, gen_sec_line_buff))
      {
        sprintf(err_msg, CAV_ERR_1014018, gen_detail[i].gen_ip, basename(gen_detail[j].gen_work));
        return FAILURE_RETURN;
      }

      if((!strcmp(gen_first_line_buff, ctrl_first_line_buff)) || (!strcmp(gen_first_line_buff, ctrl_sec_line_buff)))
      {
        sprintf(err_msg, CAV_ERR_1014018, gen_detail[i].gen_ip, basename(gen_detail[i].gen_work));
        return FAILURE_RETURN;
      }
      //changes for bug 21037, allowing duplicate generator names for different controller (master)
      if((!strcmp(gen_detail[i].gen_name, gen_detail[j].gen_name)) && (!strcmp(gen_detail[i].controller_ip, gen_detail[j].controller_ip)) 
              && (!strcmp(gen_detail[i].controller_work, gen_detail[j].controller_work)))
      {
        sprintf(err_msg, CAV_ERR_1014018, gen_detail[i].gen_name, gen_detail[j].gen_name);
        return FAILURE_RETURN;
      }
      //currently no check for gen ip and gen work matching with Ctrl Ip and Ctrl work 
      /*Bug 55526: removed controller/team comparasion
      if(!strcmp(ctrl_first_line_buff, ctrl_sec_line_buff))
      {
        if(strcmp(gen_detail[i].team, gen_detail[j].team) != 0)
        {
          sprintf(err_msg, "Error: Controller, '%s' have duplicate entry as '%s', team '%s' should be same with '%s'\n", 
                            gen_detail[j].controller_ip, basename(gen_detail[j].controller_work), gen_detail[j].team, gen_detail[i].team);
          return FAILURE_RETURN;
        }
      }*/
    }
  }

  if(num_of_gen_alloc != used_generator_entries)
  {
    for(i = 0; i < used_generator_entries; i++)
    {
      /*Case 1: when generator used in scenario and not allocated to controller and IP of running machine in Gen conf file */
      if(used_gen_list[i].used_gen_status == 2) {
        sprintf(err_msg, CAV_ERR_1014019, used_gen_list[i].generator_name, cntrl_ip, cntrl_work);
      }

      /*Case 2: when generator used in scenario and not present in Generator conf file*/
      if(used_gen_list[i].used_gen_status == 0) {
        sprintf(err_msg, CAV_ERR_1014020, used_gen_list[i].generator_name);
      }
      
    }
    return FAILURE_RETURN;
  }
  NCLBDL(is_run_with_tool, "nslb_validate_gen_are_unique_as_per_gen_name() end: where num_of_gen_alloc = %d, used_generator_entries = %d", num_of_gen_alloc, used_generator_entries);

  return SUCCESS_RETURN;
}
