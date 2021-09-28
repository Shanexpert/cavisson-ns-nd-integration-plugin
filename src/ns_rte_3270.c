
/************************************************************************************
 * Name      : ns_rte_3270.c 
 * Purpose   : This file contains functions related to rte protocol 
 * Author(s) : Devendra Jain/Atul Sharma 
 * Date      : 20 May 2018 
 * Copyright : (c) Cavisson Systems
 * Modification History : Atul: Add RTE_THINK_TIME api, remove dependency on rte_invoker shell, copy pipes to vptr.
 *                        2. Vikas: Use wp and rp of vptr instead of ns_rte(Bug 48059)
 ***********************************************************************************/

#include "ns_rte_3270.h"
#include "ns_tls_utils.h"

static __thread ns3270keyMap *keyMap = NULL;
static __thread int total_key_map_entires=0;
static __thread int max_key_map_entires=0;

//generate key map from rte3270KeyMap.dat or use default 
static int ns_rte_3270_gen_key_map()
{
  char line[MAX_RTE_LINE_LENGTH];
  char name[MAX_RTE_LINE_LENGTH];
  char cmd[MAX_RTE_LINE_LENGTH];
  int i;
  char *ptr;
  char keymapfile[MAX_RTE_LINE_LENGTH];
  FILE *fp;
  
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_RTE(vptr, NULL, "Method called");

  sprintf(keymapfile, "%s/etc/rte3270KeyMap.dat", getenv("NS_WDIR"));

  fp = fopen(keymapfile, "r");
  if(!fp)
  {
    NSDL1_RTE(vptr,NULL,"Failed to open file %s",keymapfile);
    NSTL1(vptr,NULL,"Failed to open file %s",keymapfile);
    return -1;
  }

  while(1)
  {
    //Read line from file
    ptr = fgets (line, MAX_LINE_LENGTH, fp);
    if (ptr == NULL)
    {
      break;
    }
    //Ignore Blank or Commented Line
    if(line[0] == '\0' || line[0] == '#' || line[0] == '\n')
      continue;
   
    //Allocate or Reallocate Memory for KeyMap
    if(total_key_map_entires == max_key_map_entires)
    {
      max_key_map_entires += RTE_KEY_MAP_SIZE;
      MY_REALLOC(keyMap, max_key_map_entires * sizeof (ns3270keyMap), "RTE keyMap",-1);
    }

    //Parse KeyMap as name & command and store in keyMap
    sscanf(line,"%s %s",name,cmd);
    strcpy(keyMap[total_key_map_entires].name,name);
    strcpy(keyMap[total_key_map_entires].cmd,cmd);
    keyMap[total_key_map_entires].len = strlen(keyMap[total_key_map_entires].name);
    total_key_map_entires++;
  }
  if (!total_key_map_entires)
  {
    NSDL1_RTE(vptr,NULL,"No entry found in %s",keymapfile);
    NSTL1(vptr,NULL,"No entry found in %s",keymapfile);
    return -1;
  }

  NSDL4_RTE(vptr,NULL,"RTE KeyMap");
  for(i = 0; i < total_key_map_entires; i++)
    NSDL4_RTE(vptr,NULL,"%s %s",keyMap[i].name,keyMap[i].cmd);

  return 0;
}

/*Write to the terminal & Read the response*/
static int ns_rte_x3270_write(int infd, int outfd, char *buffer, int len)
{
  int sl,nr,i,get_more;
  int done = 0;
  int xs = -1;
  char output[MAX_RTE_BUF_SIZE+1] = "";
  char line[MAX_RTE_BUF_SIZE+1] = "";
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_RTE(vptr, NULL,"Method called buffer = %s, len = %d", buffer, len);

  if (write(outfd, buffer, len) < 0)
  {
    NSDL4_RTE(vptr, NULL, "Error in Write fd = %d, buffer = %s, len = %d", outfd, buffer, len);
    NSTL1(vptr, NULL, "Error in Write fd = %d, buffer = %s, len = %d", outfd, buffer, len);
    return -1;
  }

  while (!done && (nr = read(infd, output, MAX_RTE_BUF_SIZE)) > 0 ) 
  {
    get_more = 0;
    sl=0;
    NSDL4_RTE(vptr,NULL,"RTERead: %s\n",output);
    i = 0;
    do {
      while (i < nr && output[i] != '\n') {
        if (sl < MAX_RTE_BUF_SIZE - 1) 
          line[sl++] = output[i++];
      }
      if (output[i] == '\n')
        i++;
      else {
        /* Go get more input. */
        get_more = 1;
        break;
      }

      /* Process one line of output. */
      line[sl] = '\0';

      if (!strcmp(line, "ok")) {
        (void) fflush(stdout);
        done = 1;
        xs = 0;
        break;
      }

      if (!strcmp(line, "error")) {
        (void) fflush(stdout);
        done = 1;
        xs = -1;
        break;
      } 

      /* Get ready for the next. */
      sl = 0;
    } while (i < nr);

    if (get_more) {
      get_more = 0;
      continue;
    }
  }
  if (nr < 0)
  {
    NSDL1_RTE(vptr,NULL,"Error in Read infd = %d", infd);
    NSTL1(vptr,NULL,"Error in Read infd = %d", infd);
    return -1;
  }
  else if (nr == 0)
    NSDL1_RTE(vptr,NULL,"Error: Read zero bytes.");

  return xs;
}


/*--------------------------------------------------------------------------------------------- 
 * Name      : ns_rte_3270_connect
 * Purte->rpose   : This function will do following - 
 *             (1) Will create new 3270 session by given host name and user
 *             (2) Authenticate the user name and password  
 *
 * Input     :  rte structure - provide scructure of rte type
 *              host name     - provide host name from which we wants to connect
 *              user name     - provide user name
 *              password      - password 
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int ns_rte_3270_connect(ns_rte *rte, char *host , char *user , char *password)
{
  int pid;
  char cmd[MAX_RTE_CMD_LENGTH]; 
  char name[MAX_RTE_CMD_LENGTH];
  char xrm[MAX_RTE_CMD_LENGTH];
  char display[8];
  int rp[2], wp[2];
  int fd;
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_RTE(vptr, NULL,"Method called, host = %s , user = %s , password = %s", host , user, password);

  if(!rte)
  { 
    NSDL1_RTE(vptr,NULL,"NULL input received");
    NSTL1(vptr,NULL,"NULL input received");
    return -1;
  }

  if(!host && !host[0])
  {
    NSDL1_RTE(vptr,NULL,"NULL host received");
    NSTL1(vptr,NULL,"NULL host received");
    return -1;
  }

  if(!keyMap)
  {
    if (ns_rte_3270_gen_key_map() < 0)
    {
      NSDL1_RTE(vptr, NULL, "gen_key_map failed");
      NSTL1(vptr, NULL, "gen_key_map failed");
      return -1;
    }
  }

  if (pipe(wp) < 0 || pipe(rp) < 0)
  { 
    NSDL1_RTE(vptr,NULL,"pipe() failed");
    NSTL1(vptr,NULL,"pipe() failed");
    return -1;
  }
  //Check and set display and command if required  
  if(rte->terminal)
  {
    //Get the display id from {ns_rte_display} using ns_eval_string 
    //http://manpages.ubuntu.com/manpages/bionic/man1/x3270.1.html
    ns_advance_param("ns_rte_display");
    snprintf(display, 8, ":%s", ns_eval_string("{ns_rte_display}"));
    strcpy(cmd, "/home/cavisson/thirdparty/bin/x3270");
    strcpy(name, "x3270");
    strcpy(xrm, "x3270.newEnviron: false");
    NSDL1_RTE(vptr, NULL, "display = %s, cmd = %s, name = %s, xrm = %s", display, cmd, name, xrm);
  } 
  else
  {
    strcpy(cmd,"/home/cavisson/thirdparty/bin/s3270");
    strcpy(name,"s3270");
    strcpy(xrm,"s3270.newEnviron: false");
    NSDL1_RTE(vptr,NULL," cmd = %s, name = %s, xrm = %s", cmd, name, xrm);
  }

  //Create Child to execute RTE terminal
  if ((pid = fork()) < 0)
  {
    NSDL1_RTE(vptr,NULL,"fork() failed");
    NSTL1(vptr,NULL,"fork() failed");
    return -1;
  }

  if (pid == 0)
  {
    NSDL2_RTE(vptr, NULL,"In Child");
    //Close the child to parent write pipe 
    close(wp[1]);
    //Close the child to parent read pipe 
    close(rp[0]);
    //Redirect the parent to child write to child stdin so parent can write on child stdin 
    dup2(wp[0], STDIN_FILENO); close(wp[0]);
    //Redirect the parent to child read to child stdout so parent can read on child stdout
    dup2(rp[1], STDOUT_FILENO); close(rp[1]);
    fd = open("/dev/null",O_WRONLY | O_CREAT | O_CLOEXEC, 0666);   // open the file /dev/null
    dup2(fd, STDERR_FILENO); close(fd);  //redirecting stderr to /dev/null

    if(rte->terminal)
      setenv("DISPLAY", display, 1);
 
    if(user && user[0])
      execlp(cmd, name, "-user", user, "-model", "2", "-script", NULL);
    else
      execlp(cmd, name, "-xrm", xrm, "-model", "2", "-script" , NULL);
 
    NSDL1_RTE(vptr, NULL, "RTE: startup command execution failed");
    NSTL1(vptr, NULL,"RTE: startup command execution failed");
    return -1;
  }
  else
  {
    NSDL2_RTE(vptr, NULL,"In Parent");
    //Close the child to parent write pipe 
    close(wp[0]);
    //Close the child to parent read pipe 
    close(rp[1]);
    //Write on parent to child write (rte->rp[0]) 
    //Read on  parent to child read (rte->wp[1])
    vptr->wp[0] = wp[0];
    vptr->wp[1] = wp[1];
    vptr->rp[0] = rp[0];
    vptr->rp[1] = rp[1];
  }

  //Connect
  sprintf(cmd,"Connect(%s)\n",host);
  if (ns_rte_x3270_write(vptr->rp[0], vptr->wp[1], cmd, strlen(cmd)) < 0)
  {
    NSDL2_RTE(vptr, NULL, "Failed to Connect to %s", host);
    NSTL1(vptr, NULL, "Failed to Connect to %s", host);
    return -1;
  }

  //If user and password are provided
  if(user && user[0] && password && password[0])
  {
    NSDL2_RTE(vptr, NULL, "Username = %s, password = %s", user, password);
    //Wait for InputFiled 
    sprintf(cmd,"Wait(\"InputField\")\n");
    if (ns_rte_x3270_write(vptr->rp[0], vptr->wp[1], cmd, strlen(cmd)) < 0)
    {
      NSDL2_RTE(vptr, NULL, "ns_rte_x3270_write failed for Wait(InputField)");
      NSTL1(vptr, NULL, "ns_rte_x3270_write failed");
      return -1;
    }

    //Enter Password.
    NSDL2_RTE(vptr, NULL, "Entring password on Terminal");
    sprintf(cmd, "String(\"%s\")\n", password);
    if (ns_rte_x3270_write(vptr->rp[0], vptr->wp[1], cmd, strlen(cmd)) < 0)
    {
      NSDL2_RTE(vptr, NULL, "ns_rte_x3270_write failed while writing password");
     NSTL1(vptr, NULL, "ns_rte_x3270_write failed");
    return -1;
    }
    
    //Enter Enter() 
    sprintf(cmd,"Enter()\n");
    if (ns_rte_x3270_write(vptr->rp[0], vptr->wp[1], cmd, strlen(cmd)) < 0)
    {
      NSTL1(vptr,NULL,"ns_rte_x3270_write failed while entring Enter()");
      return -1;
    }
  }
  return 0;
}

/*ns_rte_3270_disconnect*/
int ns_rte_3270_disconnect(ns_rte *rte)
{
  char *cmd = "Disconnect()\n";
  VUser *vptr = TLS_GET_VPTR();
  
  int rp = vptr->rp[0];
  int wp = vptr->wp[1];

  NSDL3_RTE(vptr,NULL, "Method called");

  if(!rte)
  {
    NSTL1(vptr,NULL,"NULL input received");
    return -1;
  }
  if (ns_rte_x3270_write(rp, wp, cmd, strlen(cmd)) < 0)
  {
    NSTL1(vptr,NULL,"ns_rte_x3270_write failed");
    return -1;
  }
  close(wp);
  close(rp);
  return 0;
}

/*ns_rte_3270_send_text*/
int ns_rte_3270_send_text(ns_rte *rte , char *text)
{
  char cmd[MAX_RTE_CMD_LENGTH];
  char *textPtr;
  int i, key, rp, wp; 
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_RTE(vptr, NULL, "Method called, text = %s", text);

  if(!rte || !text || !text[0])
  {
    NSTL1(vptr, NULL,"NULL input received");
    return -1;
  }
  rp = vptr->rp[0];
  wp = vptr->wp[1];
  //Check for paramterers
  if(text[0] == '{')
  {
    textPtr = ns_eval_string(text);
  }
  else
  {
    textPtr = text;
  }
  NSDL4_RTE(vptr,NULL, "textPtr = %s", textPtr);
  while(*textPtr)
  {
    key = -1;
    //Check for <key>
    if (*textPtr == '<')
    {
      if(strchr(textPtr,'>'))
      {
        for(i=0;i<total_key_map_entires;i++)
        {
          if(!strncmp(textPtr,keyMap[i].name,keyMap[i].len))
          { 
            key = i;
            break;
          }
        }
      }
    }
    //Input is string
    if(key<0)
    {
      sprintf(cmd,"String(\"%c\")\n",*textPtr);
      if (ns_rte_x3270_write(rp, wp, cmd, strlen(cmd)) < 0)
      {
        NSTL1(vptr,NULL,"ns_rte_x3270_write failed");
        return -1;
      }
      //Wait to some time(in milli second) to get the text on screen.
      RTE_THINK_TIME(vptr,rte, 100); //need 0.1 second think time to see a small pause between keystrocks.
      textPtr++;
    }
    //Input is key
    else
    {
      sprintf(cmd,"%s\n",keyMap[key].cmd);
      if (ns_rte_x3270_write(rp, wp, cmd, strlen(cmd)) < 0)
      {
        NSTL1(vptr,NULL,"ns_rte_x3270_write failed");
        return -1;
      }
      textPtr += keyMap[key].len;
    }
  }
  return 0;
}


/*ns_rte_3270_wait_sync*/
int ns_rte_3270_wait_sync(ns_rte *rte)
{
  char cmd[MAX_RTE_CMD_LENGTH];
  VUser *vptr = TLS_GET_VPTR();

  int rp = vptr->rp[0];
  int wp = vptr->wp[1];  

  NSDL3_RTE(vptr,NULL, "Method called");

  if(!rte)
  {
    NSTL1(vptr,NULL,"NULL input received");
    return -1;
  }
  //Wait to syncronize the input 
  sprintf(cmd,"Wait(\"InputField\")\n");
  if (ns_rte_x3270_write(rp, wp, cmd, strlen(cmd)) < 0)
  {
    NSTL1(vptr,NULL,"ns_rte_x3270_write failed");
    return -1;
  }
  return 0;
}

/*ns_rte_3270_wait_text*/
int ns_rte_3270_wait_text(ns_rte *rte, char *text , int duration)
{
  char cmd[MAX_RTE_CMD_LENGTH];
  VUser *vptr = TLS_GET_VPTR();

  int rp = vptr->rp[0];
  int wp = vptr->wp[1]; 

  NSDL3_RTE(vptr,NULL, "Method called, text = %s, duration = %d", text, duration);
  if(!rte || !text || !text[0])
  {
    NSTL1(vptr,NULL,"NULL input received");
    return -1;
  }
  //Expect Command to wait for a text for given duration
  sprintf(cmd,"Expect(\"%s\",%d)\n",text,duration);
  if (ns_rte_x3270_write(rp, wp, cmd, strlen(cmd)) < 0)
  {
    NSTL1(vptr,NULL,"ns_rte_x3270_write failed");
    return -1;
  }
  return 0;
}

int ns_rte_3270_login(ns_rte *rte)
{
  return 0;
}

int ns_rte_3270_config(ns_rte *rte, char *input)
{
  return 0;
}
