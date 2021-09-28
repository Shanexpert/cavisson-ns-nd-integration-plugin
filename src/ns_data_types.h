#ifndef NS_DATA_TYPES_H
#define NS_DATA_TYPES_H 

#if ( (Fedora && RELEASE >= 14) || (Ubuntu && RELEASE >= 1204) )
  #define NS_BUILD_BITS 64
#else
  #define NS_BUILD_BITS 32
#endif


// Used for GDF
// /* typedef unsigned long Long_data; */
// /* typedef unsigned long long  Long_long_data; */
typedef double Long_data;
typedef double Long_long_data;
typedef short int Short_data;
typedef int Int_data;
typedef char Char_data;
/***********************************************************************
 NaN(nan/-nan) handling:
   NaN means "Not a Number"
   Currently NaN handling is only supporetd in double/float type numbers
typedef char Char_data;
   To support NaN in short/int/long, we assume least value of that type 
   Macros INT_NAN, SHORT_NAN, LONG_NAN used to support NaN data 
***********************************************************************/
#define INT_NaN	      -0x7fffffff 
#define SHORT_NaN     -0x7fff 
#define LONG_NaN      -0x7fffffffffffffffL 
#define CHAR_NaN      -0x7f
 
typedef long          ns_bigbuf_t; // Index in the allocated bigbuf which gets converted to pointer in bigbuf shared memory
typedef long          ns_str_ent_t; // record num in the allocated str ent (segment table) which gets converted to pointer in shm
typedef long          ns_tmpbuf_t; // Index in the allocated g_temp_buf
typedef long          ns_ptr_t;
typedef unsigned long u_ns_ptr_t;
  
/* We should use u_ns_ts_t where we are calculating time using get_ms_stamp 
 * & for size NS_TIME_DATA_SIZE.
 * Currently we are taking it as unsigned int if in future we need LONG LONG we should change following: 
 * 1. In database table data type is int.
 * 2. In function translate_time_data_size*/
typedef unsigned long long       u_ns_ts_t;  
#define NS_TIME_DATA_SIZE   sizeof(u_ns_ts_t)
  
// Note that on 64 bit OS, long and long long are 8 bytes
typedef unsigned int  u_ns_4B_t;   // 4 bytes unsigned
typedef unsigned long long u_ns_8B_t; // 8 bytes unsigned

typedef long long ns_8B_t; // 8 bytes signed long long

typedef unsigned char u_ns_char_t; // Unsigned char data type 

#endif
