#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *template="pronto.template";
char *ctl_config="ctl_config";
char *outfile="pronto5.test.conf";
int r_flag=0, c_flag=0;
FILE *fp=NULL, *fpo=NULL, *fpt;
int num_controllers;
int num_custport_per_controller = 1;
int num_users_per_controller = 1;
int init=1;
int c_ctl = 1;
char start_nasid[128];

struct count_charac_t {
  char loc[64];
  char speed[64];
  int pct;
};

struct count_charac_t count_charac[100];
int charac_count_num = 0;

int next_nasid(char *nas_id, char *next_nasid) {
int byte[6]; 
int num_bytes=0;
char *ptr;
int cary=0;
int i;
	
	ptr = strtok(nas_id, ":"); 
	while (ptr) {
	    byte[num_bytes] = strtol(ptr, NULL, 16);
	    if ((byte[num_bytes] > 255) || (byte[num_bytes] < 0))
		return 1;
	    num_bytes++;
	    ptr = strtok(NULL, ":");
	}
	if (num_bytes != 6)
	    return 1;
	
	for (i=5; i >=0 ; i--) {
		cary = (byte[i] + 1)/256;
		byte[i] = (byte[i] + 1)%256;
		if (!cary) break;
	}
	sprintf (next_nasid, "%02x:%02x:%02x:%02x:%02x:%02x", byte[0], byte[1], byte[2], byte[3], byte[4], byte[5]);
 	return 0;	
}

int check_nasid(char *nas) {
int byte[6]; 
int num_bytes=0;
char *ptr;
//int cary=0;
//int i;
char nas_id[128];
	
	strcpy(nas_id, nas);
	ptr = strtok(nas_id, ":"); 
	while (ptr) {
	    byte[num_bytes] = strtol(ptr, NULL, 16);
	    if ((byte[num_bytes] > 255) || (byte[num_bytes] < 0))
		return 1;
	    num_bytes++;
	    ptr = strtok(NULL, ":");
	}
	if (num_bytes != 6)
	    return 1;
	
 	return 0;	
}

void
insert_template(void) {
  FILE* fpt;
  char buf[2048];
  int i, j;
  int num_cont;
  int total_used_cont = 0;
  int total_pct = 0;
  char nas1[128], nas2[128];

  strcpy(nas1, start_nasid);

  if ((r_flag && c_flag) == 0) {
    printf("The START_NASID and NUM_CONTROLLERS keywords are required\n");
    exit(1);
  }
  
  //Copy  the template
  fpt = fopen(template, "r");
  if (fpt == NULL) {
    printf("Unable to open template config\n");
    exit(1);
  }
  while (fgets(buf, 2048, fpt))
    fputs(buf,fpo);
  fclose(fpt);

  if (charac_count_num) {
    for (i = 0; i < charac_count_num; i++) {
      num_cont = num_controllers * count_charac[i].pct / 100;
      for (j = 0; j < num_cont; j++) {
	fprintf(fpo, "\nUPAL %s %s %s 100\n", nas1, count_charac[i].speed, count_charac[i].loc);
	fprintf(fpo, "SG H%d %s testheart 1\n", c_ctl, nas1);
	fprintf(fpo, "SG I%d %s testinit 1\n", c_ctl, nas1);
	fprintf(fpo, "SG L%d %s auth %d\n", c_ctl, nas1, num_users_per_controller);
	fprintf(fpo, "SG S%d %s async 1\n", c_ctl, nas1);
	//fprintf(fpo, "SG C%d %s CustPortal %d\n", c_ctl, nas1, num_custport_per_controller);
	c_ctl++;
	if (next_nasid(nas1, nas2)) {
		printf("nasid %s is not in proper format\n", nas1);
		exit (1);
	}
	strcpy(nas1, nas2);
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
	fprintf(fpo, "\nUPAL %s %s %s 100\n", nas1, count_charac[charac_count_num-1].speed, count_charac[charac_count_num-1].loc);
	fprintf(fpo, "SG H%d %s testheart 1\n", c_ctl, nas1);
	fprintf(fpo, "SG I%d %s testinit 1\n", c_ctl, nas1);
	fprintf(fpo, "SG L%d %s auth %d\n", c_ctl, nas1, num_users_per_controller);
	fprintf(fpo, "SG S%d %s async 1\n", c_ctl, nas1);
	//fprintf(fpo, "SG C%d %s CustPortal %d\n", c_ctl, nas1, num_custport_per_controller);
	c_ctl++;
	if (next_nasid(nas1, nas2)) {
		printf("nasid %s is not in proper format\n", nas1);
		exit (1);
	}
	strcpy(nas1, nas2);
      }
    }
  } else {
    for (i = 1; i <= num_controllers; i++) {
      fprintf(fpo, "\nUPAL %s T1 Chicago 100\n", nas1);
      fprintf(fpo, "SG H%d %s testheart 1\n", i, nas1);
      fprintf(fpo, "SG I%d %s testinit 1\n", i, nas1);
      fprintf(fpo, "SG L%d %s auth %d\n", i, nas1, num_users_per_controller);
      fprintf(fpo, "SG S%d %s async 1\n", i, nas1);
      //fprintf(fpo, "SG C%d %s CustPortal %d\n", i, nas1, num_custport_per_controller);
      if (next_nasid(nas1, nas2)) {
		printf("nasid %s is not in proper format\n", nas1);
		exit (1);
	}
      strcpy(nas1, nas2);
    }
  }

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
// int time, unit;
// int wan_env;
 int controller_keyword = 0;
 //int num_controller_keyword = 0;
 int pct;

//char abuf[128];

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

	while (fgets (buf, 2048, fp)) {
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
		if (strcasecmp(keyword, "START_NASID") == 0) {
		   strcpy(start_nasid, val1);
		   if (check_nasid(start_nasid)) {
			printf("START_NASID <%s> not in proper format\n", val1);
			exit(1);
		   }
		   r_flag =1;
		} else if (strcasecmp(keyword, "NUM_CONTROLLERS") == 0) {

		  if (controller_keyword) {
		    printf("can't have both NUM_CONTROLLERS and CONTROLLER keyword in the same config file\n");
		    exit(-1);
		  }

		  //num_controller_keyword = 1;

		  num_controllers = atoi(val1);
		  if (num_controllers <= 0) {
		    printf("NUM_CONTROLLERS entries must be greater than 0\n");
		    exit(1);
		  }
		  c_flag = 1;
		} else if (strcasecmp(keyword, "NUM_USERS_PER_CONTROLLER") == 0) {

		  //num_controller_keyword = 1;

		  if (num != 2) {
		    printf("NUM_USERS_PER_CONTROLLER MUST have 1 value\n");
		    exit(1);
		  }
		  num_users_per_controller = atoi(val1);
		} else if (strcasecmp(keyword, "NUM_CUSTPORT_PER_CONTROLLER") == 0) {
		  
		  //num_controller_keyword = 1;

		  if (num != 2) {
		    printf("NUM_CUSTPORT_PER_CONTROLLER MUST have 2 value\n");
		    exit(1);
		  }
		  num_custport_per_controller = atoi(val1);
		} else if (strcasecmp(keyword, "CONTROLLER") == 0) {
		  
		  controller_keyword = 1;

		  if (num != 4) {
		    printf("CONTROLLER MUST have 3 values\n"); 
		    exit(1);
		  }
		  if (strcasecmp(val1, "T1") &&
		      strcasecmp(val1, "384K-DSL") &&
		      strcasecmp(val1, "1M-DSL") &&
		      strcasecmp(val1, "2.5M-DSL"))  {
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
