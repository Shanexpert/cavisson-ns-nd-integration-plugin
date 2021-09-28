/* Name            : runcmd.c
 * Purpose         : To validate commands executed with nsu_server_admin using BLACKLIST/WHITELIST file.
 * Initial Version : Sat, Jan 11 2014 
 * Author          : Krishna Tayal
 */

#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <stdlib.h>
#include "ns_server_admin_utils.h"
#include "ns_server_admin_validate_cmd.h"

/*  This function matches given command with content of given file.
 *  Input:  command, file path.
 *  Output: 0 if command is valid, -1 if command is not valid or error occurs.
 */
static int match_cmd(char *cmd)
{
  FILE *fp;
  char buf[MAX_BUF_SIZE + 1] = {0};
  char keyword = -1, is_cmd_present = 0, flag = 0;
  char *ptr;
  char command_list_file[MAX_BUF_SIZE + 1] = {0};

  /*  When build is upgraded, RunCommandList.dat.sample file is copied in etc/ directory.
   *  Also if commandlist.dat file is not present in sys/ directory,
   *  new commandlist.dat file is created at build upgadation time.    
   *  Hence, if commandlist.dat file is not found, every command will be Blocked.
   */
  sprintf(command_list_file, "%s/sys/RunCommandList.dat", get_ns_wdir());
  fp = fopen(command_list_file, "r");
  if(!fp)
  {
    fprintf(stderr, "File : %s not found. Hence Blocking command.\n", command_list_file);
    return -1;
  }
  while(nslb_fgets(buf, MAX_BUF_SIZE, fp, 0))
  {
    /*  Here we are ignoring '#', <space>, <tab> and blank lines */
    flag = 0;
    ptr = buf;
    CHECK_IF_COMMENTED_OR_BLANK_LINE(ptr, flag);
    /*  flag will be set if line is commented and *ptr will be '\0' if line is blank. */
    if (flag || !*ptr)
      continue;

    if(-1 == keyword)
    {
      /*  We are comparing first 18 chars of read line.
      *  Everything else written after "FILETYPE=BLACKLIST/WHITELIST" will be ignored.
      */  
      if (!strncasecmp(buf, "FILETYPE=BLACKLIST", 18))
        keyword = BLACKLIST;
      else if (!strncasecmp(buf, "FILETYPE=WHITELIST", 18))
        keyword = WHITELIST;

      /*  If None of 'BLACKLIST' and 'WHITELIST' keywords is present, error will be thrown, and comaand won't execute. 
       *  We are not handling the case if both keywords are present.  
       */
      if(-1 == keyword)
      {
        buf[strlen(buf) - 1] = '\0';   // Removing NewLine Character.
        fprintf(stderr, "Syntax '%s' is not valid, hence blocking the command.\n", buf);
        fprintf(stderr, "%s\n", "Valid format: FILETYPE=BLACKLIST or FILETYPE=WHITELIST");
        fclose(fp);
        return -1;
      }
    }
    else
    {
      /*  Data should be in format '<Command>|NA|NA|NA|<Description>';
       *  If '|' is not found; error will be thrown.
       *  We are taking care of first '|' only, to get command.
       *  In future if we need remaining sections, we'll have to take care of those.
       */
      ptr = strstr(buf, "|");
      if(!ptr)
      {
        buf[strlen(buf) - 1] = '\0';   // Removing NewLine Character.
        fprintf(stderr, "Line '%s' is not in valid format, hence ignoring the line.\n", buf);
        fprintf(stderr, "%s\n", "Valid format: <Command>|NA|NA|NA|<Description>\n");
        continue;
      }
      *ptr = '\0';
      /*  If comparing given command from file. */
      if(!strcmp(buf, cmd))
      {
        is_cmd_present = 1;
        break;
      }
    }
  } 
 
  /*  Command will be blocked in 1 of two cases.
   *  1. command is not present in WHITELIST.
   *  2. command is present in BLACKLIST.
   */  
  if((WHITELIST == keyword && !is_cmd_present) || (BLACKLIST == keyword && is_cmd_present))
  {
    //fprintf(stderr, "Command %s is Blocked on Server.\n", cmd);
    fclose(fp);
    return -1;
  }

  fclose(fp);
  return 0;
}

/*  This function validates given command using given file name.
 *  Command is valid if either command is present in WITELIST or is not present in BLACKLIST.
 *  Input : Command, Path to file.
 *  Output: 0 if command is valid. -1 if command is not valid or error is found.
 */
int validate_command(char *cmd_args)
{
  char cmd[MAX_BUF_SIZE + 1] = {0};
  char *ptr;  
  
  /*  while parsing command we put ':' in place of first ' ',
   *  command 'ls -ltr' will be 'ls:-ptr' after parsing.  
   */
  if((ptr = strstr(cmd_args, ":")))
    strncpy(cmd, cmd_args, ptr-cmd_args);
  else
    strcpy(cmd, cmd_args);

  /*  We may get command in form of "/bin/ls"; Hence we are using basename() to get command.
   *  Also, bsename() function may return pointer to statically allocated memory which may be overwritten by subsequent  calls. 
   *  Alternatively, this may return a pointer to some part of path, so that the string referred to by path should not be modified 
   *  or freed until the pointer returned by the function is no longer required.
   */
  ptr = basename(cmd);

  return(match_cmd(ptr));
}
