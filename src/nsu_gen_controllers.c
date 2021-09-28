#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *template="pronto.template";
char *ctl_config="ctl_config";
char *outfile="pronto5.test.conf";
int hb_time = 60000;
int init_time = 7200000;
int sync_time = 600000;
int custport_time = 345600000;
int update_time = 300000;
int loginlogout_time = 10800000;
int r_flag=0, c_flag=0, wan_flag=0, server_flag = 0;
FILE *fp=NULL, *fpo=NULL, *fpt;
char ip_address[64];
int num_controllers;
int num_custport_per_controller = 1;
int num_users_per_controller = 1;
int init=1;
int num_admin_noc = 3, num_admin_wisps = 2, num_admin_accmgnt = 2;
int logging = 1;
int c_ctl = 1;

struct count_charac_t {
  char loc[16];
  char speed[16];
  int pct;
};

struct count_charac_t count_charac[100];
int charac_count_num = 0;

void
insert_template(void) {
  FILE* fpt;
  char buf[2048];
  int i, j;
  int num_cont;
  int total_used_cont = 0;
  int total_pct = 0;

  if ((r_flag && c_flag && wan_flag) == 0) {
    printf("The RUN_TIME, NUM_CUSTOMERS and WAN_EMULATION keywords are all required\n");
    exit(1);
  }
  
  fprintf(fpo, "SG_PACING_INTERVAL HeartBeat1 1 0 0 0 %d\n", hb_time);
  fprintf(fpo, "SG_PACING_INTERVAL Init 2 0 0 1 %d\n", init_time);
  fprintf(fpo, "SG_PACING_INTERVAL Login_Logout1 2 0 0 1 %d\n", loginlogout_time);
  fprintf(fpo, "SG_PACING_INTERVAL Sync 1 0 0 0 %d\n", sync_time);
  fprintf(fpo, "SG_PACING_INTERVAL CustPortal 2 0 0 1 %d\n", custport_time);
  fprintf(fpo, "PAGE_THINK_TIME Login_Logout1 update 2 %d\n", update_time);

  if (server_flag)
  fprintf(fpo, "SERVER_HOST engoss.prontonetworks.com %s SanFrancisco\n", ip_address);
  
  //Copy  the template
  fpt = fopen(template, "r");
  if (fpt == NULL) {
    printf("Unable to open template config\n");
    exit(1);
  }
  while (nslb_fgets(buf, 2048, fpt, 0))
    fputs(buf,fpo);
  fclose(fpt);

  if (charac_count_num) {
    for (i = 0; i < charac_count_num; i++) {
      num_cont = num_controllers * count_charac[i].pct / 100;
      for (j = 0; j < num_cont; j++) {
	fprintf(fpo, "UPAL UserProf%d %s %s 100\n", c_ctl, count_charac[i].speed, count_charac[i].loc);
	fprintf(fpo, "SG H%d UserProf%d HeartBeat1 1 %d\n", c_ctl, c_ctl, c_ctl);
	fprintf(fpo, "SG I%d UserProf%d Init %d %d\n", c_ctl, c_ctl, num_users_per_controller, c_ctl);
	fprintf(fpo, "SG L%d UserProf%d Login_Logout1 1 %d\n", c_ctl, c_ctl, c_ctl);
	fprintf(fpo, "SG S%d UserProf%d Sync 1 %d\n", c_ctl, c_ctl, c_ctl);
	fprintf(fpo, "SG C%d UserProf%d CustPortal 1 %d\n", c_ctl, c_ctl, num_custport_per_controller);
	c_ctl++;
      }
      total_used_cont += num_cont;
      total_pct += count_charac[i].pct;
    }
    
    if (total_pct != 100) {
      printf("controller pct number add up to 100\n");
      exit(-1);
    }

    if (total_used_cont < num_controllers) {
      for (j = total_used_cont; j < num_controllers; j++) {
	fprintf(fpo, "UPAL UserProf%d %s %s 100\n", c_ctl, count_charac[charac_count_num-1].speed, count_charac[charac_count_num-1].loc);
	fprintf(fpo, "SG H%d UserProf%d HeartBeat1 1 %d\n", c_ctl, c_ctl, c_ctl);
	fprintf(fpo, "SG I%d UserProf%d Init %d %d\n", c_ctl, c_ctl, num_users_per_controller, c_ctl);
	fprintf(fpo, "SG L%d UserProf%d Login_Logout1 1 %d\n", c_ctl, c_ctl, c_ctl);
	fprintf(fpo, "SG S%d UserProf%d Sync 1 %d\n", c_ctl, c_ctl, c_ctl);
	fprintf(fpo, "SG C%d UserProf%d CustPortal 1 %d\n", c_ctl, c_ctl, num_custport_per_controller);
	c_ctl++;
      }
    }
  } else {
    for (i = 1; i <= num_controllers; i++) {
      fprintf(fpo, "UPAL UserProf%d T1 Chicago 100\n", i);
      fprintf(fpo, "SG H%d UserProf%d HeartBeat1 1 %d\n", i, i, i);
      fprintf(fpo, "SG I%d UserProf%d Init %d %d\n", i, i, num_users_per_controller, i);
      fprintf(fpo, "SG L%d UserProf%d Login_Logout1 1 %d\n", i, i, i);
      fprintf(fpo, "SG S%d UserProf%d Sync 1 %d\n", i, i, i);
      fprintf(fpo, "SG C%d UserProf%d CustPortal 1 %d\n", i, i, num_custport_per_controller);
    }
  }

  fprintf(fpo, "SG C 1M_DSL_US_ALL CreateController %d 0\n", i++, num_admin_noc);
  fprintf(fpo, "SG W 1M_DSL_US_ALL WISPReport %d 0\n", i++, num_admin_wisps);
  fprintf(fpo, "SG A 1M_DSL_US_ALL CustSearch %d 0\n", i++, num_admin_accmgnt);
  
  init = 0;
}

int main ()
{
FILE *fp=NULL;
char buf[2048];
int num, len;
char keyword[64];
char val1[64];
char val2[64];
char val3[64];
char val4[64];
 int time, unit;
 int wan_env;
 int controller_keyword = 0;
 int num_controller_keyword = 0;
 int pct;

//char abuf[128];

 ip_address[0] = '\0'; 

	fp = fopen(ctl_config, "r");
	if (fp == NULL) {
	  printf("Unable to open ctl_config\n");
	  exit(1);
	}

	fpo = fopen(outfile, "w");
	if (fpo == NULL) {
	  printf("Unable to open the conf file\n");
	  exit(1);
	}

	while (nslb_fgets (buf, 2048, fp, 0)) {
		len = strlen(buf);
		if (buf[len-1] == '\n') {
			buf[len-1] = '\0';
		}
		if (buf[0] == '#')
			continue;
		if (buf[0] == '\0')
			continue;

		num = sscanf(buf, "%s %s %s %s %s", keyword, val1, val2, val3, val4);
		if (num < 2) continue;
		if (strcasecmp(keyword, "RUN_TIME") == 0) {
		  if (num != 3) {
		    printf("RUN_TIME MUST have 2 values\n"); 
		    exit(1);
		  }
		  time = atoi(val1);
		  switch(val2[0]) {
		  case 'S':
		  case 's':
		    unit =1;
		    break;
		  case 'M':
		  case 'm':
		    unit =2;
		    break;
		  case 'H':
		  case 'h':
		    unit =3;
		    break;
		  default:
		    printf("RUN_TIME: Time unit could be S, M or H\n");
		    exit(1);
		  }
		  r_flag = 1;
		  if (unit == 1)
		    fprintf(fpo, "RUN_TIME %d S\n", time);
		  else if (unit == 2)
		    fprintf(fpo, "RUN_TIME %d M\n", time);
		  else
		    fprintf(fpo, "RUN_TIME %d H\n", time);
		} else if (strcasecmp(keyword, "NUM_CONTROLLERS") == 0) {

		  if (controller_keyword) {
		    printf("can't have both NUM_CONTROLLERS and CONTROLLER keyword in the same config file\n");
		    exit(-1);
		  }

		  num_controller_keyword = 1;

		  num_controllers = atoi(val1);
		  if (num_controllers <= 0) {
		    printf("NUM_CONTROLLERS entries must be greater than 0\n");
		    exit(1);
		  }
		  c_flag = 1;
		} else if (strcasecmp(keyword, "WAN_EMULATION") == 0) {
		  wan_env = atoi(val1);
		  if ((wan_env != 0) && (wan_env != 1)) {
		    printf("WAN_EMULATION must be 0 or 1\n");
		    exit(1);
		  }
		  wan_flag = 1;
		  fprintf(fpo, "WAN_ENV %d\n", wan_env);
#if 0
		} else if (strcasecmp(keyword, "LOGGING") == 0) {
		  logging = atoi(val1);
		  if ((logging != 1) && (logging != 0)) {
		    printf("LOGGING can be only 1 or 0\n");
		    exit(1);
		  }
		  if (logging)
		    fprintf(fpo, "LOGGING 3 2 7 2\n");
#endif
		} else if (strcasecmp(keyword, "HB_TIME") == 0) {

		  if (controller_keyword) {
		    printf("HB_TIME must be defined before the CONTROLLER keyword\n");
		    exit(-1);
		  }

		  if (num != 2) {
		    printf("HB_TIME MUST have 1 values\n"); 
		    exit(1);
		  }
		  hb_time = atoi(val1);
		} else if (strcasecmp(keyword, "SYNC_TIME") == 0) {
		  if (controller_keyword) {
		    printf("SYNC_TIME must be defined before the CONTROLLER keyword\n");
		    exit(-1);
		  }

		  if (num != 2) {
		    printf("SYNC_TIME MUST have 1 values\n"); 
		    exit(1);
		  }
		  sync_time = atoi(val1);
		} else if (strcasecmp(keyword, "INIT_TIME") == 0) {
		  if (controller_keyword) {
		    printf("INIT_TIME must be defined before the CONTROLLER keyword\n");
		    exit(-1);
		  }

		  if (num != 2) {
		    printf("INIT_TIME MUST have 1 values\n"); 
		    exit(1);
		  }
		  init_time = atoi(val1);
		} else if (strcasecmp(keyword, "SERVER") == 0) {
		  if (num != 2) {
		    printf("SERVER MUST have 1 value\n");
		    exit(1);
		  }
		  strcpy(ip_address, val1);
		  server_flag = 1;
		} else if (strcasecmp(keyword, "NUM_USERS_PER_CONTROLLER") == 0) {

		  num_controller_keyword = 1;

		  if (num != 2) {
		    printf("NUM_USERS_PER_CONTROLLER MUST have 1 value\n");
		    exit(1);
		  }
		  num_users_per_controller = atoi(val1);
		} else if (strcasecmp(keyword, "NUM_CUSTPORT_PER_CONTROLLER") == 0) {
		  
		  num_controller_keyword = 1;

		  if (num != 2) {
		    printf("NUM_CUSTPORT_PER_CONTROLLER MUST have 2 value\n");
		    exit(1);
		  }
		  num_custport_per_controller = atoi(val1);
		} else if (strcasecmp(keyword, "ADMIN_WISPS") == 0) {
		  if (num != 2) {
		    printf("ADMIN_WISPS MUST have 1 value\n"); 
		    exit(1);
		  }
		  num_admin_wisps = atoi(val1);
		} else if (strcasecmp(keyword, "ADMIN_NOCREPORTS") == 0) {
		  if (num != 2) {
		    printf("ADMIN_NOCREPORTS MUST have 1 value\n"); 
		    exit(1);
		  }
		  num_admin_noc = atoi(val1);
		} else if (strcasecmp(keyword, "ADMIN_ACC_MNGT") == 0) {
		  if (num != 2) {
		    printf("ADMIN_SEARCHCUST MUST have 1 values\n"); 
		    exit(1);
		  }
		  num_admin_accmgnt = atoi(val1);
		} else if (strcasecmp(keyword, "CONTROLLER") == 0) {
		  
		  controller_keyword = 1;

		  if (num != 4) {
		    printf("CONTROLLER MUST have 3 values\n"); 
		    exit(1);
		  }
		  if (strcasecmp(val1, "T1") &&
		      strcasecmp(val1, "384K_DSL") &&
		      strcasecmp(val1, "1M_DSL") &&
		      strcasecmp(val1, "2.5M_DSL"))  {
		    printf("CONTROLLER Entry may have  T1|384K-DSL|1M-DSL|2.5M-DSL as the Internel Access values\n");
		    exit(1);
		  }
		  if (strcasecmp(val2, "NewYork") &&
		      strcasecmp(val2, "SanFrancisco") &&
		      strcasecmp(val2, "Chicago") &&
		      strcasecmp(val2, "London") &&
		      strcasecmp(val2, "Shanghai")) {
		    printf("CONTROLLER Entry may have  NewYork|SanFrancisco|Chicago|Shanghai|London as the Location values\n");
		    exit(1);
		  }
		  pct = atoi(val3);
		  if ((pct < 0) || (pct > 100)) {
		    printf("CONTROLLER pct can only be between 0-100\n");
		    exit(-1);
		  }

		  strcpy(count_charac[charac_count_num].speed, val1);
		  strcpy(count_charac[charac_count_num].loc, val2);
		  count_charac[charac_count_num].pct = pct;
		  charac_count_num++;
		}
	}

	if (init)
	  insert_template();

	if (fpo) fclose(fpo);
	if (fp) fclose(fp);

	return 0;
}
