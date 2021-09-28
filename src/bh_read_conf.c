/******************************************************************

 * Name    :    read_input.c
 * Author  :    Abhishek
 * Purpose :   The following comment below are added to describe the purpose of using this file
               This file will take two file as input one, one is keyword file and secong is area.txt
 * Uage:  read_input [FileName1 FileName2]

 * Modification History:
 *  23/09/06 : Abhishek - Initial Version


* COMMENT START**************************************************
*****************************************************************
*****************************************************************
read_input.c

will read a data file passed as its argument.

Format of input data File, It is similar to netstorm config file.
Each line has a keyword and 1 and more values values followed by it.
Igonore all lines starting with #
Ignore all blank lines

It will have following keywords:

1)

DEVICE_DIST_PCT_OVER_USERS  50 1

This keyword can repeat and will have two values.

create a dynamic sized table with initail size of 5. row will be
tyepdef struct {
int pct;
int num;
} A2BDist;

A2BDist *d2uDist;
int total_d2u_entries; //contains the mumber of entries;

First values goes into pct and second goes into num.
First calue can be between 1-100,
second value is a postive number
Second value can be a range also. Like 2-9.
In that case, this single entry is equivalent to multiple entries.
Like
DEVICE_DIST_PCT_OVER_USERS  5 2-4
is same as
DEVICE_DIST_PCT_OVER_USERS  5 2
DEVICE_DIST_PCT_OVER_USERS  5 3
DEVICE_DIST_PCT_OVER_USERS  5 4

In that case, multiple entries in data table.

Constraints: Make sure sum of pct is 100.
And Second number should be unique if there are multiple
entreies,
For example
DEVICE_DIST_PCT_OVER_USERS  5 2-4
DEVICE_DIST_PCT_OVER_USERS  50 2

are worng as 2 shows in two entries,

if contraint fails. give error and exit.
While reading the keyword, just just dump in the table.
After whole file is read, apply checks.


2)
USER_DIST_PCT_OVER_DEVICES  50 1

Very much line last. It can repeat. Row will be of same type.

tyepdef struct {
int pct;
int num;
} A2BDist;

A2BDist *u2dDist;
int total_u2d_entries; //contains the mumber of entries;

Make sure sum of pct is 100.

3)
DEVICE_DIST_PCT_OVER_IP  50 1

Very much line last. It can repeat. Row will be of same type.

tyepdef struct {
int pct;
int num;
} A2BDist;

A2BDist *d2iDist;
int total_d2i_entries; //contains the mumber of entries;

Make sure sum of pct is 100.

4)
IP_DIST_PCT_OVER_DEVICES 50 1

Very much line last. It can repeat. Row will be of same type.

tyepdef struct {
int pct;
int num;
} A2BDist;

A2BDist *i2dDist;
int total_i2d_entries; //contains the mumber of entries;

Make sure sum of pct is 100.

5)GEN_DATA_SIZE DEVICE|USER|IP num

This keyword cannot repeat.
It has two values
First one be DEVICE, USER or IP

Set global variable: driver_count to 0, if DEVICE, 1 if USER and 2 if IP
set total_device, or total_device or total_ip depedning upon first value.

6)
GEOGRAPHIC_USER_DISTRIBUTION USCA 30
This keyword can repeat has two values, first is GeopIP area name and second
is pct,

Fill up the table
typedef struct {
        short acode;
        short pct;
        int cumpct;
} Udist;

Make sure, sum of pct for all entries is 100.
Using area_ip.txt pass on command line as second argument convert area name
to acrea code.
also, fill in cumpct (cumulative percentrage) by adding subsequent % ages.
So, if % are -
10
20
50
20

Cum pct will be
10
30
80
100


-After reading keywords do following calculations:
Sort all table in first 4 keyword on num.

Calculate  D2Usum as the sum of pct's of d2uDist table.
         and D2UWsum is the sum of multiple of pct and num for each row in
d2UDist table.
For example d2uDist Table is
50 1
40 2
10 4

D2Usum is 50+40+10 = 100
D2UWsum = 50*1+40*2+10*4 = 50+80+40 = 170
si,ilarly calculate -
U2DSum U2DWsum D2Isum D2IWsum I2DSum I2DWsum 
******************************************************************
******************************************************************
*COMMENT END******************************************************

   *****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bh_read_conf.h"

#define MAX_DATA_LINE_LENGTH 4096
#define DELTA_DIST_ENTRIES 10

int runningAsTool = 0;
int bh_debug = 0;
A2BDist *d2uDist; // DEVICE_DIST_PCT_OVER_USERS
A2BDist *u2dDist; // USER_DIST_PCT_OVER_DEVICES
A2BDist *d2iDist; // DEVICE_DIST_PCT_OVER_IP
A2BDist *i2dDist; // IP_DIST_PCT_OVER_DEVICES
Udist   *udist;   // GEOGRAPHIC_USER_DISTRIBUTION

int total_d2u_entries = 0; //contains the number of entries disk to user 
int total_u2d_entries = 0; //contains the number of entries user to disk 
int total_d2i_entries = 0; //contains the number of entries disk to ip
int total_i2d_entries = 0; //contains the number of entries ip to disk
int total_u_entries = 0;

int max_d2u_entries = 0; //contains the max entry disk to user
int max_u2d_entries = 0; //contains the max entry user to disk
int max_d2i_entries = 0; //contains the max entry disk to ip
int max_i2d_entries = 0; //contains the max entry ip to disk
int max_u_entries;   //contains the max entry geographic user distribution

int driver_count; // gen_data 
int total_device; // gen_data 
int total_user;   // gen_data
int total_ip;     // gen_data

int d2usum;  // sum of pct's of d2udist table
int u2dsum;  // sum of pct's of u2ddist table
int d2isum;  // sum of pct's of d2idist table
int i2dsum;  // sum of pct's of i2ddist table
int d2uwsum; // sum of multiple of pct and num for each row in d2udist table
int u2dwsum; // sum of multiple of pct and num for each row in u2ddist table
int d2iwsum; // sum of multiple of pct and num for each row in d2idist table
int i2dwsum; // sum of multiple of pct and num for each row in i2ddist table

int g_uname_var_len = 9;
int g_uname_var_type = 0;
char g_uname_pfx[16];
char g_uname_sfx[16];

char read_area_file[MAX_DATA_LINE_LENGTH]; // store file area.txt is passed as second input from command line

void store_keyword_values_to_struct(char * keyword, char *text1, char *text2);
void print_usage(char *s);
int comfuncnum(const void *a, const void *b);
int comfuncunum(const void *a, const void *b);
void load_geo_user_dist(char* keyword, char* text1, char* text2);
int  create_table_entry(int *row_num, int *total, int *max, char **ptr, int size, char *name);
int calc_udist_compct();
int validate_a2bdist(int total, A2BDist *ptr, int *globsum, int *globwsum);
void load_a2bdist(char *text1, char *text2, int *total, int *max, A2BDist **ptr, int size, char *name);
void print_debug(char *msg);



int create_table_entry(int *row_num, int *total, int *max, char **ptr, int size, char *name)
{
  if (bh_debug) printf("row_num = %d, total = %d, max = %d, ptr = %p, size = %d, name = %s\n",
                       *row_num, *total, *max, *ptr, size, name);

  if (*total == *max)
  {
    if (bh_debug) printf("doing realloc\n");
    *ptr = realloc ((char *)*ptr, (*max + DELTA_DIST_ENTRIES) * size);
    if (!*ptr)
    {
      fprintf(stderr, "error in allocating more memory for %s\n", name);
      return -1;
    }
    else *max += DELTA_DIST_ENTRIES;
  }
  *row_num = (*total)++;
  if (bh_debug) printf("row_num = %d, total = %d, max = %d, ptr = %p, size = %d, name = %s\n",
                       *row_num, *total, *max, *ptr, size, name);
  return 0;
}

/**
void create_u_table_entry(int *row_num_u)
{
  if (total_u_entries == max_u_entries)
  {
    udist = (udist *) realloc ((char *)udist, (max_u_entries + delta_dist_entries) * sizeof(udist));
    if (!udist)
    {
      fprintf(stderr,"create_u_table_entry(): error allocating more memory for user entries\n");
      exit(1);
    }
    else max_u_entries += delta_dist_entries;
  }
  *row_num_u = total_u_entries++;
}
**/

//For all D-U and D-I distri, eentry for 1 must exist.
//If not add with num=0, pct=0
void 
add_default_entry_if_needed()
{
  if (d2uDist[0].num != 1) {
    load_a2bdist("0", "1", &total_d2u_entries, &max_d2u_entries, &d2uDist, sizeof(A2BDist), "PCT_DEVICE_DIST_OVER_USERS");
    qsort(d2uDist, total_d2u_entries, sizeof(A2BDist), comfuncnum);
  }
  if (u2dDist[0].num != 1) {
    load_a2bdist("0", "1", &total_u2d_entries, &max_u2d_entries, &u2dDist, sizeof(A2BDist), "PCT_USER_DIST_OVER_DEVICES");
    qsort(u2dDist, total_u2d_entries, sizeof(A2BDist), comfuncnum);
  }
  if (d2iDist[0].num != 1) {
    load_a2bdist("0", "1", &total_d2i_entries, &max_d2i_entries, &d2iDist, sizeof(A2BDist), "PCT_DEVICE_DIST_OVER_IP");
    qsort(d2iDist, total_d2i_entries, sizeof(A2BDist), comfuncnum);
  }
  if (i2dDist[0].num != 1) {
    load_a2bdist("0", "1", &total_i2d_entries, &max_i2d_entries, &i2dDist, sizeof(A2BDist), "PCT_IP_DIST_OVER_DEVICES");
    qsort(i2dDist, total_i2d_entries, sizeof(A2BDist), comfuncnum);
  }
}

void 
bh_read_file(char *read_keyword_file, char *area_file)
{
  FILE *keyword_fp;
  //char *tok;
  
  char buf[MAX_DATA_LINE_LENGTH];
  //char cbuf[MAX_DATA_LINE_LENGTH];
  char text1[MAX_DATA_LINE_LENGTH];
  char text2[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];

  int num, ret;
  
  strcpy(read_area_file, area_file);
  if(runningAsTool)
    if (bh_debug) printf("opening %s\n", read_keyword_file);

  keyword_fp = fopen (read_keyword_file, "r");

  if (keyword_fp == NULL )
  {
    if (bh_debug) printf("unable to open file %s\n", read_keyword_file);
    exit(1);
  }

  if(runningAsTool)
    printf("reading %s\n", read_keyword_file);

  //cbuf[0] = '\0';

  while(fgets(buf, 4096, keyword_fp) != NULL)
  {
    if ((num = sscanf(buf, "%s %s %s", keyword, text1, text2)) < 2)
      continue;
    else if(strchr(buf, '#'))
      continue;
    else
      store_keyword_values_to_struct(keyword, text1, text2);
  }
  fclose(keyword_fp);
  
  qsort(d2uDist, total_d2u_entries, sizeof(A2BDist), comfuncnum);
  qsort(u2dDist, total_u2d_entries, sizeof(A2BDist), comfuncnum);
  qsort(d2iDist, total_d2i_entries, sizeof(A2BDist), comfuncnum);
  qsort(i2dDist, total_i2d_entries, sizeof(A2BDist), comfuncnum);

  add_default_entry_if_needed();

  qsort(udist, total_u_entries, sizeof(Udist), comfuncunum);
  
  ret = validate_a2bdist(total_d2u_entries, d2uDist, &d2usum, &d2uwsum);
  if(ret == -1) {
      printf("table d2udist contain the duplicate num values !\n");
      exit(-1);
  } else if(ret == 1) {
      printf("table d2udist contain percentage not 100 !\n");
      exit(1);
  }

  ret = validate_a2bdist(total_u2d_entries, u2dDist, &u2dsum, &u2dwsum);
  if(ret == -1) {
      printf("table u2ddist contain the duplicate num values !\n");
      exit(-1);
  } else if(ret == 1) {
      printf("table u2ddist contain percentage not 100 !\n");
      exit(1);
  }
  
  ret = validate_a2bdist(total_d2i_entries, d2iDist, &d2isum, &d2iwsum);
  if(ret == -1) {
      printf("table d2idist contain the duplicate num values !\n");
      exit(-1);
  } else if(ret == 1) {
      printf("table d2idist contain percentage not 100 !\n");
      exit(1);
  }
  
  ret = validate_a2bdist(total_i2d_entries, i2dDist, &i2dsum, &i2dwsum);
  if(ret == -1) {
      printf("table i2ddist contain the duplicate num values !\n");
      exit(-1);
  } else if(ret == 1) {
      printf("table i2ddist contain percentage not 100 !\n");
      exit(1);
  }
  
  ret = calc_udist_compct();
  if(ret == -1) {
      printf("table udist contain percentage not  100 !\n");
      exit(1);
  }
}

int calc_udist_compct() 
{
  int i;
  int sumcompct = 0;

  for(i = 0; i < total_u_entries; i++) 
  {
      sumcompct += udist[i].pct;
      udist[i].cumpct = sumcompct;  
      //printf("udist pct %d - cumpct %d\n",udist[i].pct,udist[i].cumpct);
  }

  if(sumcompct !=  100) 
  {
    return -1;
  }

  return 0;
}

int validate_a2bdist(int total, A2BDist *ptr, int *globsum, int *globwsum) 
{
  int i;
  int last = -1;

  //printf("checking dulpicate\n");

  *globsum = 0;
  *globwsum = 0;
  for(i = 0; i < total; i++)
  {
    if(last == ptr[i].num)
        return -1;
    else
	last = ptr[i].num;

    *globsum = *globsum + ptr[i].pct;
    *globwsum = *globwsum + (ptr[i].pct*ptr[i].num);
   // printf("d2udist pct %d - num %d\n",d2udist[i].pct,d2udist[i].num);
  }
  //printf("sum of d2udist %d - pct*num %d\n",d2usum,d2uwsum);
  if(*globsum != 100) 
  {
    return 1;
  }
  return 0;
}

void load_a2bdist(char *text1, char *text2, int *total, int *max, A2BDist **ptr, int size, char *name)
{
  //A2BDist *ptr1 = ptr;  
  int start, end, num;
  int pct = atoi(text1);
  int row_num;

  if ((pct < 0 ) || (pct > 100)) {
    printf("pct value %d for %s should between 0-100 inclusive \n", pct, name);
    exit (1);
  }

  if(strchr(text2, '-'))
  {
    char* tokd2u;
    tokd2u = strtok(text2, "-");
    start = atoi(tokd2u);
    tokd2u = strtok(NULL, "-");
    end = atoi(tokd2u);
  }
  else
  {
    start = end = atoi(text2);
  }

  if (start > end)
  {
    printf("start num value %d for %s is greater than end num %d value\n", start, name, end);
    exit (1);
  }

  if (start <= 0) {
    printf("start num value %d for %s is should be a postive value\n", start, name);
    exit (1);
  }

  if (end <= 0) {
    printf("end num value %d for %s is should be a postive value\n", end, name);
    exit (1);
  }

  for(num = start; num <= end; num++)
  {
    if(runningAsTool)
      printf("loading %s, pct = %d, num = %d\n", name, pct, num);
    create_table_entry(&row_num, total, max, (char **) ptr, size, name);
    A2BDist *tmpptr = *ptr;
    tmpptr += row_num;
    //if (bh_debug) printf("ptr = %x, tmpptr = %x\n", *ptr, tmpptr);
 
    tmpptr->pct = pct;
    tmpptr->num = num;
  }
}

void store_keyword_values_to_struct(char *keyword, char *text1, char *text2) 
{
  //printf ("processing %s\n", keyword);
  if (strcasecmp(keyword, "PCT_DEVICE_DIST_OVER_USERS") == 0) {
    load_a2bdist(text1, text2, &total_d2u_entries, &max_d2u_entries, &d2uDist, sizeof(A2BDist), keyword);
  } else if(strcasecmp(keyword, "PCT_USER_DIST_OVER_DEVICES") == 0) {
    load_a2bdist(text1, text2, &total_u2d_entries, &max_u2d_entries, &u2dDist, sizeof(A2BDist), keyword);
  } else if(strcasecmp(keyword, "PCT_DEVICE_DIST_OVER_IP") == 0) {
    load_a2bdist(text1, text2, &total_d2i_entries, &max_d2i_entries, &d2iDist, sizeof(A2BDist), keyword);
  } else if(strcasecmp(keyword, "PCT_IP_DIST_OVER_DEVICES") == 0) {
    load_a2bdist(text1, text2, &total_i2d_entries, &max_i2d_entries, &i2dDist, sizeof(A2BDist), keyword);
  } else if(strcasecmp(keyword, "GEN_DATA_SIZE") == 0) {
    if(strcasecmp(text1, "DEVICE")== 0) {
      driver_count = 1;
      total_device = atoi(text2);
    } else if(strcasecmp(text1, "USER") == 0) {
      driver_count = 2;
      total_user = atoi(text2);
    } else if(strcasecmp(text1, "IP") == 0) {
      driver_count = 3;
      total_ip = atoi(text2);
    }
  } else if(strcasecmp(keyword, "USER_GEO_DIST") == 0) {
    load_geo_user_dist(keyword, text1, text2); 
  } else if(strcasecmp(keyword, "DEBUG") == 0) {
	bh_debug = atoi(text1);
  } else if(strcasecmp(keyword, "USER_NAME_PFX") == 0) {
	//printf ("g_uname_pfx = %s\n", text1);
	strncpy(g_uname_pfx, text1, 16);	
	g_uname_pfx[15] = 0;
  } else if(strcasecmp(keyword, "USER_NAME_SFX") == 0) {
	strncpy(g_uname_sfx, text1, 16);	
	g_uname_sfx[15] = 0;
  } else if(strcasecmp(keyword, "USER_NAME_VAR_TYPE") == 0) {
        if(strcasecmp(text1, "D")== 0)
	    g_uname_var_type = 0;
        else if(strcasecmp(text1, "X")== 0)
	    g_uname_var_type = 1;
  } else if(strcasecmp(keyword, "USER_NAME_VAR_LEN") == 0) {
	g_uname_var_len = atoi(text1);
	if ((g_uname_var_len < 1) || (g_uname_var_len > 31))  {
	    g_uname_var_len = 9;
	    printf ("Ignoring User Name variable part length spec. should betwen 0 and 31\n");
	}
  }
}


void load_geo_user_dist(char* keyword, char* text1, char* text2) 
{
  FILE *fp_area_file;
  char *tok;
  char buf[4096];
  int tempareaid;
  //char* tempareacode;
  //printf("opening area.txt\n");

  fp_area_file = fopen (read_area_file, "r");

  if (fp_area_file == NULL )
  {
    printf("unable to open file %s\n", read_area_file);
    exit(1);
  }

  while (fgets(buf, 4096, fp_area_file)) 
  {
    //printf("area data %s\n", buf);
    //create_user_table_entry(&rnum);
    tok = strtok(buf, ",");
    tempareaid = atoi(tok);
    //printf("area id %d ",tempareaid);
    tok = strtok(NULL, ",");
    tok[strlen(tok) -1] = '\0';
    //printf("2area code %s text=%s\n",tok,text1);
    if(!strcmp(tok, text1)) 
    {
      int row_num;
      //create_u_table_entry(&row_num_u);
      create_table_entry(&row_num, &total_u_entries, &max_u_entries, (char **)&udist, sizeof(udist), keyword);
      udist[row_num].acode = tempareaid;
      udist[row_num].pct = atoi(text2);
      if (bh_debug) printf("adding acode: %hd pct %hd\n", udist[row_num].acode, udist[row_num].pct);
      break;
    }
  }
  fclose(fp_area_file);
}

int comfuncnum(const void *a, const void *b)
{
   return (((A2BDist *)a)->num - ((A2BDist *)b)->num);
}

int comfuncunum(const void *a, const void *b)
{ 
  //printf ("comp %hd %hd acode %hd %hd\n", ((Udist *)a)->pct, ((Udist *)b)->pct, ((Udist *)a)->acode, ((Udist *)b)->acode);
   return ((int)(((Udist *)a)->pct - ((Udist *)b)->pct));
}

void print_usage(char *cmd)
{
  printf("usage: %s <keyword file> <area file>\n", cmd);
  exit(1);
}

void print_debug(char *msg) 
{
int i;
  printf ("%s\n", msg);
  printf("D2Usum %d- U2Dsum %d- I2Dsum %d- D2Isum %d- D2UWsum %d- U2DWsum %d-I2DWsum %d- D2IWsum %d\n", d2usum, u2dsum, i2dsum, d2isum, d2uwsum, u2dwsum, i2dwsum, d2iwsum);
  printf("driver_count %d -total_device %d -total_user %d -total_ip %d\n", driver_count, total_device, total_user, total_ip );

  for(i = 0; i < total_d2u_entries; i++)
    printf("D2U : %d %d\n", d2uDist[i].num, d2uDist[i].pct);
  for(i = 0; i < total_u2d_entries; i++)
    printf("U2D : %d %d\n", u2dDist[i].num, u2dDist[i].pct);
  for(i = 0; i < total_d2i_entries; i++)
    printf("D2I : %d %d\n", d2iDist[i].num, d2iDist[i].pct);
  for(i = 0; i < total_i2d_entries; i++)
    printf("I2D : %d %d\n", i2dDist[i].num, i2dDist[i].pct);

  for(i = 0; i < total_u_entries; i++)
  {
    printf("acode=%hd pct=%hd cumpct=%d\n", udist[i].acode, udist[i].pct, udist[i].cumpct);
  }
}

void
adjust_pct_a2b (int *cur_sum, int *wtsum, int new_sum, int total, A2BDist *a2b) 
{
int left, sum, wsum;
int i;

	sum = 0;
	for (i=0; i < total; i++) {
	    a2b[i].pct =  a2b[i].pct * new_sum / (*cur_sum);
	    sum += a2b[i].pct;
	}

	left = new_sum - sum;
	a2b[0].pct += left;

	sum = 0;
	wsum = 0;
	for (i=0; i < total; i++) {
	    sum += a2b[i].pct;
	    wsum += a2b[i].pct * a2b[i].num;
	}

	*cur_sum = sum;
	*wtsum = wsum;
}

void
adjust_wt_pct_a2b (int *cur_sum, int *wtSum, int new_wt_sum, int total, A2BDist *a2b) 
{
int left, sum, wsum;
int i;
	sum = 0;
	wsum=0;
	for (i=0; i < total; i++) {
	    a2b[i].pct =  a2b[i].pct * new_wt_sum / (*wtSum);
	    wsum += a2b[i].pct * a2b[i].num;
	}

	left = new_wt_sum - wsum;
	a2b[0].pct += left;

	sum = 0;
	wsum = 0;
	for (i=0; i < total; i++) {
	    sum += a2b[i].pct;
	    wsum += a2b[i].pct * a2b[i].num;
	}

	*cur_sum = sum;
	*wtSum = wsum;
}

char *
bh_get_usr_mask()
{
static char usrmask[128];
char dtype[2];

    switch (g_uname_var_type) {
	case 0:
	    strcpy(dtype, "d");
	    break;
	case 1:
	    strcpy(dtype, "x");
	    break;
	default:
	    strcpy(dtype, "d");
    }

    printf ("pfx=%s len=%d type=%d sfx=%s\n", g_uname_pfx, g_uname_var_len, g_uname_var_type, g_uname_sfx);
    snprintf (usrmask, 128, "%s%%0%d%s%s", g_uname_pfx, g_uname_var_len, dtype, g_uname_sfx);

    usrmask[127] = '\0';
    return (usrmask);
}

void 
bh_calc_u_d_i () 
{
char ubuf[128];
char tbuf[256];

    /**	
	Oveall : U~D~I binding
	#D is same for between D-U and D-I bindings
	wigthed sum is same for D-U and U-D 
	also weighted sum same for D-I and I-D bindings

        Terminology : DEFUALT entry is A2B entry where B =1
	First get num device
	In situation of num dev
	  In D2U and D2I , # dev has to be same
	  by default - input will have both 100.
	  proportionate num dev (to reach target num_dev) in D2U and D2Itables and adjust by incrementing D in DEAFULT entry of these tables
	      Calc D2U bindings and U2D bindings (weigthed sum)
	      Adjust U to to get D2U=U2D bindings
	        Map U's in U2D table by D2UWsum/U2DWsum, 
	        recalc U2DWsum, adjust U's in DEFULT entry in U2D table

	      Calc D2I bindings and I2D bindings (weigthed sum)
	      Adjust I to to get D2I=I2D bindings
	        Map I's in I2D table by D2IWsum/I2DWsum, 
	        recalc I2DWsum, adjust I's in DEFULT entry in I2D table

	In situation of num user
	  proportionate num user (to reach taget num user) in U2D adjust by incrementing U in DEAFULT entry of these tables
	      Calc D2U bindings and U2D bindings (weigthed sum)
	      Adjust D to to get D2U=U2D bindings
	        Map D's in D2U table by U2DWsum/D2UWsum, 
	        recalc D2UWsum, adjust D's in DEFULT entry in D2U table (got num_dev)
	  In D2U and D2I , # dev has to be same
	  proportionate num dev in D2I tables (to reach earlier caled num_dev) and adjust by incrementing D in DEAFULT entry of D2I table
	      Calc D2I bindings and I2D bindings (weigthed sum)
	      Adjust I to to get D2I=I2D bindings
	        Map I's in I2D table by D2IWsum/I2DWsum, 
	        recalc I2DWsum, adjust I's in DEFULT entry in I2D table

	In situation of num IP
	  proportionate num IP (to reach taget num IP) in I2D adjust by incrementing I in DEAFULT entry of I2D table
	      Calc D2I bindings and I2D bindings (weigthed sum)
	      Adjust D to to get D2I=I2D bindings
	        Map D's in D2I table by I2DWsum/D2IWsum, 
	        recalc D2IWsum, adjust D's in DEFULT entry in D2I table (got num_dev)
	  In D2U and D2I , # dev has to be same
	  proportionate num dev in D2U tables (to reach earlier calc'ed num_dev) and adjust by incrementing D in DEAFULT entry of D2U table
	      Calc D2U bindings and U2D bindings (weigthed sum)
	      Adjust U to to get D2U=U2D bindings
	        Map U's in U2D table by D2UWsum/U2DWsum, 
	        recalc U2DWsum, adjust U's in DEFULT entry in U2D table (got num_user_

    ***/

    if (driver_count == 1) { //Device
	adjust_pct_a2b (&d2usum, &d2uwsum, total_device, total_d2u_entries, d2uDist);
  	//print_debug("After D2USUM");
	adjust_pct_a2b (&d2isum, &d2iwsum, total_device, total_d2i_entries, d2iDist);
  	//print_debug("After D2ISUM");
	adjust_wt_pct_a2b (&u2dsum, &u2dwsum, d2uwsum, total_u2d_entries, u2dDist);
	if (u2dwsum != d2uwsum) printf ("Target d2uw sum %d not reached now %d\n", d2uwsum, u2dwsum);
  	//print_debug("After U2DWSUM");
	adjust_wt_pct_a2b (&i2dsum, &i2dwsum, d2iwsum, total_i2d_entries, i2dDist);
	if (i2dwsum != d2iwsum) printf ("Target d2iw sum %d not reached now %d\n", d2iwsum, i2dwsum);
  	//print_debug("After I2DWSUM");
	total_user = u2dsum;
	total_ip = i2dsum;
    } else if (driver_count == 2) { //Users
	adjust_pct_a2b (&u2dsum, &u2dwsum, total_user, total_u2d_entries, u2dDist);
	adjust_wt_pct_a2b (&d2usum, &d2uwsum, u2dwsum, total_d2u_entries, d2uDist);
	total_device = d2usum;
	adjust_pct_a2b (&d2isum, &d2iwsum, total_device, total_d2i_entries, d2iDist);
	adjust_wt_pct_a2b (&i2dsum, &i2dwsum, d2iwsum, total_i2d_entries, i2dDist);
	total_ip = i2dsum;
    } else if (driver_count == 3) { //IP
	adjust_pct_a2b (&i2dsum, &i2dwsum, total_ip, total_i2d_entries, i2dDist);
	adjust_wt_pct_a2b (&d2isum, &d2iwsum, i2dwsum, total_d2i_entries, d2iDist);
	total_device = d2isum;
	adjust_pct_a2b (&d2usum, &d2uwsum, total_device, total_d2u_entries, d2uDist);
	adjust_wt_pct_a2b (&u2dsum, &u2dwsum, d2uwsum, total_u2d_entries, u2dDist);
	total_user = u2dsum;
    }
    strcpy (ubuf, bh_get_usr_mask());
    sprintf (tbuf, ubuf, total_user);
    printf ("User Name format (%s) and MAX_USER_NAME buf length=%u\n", ubuf, (unsigned int)strlen(tbuf)+1);
}

#ifdef TEST
int main(int argc, char *argv[])
{
  runningAsTool = 1;

  if(argc != 3)
    print_usage(argv[0]);

  bh_read_file(argv[1], argv[2]);

  	print_debug("Begin");


  bh_calc_u_d_i ();
  	print_debug("End");
  return 0;
}
#endif
