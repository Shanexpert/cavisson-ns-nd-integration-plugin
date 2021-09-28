#define _GNU_SOURCE
#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_sf_gamma.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define STRING_LENGTH 200
#define STRINGMEAN "# name: mean_var\n# type: scalar\n"
#define STRINGMEDIAN "# name: median_var\n# type:scalar\n"
#define STRINGVARIANCE "# name: var_var\n# type:scalar\n"
#define STRINGA "# name: a\n# type:scalar\n"
#define STRINGB "# name: b\n# type:scalar\n"

#define MEAN 1
#define MEDIAN 2
#define VARIANCE 4

#define MEAN_MEDIAN MEAN | MEDIAN
#define MEAN_VARIANCE MEAN | VARIANCE
#define MEDIAN_VARIANCE MEDIAN | VARIANCE
#define MEAN_MEDIAN_VARIANCE MEAN | MEDIAN | VARIANCE

char octave_file_name[512] = "/tmp/octave.out"; //Default path for file octave.out

static int commands[3] = {MEAN_MEDIAN, MEAN_VARIANCE, MEDIAN_VARIANCE};

static int do_iteration1(int, double*, double*);
static int do_iteration2(int, int, int, int, float, double*, double*);
static int do_iteration3(int, int, int, float, double*, double*);
static int call_octave(char*, int, char**);
static int within_error(int, int, int, int, float, double, double);
static double calculate_mean();
static double calculate_median();
static double calculate_variance();

gsl_rng* alloc_exp_gen(unsigned int seed) 
{
  gsl_rng* return_ptr;
  
  return_ptr = gsl_rng_alloc(gsl_rng_default);
  gsl_rng_set(return_ptr, seed);
  return return_ptr;
}

gsl_rng* alloc_weib_gen(unsigned int seed) 
{
  gsl_rng* return_ptr;
  
  return_ptr = gsl_rng_alloc(gsl_rng_default);
  gsl_rng_set(return_ptr, seed);
  return return_ptr;
}

int open_file_fd_then_fp(char *file, FILE **data_file_fp)
{
  int data_file_fd;
  if((data_file_fd = open(file, O_CREAT|O_WRONLY|O_CLOEXEC, 00666)) < 0)
  {
    printf("error in opening file '%s'\n", file);
    perror("open");
    return -1;
  }

  if ((*data_file_fp = fdopen(data_file_fd, "w")) == NULL) {
    printf("error in opening FD into file pointer for %s file.\n", file);
    perror("fdopen");
    return -1;
  }
  return 0;
}

int ns_weibthink_calc(int mean, int median, int variance, double* a, double* b)
{
  FILE* data_file;
  char data_string[STRING_LENGTH];
  int num_inputted = 0;
  int mode = 0;
  int i = 0;
  int ret;
  
  if (chdir("./etc")) {
    fprintf(stderr, "error in changing directory to './etc'\n");
    perror("ns_weibthink_calc");
    return(-1);
  }
  
  if (mean >= 0) {
    mode |= MEAN;
    num_inputted ++;
  }
  
  if (median >= 0) {
    mode |= MEDIAN;
    num_inputted ++;
  }

  if (variance >= 0) {
    mode |= VARIANCE;
    num_inputted ++;
  }

  if (!num_inputted) {
    if (chdir("../")) {
      fprintf(stderr, "error in changing directory to '../'\n");
      perror("ns_weibthink_calc");
      return(-1);
    }
    return -1;
  }

  ret = open_file_fd_then_fp("data", &data_file);
  if(ret != 0)
    return -1;
  
  memset(data_string, 0, STRING_LENGTH);

  if (mean)
    sprintf(data_string, "%s %d\n", STRINGMEAN, mean);

  if (median)
    sprintf(data_string, "%s%s %d\n", data_string, STRINGMEDIAN, median);

  if (variance)
    sprintf(data_string, "%s%s %d\n", data_string, STRINGVARIANCE, variance);

  /* We write this in even if we have more than one inputted values, since some iteration3 and/or iteration2 may fail and we may have to run iteration1 */
  sprintf(data_string, "%s%s %lf\n", data_string, STRINGB, 0.5);

  if (fwrite(data_string, sizeof(char), strlen(data_string) , data_file) != (sizeof(char) * strlen(data_string))) {
    printf("error in writing to file DATA\n");
    return(-1);
  }
  
  fclose(data_file);
      
  if (num_inputted == 3) {
    /* First try to find an A and a B that will fit the inputted 3 values */
    if(!do_iteration3(mean, median, variance, .10, a, b)) {
      if (chdir("../")) {
	fprintf(stderr, "error in changing directory to '../'\n");
	perror("ns_weibthink_calc");
	return(-1);
      }
      return 0;  /* We found an A and B that will fit the Weibull dist. curve and is within 10% of given values*/
    }

    if (!do_iteration3(mean, median, variance, .15, a, b)) {
      if (chdir("../")) {
	fprintf(stderr, "error in changing directory to '../'\n");
	perror("ns_weibthink_calc");
	return(-1);
      }
      return 0;  /* We found an A and B that will fit the Weibull dist. curve and is within 15% of given values*/
    }

    for (i=0; i<3; i++) {
      /* We could not find an A and a B that fit the inputted 3 values */
      /* Now we must find an A and a B that will fit 2 inputted values */
      if (!do_iteration2(mean, median, variance, commands[0], .15, a, b)) {
	if (chdir("../")) {
	  fprintf(stderr, "error in changing directory to '../'\n");
	  perror("ns_weibthink_calc");
	  return(-1);
	}
	return 0;
      }
    }
    
    do_iteration1(MEAN, a, b);
    
    if (chdir("../")) {
      fprintf(stderr, "error in changing directory to '../'\n");
      perror("ns_weibthink_calc");
      return(-1);
    }
    return 0;
  }
      
  if (num_inputted == 2) {
    if (!do_iteration2(mean, median, variance, mode, .15, a, b)) {
      if (chdir("../")) {
	fprintf(stderr, "error in changing directory to '../'\n");
	perror("ns_weibthink_calc");
	return(-1);
      }
      return 0;
    }

    if (mode & MEAN) {
      do_iteration1(MEAN, a, b);
    }
    else {
      do_iteration1(MEDIAN, a, b);
    }

    if (chdir("../")) {
      fprintf(stderr, "error in changing directory to '../'\n");
      perror("ns_weibthink_calc");
      return(-1);
    }
    return 0;

  }

  if (num_inputted == 1) {
    do_iteration1(mode, a, b);
    if (chdir("../")) {
      fprintf(stderr, "error in changing directory to '../'\n");
      perror("ns_weibthink_calc");
      return(-1);
    }
    return 0;
  }


  if (chdir("../")) {
    fprintf(stderr, "error in changing directory to '../'\n");
    perror("ns_weibthink_calc");
    return(-1);
  }
  return 0;

}

static int do_iteration3(int mean, int median, int variance, float error, double* a, double* b)
{
  int i;
  char octave_command[STRING_LENGTH];
  char first_return_arg[STRING_LENGTH];
  char second_return_arg[STRING_LENGTH];
  char* return_array[2];
  
  for (i = 0; i < 3; i++) {
    memset(first_return_arg, 0, STRING_LENGTH);
    memset(second_return_arg, 0, STRING_LENGTH);
    memset(octave_command, 0, STRING_LENGTH);
    sprintf(octave_command, "%s%d%s 2>>%s", "export TERM=; octave -q ./calculateab", commands[i], ".m", octave_file_name);

    return_array[0] = first_return_arg;
    return_array[1] = second_return_arg;
    
    sighandler_t prev_handler;
    prev_handler = signal( SIGCHLD, SIG_IGN);
    int ret = call_octave(octave_command, 2, return_array);
    (void) signal( SIGCHLD, prev_handler);

    if (ret)
    return -1;

    *a = strtod(first_return_arg, NULL);
    *b = strtod(second_return_arg, NULL);    

    if (within_error(mean, median, variance, MEAN_MEDIAN_VARIANCE, error, *a, *b))
      return 0;
  }

  return -1;
}
   

static int do_iteration2(int mean, int median, int variance, int mode, float error, double* a, double* b) 
{
  char octave_command[STRING_LENGTH];
  char first_return_arg[STRING_LENGTH];
  char second_return_arg[STRING_LENGTH];
  char* return_array[2];

  return_array[0] = first_return_arg;
  return_array[1] = second_return_arg;

  memset(first_return_arg, 0, STRING_LENGTH);
  memset(second_return_arg, 0, STRING_LENGTH);
  memset(octave_command, 0, STRING_LENGTH);
    sprintf(octave_command, "%s%d%s 2>>%s", "export TERM=; octave -q ./calculateab", mode, ".m", octave_file_name);
  
  sighandler_t prev_handler;
  prev_handler = signal( SIGCHLD, SIG_IGN);
  int ret = call_octave(octave_command, 2, return_array);
  (void) signal( SIGCHLD, prev_handler);

  if (ret)
  return -1;

  *a = strtod(first_return_arg, NULL);
  *b = strtod(second_return_arg, NULL);
  
  if (within_error(mean, median, variance, mode, error, *a, *b))
    return 0;

  return -1;
}


static int do_iteration1(int mode, double* a, double* b) 
{
  char octave_command[STRING_LENGTH];
  char return_arg[STRING_LENGTH];
  char* return_array[1];

  return_array[0] = return_arg;

  memset(return_arg, 0, STRING_LENGTH);
  memset(octave_command, 0, STRING_LENGTH);
  sprintf(octave_command, "%s%d%s 2>>%s", "export TERM=; octave -q calculateab", mode, ".m", octave_file_name);
  
  // Added signal handler because call_octave function uses popen to execute octave_command, 
  // for command execution popen forks a child process which after task completion exit and sends SIGCHLD signal.  
  // This sick child signal, when received by parent result into termination of test.
 
  sighandler_t prev_handler; 
  prev_handler = signal( SIGCHLD, SIG_IGN);
  int ret = call_octave(octave_command, 2, return_array);
  (void) signal( SIGCHLD, prev_handler);
  
  if (ret)
  return -1;

  *b = 0.5;
  *a = strtod(return_arg, NULL);
  
  /* no error calculation is needed for this iteration */

  return 0;
}


static int call_octave(char* command, int num_return_args, char* return_args[])
{
  int i;
  FILE* octave_run;
  char read_string[STRING_LENGTH];
  char* read_string_parser;

  /* going to call octave to calculate values of a and b */
  if ((octave_run = popen(command, "r")) == NULL) {
    printf("error in calling popen\n");
    return -1;
  }
  
  /* going to read in the output from octave */
  memset(read_string, 0, STRING_LENGTH);
  fread(read_string, sizeof(char), STRING_LENGTH, octave_run);
  
  if (ferror(octave_run)) {
    printf ("error in reading the output from octave\n");
    return -1;
  }

  read_string_parser = read_string;
  
  for (i = 0; i < num_return_args; i++) {
    sscanf(read_string_parser, "%s\n", return_args[i]);
    read_string_parser = strchr(read_string_parser, '\n');
    // Changed for FC9 port to fix warning . but code looks incorrect
    // if (read_string)
    if (strcmp(read_string, "") != 0)
      read_string_parser++;
  }
  
  pclose(octave_run);

  return 0;
}


static int within_error(int mean, int median, int variance, int mode, float error, double a, double b)
{
  char data_string[STRING_LENGTH];
  double calc_mean, calc_median, calc_variance;
  FILE* data_file;
  int ret;

  ret = open_file_fd_then_fp("data2", &data_file);
  if(ret != 0)
    return -1;

  memset(data_string, 0, STRING_LENGTH);
 
  sprintf(data_string, "%s%lf\n%s%lf\n", STRINGA, a, STRINGB, b);

  if (fwrite(data_string, sizeof(char), strlen(data_string) , data_file) != (sizeof(char) * strlen(data_string))) {
    printf("error in writing to file DATA\n");
    return -1;
  }
  
  fclose(data_file);

  if (mode & MEAN) {
    calc_mean = calculate_mean();
    if ((abs(calc_mean - mean) / mean) > error) 
      return 0;
  }
  
  if (mode & MEDIAN) {
    calc_median = calculate_median();
    if ((abs(calc_median - median) / median) > error) 
      return 0;
  }
  
  if (mode & VARIANCE) {
    calc_variance = calculate_variance();
    if ((abs(calc_variance - variance) / variance) > error) 
      return 0;
  }

  return 1;
}


static double calculate_mean()
{
  char octave_command[STRING_LENGTH];
  char return_arg[STRING_LENGTH];
  char* return_array[1];

  return_array[0] = return_arg;
  
  memset(octave_command, 0, STRING_LENGTH);
  memset(return_arg, 0, STRING_LENGTH);
  
  sprintf(octave_command, "%s 2>>%s", "export TERM=; octave -q calculate_mean.m", octave_file_name);
  
  sighandler_t prev_handler;
  prev_handler = signal( SIGCHLD, SIG_IGN);
  int ret = call_octave(octave_command, 2, return_array);
  (void) signal( SIGCHLD, prev_handler);

   if (ret)
    return -1;
  
  return strtod(return_arg, NULL);
}
    

static double calculate_median()
{
  char octave_command[STRING_LENGTH];
  char return_arg[STRING_LENGTH];
  char* return_array[1];

  return_array[0] = return_arg;
  
  memset(octave_command, 0, STRING_LENGTH);
  memset(return_arg, 0, STRING_LENGTH);
  
  sprintf(octave_command, "%s 2>>%s", "export TERM=; octave -q calculate_median.m", octave_file_name);
  
  sighandler_t prev_handler;
  prev_handler = signal( SIGCHLD, SIG_IGN);
  int ret = call_octave(octave_command, 2, return_array);
  (void) signal( SIGCHLD, prev_handler);
  
  if (ret)
    return -1;
 
  return strtod(return_arg, NULL);
}


static double calculate_variance()
{
  char octave_command[STRING_LENGTH];
  char return_arg[STRING_LENGTH];
  char* return_array[1];

  return_array[0] = return_arg;
  
  memset(octave_command, 0, STRING_LENGTH);
  memset(return_arg, 0, STRING_LENGTH);
  
  sprintf(octave_command, "%s 2>>%s", "export TERM=; octave -q calculate_variance.m", octave_file_name);
  sighandler_t prev_handler;
  prev_handler = signal( SIGCHLD, SIG_IGN);
  int ret = call_octave(octave_command, 2, return_array);
  (void) signal( SIGCHLD, prev_handler);

  if (ret)
    return -1;
  
  return strtod(return_arg, NULL);
}
