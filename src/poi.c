#include <assert.h>
#include <gsl/gsl_randist.h>
#include "ns_exit.h"

#define NS_MAX_POI_SAMPLES  1024
#define NS_MAX_POI_PROCESS 2
#define NS_MAX_RAND_PROCESS 6

extern int create_table_entry(int *row_num, int *total, int *max, char **ptr, int size, char *name);

typedef struct RandInfoEntry {
	gsl_rng *r;
	unsigned int max;
} RandInfoEntry;

RandInfoEntry *gRandTable;

typedef struct PoiInfoEntry {
	gsl_rng *r;
	double mu;
	unsigned int cur_sample;
	unsigned int *arr;
} PoiInfoEntry;

PoiInfoEntry *gPoiTable;
	
int gCurRandTblIdx = 0;
int ns_max_rand_process = 0;

int
ns_rand_init(unsigned long seed, unsigned long max)
{
  int rand_idx = gCurRandTblIdx;

	
  const gsl_rng_type * T = gsl_rng_mt19937; /* this is also the "default" generator in GSL
                                               -- gsl_rng_default  */

  if (create_table_entry(&rand_idx, &gCurRandTblIdx, &ns_max_rand_process, (char **)&gRandTable,
                         sizeof(RandInfoEntry), (char *)__FUNCTION__) == -1) {
    fprintf(stderr, "ns_rand_init: Out of memory. Exiting..\n");
    exit(1);
  }

  gRandTable[rand_idx].r = gsl_rng_alloc (T);
  gsl_rng_set(gRandTable[rand_idx].r, seed); 
  gRandTable[rand_idx].max = max;

  //  gCurRandTblIdx++;

  return (rand_idx);
}

unsigned long
ns_get_random(int rand_idx)
{
	return (gsl_rng_uniform_int(gRandTable[rand_idx].r, gRandTable[rand_idx].max));
}

unsigned long
ns_get_random_max(int rand_idx, int max)
{
  return (gsl_rng_uniform_int(gRandTable[rand_idx].r, max));
}

int gCurPoiTblIdx = 0;
int ns_max_poi_process = 0;

int
ns_poi_init(double mu, unsigned long seed)
{
  int poi_idx = gCurPoiTblIdx;

	
  /*	
	Generator: gsl_rng_mt19937 -- The MT19937 generator of Makoto Matsumoto and Takuji Nishimura 
	is a variant of the twisted generalized feedback shift-register algorithm, and is known as the
       	"Mersenne Twister" generator. It has a Mersenne prime period of 2^19937 - 1 (about 10^6000) 
	and is equi-distributed in 623 dimensions. It has passed the DIEHARD statistical tests. It 
	uses 624 words of state per generator and is comparable in speed to the other generators. 
	The original generator used a default seed of 4357 and choosing s equal to zero in 
	gsl_rng_set reproduces this. 

        For more information see, Makoto Matsumoto and Takuji Nishimura, "Mersenne Twister: A 
	623-dimensionally equidistributed uniform pseudorandom number generator". ACM Transactions
       	on Modeling and Computer Simulation, Vol. 8, No. 1 (Jan. 1998), Pages 3-30 
  */

  const gsl_rng_type * T = gsl_rng_mt19937; /* this is also the "default" generator in GSL
                                               -- gsl_rng_default  */

  if (create_table_entry(&poi_idx, &gCurPoiTblIdx, &ns_max_poi_process, (char **)&gPoiTable, 
                         sizeof(PoiInfoEntry), (char *)__FUNCTION__) == -1) {
    fprintf(stderr, "ns_poi_init: Out of memory Exiting..\n");
    exit(1);
  }
        
  gPoiTable[poi_idx].r = gsl_rng_alloc (T);
  gsl_rng_set(gPoiTable[poi_idx].r, seed); 
  gPoiTable[poi_idx].mu = mu;
  gPoiTable[poi_idx].cur_sample = 0;

  //gsl_ran_poisson_array(gPoiTable[poi_idx].r, NS_MAX_POI_SAMPLES, gPoiTable[poi_idx].arr, mu);

  //	gCurPoiTblIdx++;

  return (poi_idx);
}

int ns_exp_init(double mu, unsigned long seed)
{
  return ns_poi_init(mu, seed);
}

void
ns_poi_reinit(unsigned int poi_idx, double mu)
{

#if 0
	const gsl_rng_type * T = gsl_rng_mt19937; /* this is also the "default" generator in GSL
						     -- gsl_rng_default  */
#endif

	if (poi_idx >= gCurPoiTblIdx) {
		fprintf(stderr, "ns_poi_reinit: idx %d has not been earlier initialized Exiting..\n", poi_idx);
                exit(-1);
	}

	//gPoiTable[poi_idx].r = gsl_rng_alloc (T);
	gPoiTable[poi_idx].mu = mu;
	gPoiTable[poi_idx].cur_sample = 0;

	//gsl_ran_poisson_array(gPoiTable[poi_idx].r, NS_MAX_POI_SAMPLES, gPoiTable[poi_idx].arr, mu);

}

int
ns_poi_sample(unsigned int pidx)
{
PoiInfoEntry *ptr;
int val;

	assert(pidx < ns_max_poi_process);

	ptr = &gPoiTable[pidx];
	if (ptr->cur_sample == NS_MAX_POI_SAMPLES) {
	    gsl_ran_poisson_array(ptr->r, NS_MAX_POI_SAMPLES, ptr->arr, ptr->mu);
	    ptr->cur_sample = 0;
	}

	//val = ptr->arr[ptr->cur_sample];
	val = (int)gsl_ran_exponential(ptr->r, ptr->mu);
	//ptr->cur_sample++;
	//round to nearest 10th as timers are run by truncating to nearest 10th ms.
   	val = ((((val % 10) <= 5)?0:1)+(val/10))*10;
	return (val);
}

int ns_exp_sample(unsigned int pidx)
{
  return ns_poi_sample(pidx);
}

#if TEST
int	main1(int argc, char** argv)
{
	int i;
	unsigned long av, min=0XFFFFFFFF, max=0;
	double mu = atof(argv[1]);
	unsigned int	arrSize = atoi(argv[2]);
	unsigned int*	arr = (void* )malloc(sizeof(int)* arrSize);

	/*	
	Generator: gsl_rng_mt19937 -- The MT19937 generator of Makoto Matsumoto and Takuji Nishimura 
	is a variant of the twisted generalized feedback shift-register algorithm, and is known as the
       	"Mersenne Twister" generator. It has a Mersenne prime period of 2^19937 - 1 (about 10^6000) 
	and is equi-distributed in 623 dimensions. It has passed the DIEHARD statistical tests. It 
	uses 624 words of state per generator and is comparable in speed to the other generators. 
	The original generator used a default seed of 4357 and choosing s equal to zero in 
	gsl_rng_set reproduces this. 

        For more information see, Makoto Matsumoto and Takuji Nishimura, "Mersenne Twister: A 
	623-dimensionally equidistributed uniform pseudorandom number generator". ACM Transactions
       	on Modeling and Computer Simulation, Vol. 8, No. 1 (Jan. 1998), Pages 3-30 
	*/

	const gsl_rng_type * T = gsl_rng_mt19937; /* this is also the "default" generator in GSL
						     -- gsl_rng_default  */
	gsl_rng * r = gsl_rng_alloc (T);

	gsl_ran_poisson_array(r, arrSize, arr, mu);
	for (i=0; i< arrSize; i++)
	{
		printf("%d:%f  ", arr[i], gsl_ran_poisson_pdf(arr[i], mu));
	}
	printf("\n\n");


	av = 0;
	for (i=0; i< arrSize; i++)
	{
		if (min > arr[i])
			min = arr[i];
		if (max < arr[i])
			max = arr[i];
		av += arr[i];
	}
	printf("mean = %d min=%lu max=%lu", av/arrSize, min, max);
	printf("\n\n");

	/*
	for (i=0; i< 10; i++)
	{
		printf("%d  ", gsl_ran_poisson(r, mu));
	}
	printf("\n");
	*/

	return 0;
}

main(int argc, char *argv[])
{
	int handle;
	int sum=0;
	int num;
	int i,j=0;
	double mu = 11.290;
	int loop = 1000;
	loop = atoi(argv[1]);
	mu = atof(argv[2]);

	
	//parallel = 100/mu +1;
	//mu /= parallel;
	handle = ns_poi_init(mu, 1);
	ns_poi_reinit(handle, mu);

	for (i = 0; i < loop; i++)
	{
		num = (ns_poi_sample(handle) );
		//printf("sample %d = %d\n",i ,num);
		//if ((num%10) <5) j = 0;
		//else j = 1;
		//num = (j+(num/10))*10;
		if (num%10) printf("num=%d at i=%d\n", num, i);
		sum += num;
	}
	printf("Sum = %d \n", sum);
}
#endif /* TEST */
