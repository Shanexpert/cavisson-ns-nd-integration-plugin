#ifndef WEIB_THINK_H
#define WEIB_THINK_H
#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_sf_gamma.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

struct WeibThinkInfo {
  gsl_rng *r;
  double a;
  double b;
};  

double ns_weibthink_sample(void);
extern gsl_rng* alloc_weib_gen(unsigned int seed);
extern gsl_rng* alloc_exp_gen(unsigned int seed);
extern char octave_file_name[512]; //Added to redirect octave command output to file octave.out
#endif //WEIB_THINK_H
