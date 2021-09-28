#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bh_read_conf.h"
#include "ns_string.h"

typedef struct {
	short acode; //Location
	short num_dev; //numdev associated with an user
	int start_dev; //Start dee
} User;

User * userTable;
int total_user;

typedef struct {
	short cur_user;
	short num_user;
	short num_ip;
	int start_user;
	int start_ip;
} Device;

Device * devTable;
int total_device;

typedef struct {
	short cur_dev;
	short num_dev;
	int start_dev;
	unsigned int ip_num;
} IP;

IP * ipTable;
int total_ip;

unsigned int default_ip_num;

void * my_malloc (int size)
{
void * ptr;

	ptr  = malloc (size);
	if (!ptr) {
	    printf ("malloc faile dfor size %d\n", size);
	    exit (1);
	}
	return ptr;
}

typedef struct {
	int tot_user;
	int max_user;
	int new_user;
	int max_depth;
	int user;
	struct {
	    short flag;
	    int uid;
	}ass [4096000];
}AssUser;

AssUser assUser;

int *DataArray;

void
add_user (int unum)
{
int i, max;

	max = assUser.tot_user;
	for (i = 0; i < max; i++ ) {
	    if (assUser.ass[i].uid == unum)
		return;
	}
	//Add the user

	if  (assUser.max_user == max) {
		printf ("Too many associated users\n");
		exit(1);
	}
	assUser.ass[max].flag = 1;
	assUser.ass[max].uid = unum;
	assUser.tot_user++;
	assUser.new_user++;
}

void
get_user_associations()
{
int i,j, k, l, m, max, unum, dnum, inum, Dnum, Unum;
int maxd, startd, maxi, starti, maxD, startD, maxU, startU;

	//if (++assUser.max_depth >= 10) return;
	++assUser.max_depth;

	if (assUser.new_user == 0)
	    return;

	max = assUser.tot_user;
	for (i = 0; i < max; i++) {
	    if (assUser.ass[i].flag == 0)
		continue;

	    assUser.ass[i].flag = 0;
	    assUser.new_user--;
	    unum = assUser.ass[i].uid;
	
	    //Get all devices for this user
	    maxd = userTable[unum].num_dev;
	    startd = userTable[unum].start_dev;
	    for (j = 0; j < maxd; startd++, j++) {
		dnum = DataArray[startd];

	    	//Get all IPs for this device
	    	maxi = devTable[dnum].num_ip;
	    	starti = devTable[dnum].start_ip;
	    	for (k = 0; k < maxi; starti++, k++) {
		    inum = DataArray[starti];
	    	    //Get all dev for this ip - back
	    	    maxD = ipTable[inum].num_dev;
	    	    startD = ipTable[inum].start_dev;
	    	    for (l = 0; l < maxD; startD++, l++) {
		        Dnum = DataArray[startD];
	    	        //Get all user for this dev - back
	    	        maxU = devTable[Dnum].num_user;
	    	        startU = devTable[Dnum].start_user;
	    	        for (m = 0; m < maxU; startU++, m++) {
		            Unum = DataArray[startU];
			    add_user(Unum);
			}
		    }
		}
	    }
	}
#if 0
	printf ("\nafter get_user_associations Tot=%d New=%d depth=%d user=%d\n", assUser.tot_user, assUser.new_user, assUser.max_depth,assUser.user);
	max = assUser.tot_user;
	for (i = 0; i < max; i++) {
	    printf ("\tUser=%d flag=%hd\n", assUser.ass[i].uid, assUser.ass[i].flag);
	}
#endif
	get_user_associations();
}

//Copied from IPMgmt.c
//converts unsigned int address to dotted notation
//Example: addr may be 0xC0A80112 return would be 192.168.1.18
//buffer is statically allocated. would be overwritten on next call.
char * ns_char_ip (unsigned int addr)
{
  static __thread char str_address[16];
  unsigned int a, b, c,d;
  a = (addr >>24) & 0x000000FF;
  b = (addr >>16) & 0x000000FF;
  c = (addr >>8) & 0x000000FF;
  d = (addr) & 0x000000FF;
  sprintf(str_address, "%d.%d.%d.%d", a,b,c,d);
  return str_address;
}


short
get_acode ()
{
int num, i;
//short acode;
	
	num = random()%100;
	for (i = 0; i < total_u_entries; i++) {
	    if (num < udist[i].cumpct)
		break;
	}
	 
	return udist[i].acode;
}

unsigned int
get_ipnum( short acode)
{
unsigned int ipa;
    do {
        ipa = get_an_IP_address_for_area ((unsigned int)acode);
        if (ipa%256 != 255)
            break;
    } while(1);
    return (ipa);
}

static int hwm_ass=0;
static int hwm_depth=0;
unsigned long long tot_sum_all;
unsigned long long depth_sum_all;
int ass_counters[5000];
void
assign_ip (int unum)
{
short acode;
int i, max, uid, maxd, startd, j, dnum, k, inum, maxi, starti;

	assUser.tot_user = 1;
	assUser.max_depth = 0;
	assUser.max_user = 4096000;
	assUser.new_user = 1;
	assUser.user = unum;
	assUser.ass[0].flag = 1;
	assUser.ass[0].uid = unum;

	get_user_associations();

	//printf ("\nuser_associations for %d Tot=%d New=%d depth=%d\n", unum, assUser.tot_user, assUser.new_user, assUser.max_depth);
	if (hwm_ass < assUser.tot_user) hwm_ass = assUser.tot_user;
	tot_sum_all += assUser.tot_user;
	if (hwm_depth < assUser.max_depth) hwm_depth = assUser.max_depth;
	depth_sum_all += assUser.max_depth;
	if (assUser.tot_user >= 5000)
	    ass_counters[5000]++;
	else
	    ass_counters[assUser.tot_user]++;
#if 0
	max = assUser.tot_user;
	for (i = 0; i < max; i++) {
	    printf ("\tUser=%d flag=%hd\n", assUser.ass[i].uid, assUser.ass[i].flag);
	}
#endif

	acode = userTable[unum].acode;
	
	max = assUser.tot_user;
	//printf ("User=%d num asso=%d\n", unum, max);
	for (i =0; i < max; i++) {
	    uid = assUser.ass[i].uid;
	    if ( acode != userTable[uid].acode) {
		printf ("Expecting all assocaited user to have same acode for user=%d acode=%hd found=%hd for user %d\n",
				unum, acode, userTable[uid].acode, uid);
		exit (1);
	    }
	}

	//fill acode
	if (acode == -1) {
	    acode = get_acode();
	    for (i =0; i < max; i++) {
	        uid = assUser.ass[i].uid;
	        userTable[uid].acode = acode;
		//Fill ip
	        //Get all devices for this user
	    	maxd = userTable[uid].num_dev;
	    	startd = userTable[uid].start_dev;
	    	for (j = 0; j < maxd; startd++, j++) {
		    dnum = DataArray[startd];

	    	    //Get all IPs for this device
	    	    maxi = devTable[dnum].num_ip;
	    	    starti = devTable[dnum].start_ip;
	    	    for (k = 0; k < maxi; starti++, k++) {
		        inum = DataArray[starti];
			/*
			printf ("Add IP: user=%d maxd=%d dnum=%d maxi=%d inum=%d ipnum=%lu\n",
				uid, maxd, dnum, maxi, inum, ipTable[inum].ip_num);
			*/
			if (ipTable[inum].ip_num) {
			    //printf ("Expacting IP num to be unfilled\n");
			    ;
			} else {
		     	    ipTable[inum].ip_num = get_ipnum (acode);
			}
		    }
		}
	    }
	}
}

static inline int 
is_dev_unum_ass_dup(int dnum, int unum) 
{
int i;
int dup=0;
int max = devTable[dnum].cur_user;
int start = devTable[dnum].start_user;

	for (i=0; i < max; i++) {
	    if (DataArray[start+i] == unum) {
		dup = 1;
		break;
	    }
	}
return dup;
}

inline int 
get_dev_idx (int unum)
{
int i;
static int min_dev=0;
int min_enable=1;

	for (i = min_dev; i < total_device; i++) {
	    if  (devTable[i].cur_user == devTable[i].num_user) {
		if (min_enable) min_dev = i;
	    } else if (is_dev_unum_ass_dup (i, unum)) {
		min_enable = 0;
	    } else {
		break;
	    }
	}
	if (i == total_device) {
	  for (i = min_dev; i < total_device; i++) {
	    if  (devTable[i].cur_user == devTable[i].num_user) {
		min_dev = i;
	    } else if (is_dev_unum_ass_dup (i, unum)) {
		printf ("Dup : Dev=%d (%x)Unum=%d (%x)\n", i, i, unum, unum);
		break;
	    } else {
		break;
	    }
	  }
	}
	if (i == total_device) {
	    printf ("All devices exhausted \n");
	    exit(1);
	}
	DataArray[devTable[i].start_user + devTable[i].cur_user] = unum;
	devTable[i].cur_user++;
	return i;
}

static inline int 
is_ip_dnum_ass_dup(int inum, int dnum) 
{
int i;
int dup=0;
int max = ipTable[inum].cur_dev;
int start = ipTable[inum].start_dev;

	for (i=0; i < max; i++) {
	    if (DataArray[start+i] == dnum) {
		dup = 1;
		break;
	    }
	}
return dup;
}

inline int 
get_ip_idx (int dnum)
{
int i;
static int min_ip=0;
int min_enable=1;

	for (i = min_ip; i < total_ip; i++) {
	    if  (ipTable[i].cur_dev == ipTable[i].num_dev) {
		if (min_enable) min_ip = i;
	    } else if (is_ip_dnum_ass_dup (i, dnum)) {
		min_enable = 0;
	    } else {
		break;
	    }
	}
	if (i == total_ip) {
	  for (i = min_ip; i < total_ip; i++) {
	    if  (ipTable[i].cur_dev == ipTable[i].num_dev) {
		min_ip = i;
	    } else if (is_ip_dnum_ass_dup (i, dnum)) {
		printf ("Dup : IP=%d (%x)Denum=%d (%x)\n", i, i, dnum, dnum);
		break;
	    } else {
		break;
	    }
	  }
	}
	if (i == total_ip) {
	    printf ("All IPs exhausted \n");
	    exit(1);
	}
	DataArray[ipTable[i].start_dev + ipTable[i].cur_dev] = dnum;
	ipTable[i].cur_dev++;
	return i;
}

void
create_tables ()
{
int i, marker, j, k;
int ass_idx = 0;
int maxd, startd;
int maxi, starti;
int dnum, inum;
int quant, num;
int m50=0, m80=0, m90=0, m99=0;
int t50, t80, t90, t99;
FILE *fp;
char mask_buf[256];

	default_ip_num = (unsigned int)192*256*256*256;
	default_ip_num += 168*256*256 + 100*256 +200;


	//Alloc TAbles
	userTable = my_malloc (total_user * sizeof(User));
	devTable = my_malloc (total_device * sizeof(Device));
	ipTable = my_malloc (total_ip * sizeof(IP));
	
	//DataArray = my_malloc (2 * (num_d2u_ass+num_d2i_ass) * sizeof (int));
	num = 2 * (d2uwsum+d2iwsum);
	DataArray = my_malloc (num * sizeof (int));

	for (i = 0; i < num; i++) {
	    DataArray[i] = -1;
	}
	printf ("Allc done\n");
	//resreve space for assoviatiosn
#if 0
	k=0; //unum
	for (i = 0; i < total_u2d_entries; i++)  {
	    //marker is #dev and quant is #user
	    marker = u2dDist[i].num;
	    quant = u2dDist[i].pct;
	    //for all quant users set marker dev's
	    for (j = 0; j < quant; j++,k++) {
	    	userTable[k].acode = -1;
	        userTable[k].num_dev = marker;
	    	userTable[k].start_dev = ass_idx;
	    	ass_idx += marker;
	    }
	}
	if (k != total_user) {
		printf("Total_user %d different than calculated %d\n", total_user, k);
		exit(1);
	}

	k=0; //dnum
	for (i=0; i < total_d2u_entries; i++)  {
	    //marker is #user and quant is #dev
	    marker = d2uDist[i].num;
	    quant = d2uDist[i].pct;
	    //for all quant dev set marker #user's
	    for (j = 0; j < quant; j++,k++) {
	    	devTable[k].num_user = marker;
	    	devTable[k].cur_user = 0;
	    	devTable[k].start_user = ass_idx;
	    	ass_idx += marker;
	    }
	}
	if (k != total_device) {
		printf("Total_device %d different than calculated %d\n", total_device, k);
		exit(1);
	}

	k=0; //inum
	for (i=0; i < total_i2d_entries; i++)  {
	    //marker is #device and quant is #ip
	    marker = i2dDist[i].num;
	    quant = i2dDist[i].pct;
	    //for all quant ip set marker #device's
	    for (j = 0; j < quant; j++,k++) {
	    	ipTable[k].ip_num = 0;
	    	ipTable[k].num_dev = marker;
	    	ipTable[k].cur_dev = 0;
	    	ipTable[k].start_dev = ass_idx;
	    	ass_idx += marker;
	    }
	}
	if (k != total_ip) {
		printf("Total_ip %d different than calculated %d\n", total_ip, k);
		exit(1);
	}

	k=0; //device
	for (i=0; i < total_d2i_entries; i++)  {
	    //marker is #ip and quant is #dev
	    marker = d2iDist[i].num;
	    quant = d2iDist[i].pct;
	    //for all quant dev set marker #ip's
	    for (j = 0; j < quant; j++,k++) {
	    	devTable[k].num_ip = marker;
	    	devTable[k].start_ip = ass_idx;
	    	ass_idx += marker;
	    }
	}
	if (k != total_device) {
		printf("Total_device %d different than calculated %d\n", total_device, k);
		exit(1);
	}

#endif
//#if 0

	/************Replace start ********/
	//Take care of devices
	//Each user has atleast one dev associated
	for (i = 0; i < total_user; i++) {
	    userTable[i].acode = -1;
	    userTable[i].num_dev = 1;
	}
	//srand (times(NULL));
	//marker is #dev and quant is #user
	//for (i = 9; i > 0; i--) 
	for (i = total_u2d_entries -1; i > 0; i--) 
	{
	    //marker = i+1;
	    marker = u2dDist[i].num;
	    //quant = u2d2_dist[i];
	    quant = u2dDist[i].pct;
	    //for all quant users set marker dev's
	    for (j = 0; j < quant; j++) {
		while (1) {
		    k = rand()%total_user;
	            if (userTable[k].num_dev >= marker)
		        continue;
		
	            userTable[k].num_dev = marker;
		    break;
		}
	    }
	}

	for (i = 0; i < total_user; i++) {
	    userTable[i].start_dev = ass_idx;
	    ass_idx += userTable[i].num_dev;
	}


	//Take care of devices->users
	//Each dev has atleast one user associated
	for (i = 0; i < total_device; i++) {
	    devTable[i].num_user = 1;
	    devTable[i].cur_user = 0;
	}
	//srand (times(NULL));
	//marker is #user and quant is #dev
	//for (i = 9; i > 0; i--) 
	for (i = total_d2u_entries -1; i > 0; i--) 
	{
	    //marker = i+1;
	    marker = d2uDist[i].num;
	    //quant = d2u2_dist[i];
	    quant = d2uDist[i].pct;
	    //for all quant dev set marker #user's
	    for (j = 0; j < quant; j++) {
		while (1) {
		    k = rand()%total_device;
	            if (devTable[k].num_user >= marker)
		        continue;
		
	            devTable[k].num_user = marker;
		    break;
		}
	    }
	}

	for (i = 0; i < total_device; i++) {
	    devTable[i].start_user = ass_idx;
	    ass_idx += devTable[i].num_user;
	}

	//Take care of ipTable
	for (i = 0, j = 0; i < total_ip; i++) {
	    ipTable[i].num_dev = 1;
	    ipTable[i].cur_dev = 0;
	}
	//srand (times(NULL));
	//for (i = 9; i > 0; i--) 
	for (i = total_i2d_entries-1; i > 0; i--) 
	{
	    //marker = i+1;
	    marker = i2dDist[i].num;
	    //quant = i2d2_dist[i];
	    quant = i2dDist[i].pct;
	    for (j = 0; j < quant; j++) {
		while (1) {
		    k = rand()%total_ip;
	            if (ipTable[k].num_dev >= marker)
		        continue;
		
	            ipTable[k].num_dev = marker;
		    break;
		}
	    }
	}

	for (i = 0; i < total_ip; i++) {
	    ipTable[i].start_dev = ass_idx;
	    ass_idx += ipTable[i].num_dev;
	}

	//Take care of d2i table
	for (i = 0, j = 0; i < total_device; i++) {
	    devTable[i].num_ip = 1;
	}
	//srand (times(NULL));
	//for (i = 9; i > 0; i--) 
	for (i = total_d2i_entries - 1; i > 0; i--) 
	{
	    //marker = i+1;
	    marker = d2iDist[i].num;
	    //quant = d2i2_dist[i];
	    quant = d2iDist[i].pct;
	    for (j = 0; j < quant; j++) {
		while (1) {
		    k = rand()%total_device;
	            if (devTable[k].num_ip >= marker)
		        continue;
		
	            devTable[k].num_ip = marker;
		    break;
		}
	    }
	}

	for (i = 0; i < total_device; i++) {
	    devTable[i].start_ip = ass_idx;
	    ass_idx += devTable[i].num_ip;
	}

	/***************Replace end ********/
//#endif

	printf ("Reservation done\n");
	//Create associations
	for (i = 0 ; i < total_user; i++) {
	    for (j = 0; j < userTable[i].num_dev; j++)
	        DataArray[userTable[i].start_dev +j] = get_dev_idx(i);
	}

	for (i = 0; i < total_device; i++) {
	    for (j = 0; j < devTable[i].num_ip; j++)
	        DataArray[devTable[i].start_ip +j] = get_ip_idx(i);
	}
	
	//printt association
#if 0
	for (i = 0; i < total_user; i++) {
	    printf ("user=%d num_dev = %hd start_dev=%d\n", i, userTable[i].num_dev, userTable[i].start_dev);
	    for (j = 0; j <userTable[i].num_dev; j++) {
		printf ("\tDevice=%d\n", DataArray[userTable[i].start_dev + j]);
	    }
	}

	for (i = 0; i < total_device; i++) {
	    printf ("device=%d num_user = %hd cur_user=%hd start_user=%d\n", i, devTable[i].num_user, devTable[i].cur_user, devTable[i].start_user);
	    if (devTable[i].num_user != devTable[i].cur_user)
		printf ("Bad: User data on dev table for dev=%d\n", i);
	    for (j = 0; j <devTable[i].num_user; j++) {
		printf ("\tUser=%d\n", DataArray[devTable[i].start_user + j]);
	    }
	}

	for (i = 0; i < total_device; i++) {
	    printf ("device=%d num_ip = %hd start_ip=%d\n", i, devTable[i].num_ip, devTable[i].start_ip);
	    for (j = 0; j <devTable[i].num_ip; j++) {
		printf ("\tIP=%d\n", DataArray[devTable[i].start_ip + j]);
	    }
	}

	for (i = 0; i < total_ip; i++) {
	    printf ("IP=%d num_dev = %hd cur_dev=%hd start_dev=%d\n", i, ipTable[i].num_dev, ipTable[i].cur_dev, ipTable[i].start_dev);
	    if (ipTable[i].num_dev != ipTable[i].cur_dev)
		printf ("Bad: Dev data on ip table for ip=%d\n", i);
	    for (j = 0; j <ipTable[i].num_dev; j++) {
		printf ("\tdev=%d\n", DataArray[ipTable[i].start_dev + j]);
	    }
	}
#endif

	printf ("Association done\n");
	/*        assign_ip (58);
		exit(0);*/
	//assign IP
	for (i = 0; i < total_user; i++) 
	//for (i = 2899958; i < total_user; i++)
 	{
	        assign_ip (i);
	}

	printf ("IP Assignment done\n");
	//print associations

	fp = fopen("bindings.dat", "w");
	if (!fp) {
	    printf ("unable to open bindings.dat\n");
	    exit(1);
	}
        fprintf(fp, "%10d %10d\n", total_device, 10000);
	marker = 0;
	for (i = 0; i < total_user; i++) {
	    //Get all devices for this user
	    maxd = userTable[i].num_dev;
	    startd = userTable[i].start_dev;
	    for (j = 0; j < maxd; startd++, j++) {
		dnum = DataArray[startd];

	    	//Get all IPs for this device
	    	maxi = devTable[dnum].num_ip;
	    	starti = devTable[dnum].start_ip;
	    	for (k = 0; k < maxi; starti++, k++) {
		    inum = DataArray[starti];
		    marker++;
		    fprintf(fp, "%x,%x,%s\n", i, dnum, ns_char_ip(ipTable[inum].ip_num));
		}
	    }
	}
	rewind(fp);	
        fprintf(fp, "%10d %10d\n", total_device, marker);
	fclose(fp);

	//print users
	fp = fopen("users.dat", "w");
	if (!fp) {
	    printf ("unable to open users.dat\n");
	    exit(1);
	}
        fprintf(fp, "%d\n", total_user);
	strcpy (mask_buf, bh_get_usr_mask());
	strcat (mask_buf, ",%hd,-1,0\n");
	for (i = 0; i < total_user; i++) {
	    //fprintf(fp, "U%08X,%hd,-1,0\n", i, userTable[i].acode);
	    fprintf(fp, mask_buf, i, userTable[i].acode);
	}
	fclose (fp);
	
	i = tot_sum_all/total_user;
	j = depth_sum_all/total_user;
	printf ("Num User = %d Num Devices = %d Num IP's %d\n", total_user, total_device, total_ip);
	printf ("Num associations = %d hwm_ass = %d avg=%d hwm_depth=%d avg=%d\n", marker, hwm_ass, i, hwm_depth, j);
	if (hwm_ass <5000) 
	    marker = hwm_ass;
	else
	    marker = 5000;
	quant = 0;

	t50=total_user/2; t80=total_user*8/10; t90=total_user*9/10; t99=total_user*99/100;
	for (i=0; i < marker; i++) {
	    quant += ass_counters[i];
	    if ((!m50) && (t50 < quant)) m50 = i;
	    if ((!m80) && (t80 < quant)) m80 = i;
	    if ((!m90) && (t90 < quant)) m90 = i;
	    if ((!m99) && (t99 < quant)) m99 = i;
	}
	printf ("m50 = %d m80=%d m90=%d m99=%d\n", m50, m80, m90, m99);
	printf ("t50 = %d t80=%d t90=%d t99=%d max=%d last = %d\n", t50, t80, t90, t99, marker, quant);
}

int
main(int argc, char *argv[])
{

char areabuf[128];;
#if 0
int multiplier;
int i;
	total_device = 5000000;
	total_user = 4200000;
	total_ip = 5700000;
#endif

	if (argc < 2) {
	    printf ("Usage: %s <data-specification-file>\n", argv[0]);
	    exit(1);
	}

	if (getenv("NS_WDIR")) {
	    sprintf(areabuf, "%s/etc/area.txt", getenv("NS_WDIR"));
	} else {
	    strcpy(areabuf, "/home/cavisson/work/etc/area.txt");
	}
	bh_read_file (argv[1], areabuf);	
	bh_calc_u_d_i();
#if 0
	multiplier = atoi(argv[1]);
	total_device = 10000;
	total_user = 8400;
	total_ip = 11400;

	total_device *= multiplier;
	total_user *= multiplier;
	total_ip *= multiplier;
	num_d2u_ass *= multiplier;
	num_d2i_ass *= multiplier;

	for (i=0; i < 10; i++ ) {
		d2u2_dist[i] *=  multiplier;
		u2d2_dist[i] *=  multiplier;
		d2i2_dist[i] *=  multiplier;
		i2d2_dist[i] *=  multiplier;
	}
#endif

create_tables ();
return 0;
}
