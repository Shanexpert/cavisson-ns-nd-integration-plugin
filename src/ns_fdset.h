#ifndef NS_FDSET_H
#define NS_FDSET_H

#include <stdlib.h>
#include <string.h>

typedef struct {
  char* bits;
  unsigned short max_bit;
  unsigned short bytes;
} NSFDSet;

inline NSFDSet* ns_fd_init(NSFDSet* set, unsigned int size);
void inline ns_fd_setbit(NSFDSet* set, unsigned int bit);
void inline ns_fd_unsetbit(NSFDSet* set, unsigned int bit);
int inline ns_fd_isset(NSFDSet* set, unsigned int bit);
void inline ns_fd_zero(NSFDSet* set);
void inline ns_fd_free(NSFDSet* set);

#endif
