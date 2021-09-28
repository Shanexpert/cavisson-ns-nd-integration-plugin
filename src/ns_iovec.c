#include <stdio.h>
#include "util.h"
#include "ns_trace_level.h"
#include "ns_log.h"
#include "ns_parse_scen_conf.h"
#include "nslb_util.h"
#include "ns_global_settings.h"
#include "ns_common.h"
#include "ns_msg_def.h"
#include "ns_data_types.h"
#include "netstorm.h"
#include "ns_vuser_tasks.h"
#include "ns_gdf.h"
#include "ns_group_data.h"
#include "ns_data_types.h"
#include <libgen.h>
#include "ns_child_msg_com.h"
#include "nslb_sock.h"
#include "output.h"
#include "ns_trans.h"

#include "ns_iovec.h"
#include "ns_alloc.h"

char *null_iovec = "";
// This method will allocate io_vector. io vector is made global and configrable 
void init_io_vector()
{
  NS_MALLOC_IOVEC(g_req_rep_io_vector, io_vector_init_size);
  NS_MALLOC_IOVEC(g_scratch_io_vector, io_vector_init_size);

  NSTL1(NULL, NULL, "Allocation done for vector for initial size = %d " 
                    "g_req_rep_io_vector.vector = %p, g_scratch_io_vector.vector = %p",
                     io_vector_init_size, g_req_rep_io_vector.vector, g_scratch_io_vector.vector);
}

/*This method is used to reallocate io_vectors in case size more that io_vector_size is required */
int grow_io_vector(VUser *vptr, NSIOVector *ns_iovec, int vec_needed)
{
  NSDL2_PARENT(NULL, NULL, "Method called. Vptr = %p, vec_needed = %d", vptr, vec_needed);

  int cur_size = ns_iovec->tot_size;
  int req_size = ns_iovec->cur_idx + vec_needed;
  char *flow_name="NA";
  char *page_name="NA";

  if(vptr->cur_page)
  {
     flow_name=vptr->cur_page->flow_name;     
     page_name=vptr->cur_page->page_name;     
  }
  
  if(req_size > io_vector_max_size){
    //Trace Max limit reached
      NSTL1(NULL, NULL, "Warning: Max limit of vector reached. initial_size = %d, current_size = %d, required vectors = %d, "
                        "max_limit = %d, vector = %p, flags = %p. Scenario group = %s, Script = %s, Page = %s",
                         io_vector_init_size, cur_size, req_size, io_vector_max_size, ns_iovec->vector, ns_iovec->flags,
                         runprof_table_shr_mem[vptr->group_num].scen_group_name, flow_name, page_name);
     NSDL2_PARENT(NULL, NULL, "Warning: Max limit of vector reached. initial_size = %d, current_size = %d, required vectors = %d, "
                        "max_limit = %d, vector = %p, flags = %p. Scenario group = %s, Script = %s, Page = %s",
                         io_vector_init_size, cur_size, req_size, io_vector_max_size, ns_iovec->vector, ns_iovec->flags,
                         runprof_table_shr_mem[vptr->group_num].scen_group_name, flow_name, page_name);
      return -1; // Fails
  }
  
  NS_REALLOC_IOVEC(*ns_iovec, req_size + io_vector_delta_size);

  NSTL1(NULL, NULL, "Reallocation done: initial_size = %d, current_size = %d, required vectors = %d, "
                        "max_limit = %d, vector = %p, flags = %p. Scenario group = %s, Script = %s, Page = %s",
                         io_vector_init_size, cur_size, req_size, io_vector_max_size, ns_iovec->vector, ns_iovec->flags,
                         runprof_table_shr_mem[vptr->group_num].scen_group_name, flow_name, page_name);
  NSDL2_PARENT(NULL, NULL, "Reallocation done: initial_size = %d, current_size = %d, required vectors = %d, "
                        "max_limit = %d, vector = %p, flags = %p. Scenario group = %s, Script = %s, Page = %s",
                         io_vector_init_size, cur_size, req_size, io_vector_max_size, ns_iovec->vector, ns_iovec->flags,
                         runprof_table_shr_mem[vptr->group_num].scen_group_name, flow_name, page_name);
  return 1; //Reallocation done
}
