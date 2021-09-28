#ifndef TIMING_H
#define TIMING_H

// Timings related macros
#define MIN_IN_JIFFY 6000             //number of jiffies in minute
#define INTVL_MULT 10                 //unit of intvl in ms
#define NS_MAX_DISCRETES MIN_IN_JIFFY

typedef struct {
  int rates_intvals[NS_MAX_DISCRETES];
  int num_rate_intvals;              
  unsigned int num_user_every;  
  int rpm_call_counter;
} RpmTimings;

#endif

