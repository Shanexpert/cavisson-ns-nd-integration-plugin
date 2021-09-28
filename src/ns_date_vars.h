
#ifndef NS_DATE_VARS_H
#define NS_DATE_VARS_H

#define INIT_DATEVAR_ENTRIES 10 
#define DELTA_DATEVAR_ENTRIES 5
#define DELTA_HOLIDAY_ENTRIES 5

#define ALL_DAYS 0
#define WORKING_DAYS 1
#define NON_WORKING_DAYS 2

#define NO  0
#define YES 1

#define SUNDAY 0
#define MONDAY 1
#define TUESDAY 2
#define WEDNESDAY 3
#define THRUSDAY 4
#define FRIDAY 5
#define SATURDAY 6

//MaxDays Range 
#define ONE_YEAR 365

typedef struct DateVarTableEntry {
  ns_bigbuf_t name; /* index into the big buffer */
  int date_time;
  ns_bigbuf_t format;
  int format_len;
  int sess_idx;
  int refresh;
  //int days; //??? for what
  int time_offset;
  int day_type;
  int unique_date;
  int day_offset;
  int min_days;
  int max_days;
} DateVarTableEntry;

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

typedef struct Cav_holi_days
{
  //To save the date.We are taking size 11 array.
  //Assuming date will be MM/DD/YYYY 2+1+2+1+4+1
  char date[11];
  char description[2 * 1024]; // Description
}Cav_holi_days;

extern DateVarTableEntry_Shr* copy_datevar_into_shared_mem (void);
#ifndef CAV_MAIN
extern DateVarTableEntry* dateVarTable;
#else
extern __thread DateVarTableEntry* dateVarTable;
#endif
extern int input_datevar_data(char* line, int line_number, int sess_idx, char *script_filename);
extern DateVarTableEntry_Shr *copy_Datevar_into_shared_mem(void);
extern void init_datevar_info();
extern char* get_date_var_value(DateVarTableEntry_Shr* var_ptr, VUser* vptr, int var_val_flag, int* total_len);
extern int find_datevar_idx(char* name, int sess_idx);
extern void fill_uniq_date_var_data ();
extern void clear_uvtable_for_date_var(VUser *vptr);
#endif /* DATE_VARS_H */

