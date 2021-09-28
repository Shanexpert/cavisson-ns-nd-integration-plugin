/* 
 * Output will be :
 * ----------------------------------------------------------- 
    TR|Duration|OrigDuration|Operation|Status (header line)
    1234|00:10:10|00:00:00|get or update|OK or NotOK or NotZero
*/

/* summary.top has 16 pipe seperated fields,
   Test Run|Scenario Name|Start Time|Report Summary|Page Dump|Report Progress|Report Detail|
   Report User|Report Fail|Report|Page Break Down|WAN_ENV|REPORTING|Test Name|Test Mode
   |Run Time|Virtual Users
*/
#include <dirent.h>
#include <errno.h>
#define NS_EXIT_VAR
#include "ns_exit.h"

#include "ns_tr_duration.h"

void do_all_stuff(int trnum, char *nswdir, int gflag, int uflag, char *operation);

void Usage(void)
{
  printf ("usage:\n");
  printf ("     nsi_tr_duration [-g] [-u] [-n TR Number]\n");
  printf ("     where:\n");
  printf ("          -g :To get Duration\n");
  printf ("          -u :To update Duration\n");
  /*printf ("          -v :To varify\n");*/
  printf ("          -n :To provide TR Number\n");
  exit(-1);
}

void get_verify(int duration, int orig_duration, char *operation, int trnum, int hflag)
{
  char str_orig_duration[MAXLENGTH + 1];
  char str_duration[MAXLENGTH + 1];
  int diff;
  char status[MAXLENGTH + 1];

  
  diff = duration - orig_duration;
  diff = labs (diff);
    
  strcpy (str_orig_duration, format_time (orig_duration)); 
  strcpy (str_duration, format_time (duration)); 

  if (duration == 0 && orig_duration == 0)
    strcpy (status, "OK");
  else if (diff <= 10) 
    strcpy (status, "OK");
  else 
    strcpy (status, "NotOK");

  print_result (trnum, str_duration, str_orig_duration, operation,status, hflag);
}

void update(int duration, int orig_duration, char *operation, int trnum, char *summary_top_path, int hflag)
{
  char str_orig_duration[MAXLENGTH + 1];
  char str_duration[MAXLENGTH + 1];
  int ret_val;
   
  //strcpy (str_orig_duration, get_orig_duration (summary_top_path, &trnum)); 
  strcpy (str_duration, format_time (duration)); 
  strcpy (str_orig_duration, format_time (orig_duration)); 

  //TODO update always
  //if (orig_duration > 0) {
  //  print_result (trnum, str_duration, str_orig_duration, operation, "NotZero",hflag);
  //} else if (orig_duration == 0){ 
  ret_val = write_summary (str_duration, summary_top_path);
  if (ret_val != -1) 
    print_result (trnum, str_duration, str_orig_duration, operation, "OK", hflag);
  else
    print_result (trnum, str_duration, str_orig_duration, operation, "NotOK", hflag);
  //}
}

/*
 *Description: To filter directories starting with TR in scandir.
 *inputs : *a - DIR structure.
 *outputs : 1 on succes ,0 on failure.
 *error : none
 *algo : simple
*/
int tr_only_tr(const struct dirent *a) 
{
  if ( (a->d_name[0] == 'T') && (a->d_name[1] == 'R') )
    return 1;
  else
    return 0;
}

/*
 *Description : Comparison function for scandir
 *inputs : **a, **b - pointers to DIR structure. 
 *outputs : 1 on succes ,0 on failure
 *error : none
 *algo : simple
*/
#if ( (Fedora && RELEASE >= 14) || (Ubuntu && RELEASE >= 1204) )
int tr_alpha_sort(const struct dirent **aa, const struct dirent **bb) 
#else
int tr_alpha_sort(const void *aa, const void *bb) 
#endif
{
  const struct dirent **a = (const struct dirent **) aa;
  const struct dirent **b = (const struct dirent **) bb;
            
  //fprintf (stderr, "(a)->d_name  = %s, (b)->d_name = %s\n", (*a)->d_name, (*b)->d_name);
  if (atoi((*a)->d_name + 2) < atoi((*b)->d_name + 2))
    return 1;
  else
    return 0;
}


void get_update_all_test_runs(char *ns_wdir, int gflag , int uflag, char *operation, int trnum)
{
  char pathname[MAXLENGTH + 1] = {0};
  int n;
  int hflag = 1;  
  struct stat stat_buf;
  
  struct dirent **namelist;
  strcat (ns_wdir, "/logs");
  DIR *dir_fp = opendir(ns_wdir);
  
  if (dir_fp == NULL) {
     NS_EXIT(1, "Unable to open dir (%s) for reading.\n", nslb_strerror(errno));
  }
  
  //n = scandir (ns_wdir, &namelist, tr_only_tr, ((int *)(const struct dirent **, const struct dirent **))tr_alpha_sort);
  n = scandir (ns_wdir, &namelist, tr_only_tr, tr_alpha_sort);
  if (n < 0)
    perror ("scandir");
  else {
    while (n--) {
      if (namelist[n]->d_name[0] == 'T' && namelist[n]->d_name[1] == 'R') {

      sprintf(pathname, "%s/logs/TR%d", ns_wdir, trnum);
 
      if (stat (pathname, &stat_buf) == -1)
      {
        printf ("ERROR: %s does not exist.\n", pathname);
        continue;
      }
        
        do_all_stuff(trnum, ns_wdir, gflag, uflag, operation);

      /*  sprintf (sum_top_file, "%s/%s/summary.top", ns_wdir, namelist[n]->d_name);
        //TODO check in partition
        sprintf (rtg_msg_file, "%s/%s/rtgMessage.dat", ns_wdir, namelist[n]->d_name);
        sprintf (test_run_file, "%s/%s/testrun.gdf", ns_wdir, namelist[n]->d_name);
        //sscanf (namelist[n]->d_name,"%s%d",trnum); 
        //printf ("files 1:%s 2:%s 3:%s\n", sum_top_file, rtg_msg_file, test_run_file);
        ret_val = test_files (sum_top_file, rtg_msg_file, test_run_file);
        if (ret_val == 1){
          no_rtg_msg_file = 0;   
        } else if (ret_val == -1 ) { 
          continue; 
        } else if (ret_val == 0) {
          no_rtg_msg_file = 1;
        }
      */

        /*getting duration from summary.top */
       /* strcpy (str_orig_duration, get_orig_duration (sum_top_file, &trnum));
        orig_duration = (get_tm_from_format (str_orig_duration)) / 1000; // divide by 1000 to do in secs*/
        
        
        /*calculating duration by finidng the block size from testrun.gdf and size of rgtMessage.dat .
        ** tot_block = (size of rgtMessage.dat)/ block size
        ** duration = tot_block * progress interval
        ** progress interval is also taken from testrun.gdf
        **/
        /*if (no_rtg_msg_file == 0) {
          //TODO do for partition
          duration = get_actual_duration (sum_top_file, rtg_msg_file, test_run_file, 0);
          //printf("duration:%d",duration);
        } else {
          duration = 0;
        }

        if (gflag == 1) { // to calculate duration of a test run
          get_verify (duration, orig_duration, operation, trnum, hflag) ;
        } else if (uflag == 1) { //To update duration of test run in summary.top
          update (duration, orig_duration, operation, trnum, sum_top_file, hflag);
        }*/
        if (hflag == 1)
          hflag = 0;
      }
      free(namelist[n]);
    }
    free(namelist);
  }
  closedir(dir_fp);
}

int test_files (char *sum_top, char *rtg_msg, char *test_run)
{
  struct stat stat_buf;
  if (stat (sum_top ,&stat_buf) == -1) {
    fprintf(stderr, "Unable to get %s\n", sum_top); 
    return (-1);  
  }
  if (stat (rtg_msg ,&stat_buf) == -1) {
    //perror("rtgMessage.dat");
    return 0;
  }
  if (stat (test_run ,&stat_buf) == -1) {
    fprintf(stderr, "Unable to get %s\n", test_run); 
    return (-1);  
  }
  return 1;
}

int 
main(int argc, char *argv[]) 
{
  char c;
  int trnum ;
  int gflag = 0;
  int uflag = 0;
  char ns_wdir[MAXLENGTH + 1]; 
  char pathname[MAXLENGTH + 1];
  char operation[20];
  struct stat stat_buf;
  int all = 0;

  while ((c = getopt(argc, argv, "gun:?")) != -1) {
    switch (c) {
      case 'g':
        gflag = 1;
        strcpy (operation, "get");
        break;
      case 'u':
        uflag = 1;
        strcpy (operation, "update");
        break;
      case 'n':
        if (!strcasecmp (optarg, "ALL")) {
          all = 1 ;
        } else {
          trnum = atoi (optarg);
        }

        break;
      case '?':
        Usage();
        break;
      default:
        Usage();
    } //switch
  } //while
  
  if (argc < 2) {
    Usage();
  } if (gflag == 1 && uflag == 1) {
    Usage();
  }
  
  strcpy (ns_wdir ,getenv("NS_WDIR"));
  if (ns_wdir == NULL) {
    strcpy (ns_wdir, "/home/cavisson/work");    
  }
  strcpy (pathname, trpath (trnum, ns_wdir));
  
  if (all == 0) 
  {
    sprintf(pathname, "%s/logs/TR%d", ns_wdir, trnum);

    if (stat (pathname, &stat_buf) == -1)
    {
      NS_EXIT(-1, "ERROR: TR%d directory does not exist.\n", trnum);
    }
    do_all_stuff(trnum, ns_wdir, gflag, uflag, operation);
    
    /*strcpy (summary_top_path, pathname);
    strcat (summary_top_path, "/summary.top");
    strcpy (rtg_msg_dat_path, pathname);
    strcat (rtg_msg_dat_path, "/rtgMessage.dat");
    strcpy (test_run_gdf_path, pathname);
    strcat (test_run_gdf_path, "/testrun.gdf");
    
    ret_val = test_files (summary_top_path, rtg_msg_dat_path, test_run_gdf_path); 
    if (ret_val == 0)
      no_rtg_msg_file = 1;
    else if (ret_val == -1)
      exit(-1);

    //getting duration from summary.top 
    strcpy (str_orig_duration, get_orig_duration (summary_top_path, &trnum)); 
    orig_duration = (get_tm_from_format (str_orig_duration)) / 1000; // divide by 1000 to do in secs */
    
    /*calculating duration by finidng the block size from testrun.gdf and size of rgtMessage.dat .
    * tot_block = (size of rgtMessage.dat)/ block size 
    * duration = tot_block * progress interval
    * progress interval is also taken from testrun.gdf 
    */
    
     /* //printf ("orig_duration------>>>%d\n",orig_duration);
      //TODO get for partition
      duration = get_actual_duration (summary_top_path, rtg_msg_dat_path, test_run_gdf_path, 0);
      //printf ("duration in main------>>>%d\n",duration);
      
    if (gflag == 1) { // to calculate duration of a test run
     get_verify (duration, orig_duration, operation, trnum, 1); 
    } else if (uflag == 1) { //To update duration of test run in summary.top
     //TODO: UPDATE TR & LAST PARTITION SUMMMARY.TOP
     update (duration, orig_duration, operation, trnum, summary_top_path, 1); 
    }*/
  } else {
    //printf("--------------->>\n");
    get_update_all_test_runs (ns_wdir, gflag, uflag, operation, trnum);     
  }  
  return 0;
}


int check_if_test_in_partition_mode(char *TRPath)
{
  struct stat s;
  char buf[1024 + 1] = {0};

  sprintf(buf, "%s/.curPartition", TRPath);

  if(stat(buf, &s) == 0)
    return  1;    //test is in partition mode
  else
    return 0;     //Non partiiton test
}

void do_all_stuff(int trnum, char *nswdir, int gflag, int uflag, char *operation)
{
  int partition_mode;
  char NSLogsPath[1024];
  char pathname[1024];
  char total_duration[256] = {0};
  char last_partition_duration[256] = {0};
  char partition_name[PARTITION_ARRAY_SIZE][PARTITION_LENGTH] = {0};
  char str_orig_duration[20];
  char rtg_msg_dat_path[MAXLENGTH + 1];
  char test_run_gdf_path[MAXLENGTH + 1];;
  char summary_top_path[MAXLENGTH + 1]; 
  long long total_orig_duration = 0;
  long long orig_duration_for_partition = 0;
  int duration;

  sprintf(NSLogsPath, "%s/logs", nswdir);
  sprintf(pathname, "%s/logs/TR%d", nswdir, trnum);
  partition_mode = check_if_test_in_partition_mode(pathname);



/*  if(!partition_mode)
  {
    strcpy (rtg_msg_dat_path, pathname);
    strcpy (test_run_gdf_path, pathname);
    
    strcat (rtg_msg_dat_path, "/rtgMessage.dat");
    strcat (test_run_gdf_path, "/testrun.gdf");
  } */
    
  /*getting duration from TR/ summary.top in both case partition/non-partition */
  sprintf (summary_top_path, "%s/summary.top", pathname);
  strcpy (str_orig_duration, get_orig_duration (summary_top_path, &trnum)); 
  total_orig_duration = (get_tm_from_format (str_orig_duration)) / 1000; // divide by 1000 to do in secs 
  //orig_duration = (get_tm_from_format (str_orig_duration)) / 1000; // divide by 1000 to do in secs 
    
  if(partition_mode)
  {
    get_first_and_last_partition(NSLogsPath, trnum, partition_name);
    sprintf (summary_top_path, "%s/%s/summary.top", pathname, partition_name[1]);
    strcpy (str_orig_duration, get_orig_duration (summary_top_path, &trnum)); 
    orig_duration_for_partition = (get_tm_from_format (str_orig_duration)) / 1000; // divide by 1000 to do in secs 

    sprintf(NSLogsPath, "%s/logs", nswdir);
    //TODO duration_str has nothing to do here
    duration = get_duration(trnum, NSLogsPath, total_duration, 0, 0, last_partition_duration) / 1000;
  }
  else
  {
    sprintf(rtg_msg_dat_path, "%s/rtgMessage.dat", pathname);
    sprintf(test_run_gdf_path, "%s/testrun.gdf", pathname);
    
    duration = get_actual_duration (summary_top_path, rtg_msg_dat_path, test_run_gdf_path, 0);
  }
      
  if (gflag == 1) 
  { // to verify duration of a test run
    get_verify (duration, total_orig_duration, operation, trnum, 1); 
  } 
  else if (uflag == 1) //To update duration of test run in summary.top
  {
    //Update last partition summary.top duration
    if(partition_mode)
    {
      update (get_tm_from_format(last_partition_duration)/1000, orig_duration_for_partition, operation, trnum, summary_top_path, 1); 
    }
      //update (last_partition_duration, orig_duration, operation, trnum, summary_top_path, 1); 

    //Update TR/ summary.top duration
    sprintf (summary_top_path, "%s/summary.top", pathname);
    update (duration, total_orig_duration, operation, trnum, summary_top_path, 1); 
  }
}
