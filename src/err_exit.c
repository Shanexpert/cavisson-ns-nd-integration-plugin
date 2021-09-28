#include <stdio.h>
#include <stdlib.h>

inline void *
Malloc (int size, char *msg) 
{
void *ptr;
	ptr = malloc(size);
	if (!ptr) {
	    fprintf(stderr, "out of memory: malloc failed for %s", msg);
	    exit(1);
	}
	return ptr;
}

inline void *
Realloc (void *in, int size, char *msg) 
{
void *ptr;
	ptr = realloc(in, size);
	if (!ptr) {
	    fprintf(stderr, "out of memory: realloc failed for %s", msg);
	    exit(1);
	}
	return ptr;
}
