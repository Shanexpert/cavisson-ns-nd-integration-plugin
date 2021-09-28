#include <stdio.h>
#include <stdlib.h>
#include "ns_fdset.h"
#include "ns_alloc.h"
#include "ns_log.h"

extern void end_test_run( void );

inline NSFDSet*
ns_fd_init(NSFDSet* set, unsigned int size) {
  int size_bytes;

  size_bytes = size >> 3;
  
  if (size%8)
    size_bytes++;

  set->max_bit = size;
  set->bytes = size_bytes;
  MY_MALLOC (set->bits, size_bytes, "set->bits", -1);
  if (set->bits)
    return set;
  else
    return NULL;
}

void inline
ns_fd_setbit(NSFDSet* set, unsigned int bit) {
  int bit_offset = bit >> 3;
  int bit_num = bit%8;

  if (bit < set->max_bit)
    set->bits[bit_offset] = set->bits[bit_offset] | (1 << bit_num);
}

void inline
ns_fd_unsetbit(NSFDSet* set, unsigned int bit) {
  int bit_offset = bit >> 3;
  int bit_num = bit%8;

  if (bit < set->max_bit)
    set->bits[bit_offset] = set->bits[bit_offset] & (~(1<<bit_num));
}

int inline
ns_fd_isset(NSFDSet* set, unsigned int bit) {
  int bit_offset = bit >> 3;
  int bit_num = bit%8;
  
  if (bit < set->max_bit)
    return set->bits[bit_offset] & (1 << bit_num);
  else
    return 0;
}

void inline
ns_fd_zero(NSFDSet* set) {
  memset(set->bits, 0, set->bytes);
}

void inline
ns_fd_free(NSFDSet* set) {
  FREE_AND_MAKE_NULL(set->bits, "set->bits", -1);
}
