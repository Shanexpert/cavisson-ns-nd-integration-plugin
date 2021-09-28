#include <stdio.h>
#include <strings.h>

#include "ns_server.h"
#include "timing.h"
#include "ns_msg_def.h"
#include "tmr.h"
#include "ns_schedule_phases.h"
#include "ns_log.h"
#include "ns_global_settings.h"

//#define MIN_IN_JIFFY 6000             //number of jiffies in minute
//#define INTVL_MULT 10                 //unit of intvl in ms
//#define NS_MAX_DISCRETES 10         //Max # of intvals
//#define NS_MAX_DISCRETES MIN_IN_JIFFY
//static int rates_intvals[NS_MAX_DISCRETES];

/* num_rate_intvals is set to 0 & num_user_every set to 1, 
   if calc_iid_ms_from_rpm not called than get_rpm_users will return 1 user 
   Changed on Oct 6, 08 to handle ramp up rate < 100/Minute
*/ 
//static int num_rate_intvals = 0;              
//static unsigned int num_user_every = 1;  

//static int num_rate_intvals;              
//static unsigned int num_user_every;  

//as methods in file are used for ramp up & ramp down, so call init_rate_intervals(), at the start of every run phase(ramp up or ramp down)

void init_rate_intervals(Schedule *ptr)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called");

  ptr->rpm_timings.num_rate_intvals = 0;
  ptr->rpm_timings.num_user_every = 1;
  bzero(ptr->rpm_timings.rates_intvals, sizeof(int) * NS_MAX_DISCRETES); 
}

int
calc_iid_ms_from_rpm(Schedule *ptr, int rpm)
{
  int i, j, a, num, ival;
  int bN, bD;
  int frpm;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, required rpm=%d", rpm);

  i = 1;
  a = rpm;

  while (a*i/MIN_IN_JIFFY < 1) i++;

  ival = i;
  num = a*i/MIN_IN_JIFFY;
  ptr->rpm_timings.num_user_every = num;
  frpm = (num*MIN_IN_JIFFY)/ival;

  NSDL2_SCHEDULE(NULL, NULL, "ival=i=%d, ptr->num_user_every=num=%d, frpm=%d", i, num, frpm);

  j=1;
  bN = (a*i) % MIN_IN_JIFFY;
  bD = MIN_IN_JIFFY;

  NSDL2_SCHEDULE(NULL, NULL, "bN=%d, bD=MIN_IN_JIFFY=%d", bN, bD);

  while (1) {
    for (i = j; i < NS_MAX_DISCRETES; i++) {

      num = ((bN*i)/(bD));
      if (num >= 1) {
        j = i;
        break;	
      }
    }
    if (i == NS_MAX_DISCRETES)
      break;
    bN = (bN * j)% bD;
    bD = bD*j;
    frpm += MIN_IN_JIFFY/(ival*j);
    ptr->rpm_timings.rates_intvals[ptr->rpm_timings.num_rate_intvals++] = j;
    if (ptr->rpm_timings.num_rate_intvals >= NS_MAX_DISCRETES)
      break;
    if (!bN)
      break;
  }

  NSDL2_SCHEDULE(NULL, NULL, "ival=%d, num_rate_intvals=%d, ival*INTVL_MULT=%d, Final RPM =%d", 
                 ival, ptr->rpm_timings.num_rate_intvals, ival*INTVL_MULT, frpm);
  return (ival * INTVL_MULT);
}

//This method is a wrapper for init_rate_intervals() & calc_iid_ms_from_rpm()
int
init_n_calc_iid_ms_from_rpm(Schedule *ptr, int rpm)
{
  int iid_mu;
  NSDL2_SCHEDULE(NULL, NULL, "Method called. rpm=%d", rpm);
  init_rate_intervals(ptr);
  iid_mu = calc_iid_ms_from_rpm(ptr, rpm);
  NSDL2_SCHEDULE(NULL, NULL, "Exitting Method iid_mu=%d", iid_mu);
  return iid_mu;
}

int
get_rpm_users (Schedule *ptr, int surplus_user_ratio)
{
  //static int rpm_call_counter = 0;
  int i;
  int users;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. num_user_every=%d, rpm_call_counter = %d, surplus_user_ratio = %d", 
                 ptr->rpm_timings.num_user_every, ptr->rpm_timings.rpm_call_counter, surplus_user_ratio); 
  users =  ptr->rpm_timings.num_user_every;
  for (i = 0; i < ptr->rpm_timings.num_rate_intvals; i++) {
    if ((ptr->rpm_timings.rpm_call_counter % ptr->rpm_timings.rates_intvals[i]) == 0)
      users++;
  }

  users = users * (1 + surplus_user_ratio);

  ptr->rpm_timings.rpm_call_counter++;

  NSDL4_SCHEDULE(NULL, NULL, "users = %d", users);

  return users;
}


#ifdef TEST
main (int argc, char *argv[])
{
  int rpm = atoi(argv[1]);
  int iid = calc_iid_ms_from_rpm(ptr, rpm);
  int i;
  int numu=0;

  for (i=0; i < 600*1000; i+=iid) {
    numu += get_rpm_users(ptr);	
  }

  printf("10 min users = %d\n", numu);
}
#endif
