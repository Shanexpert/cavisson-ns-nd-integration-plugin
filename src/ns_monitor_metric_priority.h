#ifndef NS_MONITOR_METRIC_PRIORITY
#define NS_MONITOR_METRIC_PRIORITY 

/* Metric Priority */
#define METRIC_PRIORITY_HIGH                   0
#define METRIC_PRIORITY_MEDIUM                 1
#define METRIC_PRIORITY_LOW                    2
#define MAX_METRIC_PRIORITY_LEVELS             3 //H,M and L

#define MAX_HML_VECTORS                        16
#define MAX_GP_LINE_LEN                        1024
#define MAX_VECTOR_NAME_LEN                    256
#define HML_METRIC_DELTA_SIZE                  4096

typedef struct 
{
  char *group_line;
  char *graph_lines;
  char *vector_lines;
  int num_vectors;
  int group_line_size;
  int graph_lines_size;
  int vector_lines_size;
  int graph_lines_write_index;
  int vector_lines_write_index;
} HMLMetric;

#define CREATE_HML_METRIC_ARR(num_entries) \
{ \
  int i; \
  MY_MALLOC_AND_MEMSET(hml_metrics, sizeof(HMLMetric) * MAX_METRIC_PRIORITY_LEVELS, "HMLMetric", -1); \
  for(i = 0; i < MAX_METRIC_PRIORITY_LEVELS; i++) \
  { \
    MY_MALLOC_AND_MEMSET(hml_metrics[i].group_line, MAX_GP_LINE_LEN, "HMLMetric Group Line", -1); \
    MY_MALLOC_AND_MEMSET(hml_metrics[i].graph_lines, num_entries * MAX_GP_LINE_LEN, "HMLMetric Graph Line", -1); \
    MY_MALLOC_AND_MEMSET(hml_metrics[i].vector_lines, num_entries * MAX_VECTOR_NAME_LEN, "HMLMetric Vector List", -1); \
    hml_metrics[i].group_line_size = MAX_GP_LINE_LEN; \
    hml_metrics[i].graph_lines_size = num_entries * MAX_GP_LINE_LEN; \
    hml_metrics[i].vector_lines_size = num_entries * MAX_VECTOR_NAME_LEN; \
  } \
}

#define RESET_HML_METRIC_ARR \
{ \
  int i; \
  if(hml_metrics) \
  { \
    NSDL2_GDF(NULL, NULL, "RESET_HML_METRIC_ARR: reset hml_metrics"); \
    for(i = 0; i < MAX_METRIC_PRIORITY_LEVELS; i++) \
    { \
      hml_metrics[i].group_line[0] = 0; \
      hml_metrics[i].graph_lines[0] = 0; \
      hml_metrics[i].vector_lines[0] = 0; \
      hml_metrics[i].num_vectors = 0; \
      hml_metrics[i].graph_lines_write_index = 0; \
      hml_metrics[i].vector_lines_write_index = 0; \
    } \
  } \
}

#define FREE_HML_METRIC_ARR \
{ \
  int i; \
  if(hml_metrics) \
  { \
    for(i = 0; i < MAX_METRIC_PRIORITY_LEVELS; i++) \
    { \
      if(hml_metrics[i].group_line) \
        FREE_AND_MAKE_NULL_EX(hml_metrics[i].group_line, hml_metrics[i].group_line_size, "Releasing memory for HMLMetric Group Line", -1); \
      if(hml_metrics[i].graph_lines) \
        FREE_AND_MAKE_NULL_EX(hml_metrics[i].graph_lines, hml_metrics[i].graph_lines_size, "Releasing memory for HMLMetric Graph Line", -1); \
      if(hml_metrics[i].vector_lines) \
        FREE_AND_MAKE_NULL_EX(hml_metrics[i].vector_lines, hml_metrics[i].vector_lines_size, "Releasing memory for HMLMetric Vector List", -1); \
      hml_metrics[i].group_line_size = 0; \
      hml_metrics[i].graph_lines_size = 0; \
      hml_metrics[i].vector_lines_size = 0; \
      hml_metrics[i].graph_lines_write_index = 0; \
      hml_metrics[i].vector_lines_write_index = 0; \
    } \
    FREE_AND_MAKE_NULL_EX(hml_metrics, hml_metrics, "Releasing memory for HMLMetric", -1); \
  } \
}

extern char get_metric_priority_by_id(int id);

#define FILL_HML_GROUP_LINE(group_ptr) \
{ \
  int i; \
  if(hml_metrics) \
  { \
    for(i = 0; i < MAX_METRIC_PRIORITY_LEVELS; i++) \
    { \
      NSDL3_GDF(NULL, NULL, "FILL_HML_GROUP_LINE: i = %d, num_vectors = %d", i, hml_metrics[i].num_vectors);\
      if(hml_metrics[i].num_vectors) \
        sprintf(hml_metrics[i].group_line, "Group|%s|%d:%c|%s|%d|%d|%s|%s%s|%s", \
               group_ptr->group_name, group_ptr->rpt_grp_id, get_metric_priority_by_id(i), "vector", \
               group_ptr->num_actual_graphs[i], hml_metrics[i].num_vectors, \
               group_ptr->groupMetric, STORE_PREFIX, group_ptr->Hierarchy, \
               group_ptr->group_description); \
    } \
  } \
}

/* #define FILL_HML_GRAPH_LINE(idx, graph_ptr) \
{ \
  if(group_ptr->num_vectors[idx]) \
    sprintf(hml_metrics[idx].graph_line, "Graph|%s|%hu|%s|%s|%u|%s|%hu|%s|%d|%d|%s|%s|%s", \
             graph_ptr->graph_name, graph_ptr->rpt_id, (graph_ptr->graph_type == GROUP_GRAPH_TYPE_SCALAR)?"scalar":"vector", \
             num_to_data_type(graph_ptr->data_type), graph_ptr->graph_msg_index[idx], \
             num_to_formula(graph_ptr->formula, formula), graph_ptr->num_vectors, \
             num_to_graph_state(graph_ptr->graph_state), \
             graph_ptr->pdf_id, graph_ptr->pdf_data_index, graph_ptr->metric_priority, graph_ptr->field_13, \
             graph_ptr->graph_discription); \
}*/

#define FILL_HML_GRAPH_LINE(group_ptr, graph_ptr) \
{ \
  int i; \
  int mp; \
  int wbytes = 0; \
  if(hml_metrics) \
  { \
    NSDL3_GDF(NULL, NULL, "FILL_HML_GRAPH_LINE: group_ptr = %p, graph_ptr = %p", group_ptr, graph_ptr); \
    for(i = 0; i < group_ptr->num_graphs; i++, graph_ptr++) \
    { \
      mp = graph_ptr->metric_priority; \
      NSDL3_GDF(NULL, NULL, "i = %d, num_graphs = %d, mp = %d, graph_lines_write_index = %d, num_vectors[%d] = %d", \
                             i, group_ptr->num_graphs, mp, hml_metrics[mp].graph_lines_write_index, mp, group_ptr->num_vectors[mp]); \
      if(group_ptr->num_vectors[mp] && (graph_ptr->graph_state != GRAPH_EXCLUDED)) \
      { \
        if((hml_metrics[mp].graph_lines_size - hml_metrics[mp].graph_lines_write_index) < (strlen(graph_ptr->gline) + 1)) \
        { \
          MY_REALLOC(hml_metrics[mp].graph_lines, hml_metrics[mp].graph_lines_size + HML_METRIC_DELTA_SIZE, "Realloc HMLMetric Vector List", -1); \
          hml_metrics[mp].graph_lines_size += HML_METRIC_DELTA_SIZE; \
        } \
        wbytes = sprintf(hml_metrics[mp].graph_lines + hml_metrics[mp].graph_lines_write_index, "%s\n", graph_ptr->gline); \
        hml_metrics[mp].graph_lines_write_index += wbytes; \
      } \
    } \
  } \
}

/* FILL_HML_VECTOR_LINE called form -
   1. Init time, in this case for loop must run once and fill vector at provided idx 
   2. Runtime, in this case loop must run from 0 to monitor priority and fill vector in HML places */
#define FILL_HML_VECTOR_LINE(mon_ptr, idx, max) \
{ \
  int wbytes = 0, i; \
  char *vect_ptr; \
  \
  if(hml_metrics) \
  { \
    NSDL2_GDF(NULL, NULL, "FILL_HML_VECTOR_LINE: mon_ptr = %p", mon_ptr); \
    if(mon_ptr->flags & MON_BREADCRUMB_SET) \
      vect_ptr = mon_ptr->mon_breadcrumb; \
    else \
      vect_ptr = mon_ptr->vector_name; \
    \
    if(idx == -1) \
    { \
      i = 0; \
      /*max = mon_ptr->metric_priority; */\
    } \
    else \
    { \
      i = idx; \
      max = idx; \
    } \
    NSDL2_GDF(NULL, NULL, "i = %d, max = %d, flag = %0x", i, max, mon_ptr->flags); \
    for(; i <= max; i++) \
    { \
      NSDL2_GDF(NULL, NULL, "i = %d, vector_lines_write_index = %d, vector_lines_size = %d, rtg_index = %d, mon_ptr = %p", \
                             i, hml_metrics[i].vector_lines_write_index, hml_metrics[i].vector_lines_size, mon_ptr->rtg_index[i], mon_ptr); \
      if((hml_metrics[i].vector_lines_size - hml_metrics[i].vector_lines_write_index) < (strlen(vect_ptr) + sizeof(long) + 2 + 1)) \
      { \
        MY_REALLOC(hml_metrics[i].vector_lines, hml_metrics[i].vector_lines_size + HML_METRIC_DELTA_SIZE, "Realloc HMLMetric Vector List", -1); \
        hml_metrics[i].vector_lines_size += HML_METRIC_DELTA_SIZE; \
      } \
      \
      if(mon_ptr->flags & RUNTIME_DELETED_VECTOR) \
        wbytes = sprintf(hml_metrics[i].vector_lines + hml_metrics[i].vector_lines_write_index, \
                           "#-%s %ld\n", vect_ptr, mon_ptr->rtg_index[i]); \
      else \
        wbytes = sprintf(hml_metrics[i].vector_lines + hml_metrics[i].vector_lines_write_index, \
                           "%s %ld\n", vect_ptr, mon_ptr->rtg_index[i]); \
      hml_metrics[i].vector_lines_write_index += wbytes; \
      hml_metrics[i].num_vectors++; \
      NSDL2_GDF(NULL, NULL, "vector_lines = %s", hml_metrics[i].vector_lines); \
    } \
  } \
} 

#define WRITE_HML_IN_TESTRUN_GDF(fp) \
{ \
  int i; \
  if(hml_metrics) \
  { \
    for(i = 0; i < MAX_METRIC_PRIORITY_LEVELS; i++) \
    { \
      if(hml_metrics[i].group_line[0] && hml_metrics[i].graph_lines[0] && hml_metrics[i].vector_lines[0]) \
      { \
        NSDL3_GDF(NULL, NULL, "WRITE_HML_IN_TESTRUN_GDF: i = %d, fp = %p, group_line = %s, vector_lines = %s, graph_lines = %s", \
                               i, fp, hml_metrics[i].group_line, hml_metrics[i].vector_lines, hml_metrics[i].graph_lines); \
        if(hml_metrics[i].vector_lines[hml_metrics[i].vector_lines_write_index - 1] == '\n') \
          hml_metrics[i].vector_lines[hml_metrics[i].vector_lines_write_index - 1] = '\0'; \
        fprintf(fp, "%s\n", hml_metrics[i].group_line); \
        fprintf(fp, "%s\n", hml_metrics[i].vector_lines); \
        fprintf(fp, "%s\n", hml_metrics[i].graph_lines); \
      } \
    } \
  } \
}

#define  HML_START_RTG_IDX           0
#define  HML_MSG_DATA_SIZE           1
extern ns_ptr_t hml_msg_data_size[MAX_METRIC_PRIORITY_LEVELS + 1][2];

extern char g_metric_priority;
extern HMLMetric *hml_metrics;

extern char get_metric_priority_id(char *priority, int flag); 
extern int is_graph_excluded(char *graph_priority, char *graph_state, int mon_priority);
extern int kw_enable_hml_group_in_testrun_gdf(char *keyword, char *buf, char *err_msg, int runtime_flag);
extern inline void init_hml_msg_data_size();

#endif
