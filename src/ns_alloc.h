
#ifndef __NS_ALLOC_H__
#define __NS_ALLOC_H__

#include "ns_data_types.h"
#include "nslb_alloc.h"

//extern void end_test_run();
// Added extern as many files are including ns_alloc.h but not util.h
extern unsigned char my_port_index; /* will remain -1 (255) for parent */
extern inline void kill_all_children(char *function_name, int line_num, char *file_name);

// For parent (my_port_index == 255) call kill_all_children

#define END_TEST_RUN {     \
  if(my_port_index == 255){ \
    kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__); \
    exit(-1);             \
  }                       \
  else \
    end_test_run(); \
}

//Every child will call this at the start so that parent allocated is not coming in nvm report
#define INIT_ALL_ALLOC_STATS {                          \
    g_c_mem_allocated = 0; \
    g_mem_allocated = 0; \
    g_c_mem_freed = 0; \
    g_mem_freed = 0; \
    g_c_alloc_count = 0;\
    g_alloc_count = 0;\
    g_c_free_count = 0;\
    g_free_count = 0;\
    g_c_shared_mem_alloc_count = 0;\
    g_shared_mem_alloc_count = 0;\
    g_c_shared_mem_allocated = 0;\
    g_shared_mem_allocated = 0;\
  }

// NVM will call this after sending progress report
#define INIT_PERIODIC_ALLOC_STATS {                             \
    g_mem_allocated = 0; \
    g_mem_freed = 0; \
    g_alloc_count = 0;\
    g_free_count = 0;\
    g_shared_mem_alloc_count = 0;\
    g_shared_mem_allocated = 0;\
  }

#define PRINT_AND_INIT_ALLOC_STATS \
    if(!global_settings->smon) \
      NSTL3(NULL, NULL, "NVM%d - MEM_STATS: \n\tCumulative allocated memory = %'.3f MB\n\tperiodic allocated memory = %'.3f MB\n\tCumulative freed memory = %'.3f MB\n\tperiodic freed memory = %'.3f MB\n\tCumulative Net allocated memory =  %'.3f MB\n\tPeriodic Net allocated memory =  %'.3f MB", my_port_index, BYTES_TO_MB(g_c_mem_allocated), BYTES_TO_MB(g_mem_allocated), BYTES_TO_MB(g_c_mem_freed), BYTES_TO_MB(g_mem_freed), BYTES_TO_MB((g_c_mem_allocated - g_c_mem_freed)),  BYTES_TO_MB((g_mem_allocated - g_mem_freed))); \
    INIT_PERIODIC_ALLOC_STATS;

//#define BYTES_TO_MB(size) (double )(((double )size)/(1024*1024))

#define MY_MALLOC(new, size, msg, index) NSLB_MALLOC(new, size, msg, index, NULL)

#define MY_MALLOC_NO_EXIT(new, size, msg) NSLB_MALLOC_NO_EXIT(new, size, msg, NULL)

#define MY_MALLOC_AND_MEMSET(buf, size, msg, index)  NSLB_MALLOC_AND_MEMSET(buf, size, msg, index, NULL)

#define MALLOC_AND_COPY(src, dest, size, msg, index)  NSLB_MALLOC_AND_COPY(src, dest, size, msg, index, NULL)

#define MY_MALLOC_AND_MEMSET_WITH_MINUS_ONE(buf, size, msg, index)  NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(buf, size, msg, index, NULL)



#define MY_REALLOC(buf, size, msg, index) NSLB_REALLOC(buf, size, msg, index, NULL)

#define MY_REALLOC_NO_EXIT(new, size, msg) NSLB_REALLOC_NO_EXIT(new, size, msg, NULL)

#define MY_REALLOC_AND_MEMSET(buf, size, old_size, msg, index)  NSLB_REALLOC_AND_MEMSET(buf, size, old_size, msg, index, NULL)

#define REALLOC_AND_COPY(src, dest, size, msg, index)   NSLB_REALLOC_AND_COPY(src, dest, size, msg, index, NULL)

// Call this macro if old size is known. If buf is NULL, then old size will also be 0
#define MY_REALLOC_EX(buf, size, old_size, msg, index)  NSLB_REALLOC_EX(buf, size, old_size, msg, index, NULL)

#define MY_REALLOC_AND_MEMSET_EX(buf, size, old_size, msg, index)  NSLB_REALLOC_AND_MEMSET_EX(buf, size, old_size, msg, index, NULL)

#define MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(buf, size, old_size, msg, index)  NSLB_REALLOC_AND_MEMSET_WITH_MINUS_ONE(buf, size, old_size, msg, index, NULL)


#define FREE_AND_MAKE_NULL(to_free, msg, index)  NSLB_FREE_AND_MAKE_NULL(to_free, msg, index, NULL)

#define FREE_AND_MAKE_NOT_NULL(to_free, msg, index)  NSLB_FREE_AND_MAKE_NOT_NULL(to_free, msg, index, NULL)

/* These macro are to be used when size of memory to be freed to known.
 * Previously we were calculating the size after freeing char array which caused core dump, therefore INC_AND_LOG_FREE_STATS(size) macro has been placed before making it (to_free) free. */

#define FREE_AND_MAKE_NULL_EX(to_free, size, msg, index)  NSLB_FREE_AND_MAKE_NULL_EX(to_free, size, msg, index, NULL)

#define FREE_AND_MAKE_NOT_NULL_EX(to_free, size, msg, index)  NSLB_FREE_AND_MAKE_NOT_NULL_EX(to_free, size, msg, index, NULL)

#endif
