
#ifndef NS_RANDOM_VARS_H
#define NS_RANDOM_VARS_H

#ifdef CAV_MAIN
#include "netstorm.h"
#endif

#define INIT_RANDOMVAR_ENTRIES 64
#define DELTA_RANDOMVAR_ENTRIES 32
#define INIT_RANDOMPAGE_ENTRIES 64
#define DELTA_RANDOMPAGE_ENTRIES 32


/*This function parses the format */
#define PARSE_FORMAT(buf)                                \
{                                                        \
  NSDL1_VARS(NULL, NULL, "Line Ptr = %c", *line_ptr);    \
  /* For the format to be detected we look for the '%' character and then we can have either a digit 
   * or alphanumeric character(d/ld/lu). If not found then we will give an error.
   */                                                    \
  if(format_begins)                                      \
  {                                                      \
    if(isdigit(*line_ptr))                               \
    {                                                    \
      /*Digit found after the percentage so filling it in the buffer*/ \
      buf[i] = *line_ptr;                                \
      continue;                                          \
    }                                                    \
    else if(isalpha(*line_ptr))                          \
    {                                                    \
      /* Alphanumeric character found so checking if it is d/ld/lu */\
      if(*line_ptr == 'd')                               \
      {                                                  \
        buf[i] = *line_ptr;                              \
        format_complete++;                               \
        format_begins = 0;                               \
        continue;                                        \
      }                                                  \
      else if(*line_ptr == 'l' && (*(line_ptr + 1) == 'u' || *(line_ptr + 1) == 'd' )) \
      {                                                  \
        buf[i] = *(line_ptr);                            \
        line_ptr++;                                      \
        i++;                                             \
        buf[i] = *(line_ptr);                            \
        format_complete++;                               \
        format_begins = 0;                               \
        continue;                                        \
      }                                                  \
      else if(*line_ptr == 'l' && *(line_ptr + 1) == 'l' && (*(line_ptr + 2) == 'u' || *(line_ptr + 2) == 'd' )) \
      {                                                  \
        buf[i] = *(line_ptr);                            \
        line_ptr++;                                      \
        i++;                                             \
        buf[i] = *(line_ptr);                            \
        format_complete++;                               \
        format_begins = 0;                               \
        continue;                                        \
      }                                                  \
      else                                               \
      {                                                  \
        fprintf(stderr, "%s Wrong format specified as after '%%' we expect either a digit or ld/lu/d specifiers\n", msg); \
        return -1;                                       \
      }                                                  \
    }                                                    \
    else                                                 \
    {                                                    \
      fprintf(stderr, "%s Wrong format specified as after '%%' we expect either a digit or ld/lu/d specifiers\n", msg); \
      return -1;                                         \
    }                                                    \
  }                                                      \
  else if(format_complete)                               \
  {                                                      \
    /* Checking if format is again given or not */       \
    if(*line_ptr == '%' && *(line_ptr + 1) != '%')       \
    {                                                    \
      fprintf(stderr, "%s Two Format specifiers are not allowed\n", msg); \
      return -1;                                         \
    }                                                    \
    else if(*line_ptr == '%' && *(line_ptr + 1) == '%')  \
    {                                                    \
      buf[i++] = *line_ptr;                              \
      line_ptr++;                                        \
      buf[i] = *line_ptr;                                \
      continue;                                          \
    }                                                    \
  }                                                      \
                                                         \
  if(*line_ptr == '%' && *(line_ptr + 1) != '%' && !format_complete)\
  {                                                      \
    buf[i] = *line_ptr;                                  \
    format_begins++;                                     \
    continue;                                            \
  }                                                      \
  buf[i] = *line_ptr;                                    \
}

typedef struct RandomVarTableEntry {
  ns_bigbuf_t name; /* index into the big buffer */
  int max;
  int min;
  ns_bigbuf_t format;
  int format_len;
  int sess_idx;
  int refresh;
} RandomVarTableEntry;
#if 0
typedef struct RandomVarTableEntry_Shr {
  char* var_name;
  int hash_idx;
  int max;
  int min;
  char *format;
  int format_len;
  int refresh;
} RandomVarTableEntry_Shr;
Moved to util.h 
#endif

//extern int find_randomvar_idx(char* name, int sess_idx);
extern int input_randomvar_data(char* line, int line_number, int sess_idx, char *script_filename);
extern RandomVarTableEntry_Shr *copy_randomvar_into_shared_mem(void);
extern void init_randomvar_info();
extern char* get_random_var_value(RandomVarTableEntry_Shr* var_ptr, VUser* vptr, int var_val_flag, int* total_len);
extern char* get_random_number( double min , double  max, char *format, int format_len, int *total_len, int malloc_or_static);
//prototype

#ifndef CAV_MAIN
extern RandomVarTableEntry* randomVarTable;
#else
extern __thread RandomVarTableEntry* randomVarTable;
#endif
//extern RandomVarTableEntry_Shr *randomvar_table_shr_mem;
extern int find_randomvar_idx(char* name, int sess_idx);
extern void clear_uvtable_for_random_var(VUser *vptr);
#endif /* RANDOM_VARS_H */

