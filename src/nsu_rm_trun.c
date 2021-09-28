/*------------------------------------------------------------------------------
#  Name    : nsu_rm_trun.c
#  Author  : Anil
#  Purpose : To remove disk used coponets of a test run(s) and remove test run(s)
#  Usage   : nsu_rm_trun {-n run# | {-s run#} {-e run#}} {-d} {-g} {-p} {-S} {-D} {-l} {-r}
#  Argument : 1 or More Test Runs and other flags
#  Exit Values :
#    0 - Success
#    1 - Validation errors
#    2 - System errors
#  Modification History:
#    11/21/06:Atul/Neeraj:1.4.2 - Added optins for raw graph data, graphs, scripts and page dumps
#
---------------------------------------------------------------------------------*/


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#include "nslb_util.h"   //for get_running_test_runs
#include "nslb_partition.h"   //for get_running_test_runs
#include "nslb_server_admin.h"
#define MAX_FILENAME_LENGTH 1024
#define TR_IS_UNLOCKED     0
#define TR_IS_LOCKED       1

int run_num = 0;
short clear_all_test_id = 1;
short remove_db_info = 0;
short remove_all_log_files = 0;
short remove_all_reports = 0;
short remove_all_graphs = 0;
short remove_all_scripts = 0;
short remove_all_pagedump = 0;
short remove_all_rawGraphs = 0;
short remove_everything = 1;
char g_ns_wdir[MAX_FILENAME_LENGTH];
TestRunList *test_run_info = NULL;
int uflag=0;
char g_user_name[MAX_FILENAME_LENGTH];
char g_test_owner[MAX_FILENAME_LENGTH];

void get_answer()
{
  char c;
  c = getchar();
  if ((c != 'y') && (c != 'Y'))
  {
    fprintf(stderr, "Cancelling Action. Exiting without removing anything\n");
    exit (0);
  }
  getchar();  /* read in the \n */
}
//checking whether test is running,If running Don't delete it(bug 2108)
int check_running_test(char *gen_wdir){  
  int test_run_running = 0;
  int TRNum,i, ret_val = 0;
  //Malloc running test run table
  test_run_info = (TestRunList *)malloc(256 * sizeof(TestRunList));
  memset(test_run_info, 0, (256 * sizeof(TestRunList)));
  test_run_running = get_running_test_runs_ex(test_run_info, gen_wdir);
  for (i = 0; i < test_run_running; i++) {
    TRNum = test_run_info[i].test_run_list;
    if(TRNum == run_num){
      ret_val = 1;
    }
  }
  //Free and make NULL test_run_info pointer
  free(test_run_info);
  test_run_info = NULL;
  return ret_val;
}

#define PREVIOUS_PARTITION_KW_LENGTH 18 //17 + 1
#define NEXT_PARTITION_KW_LENGTH 14 //13 + 1 

//In file 'file_path', line where found 'keyword', replace that line with 'new_line'
void update_file(char *file_path, char *keyword, char *new_line)
{
  char line_buf[MAX_FILENAME_LENGTH + 1] = {0} ;
  FILE *fp = NULL;
  FILE *tmpfp = NULL;

  fp = fopen(file_path, "r");
  if(fp == NULL)
  {
    fprintf(stderr, "Error in opening file '%s'. Does not have .partition_info.txt.\n", file_path);
    exit (-1);
  }
    
  strcat(file_path, ".tmp");
  tmpfp = fopen(file_path, "w");
  if(tmpfp == NULL)
  {
    fprintf(stderr, "Error in opening file '%s'. Does not have .partition_info.txt.\n", file_path);
    exit (-1);
  }

  while(fgets(line_buf, MAX_FILENAME_LENGTH, fp) != NULL)
  {
    if(strstr(line_buf, keyword) != NULL)
      sprintf(line_buf, "%s\n", new_line);

    if(fputs(line_buf, tmpfp) < 0)
    {
      //TODO Restore function
      fprintf(stderr, "Error in updating 'NextPartition' keyword value in file '%s'", file_path);
      exit(-1);
    }
  }
  fclose(fp);
  fp = NULL;
  fclose(tmpfp);
  tmpfp = NULL;
}

//Reset partition info linking
void establish_linking_btw_partitions(char *ns_wdir, char *tr_or_partition, int run_num, PartitionInfo part_info)
{
  char file_path[2*MAX_FILENAME_LENGTH + 1] = {0};
  char buf[1024] = {0};

  //if have valid previous and next partition
  if(part_info.prev_partition < 0 || part_info.next_partition < 0)
    return;
  
  if(part_info.prev_partition > 0)
  {
    sprintf(file_path, "%s/logs/TR%d/%lld/.partition_info.txt", ns_wdir, run_num, part_info.prev_partition);
    sprintf(buf, "NextPartition=%lld", part_info.next_partition);
    update_file(file_path, "NextPartition", buf);
  }

  if(part_info.next_partition > 0)
  {
    sprintf(file_path, "%s/logs/TR%d/%lld/.partition_info.txt", ns_wdir, run_num, part_info.next_partition);
    sprintf(buf, "PreviousPartition=%lld", part_info.prev_partition);
    update_file(file_path, "PreviousPartition", buf);
  }
}

void make_linking_btw_partitions(char *ns_wdir, int run_num, PartitionInfo part_info)
{
  char old_path[1024] = {0};
  char new_path[1024] = {0};
   
  if(part_info.prev_partition > 0)
  {
    sprintf(old_path, "%s/logs/TR%d/%lld/.partition_info.txt.tmp", ns_wdir, run_num, part_info.prev_partition);
    sprintf(new_path, "%s/logs/TR%d/%lld/.partition_info.txt", ns_wdir, run_num, part_info.prev_partition);
    rename(old_path, new_path);
  }

  if(part_info.next_partition > 0)
  {
    sprintf(old_path, "%s/logs/TR%d/%lld/.partition_info.txt.tmp", ns_wdir, run_num, part_info.next_partition);
    sprintf(new_path, "%s/logs/TR%d/%lld/.partition_info.txt", ns_wdir, run_num, part_info.next_partition);
    rename(old_path, new_path);
  }
}

//If partition have actual scrip not soft link then take script backup and copy script again in partition
void establish_link_btw_scripts(char *ns_wdir, char *tr_or_partition, int run_num) 
{
  char path[MAX_FILENAME_LENGTH] = {0};
  char cmd[MAX_FILENAME_LENGTH] = {0};
  struct stat s;
  int ret = 0;

  sprintf(path, "%s/logs/%s/scripts", ns_wdir, tr_or_partition);
  ret = lstat(path, &s);

  if(ret == 0) //found softlink
    return;
  
  sprintf(cmd, "cp -r %s/logs/TR%s/scripts %s/logs/%d", ns_wdir, tr_or_partition, ns_wdir, run_num);
  system(cmd);
}

//Remove all the multidisk supported TR 
void remove_multidisk_path(char *tr_or_partition, int test_idx)
{
  FILE *fp = NULL;
  char buff[512];
  char command[1024];
  char file_path[256];
  char *tmp_ptr = NULL;

  if(test_idx)
    sprintf(file_path, "%s/logs/TR%d/.multidisk_path", g_ns_wdir, test_idx);
  else
    sprintf(file_path, "%s/logs/%s/.multidisk_path", g_ns_wdir, tr_or_partition);
  
  //Test is not multidisk supported
  if ((fp = fopen(file_path, "r")) == NULL)
   return;
 
  while (fgets(buff, 1024, fp) != NULL)
  {
    if((tmp_ptr = strchr(buff, '\n')) != NULL){
       *tmp_ptr = '\0';
        tmp_ptr++;
    }
   
    sprintf(command, "rm -rf %s/%s", buff, tr_or_partition);
    system(command);
  }

  if(fp)
   fclose(fp);
   fp = NULL;
}

void remove_partition_database(char *ns_wdir, long long partition_idx, int run_num)
{
  char buf[MAX_FILENAME_LENGTH] = {0};
  char data[MAX_FILENAME_LENGTH] = {0};
  FILE *fp = NULL;
  int is_nd = 0;

  //delete NS database 
  sprintf(buf, "%s/bin/neu_drop_partition_table %d %lld \"ALL\" >/dev/null", ns_wdir, run_num, partition_idx);
  system(buf);
  
  //check if ND enable, if yes then delete nd database
  sprintf(buf, "grep ^NET_DIAGNOSTICS_SERVER %s/logs/TR%d/sorted_scenario.conf | cut -d ' ' -f2", ns_wdir, run_num);
  fp = popen(buf, "r");
  if(fp != NULL)
  {
    fgets(data, MAX_FILENAME_LENGTH, fp);
    is_nd = atoi(data);  
    pclose(fp);
  }

  if(is_nd > 0)
  {
    sprintf(buf, "%s/bin/neu_nd_drop_partition_table %d %lld \"ALL\" >/dev/null", ns_wdir, run_num, partition_idx);
    system(buf);
  }  
}

int delete_test_run_from_ns_repo(int run_num, char *username)
{
  char buf[MAX_FILENAME_LENGTH];
  sprintf(buf, "%s/bin/nsi_repo_utils -o run -c nsu_rm_trun -a \"-n %d -u %s -f\" | grep -w Successfully 1>/dev/null 2>&1",g_ns_wdir, run_num, username);
  return system(buf);
}

/*This is to delete TestRuns present in file NS_WDIR/logs/TR/NetCloud/NetCloud.data 
 * This file "NetCloud.data" gets created only when we are running NetCloud test. */
int delete_test_run_frm_generators(char *g_ns_wdir, int run_num)
{
  char net_cloud_dir[MAX_FILENAME_LENGTH];
  char net_cloud_file_path[MAX_FILENAME_LENGTH];
  char file_data[MAX_FILENAME_LENGTH];
  char cmd[MAX_FILENAME_LENGTH]="\0";
  char text[MAX_FILENAME_LENGTH];
  char keyword[MAX_FILENAME_LENGTH];
  char *field[25];
  int num_flds;

  FILE *gen_tr_fptr = NULL;

  ServerCptr server_ptr;
  memset(&server_ptr, 0, sizeof(ServerCptr));
  server_ptr.server_index_ptr = (ServerInfo *) malloc(sizeof(ServerInfo));
  memset(server_ptr.server_index_ptr, 0, sizeof(ServerInfo));
  server_ptr.server_index_ptr->server_ip = (char *)malloc(sizeof(char) * 20);

  memset(field, 0, sizeof(field));

  //KEYWORD TESTRUN|GEN NAME|GEN IP|CAVMON PORT|/home/cavisson/work6
  //NETCLOUD_GENERATOR_TRUN 33617|Chicago|192.168.1.66|7891|/home/cavisson/work6
  sprintf(net_cloud_dir, "%s/logs/TR%d/NetCloud", g_ns_wdir, run_num);

  //if(stat(net_cloud_dir, &st) != 0)
  //{
    //fprintf(stderr, "NetCloud dir %s does not exists. Hence not deleting TestRuns of generators.\n", net_cloud_dir);
   // return;
  //}

  sprintf(net_cloud_file_path, "%s/NetCloud.data", net_cloud_dir);

  if ((gen_tr_fptr = fopen(net_cloud_file_path, "r")) == NULL) {
   // fprintf(stderr, "Error in opening file %s.\n", net_cloud_file_path);
    //perror("fopen");
    return -1;
  }

  while (fgets(file_data, MAX_FILENAME_LENGTH, gen_tr_fptr) != NULL) 
  {
    num_flds = 0;
    if ((sscanf(file_data, "%s %s", keyword, text)) != 2)
    {
      fprintf(stderr, "At least two space separated fields required %s\n", file_data);
      continue;
    }
    else
    {
      //If keyword does not match then continue
      if(strcmp (keyword, "NETCLOUD_GENERATOR_TRUN"))
       continue;

      if(strstr(text, "|"))
      {
        num_flds = get_tokens(text, field, "|", 5);
      }
      //Added -i option to command, in case of Netcloud one might be using IP instead of machine names
      //e.g. server/server.dat entry: NCLasVegas1|Y|root|Caviss0n|/apps/java/jdk1.6.0_24/bin/java|/opt/cavisson/monitors/|N|LinuxEx|NA|NA|NA
      //whereas while running nsu_server_admin we are using IP,
      //nsu_server_admin -s 15.185.92.65 -P 7891 -c '/home/cavisson/work/bin/nsu_rm_trun -w /home/cavisson/work -n 600040 -f
      if(num_flds != 5) {
        fprintf(stderr, "Error: Wrong format of NetCloud.data file, skipping testrun %d\n", run_num);
        return 2;
      }
      
      sprintf(server_ptr.server_index_ptr->server_ip, "%s", field[2]);
      sprintf(cmd, "%s/bin/nsu_rm_trun -w %s -n %s -f", field[4], field[4], field[0]);
      nslb_encode_cmd(cmd);
      if(nslb_run_users_command(&server_ptr, cmd) != 0)
        fprintf(stderr, "Error in running command %s on generator = %s. Error: '%s'.\n", cmd, field[1], server_ptr.cmd_output);
      
      //Remove TRs from Controller also. It may possible that we have copied 
      //Generators TR to controller
      //remove_trun(atoi(field[0]));
    }
  }   
  fclose(gen_tr_fptr);
  return 0;
}

/* ----------------------------------------------------------------------------------
   Name		: is_tr_locked()
   Purpose	: This function will check that provided test run is locked or not.
		  If in $NS_WDIR/logs/TRxx/summary.top 13 field has value 'R', then
		  that Test Run will be consider as locked.
		  If value of 13 field is 'W'. It means test run is unlocked.
		  And field value is 'AR' that means tesrt run is archive. 

		  summary.top example: 4518|default/default/scen_prof1|02/09/18  03:45:31|Y|Available_New|netstorm|Unavailable|Unavailable|Available|Available|0|2|Test|W|00:00:16|10 

   Return	: 0 -> if Test Run is not locked
		  1 -> if Test Run is locked
		 0< -> if any error occured

   Author(s)	: Ayush Kr. 
   Date		: 20 Feb 2018
-------------------------------------------------------------------------------------*/
int is_tr_locked(char *path_to_tr, int run_num)
{
  char summtop_file[512 + 1];
  char line[1024 +1];
  char *fields[20];
  FILE *fp = NULL;

  //Open summary.top file
  sprintf(summtop_file, "%s/summary.top", path_to_tr);
  
  if((fp = fopen(summtop_file, "r")) == NULL)
  {
    if(errno == ENOENT) //If TR made by GUI due to compile script operation then summary.top not made
      return TR_IS_UNLOCKED;
    else
    {
      fprintf(stderr, "Error: unable open file %s, errno = %d, errstring = %s\n", summtop_file, errno, strerror(errno));
      return -1;
    }
  }

  //Summary.top has only one line 
  while(fgets(line, 1024 , fp)!= NULL)
  {
    get_tokens(line, fields, "|", 20);
    strcpy (g_test_owner, fields[5]);
    if((!strcmp(fields[13], "R")) || (!strcmp(fields[13], "AR")))
    {
      fprintf(stderr,"Test Run %d is locked. So it can not be deleted.\n", run_num);
      fclose(fp);
      return TR_IS_LOCKED;
    }
    else
    {
      fclose(fp);
      return TR_IS_UNLOCKED;
    }
  }
  fclose(fp);
  return TR_IS_UNLOCKED;
}

void remove_trun(char *ns_wdir, char *tr_or_partition, int run_num, int Pflag, long long partition_idx)
{
  char buf[MAX_FILENAME_LENGTH] = {0};
  int ret;
  char tr_path[1024] = {0};

  //TODO: Partition deletion on generator not supported right now.
  //First delete TRs from generators if exist

  sprintf(tr_path, "%s/logs/TR%d", ns_wdir, run_num);
  ret = is_tr_locked(tr_path, run_num);
  if((ret == TR_IS_LOCKED) || (ret == -1))  
    return;

  /*If -u option is given then user name and test onwer must be matched*/
  /*
    This is requried in case of Netstorm Repository as someone has deleted downloaded TestRun from local machine 
    MUST NOT be deleted from Netstorm Repository if it is executed by another user as local machine and 
    Netstorm Repository are in sync.
   */ 

  if(uflag && strcmp(g_user_name, g_test_owner) )
    return;

  ret = delete_test_run_frm_generators(ns_wdir, run_num);  //TODO: CHECK
  if(ret == 2)
    return;

  //Remove DB tables
  if ((!Pflag) && (remove_db_info || remove_everything))
  {
    sprintf(buf, "nsu_rm_dbinfo %d > /dev/null 2>&1", run_num);
    system(buf);
  }

  if(remove_everything)
  {
    remove_multidisk_path(tr_or_partition, run_num);
    
    if(Pflag)
    {
      PartitionInfo part_info;
      
      //Reset .partition_info.txt file linking, Manage script linking if it has script
      
      if(check_if_link_of_script_exists(ns_wdir, run_num, partition_idx, &part_info) == 1)
      {
        fprintf(stderr, "Error: Partition %lld is start partition and has script. "
                        "This script is linked by other partitions, hence cannot delete this partition.\n", partition_idx);
        exit (-1);
      }

      establish_linking_btw_partitions(ns_wdir, tr_or_partition, run_num, part_info);

      //Manage script linking if it has script
      //establish_link_btw_scripts(ns_wdir, tr_or_partition, run_num);

      //Delete partition database
      remove_partition_database(ns_wdir, partition_idx, run_num);
  
      sprintf(buf, "rm -rf %s/logs/%s", ns_wdir, tr_or_partition);
      system(buf);

      make_linking_btw_partitions(ns_wdir, run_num, part_info);

      //copy scripts from TR/ to TR/partition/
      //sprintf(buf, "cp -r %s/logs/TR%d/scripts/ %s/logs/%s/", ns_wdir, run_num, ns_wdir, tr_or_partition);
      //system(buf);
    }
    else
    {
      //remove TR or partitio
      sprintf(buf, "rm -rf logs/%s", tr_or_partition);
      system(buf);
    }
  }
  else
  {
    if (remove_all_log_files)
    {
      sprintf(buf, "rm -f logs/TR%d/reports/csv/*.csv logs/TR%d/*log", run_num, run_num);
      system(buf);
    }
    if (remove_all_reports)
    {
      sprintf(buf, "rm -rf logs/TR%d/*.report logs/TR%d/reports > /dev/null 2>&1", run_num, run_num);
      system(buf);
    }
    if(remove_all_scripts)
    {
      //TODO: Reset partition info linking, Manage script linking if it has script
      sprintf(buf,"rm -rf logs/TR%d/scripts > /dev/null 2>&1", run_num);
      system(buf);
    }
    if(remove_all_graphs)
    {
      sprintf(buf,"rm -rf logs/TR%d/graphs", run_num);
      system(buf);
    }
    if(remove_all_pagedump)
    {
      sprintf(buf, "rm -rf logs/TR%d/docs", run_num);
      system(buf);
    }
    if(remove_all_rawGraphs)
    {
      sprintf(buf, "rm -f logs/TR%d/*.dat > /dev/null 2>&1", run_num);
      system(buf);
    }
  }
}

int chk_duplicate_arg(char *flag_name, int flag, short *rem_flag)
{
  if(flag)
  {
    fprintf(stderr, "%s %s", flag_name, "option cannot be specified more than once\n");
    exit (1);
  }
  if(rem_flag != 0)
  {
    *rem_flag = 1;
    remove_everything = 0;
  }
  return(++flag);
}

int check_running_partition(char *ns_wdir, char *tr_or_partition)
{
  char file_path[2*MAX_FILENAME_LENGTH + 1] = {0};
  int ret_val = 0;

  sprintf(file_path, "%s/logs/%s/partition.status", ns_wdir, tr_or_partition);

  if(access(file_path, R_OK) != 0)
    ret_val = 1;

  return ret_val;
}


void format_time(char *output){
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    int offset = 0;
    // 07/06/21 15:53:58
    strftime(output, 1024, "%m/%d/%y %H:%M:%S", timeinfo);
}

int move_test_run_to_recycle_bin (int testrun, int flag_to_delete, char *recycle_bin_flag)
{
    struct stat st;
    char cmd[1024];

    char olddir[1024];

    sprintf (olddir, "%s/logs/TR%d", g_ns_wdir, testrun);

    //TR is getting moved to recyclebin even it is locked.
    int ret = is_tr_locked(olddir, testrun);
    if((ret == TR_IS_LOCKED) || (ret == -1))  
      return 0;
 
    /*If -u option is given then user name and test onwer must be matched*/
    /*
      This is requried in case of Netstorm Repository as someone has deleted downloaded TestRun from local machine 
      MUST NOT be deleted from Netstorm Repository if it is executed by another user as local machine and 
      Netstorm Repository are in sync.
     */ 
    if(uflag && strcmp(g_user_name, g_test_owner))
      return 0;

    sprintf (cmd, "%s/.recyclebin", g_ns_wdir);
    if (stat (cmd, &st)) 
    {
      sprintf(cmd, "mkdir %s/.recyclebin", g_ns_wdir);
      if (system (cmd) < 0)
      {
        fprintf (stderr, "Error: Unable to create recyclebin directory\n");
        return -1;
      }
      sprintf(cmd, "chown cavisson:cavisson %s/.recyclebin", g_ns_wdir);
      if (system (cmd) < 0)
      {
        fprintf (stderr, "Error: Unable to create recyclebin directory\n");
        return -1;
      }
    }
    sprintf (cmd, "%s/.recyclebin/TR%d", g_ns_wdir, testrun);
    if (flag_to_delete)
    {
      sprintf (cmd, "rm -rf %s/.recyclebin/TR%d", g_ns_wdir, testrun);
      if (system (cmd) < 0)
      {
        fprintf (stderr, "Error: Unable to delete directory in recyclebin\n");
        return -1;
      }
      fprintf (stdout, "Successfully deleted TR%d from the machine\n", testrun);
      return 0;
    }

    if (rename (olddir, cmd) < 0)
    {
      fprintf (stderr, "Error: Unable to mv testrun to recyclebin directory %s, %s. %s\n", olddir, cmd, strerror(errno));
      return -1;
    }
    else
     fprintf (stdout, "Successfully moved TR%d to .recyclebin directory\n", testrun);

    sprintf (cmd, "%s/.recyclebin/TR%d/.remove_info", g_ns_wdir, testrun);
    FILE *fp = fopen (cmd, "w");
    if (fp)
    {
      cmd[0] = '\0';
      format_time(cmd);
      fprintf (fp, "%s|%s\n", cmd, recycle_bin_flag);
      fclose (fp); 
    }
    return 0;
}

int main(int argc, char** argv)
{
  FILE* test_num_fptr;
  char buf[MAX_FILENAME_LENGTH];
  char tr_or_partition[MAX_FILENAME_LENGTH + 1] = {0};
  char *ptr;
  //char action[256];
  char c;
  int i;
  int srun =0;
  int erun =0;
  int sflag = 0;
  int eflag = 0;
  int nflag = 0;
  int dflag = 0;
  int lflag = 0;
  int rflag = 0;
  int fflag = 0;
  // ----Added by Atul-----
  int pflag = 0;
  int Sflag = 0;
  int gflag = 0;
  int Dflag = 0;
  int Wflag = 0;
  int Pflag = 0;
  int delete_from_recyclle_bin = 0;
  long long partition_idx;
  int is_partition_running = 0;
  int is_test_running = 0;
  
  char test_id_path[256 + 1];
  char recycle_bin_flag[256] = "";

  if(getuid() == 0)
  {
    fprintf(stderr, "Error: nsu_rm_trun can't run using root user.\n");
    exit (-1);
  }

  sprintf(test_id_path , "/home/cavisson/etc/test_run_id");

  if ((test_num_fptr = fopen(test_id_path , "r")) == NULL)
  {
    perror("fopen");
    fprintf(stderr, "Error in opening file 'test_run_id'\n");
    exit (-1);
  }
  else
  {
    if (fgets(buf, MAX_FILENAME_LENGTH, test_num_fptr))
    {
      erun = atoi(buf);
    }
    else
    {
      fprintf(stderr, "Error in getting info from file '/home/cavisson/etc/test_run_id'\n");
      exit(-1);
    }
  }

  while ((c = getopt(argc, argv, "s:e:dlgSDpn:rfw:P:R:k:u:")) != -1)
  {
    switch (c)
    {
      case 'p':                                                            /*p option for page dump*/
        pflag = chk_duplicate_arg("p", pflag, &remove_all_pagedump);
        break;
      case 'g':                             /*g option for graph*/
        gflag = chk_duplicate_arg("g", gflag, &remove_all_graphs);
        break;
      case 'S':                             /*S option for Scripts*/
        Sflag = chk_duplicate_arg("S", Sflag, &remove_all_scripts);
        break;
      case 'D':                             /*D option for Raw Graph Data*/
        Dflag = chk_duplicate_arg("D", Dflag, &remove_all_rawGraphs);
        break;
      case 'd':                             /*d option for Detailed Data*/
        dflag = chk_duplicate_arg("d", dflag, &remove_db_info);
        break;
      case 'l':                             /*l option for logs*/
        lflag = chk_duplicate_arg("l", lflag, &remove_all_log_files);
        break;
      case 'n':                             /*n option for No. Of Test Run*/
        nflag = chk_duplicate_arg("n", nflag, 0);
        run_num = atoi(optarg);
        clear_all_test_id = 0;
        break;
      case 's':                             /*s option for Start No. Of Test Run*/
        sflag = chk_duplicate_arg("s", sflag, 0);
        srun = atoi(optarg);
        clear_all_test_id = 2;
        break;
      case 'e':                             /*e option for End No. Of Test Run*/
        eflag = chk_duplicate_arg("e", eflag, 0);
        erun = atoi(optarg);
        clear_all_test_id = 2;
        break;
      case 'f':                             /*f option for Forcefully*/
        fflag++;
        //clear_all_test_id = 0;
        break;
#if 0
      case 'N':
        run_num = atoi(optarg);
        clear_all_test_id = 2;
        break;
#endif
      case 'r':                             /*r option for reports of Test Run*/
        rflag = chk_duplicate_arg("r", rflag, &remove_all_reports);
        break;
      case 'w':  /*using this option in Netcloud only because 
                 here we need to pass the generator environment.*/ 
        Wflag++;
        strcpy(g_ns_wdir, optarg);
        break;
      case 'P':                             /*P option for partition*/
        Pflag = chk_duplicate_arg("P", Pflag, 0);
        partition_idx = atoll(optarg);
        clear_all_test_id = 0;
        break;
      case 'R':
        strcpy (recycle_bin_flag, optarg);
        break;
      case 'k':
        delete_from_recyclle_bin = atoi (optarg);
        break;
      case 'u':
        uflag = chk_duplicate_arg("u", uflag, 0);
        strcpy (g_user_name, optarg);
        break;
      case ':':
      case '?':                             /*For help*/
        printf("Usage: ./nsu_rm_trun {-n run# | {-s run#} {-e run#}} {-d} {-g} {-p} {-S} {-D} {-l} {-r}\n");
        exit(1);
    }
  }

  if(Wflag)
  {
    if (chdir(g_ns_wdir))
    {
      fprintf(stderr, "could not change dir to $NS_WDIR");
      exit (1);
    }
  }
  else
  {
    ptr = getenv("NS_WDIR");
    if (!ptr)
    {
      fprintf(stderr, "NS_WDIR env variable must be defined\n");
      exit (1);
    }
    else
    {
      strcpy (g_ns_wdir, ptr);
      if (chdir(ptr))
      {
        perror("could not change dir to $NS_WDIR");
        fprintf(stderr, "could not change dir to $NS_WDIR");
        exit (1);
      }
    }
  } 

  if(nflag)
  {
    if(Pflag)
      sprintf(tr_or_partition, "TR%d/%lld",  run_num, partition_idx);
    else
      sprintf(tr_or_partition, "TR%d",  run_num);
  }
  
  if(optind != argc)
  {
    printf("Usage: ./nsu_rm_trun {-n run#|{-s run#} {-e run#}} {-d} {-g} {-p} {-S} {-D} {-l} {-r}\n");
    exit(1);
  }
 
    
  if((nflag) && (sflag||eflag))
  {
    fprintf(stderr, "s and/or e options may not be specified with n option\n");
    exit (1);
  }
  if((!nflag) && (Pflag))
  {
    fprintf(stderr, "P options cannot be specified without n option\n");
    exit (1);
  }

  is_test_running = check_running_test(g_ns_wdir); 

  if(Pflag)
  {
    is_partition_running = check_running_partition(g_ns_wdir, tr_or_partition); 
    if(is_test_running && is_partition_running)
    {
      fprintf(stderr, "Error: Running partition cannot be deleted. Does not have partition.status.\n");
      exit (-1);
    }
  }
  else if(nflag)
  {
    if(is_test_running)
    {
      fprintf(stderr, "Error: Running testrun cannot be deleted.\n");
      exit (-1);
    }
  }

    if(clear_all_test_id == 1)  // All test runs are to be deleted
    {
      struct dirent **entry;
      char path[1024];
      int n;
      char *ptr = NULL; 
    
      /* Open log dir and read TR's summary.top file to check TR is locked or not, If not locked then remove TR*/
      sprintf(path, "%s/logs/", g_ns_wdir);
     
      n = scandir(path, &entry, 0, alphasort);
      if(!fflag)
      {
        printf("All DB, logs & report Records for All runs would be removed, are you sure? (y/n) ");
        get_answer();                       /* to get answer from user to remove or not*/
      }
    
      while(n--)
      {
        if(nslb_get_file_type(path, entry[n]) == DT_DIR && !strncmp(entry[n]->d_name, "TR", 2))
        {
          ptr = entry[n]->d_name + 2; //skip TR
          run_num = atoi(ptr);
          if (recycle_bin_flag[0] || delete_from_recyclle_bin)
            if(move_test_run_to_recycle_bin (run_num, delete_from_recyclle_bin, recycle_bin_flag) != 0)
              exit(1);

          remove_trun(g_ns_wdir, entry[n]->d_name, run_num, 0, 0); 
        }
      }
      //TODO: Partition deletion on generator not supported right now.
      //delete TRs from generators if exist
    }
    else if (clear_all_test_id == 2) /* if Range is given option(s and e) */
    {
      i = 0;
      if(!fflag)
      {
        if (remove_everything)
          printf("All DB, logs & report Records for All runs in the test run range from %d to %d would be removed, are you sure? (y/n) ", srun, erun);
        else
          printf("All Selected types of  Records for All runs in the test run range from %d to %d would be removed, are you sure? (y/n) ", srun, erun);
        get_answer();
      }
      //Get the list of test run's with Db records
      sprintf(buf, "psql -q test cavisson >/tmp/t1 <<END\nselect table_name From information_schema.tables where table_name like 'testcase%%'\nEND");
      system(buf);
      sprintf(buf, "grep -i testcase /tmp/t1 | awk -F '_' '{ print $2 }' >/tmp/t3");
      system(buf);
      //Get the test runs that have any kind of logs
      sprintf(buf, "ls -1d logs/TR* | cut -c 8- >>/tmp/t3");
      system(buf);
      //Make an uniq sorted list
      sprintf(buf, "sort -nu /tmp/t3 >/tmp/t1");
      system(buf);
      if ((test_num_fptr = fopen("/tmp/t1", "r")) == NULL)
      {
        perror("fopen");
        fprintf(stderr, "Error in opening file '/tmp/t1'\n");
        exit (-1);
      }
      while (fgets(buf, MAX_FILENAME_LENGTH, test_num_fptr))
      {
        i = atoi(buf);
        if ((i >= srun) && (i <= erun))
        {
          sprintf(tr_or_partition, "TR%d", i);
          if (recycle_bin_flag[0] || delete_from_recyclle_bin)
            if(move_test_run_to_recycle_bin (run_num, delete_from_recyclle_bin, recycle_bin_flag) != 0)
              exit(1);

          remove_trun(g_ns_wdir, tr_or_partition, i, Pflag, partition_idx);
        }
      }
    }
    else
    {                                /* if a Test Run is given option(n)*/
      if (!fflag)
      {
        if (remove_everything)
          printf("All log, report file and DB logs for specified test run or partition # %s would be removed, are you sure? (y/n) ", tr_or_partition);
        else
          printf("All specified types of records for specified test run or partition # %s would be removed, are you sure? (y/n) ", tr_or_partition);
        get_answer();
      }


      /* If -n option is given then only TestRun will be deleted from Netstorm Repository if test onwer is matched*/
      sprintf(buf, "grep \"^netstorm.repository\" %s/webapps/sys/config.ini 1>/dev/null 2>&1", g_ns_wdir);
      system(buf);
      if(!(system(buf)))
      {
        int status;
        if(uflag)
          status = delete_test_run_from_ns_repo(run_num, g_user_name);
        else
          status = delete_test_run_from_ns_repo(run_num, recycle_bin_flag);
        //Exit from here if TR does not exit on local.
        struct stat s;
        sprintf(buf, "%s/webapps/logs/TR%d", g_ns_wdir, run_num);
        if(stat(buf, &s) < 0)
        {
          if(!status)
            fprintf (stdout, "Successfully deleted TR%d from NS repo\n", run_num);
          else
            fprintf (stdout, "Error: Unable to delete TR%d from NS repo\n", run_num);
          exit (0);
        }
      }

      if (recycle_bin_flag[0] || delete_from_recyclle_bin)
        if(move_test_run_to_recycle_bin (run_num, delete_from_recyclle_bin, recycle_bin_flag) != 0)
          exit(1);
        else
          return 0;

      remove_trun(g_ns_wdir, tr_or_partition, run_num, Pflag, partition_idx);                     /* to start Processing*/
    }
#if 0
  if (delete_from_recyclle_bin == 0)
  {
    buf[0] = '\0';
    sprintf (buf, "%s/.recyclebin/TR%d/.remove_info", g_ns_wdir, run_num);
    FILE *fp = fopen (buf, "a");
    if (!fp)
      return -1;
    buf[0] = '\0';
    format_time(buf);
    fprintf (fp, "%s|%s\n", buf, recycle_bin_flag);
    fclose (fp); 
  }
#endif
  
  return 0;
}
