#ifndef BH_READ_CONF_H
#define BH_READ_CONF_H

typedef struct 
{
  int pct;
  int num;
} A2BDist;

typedef struct
{
  short acode;
  short pct;
  int cumpct;
} Udist;

extern A2BDist *d2uDist; // DEVICE_DIST_PCT_OVER_USERS
extern A2BDist *u2dDist; // USER_DIST_PCT_OVER_DEVICES
extern A2BDist *d2iDist; // DEVICE_DIST_PCT_OVER_IP
extern A2BDist *i2dDist; // IP_DIST_PCT_OVER_DEVICES
extern Udist   *udist;   // GEOGRAPHIC_USER_DISTRIBUTION

extern int total_d2u_entries; //contains the number of entries disk to user 
extern int total_u2d_entries; //contains the number of entries user to disk 
extern int total_d2i_entries; //contains the number of entries disk to ip
extern int total_i2d_entries; //contains the number of entries ip to disk
extern int total_u_entries;

extern int total_device; // gen_data 
extern int total_user;   // gen_data
extern int total_ip;     // gen_data

extern int d2usum;  // sum of pct's of d2udist table
extern int u2dsum;  // sum of pct's of u2ddist table
extern int d2isum;  // sum of pct's of d2idist table
extern int i2dsum;  // sum of pct's of i2ddist table
extern int d2uwsum; // sum of multiple of pct and num for each row in d2udist table
extern int u2dwsum; // sum of multiple of pct and num for each row in u2ddist table
extern int d2iwsum; // sum of multiple of pct and num for each row in d2idist table
extern int i2dwsum; // sum of multiple of pct and num for each row in i2ddist table

extern char * bh_get_usr_mask();

extern void bh_read_file(char *read_keyword_file, char *area_file);
extern void bh_calc_u_d_i() ;

#endif
