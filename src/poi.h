	
#ifndef POI_H
#define POI_H
extern int ns_rand_init(unsigned long seed, unsigned long max);
extern unsigned long ns_get_random(int rand_idx);
extern unsigned long ns_get_random_max(int rand_idx, int max);
extern int ns_poi_init(double mu, unsigned long seed);
extern void ns_poi_reinit(unsigned int poi_idx, double mu);
extern int ns_poi_sample(unsigned int pidx);
extern int ns_exp_init(double mu, unsigned long seed);
extern int ns_exp_sample(unsigned int pidx);

#endif
