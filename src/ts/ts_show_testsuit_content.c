#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include "nslb_util.h" 


void display_help_and_exit()
{
  printf("Usage: ts_show_test_suit_content -t testsuit_name -w <workspace_name>/<profile_name>\n");
  printf("Where: \n");
  printf("  -t testsuit name\n");
  printf("  test suit name should be line this project_name/sub_project_name/testsuit_name-\n");
  printf("  ,if project , subproject will not given then it will take default as project and sub project\n");
  printf("  -w <workspace_name>/<profile_name>, for example -w anup/ns_dev_4.6\n");
  printf("  ,it will consider default  workspace_name and profile_name, incase not provided\n");
  exit(-1);
}




int get_tokens1(char *read_buf, char *fields[], char *token, int max_flds)
{
  int totalFlds = 0;
  char *ptr;
  char *token_ptr;

  ptr = read_buf;
  while((token_ptr = strtok(ptr, token)) != NULL)
  {
    ptr = NULL;
    totalFlds++;
    if(totalFlds > max_flds)
    {
      totalFlds = max_flds;
      break;  /* break from while */
    }
    fields[totalFlds - 1] = token_ptr;
  }
  return(totalFlds);
}



/*int extract_proj_subproj_name(char *optarg, char **field, char *usr_testsuites)
{
int ret;

  ret = get_tokens1(optarg, field, "/");
  strcpy(usr_project, field[0]);
  strcpy(usr_sub_project, field[1]);
  strcpy(usr_testsuites, field[2]);
  num_projs++;

  LIB_DEBUG_LOG(1, debug_level, debug_file, "ret = %d", ret);
  return ret;
}
*/

static void set_ns_ta_dir(int argc, char *argv[], char *work_dir)
{
   /**************************************************************************/
  //if argc == 3
  char *field[6];
  switch(argc)
  {
     case 5:
       //break only if both workspace and profile name are provided
       if((get_tokens1(argv[4], field, "/", 3)) == 2)
       {
         nslb_set_ta_dir_ex1(work_dir, field[0], field[1]);
         break;
       }
     //if there is only either workspace or profile name provided then go to default
     default:
       nslb_set_ta_dir_ex1(work_dir, GET_DEFAULT_WORKSPACE(), GET_DEFAULT_PROFILE());    
  }
  //   then use default workspace and profile
  //if argc == 4 and option is -w worksapce_name  or -w /profile_name or -w 
  /**************************************************************************/
}


int main(int argc, char *argv[])
{
  char *work_dir = "/home/cavisson/work";

  static char testsuites_name[256 + 1] = "\0";
  static char test_suites_dir[1024 + 1] = "\0";
  static char testsuites_path[1024 + 1] = "\0";
  FILE *tsuites_fp;
  char buf[4096] = "\0";
  char *field[6];
  int fieldCount;
  
  if(argc < 3)
  {
    display_help_and_exit();
  }
  if(strcmp(argv[1], "-t") != 0)
  {
    printf("Invalid options:\n");
    display_help_and_exit();
  }

  if(strchr(argv[2], '/') == NULL)
    sprintf(testsuites_name, "default/default/testsuites/%s", argv[2]);
  else{
    /*********************************************************************************************/
    if((get_tokens1(argv[2], field, "/", 4)) != 3)
       display_help_and_exit();
    /***********************************************************************************************/
    sprintf(testsuites_name, "%s/%s/testsuites/%s", field[0], field[1], field[2]);  
  }
  if (getenv("NS_WDIR") != NULL)
    work_dir = (getenv("NS_WDIR"));
  //set ns ta dir 
  set_ns_ta_dir(argc, argv, work_dir);
  sprintf(test_suites_dir, "%s", GET_NS_TA_DIR());
  if(strstr(testsuites_name, ".conf"))
    sprintf(testsuites_path, "%s/%s", test_suites_dir, testsuites_name);
  else
     sprintf(testsuites_path, "%s/%s.conf", test_suites_dir, testsuites_name);
  if((tsuites_fp = fopen(testsuites_path, "r")) == NULL)
  {
    fprintf(stderr, "Unable to open testsuites file = %s, exit\n", testsuites_path);
    exit(-1);
  }
  printf("%s|Test Case|Action|Value-1|Value-2\n", WORK_PROFILE);
  while(fgets(buf, 4095, tsuites_fp) != NULL)
  {
    if(strncmp(buf, "TEST_CASE_NAME", 14) == 0)
    {
      fieldCount = get_tokens1(buf, field, " ", 5);
      
      if(fieldCount >= 3) 
      {
        if(field[fieldCount - 1][strlen(field[fieldCount - 1]) - 1] == '\n')
          field[fieldCount - 1][strlen(field[fieldCount - 1]) - 1] = '\0';
        printf("%s|%s|%s|%s|%s\n", GET_NS_PROFILE(), field[1], field[2], fieldCount>3?field[3]:" ", fieldCount>4?field[4]:" ");
      }
    }
  
  }
  fclose(tsuites_fp); 

}
