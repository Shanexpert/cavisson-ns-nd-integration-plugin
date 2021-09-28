#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//#include <db.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <libpq-fe.h>
#include "nd_aggregate_reports.h"
#include<assert.h>

#define MAX_BUCKET_COUNT 10
typedef struct NDBucket {
  int min;
  int max;
}NDBucket;

int totalAggCount = 0;
int maxAggCount = 0;
int  max_aggr_entries = 0;
int  total_aggr_entries = 0;

#define SUCCESS 0
#define  MAX_COLUMN 52
#define  MAX_ROW 100
#define MAX_ARRAY_LENGTH 10
#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__
#define DELTA_AGGR_ENTRIES 1000


/************************************************************
 Macros for finding the min, max & avg value                *
************************************************************/
#define SET_MIN(a,b) ((a) < (b) ? a : b) 
#define SET_MAX(a,b) ((a) > (b) ? a : b)
#define SET_AVG(previous_value, current_value)  (previous_value = (previous_value + current_value)/2)

#define SET_STRING_FIELD(ptr, big_buff, col, value, index)\
{\
   strcpy(ptr,value);\
   if(ptr==big_buff)\
     setNVColumnString(&col[index], ptr, 1);\
   else\
     setNVColumnString(&col[index], ptr, 0);\
   ptr+=strlen(value)+1;\
}

/*************************************************
 Macro for malloc                                *
*************************************************/
#define MY_MALLOC_(new, size, cptr, msg) {                               \
    if (size < 0) {                                                     \
      exit(1);                                                          \
    } else if (size == 0) {                                             \
      new = NULL;                                                       \
    } else {                                                            \
      new = (void *)malloc( size );                                     \
      if ( new == (void*) 0 ) {                                         \
        exit(1);                                                        \
      }                                                                 \
    }                                                                   \
}

#define MY_REALLOC_(buf, size, cptr, msg) {                              \
    if (size <= 0) {                                                    \
      exit(1);                                                          \
    } else {                                                            \
      buf = (void*)realloc(buf, size);                                  \
      if ( buf == (void*) 0 ) {                                         \
        exit(1);                                                        \
      }                                                                 \
    }                                                                   \
  }



/***************************************
  Function that increase bucket count  *
***************************************/
int select_bucket(int bucket_duration ,int num_bucket,int value )
{
 int i;
 int  bucket_end ;
 for(i=0 ;i<num_bucket;i++)
 {
   bucket_end= bucket_duration*(i+1);
   if(value < bucket_end)
     return i;
 }
 //In case if not fit in any bucket that return the last one.
 return num_bucket - 1;
} 


/*********************************************
Structure for timing data                    *
*********************************************/   
typedef struct timingdata {
  int min;
  int max;
  int avg;
  int count;
}timingdata;


/*********************************************
Structure for aggregate table                *
*********************************************/ 
typedef struct AggrTableDetail {
 unsigned long timestamp ;
 long btid;
 int totalcount;
 int percentile_50;
 int percentile_75;
 int percentile_80;
 int percentile_85;
 int percentile_90;
 int percentile_95;
 int percentile_99;
 int ff1;
 int ff2;
 int ff3;
 int ff4;
 char ffs1[256];
 char ffs2[256];
 char ffs3[256];
 char ffs4[256];
 }AggrTableDetail;

AggrTableDetail *aggrTableDetail;



/************************************************
Method to create aggregate table entry          *
************************************************/
int create_aggregate_table_entry(int* row_num)
{
  if (total_aggr_entries == max_aggr_entries)
  {
     MY_REALLOC_(aggrTableDetail, (max_aggr_entries + DELTA_AGGR_ENTRIES) * sizeof(AggrTableDetail), NULL, "aggregate table entries");
     max_aggr_entries += DELTA_AGGR_ENTRIES;
  }
  *row_num = total_aggr_entries++;
  return (*row_num);
}


/***************************************************
Macro to fill timing data in a table               *
***************************************************/
#define FILL_ND_TIMING(col, timing, index)\
{\
  setNVColumnNumber(&col[index], (int)timing.min);\
  setNVColumnNumber(&col[index+1], timing.max);\
  setNVColumnNumber(&col[index+2], timing.count?(timing.avg/timing.count):0);\
}


/**********************************************
Macro to set min, max, avg, count in a table  *
**********************************************/
#define SET_MIN_MAX_AVG_COUNT(outres, col, j, start_index, Timing, idx)\
{\
  setNVColumnNumber(&col[idx], SET_MIN(nvGetValue(outres, j,start_index)->value.num , Timing.min));\
  setNVColumnNumber(&col[idx++], SET_MAX(nvGetValue(outres, j,start_index+1)->value.num ,Timing.max));\
  setNVColumnNumber(&col[idx++], SET_AVG(start_index+2, outres));\
  Timing.count= nvGetValue((outres, j, start_index+3)->value.num)++;\
  nvAddRow(outres, row);\
}




/************************************************************************
Macro to fill timing data in structure aggregate table                  *
************************************************************************/
#define FILL_TIMING_FOR_STRUCT(value, timing) \
{ \
  if(value >= 0)\
  {\
    timing.min= value;\
    timing.max= value;\
    timing.avg= value;\
    timing.count= 1;\
  }\
}

#define NVRES_NUM(res, row, col) nvGetValue(res, row, col)->value.num
#define MERGE_TIMING(out, in) \
{\
  if(out.min > in.min)  \
    out.min = in.min;   \
  if(out.max < in.max)  \
    out.max = in.max;   \
  if(in.count + out.count > 0)  \
  {     \
    out.avg = (in.avg*in.count + out.avg*out.count)/(in.count + out.count);     \
    out.count = out.count + in.count;   \
  }     \
}

/*********************************************************************
Macro to set timing data in table when we merge the aggregates table *
*********************************************************************/
#define SET_TIMING_VALUES(TimingDataIN, TimingDataOUT, data, i, j, index, firstCol)\
{\
    TimingDataIN.min = NVRES_NUM(data, i, index);\
    TimingDataIN.max = NVRES_NUM(data, i, index+1);\
    TimingDataIN.avg = NVRES_NUM(data, i, index +2);\
    TimingDataIN.count = NVRES_NUM(data, i, index +3);\
    TimingDataOUT.min = NVRES_NUM(out, j, index);\
    TimingDataOUT.max = NVRES_NUM(out, j, index+1);\
    TimingDataOUT.avg = NVRES_NUM(out, j, index+2);\
    TimingDataOUT.count = NVRES_NUM(out, j, index+3);\
    MERGE_TIMING(TimingDataOUT, TimingDataIN);\
    setNVColumnNumber(firstCol[index], (int)TimingDataOUT.min);\
    setNVColumnNumber(firstCol[index+1], TimingDataOUT.max);\
    setNVColumnNumber(firstCol[index+2], TimingDataOUT.avg);\
    setNVColumnNumber(firstCol[index+3], TimingDataOUT.count);\
    int bid;\
    for(bid=0;bid<MAX_BUCKET_COUNT;bid++){\
     setNVColumnNumber(&col[index+4+bid],((nvGetValue(out, j, index+4+bid)->value.num)+(nvGetValue(data, i, index+4+bid)->value.num)));\
    }\
}

/*****************************************************************************
Macro to set min, max, avg, count & bucket value in structure aggregatetable *
*****************************************************************************/
#define SET_MIN_MAX_AVG_COUNT_FOR_STRUCT(value, aggValue)\
{\
  if(value >= 0)\
  {\
    aggValue.min = SET_MIN(aggValue.min, value);\
    aggValue.max = SET_MAX(aggValue.max, value);\
    aggValue.avg += value; \
    aggValue.count++;\
  }\
}



//Data collection query:
//select timestamp, flowpathinstance, httpheadername, httpheadervalue, tierid, serverid
/**************************************
ENUM for data collection query        *
**************************************/
typedef enum {
  end_timestamp,
  end_btid,
  end_50_percentile,
  end_80_percentile,
  end_85_percentile,
  end_90_percentile,
  end_95_percentile,
  end_99_percentile
} NDQuery;


/*********************************
ENUM for aggregate table records *
*********************************/
typedef enum {
  eout_timestamp,
  eout_btid,
  eout_totalcount,
  eout_50_percentile,
  eout_80_percentile,
  eout_85_percentile,
  eout_90_percentile,
  eout_95_percentile,
  eout_99_percentile,
  eout_ff1,
  eout_ff2,
  eout_ff3,
  eout_ff4,
  eout_ffs1,
  eout_ffs2,
  eout_ffs3,
  eout_ffs4,
  TotalOutEntries
}NDOutres;
 

inline void get_token(char *str, char delimiter, int fields[], int *numFields)
{

  char *ptr = str;

  int num = 0;
  *numFields = 0;

  while (ptr) {

    fields[num++] = atoi(ptr);

    ptr = strchr(ptr, delimiter);

    if(ptr) {

      *ptr = 0;

      ptr++;

    }
  }
  *numFields = num;
}

/***************************************************
Parser that create a aggregate table from raw data * 
***************************************************/
NVResultSet *nd_agg_parser(NVResultSet *data, int *numoutresultset, long timestamp)
{
  NVResultSet *outres;
  outres = getNVResultSet(TotalOutEntries, 100);
  *numoutresultset = 1;
  NVColumn col[TotalOutEntries];
  int rnum= 0;
  int idx = 0;
  int i, j, k;

  //reset total aggregate entries.
  total_aggr_entries = 0;
  
  //check weather response time is enable or disable ,if not disable all the responsetime keywords
  ND_LOG1("Method called, Total row records - %d", data->numRow);
  for(i = 0; i < data->numRow; i++)
  {
    long btID = atoll(nvGetValue(data, i, end_btid)->value.str);
    unsigned long absolutetime = atoll(nvGetValue(data, i, end_timestamp)->value.str);
    int responsetime50 = atoi(nvGetValue(data, i, end_50_percentile)->value.str);
    //int responsetime75 = atoi(nvGetValue(data, i, end_75_percentile)->value.str);
    int responsetime80 = atoi(nvGetValue(data, i, end_80_percentile)->value.str);
    int responsetime85 = atoi(nvGetValue(data, i, end_85_percentile)->value.str); 
    int responsetime90 = atoi(nvGetValue(data, i, end_90_percentile)->value.str); 
    int responsetime95 = atoi(nvGetValue(data, i, end_95_percentile)->value.str); 
    int responsetime99 = atoi(nvGetValue(data, i, end_99_percentile)->value.str); 

    //match on the basis of customdataid and btid
    //If match found then update timing details
    for(j = 0; j < total_aggr_entries; j++)
    {    
      if(aggrTableDetail[j].btid == btID)
      {
         //update totalcount 
         aggrTableDetail[j].totalcount++;

         //update 50 percentile
         aggrTableDetail[j].percentile_50 += responsetime50;
      
         //update 75 percentile
         //aggrTableDetail[j].percentile_75 += responsetime75;
   
         //update 80 percentile
         aggrTableDetail[j].percentile_80 += responsetime80;
       
         //update 85 percentile
         aggrTableDetail[j].percentile_85 += responsetime85;
 
         //update 90 percentile
         aggrTableDetail[j].percentile_90 += responsetime90;
 
         //update 95 percentile
         aggrTableDetail[j].percentile_99 += responsetime99;
         break;
      }
    }
    //If matched with any record then already done.
    if(j != total_aggr_entries)
      continue;
   
    //when match not found
    int idx= create_aggregate_table_entry(&rnum);
    memset(&aggrTableDetail[idx], 0, sizeof(AggrTableDetail));
    aggrTableDetail[idx].btid = btID;
    aggrTableDetail[idx].totalcount = 1;
    aggrTableDetail[idx].timestamp = absolutetime;
    aggrTableDetail[idx].percentile_50 = responsetime50;
    //aggrTableDetail[idx].percentile_75 = responsetime75;
    aggrTableDetail[idx].percentile_80 = responsetime80;
    aggrTableDetail[idx].percentile_85 = responsetime85;
    aggrTableDetail[idx].percentile_90 = responsetime90;
    aggrTableDetail[idx].percentile_99 = responsetime99;
  }        
  
  int total_string_length_field;
  char *big_buff = NULL;
  char *ptr = NULL;  
  for(k = 0; k < total_aggr_entries; k++)
  { 
    total_string_length_field = strlen("") + strlen("") + strlen("")+ strlen("") + 4;//+1 for each null
    big_buff= malloc(total_string_length_field);
    ptr=big_buff;
 
    setNVColumnNumber(&col[eout_timestamp], timestamp);
    //setNVColumnNumber(&col[eout_absolutetimestamp], (aggrTableDetail[k].timestamp+1486462913));
    setNVColumnNumber(&col[eout_btid], aggrTableDetail[k].btid);
    setNVColumnNumber(&col[eout_totalcount], aggrTableDetail[k].totalcount);
    setNVColumnNumber(&col[eout_50_percentile], aggrTableDetail[k].percentile_50);
    //setNVColumnNumber(&col[eout_75_percentile], aggrTableDetail[k].percentile_75);
    setNVColumnNumber(&col[eout_80_percentile], aggrTableDetail[k].percentile_80);
    setNVColumnNumber(&col[eout_85_percentile], aggrTableDetail[k].percentile_85);
    setNVColumnNumber(&col[eout_90_percentile], aggrTableDetail[k].percentile_90);
    setNVColumnNumber(&col[eout_95_percentile], aggrTableDetail[k].percentile_95);
    setNVColumnNumber(&col[eout_99_percentile], aggrTableDetail[k].percentile_99);
    setNVColumnNumber(&col[eout_ff1], 0);
    setNVColumnNumber(&col[eout_ff2], 0);
    setNVColumnNumber(&col[eout_ff3], 0);
    setNVColumnNumber(&col[eout_ff4], 0);
    SET_STRING_FIELD(ptr, big_buff, col, "", eout_ffs1);
    SET_STRING_FIELD(ptr, big_buff, col, "", eout_ffs2);
    SET_STRING_FIELD(ptr, big_buff,col,  "", eout_ffs3);
    SET_STRING_FIELD(ptr, big_buff,col,  "", eout_ffs4);
    nvAddRow(outres, col);
  }
  ND_LOG1("Aggregate Records Processed, raw records - %d, aggregate records - %d, bucketid - %d", data->numRow, outres->numRow, timestamp);
  return outres;
}

/*********************************************
Parser to merge the aggregate tables         *
*********************************************/
NVResultSet *nd_merge_agg_record(NVResultSet* out, NVResultSet *data, int numResultSet)
{
  int i, j, k;
  NVColumn col[TotalOutEntries];
  NVColumn *firstCol;
  timingdata timetoloadIN, timetoloadOUT;
  timingdata timetodomcompleteIN, timetodomcompleteOUT;
  timingdata firstbytetimeIN, firstbytetimeOUT;
  timingdata contentloadtimeIN, contentloadtimeOUT;
  timingdata loadeventtimeIN, loadeventtimeOUT;
  timingdata domtimeIN, domtimeOUT;
  timingdata unloadtimeIN, unloadtimeOUT;
  timingdata redirectiontimeIN, redirectiontimeOUT;
  timingdata cachelookuptimeIN, cachelookuptimeOUT;
  timingdata fetchtimeIN, fetchtimeOUT;
  timingdata dnstimeIN, dnstimeOUT;
  timingdata connectiontimeIN, connectiontimeOUT;
  timingdata secureconnectiontimeIN, secureconnectiontimeOUT;
  timingdata dom_processingIN, dom_processingOUT;
  timingdata front_end_networkIN, front_end_networkOUT;
  timingdata serverresponsetimeIN, serverresponsetimeOUT;
  timingdata networkIN, networkOUT;

  /*for(i = 0; i < data->numRow; i++)
  {
    //check if pageid exist in out then merge in that else add new entry.  
    for(j = 0; j < out->numRow; j++)
    {
      if(
        (!compareNVColumn(nvGetValue(out, j, eout_page_id), nvGetValue(data, i, eout_page_id))) 
        && (!compareNVColumn(nvGetValue(out, j, eout_location_id), nvGetValue(data, i, eout_location_id))) 
        && (!compareNVColumn(nvGetValue(out, j, eout_osid), nvGetValue(data, i, eout_osid)))
      )
      {
        firstCol = nvGetValue(out, j, 0);
        setNVColumnNumber(&firstCol[eout_pagecount], NVRES_NUM(out, j, eout_pagecount) + NVRES_NUM(data, i, eout_pagecount)); 
        SET_TIMING_VALUES(timetoloadIN, timetoloadOUT, data, i, j, timetoload_min, &firstCol);
        SET_TIMING_VALUES(timetodomcompleteIN, timetodomcompleteOUT, data, i, j,timetodomcomplete_min,&firstCol);
        SET_TIMING_VALUES(firstbytetimeIN, firstbytetimeOUT, data, i, j, firstbytetime_min, &firstCol);
        SET_TIMING_VALUES(contentloadtimeIN, contentloadtimeOUT, data, i, j, contentloadtime_min, &firstCol);
        SET_TIMING_VALUES(loadeventtimeIN, loadeventtimeOUT, data, i, j, loadeventtime_min, &firstCol);
        SET_TIMING_VALUES(domtimeIN, domtimeOUT, data, i, j, domtime_min, &firstCol);
        SET_TIMING_VALUES(unloadtimeIN, unloadtimeOUT, data, i, j, unloadtime_min, &firstCol);
        SET_TIMING_VALUES(redirectiontimeIN, redirectiontimeOUT, data, i, j, redirectiontime_min, &firstCol);
        SET_TIMING_VALUES(cachelookuptimeIN, cachelookuptimeOUT, data, i, j, cachelookuptime_min, &firstCol);
        SET_TIMING_VALUES(fetchtimeIN, fetchtimeOUT, data, i, j, fetchtime_min, &firstCol);
        SET_TIMING_VALUES(dnstimeIN, dnstimeOUT, data, i, j, dnstime_min, &firstCol); 
        SET_TIMING_VALUES(connectiontimeIN, connectiontimeOUT, data, i, j, connectiontime_min, &firstCol);
        SET_TIMING_VALUES(secureconnectiontimeIN, secureconnectiontimeOUT, data, i, j, secureconnectiontime_min, &firstCol);
        SET_TIMING_VALUES(dom_processingIN, dom_processingOUT, data, i, j, dom_processing_min, &firstCol);
        SET_TIMING_VALUES(front_end_networkIN, front_end_networkOUT, data, i, j, front_end_network_min, &firstCol);
        SET_TIMING_VALUES(serverresponsetimeIN, serverresponsetimeOUT, data, i, j,  serverresponsetime_min, &firstCol);
        SET_TIMING_VALUES(networkIN, networkOUT, data, i, j, network_min, &firstCol);
        //update response bucket
        for(k = responsebucket_b1; k < MAX_BUCKET_COUNT; k++)
        {
          setNVColumnNumber(&firstCol[k], NVRES_NUM(out, j, k) + NVRES_NUM(data, i, k));
        }
        break;
      }
    }
      //NNo matching entry foud.
      if(j == out->numRow)
      {
        NVColumn *data_col; 
        //add new entry
        for(k=0; k<TotalOutEntries; k++)
        {
          if(
            (k == eout_device_type)||(k == eout_ffs1)||(k == eout_ffs2)||(k == eout_ffs3)||(k == eout_ffs4)
            )
            {
              data_col = nvGetValue(data, i, k);
              setNVColumnString(&col[k], data_col->value.str, data_col->free);
              data_col->free = 0; 
              continue;
            }
            setNVColumnNumber(&col[k], nvGetValue(data, i, k)->value.num);
        }
        nvAddRow(out, col);
      }
  }
  return out;
*/
}  

int nd_parse_profile_keywords(char *conf_file, char *error_string)
{

  return 0;
}
