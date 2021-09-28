#include "ns_tr069_includes.h"

static inline void 
tr069_update_value_in_big_buf(TR069DataValue_t *value_ptr,
                              char *param_key, int param_key_len) {

  char *buf_ptr;
  ns_ptr_t val_idx = value_ptr->value_idx;

  NSDL2_TR069(NULL, NULL, "Method called,"
                          " param_key = [%*.*s], param_key_len = %d, val_idx = %d",
                          param_key_len, param_key_len, param_key,
                          param_key_len, param_key_len, val_idx);

  if(val_idx >= 0) {
    buf_ptr = RETREIVE_TR069_BUF_DATA(val_idx);
    strncpy(buf_ptr, param_key, param_key_len);
    buf_ptr[param_key_len] = '\0';
    value_ptr->data_len = param_key_len;
  }
}

char *tr069_get_value_from_big_buf(TR069DataValue_t *value_ptr, int *value_len) {

  char *buf_ptr;
  ns_ptr_t val_idx = value_ptr->value_idx;

  NSDL2_TR069(NULL, NULL, "Method called, val_idx = %d, data_len = %d",
                           val_idx, value_ptr->data_len);

  if(val_idx >= 0 && value_ptr->data_len > 0) {
    *value_len = value_ptr->data_len;
    buf_ptr = RETREIVE_TR069_BUF_DATA(val_idx);
    return buf_ptr;
  } else {
    *value_len = 0; 
    return NULL;
  }
}

char *
tr069_get_full_param_values_str(VUser *vptr, char *param_name, int param_name_len) {

  int path_table_idx;
  char *value;
  int value_len;

  TR069ParamPath_t *param_path_table_ptr = &tr069_param_path_table[(vptr->user_index * total_param_path_entries)];

  NSDL2_TR069(NULL, NULL, "Method called, param_name = [%*.*s] ", 
                           param_name_len, param_name_len, param_name);

  if(param_name_len < 0) {
    param_name_len = strlen(param_name);
  }

  path_table_idx = tr069_path_name_to_index(param_name, param_name_len);

  if(path_table_idx >= 0) {
    if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_FULL_PATH) {
      if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_STRING) {
        value = tr069_get_value_from_big_buf(param_path_table_ptr[path_table_idx].data.value, &value_len);
        return value; 
      }
    }
    return NULL;
  } else {
    NSDL2_TR069(NULL, NULL, "Parameter not found [%*.*s]", param_name_len, param_name_len, param_name);
    return NULL; 
  }
}

int
tr069_get_full_param_values_num(VUser *vptr, char *param_name, int param_name_len) {

  int path_table_idx;

  TR069ParamPath_t *param_path_table_ptr = &tr069_param_path_table[(vptr->user_index * total_param_path_entries)];

  NSDL2_TR069(NULL, NULL, "Method called, param_name = [%*.*s] ", 
                           param_name_len, param_name_len, param_name);

  if(param_name_len < 0) {
    param_name_len = strlen(param_name);
  }

  path_table_idx = tr069_path_name_to_index(param_name, param_name_len);

  if(path_table_idx >= 0) {
    if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_FULL_PATH) {
      if (param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_UINT ||
          param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_BOOLEAN) {
        return param_path_table_ptr[path_table_idx].data.value->value_idx;
      }
    }
    return -1;
  } else {
    NSDL2_TR069(NULL, NULL, "Parameter not found [%*.*s]", param_name_len, param_name_len, param_name);
    return -1; 
  }
}

inline int 
tr069_get_param_values_with_cb(VUser *vptr, char *param_name,
                               int param_name_len, TR069GPVblockCB tr069_CB,
                               char **cur_ptr, int *num_params) {

  int path_table_idx;
  char name_string[512];
  int value_len;
  char value_string[512];
  char *name, *value;
  //int name_len;
  int start_idx, num_entries, idx;

  TR069ParamPath_t *param_path_table_ptr = &tr069_param_path_table[(vptr->user_index * total_param_path_entries)];

  NSDL2_TR069(NULL, NULL, "Method called, param_name = [%*.*s] ", 
                           param_name_len, param_name_len, param_name);

  if(param_name_len < 0) {
    param_name_len = strlen(param_name);
  }

  path_table_idx = tr069_path_name_to_index(param_name, param_name_len);

  if(path_table_idx >= 0) {
    strncpy(name_string, param_name, param_name_len);
    name_string[param_name_len] = '\0';
    if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_FULL_PATH) {
      if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_STRING) {
        value = tr069_get_value_from_big_buf(param_path_table_ptr[path_table_idx].data.value, &value_len);
        tr069_CB(name_string, "string", value, cur_ptr, num_params);
      } else if (param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_UINT) { 
        value_len = sprintf(value_string, "%ld", param_path_table_ptr[path_table_idx].data.value->value_idx);
        tr069_CB(name_string, "unsignedInt", value_string, cur_ptr, num_params);
      } else if (param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_BOOLEAN) {
        value_len = sprintf(value_string, "%ld", param_path_table_ptr[path_table_idx].data.value->value_idx);
        tr069_CB(name_string, "boolean", value_string, cur_ptr, num_params);
      }
    } else {  // Its a partial path
      start_idx   =  param_path_table_ptr[path_table_idx].data.full_path.start_index;
      num_entries =  param_path_table_ptr[path_table_idx].data.full_path.num_indexes;

      NSDL2_TR069(NULL, NULL, "start_idx = %d, num_entries = %d", start_idx, num_entries);

      for(idx = start_idx; idx < start_idx + num_entries; idx++) {
        path_table_idx = tr069_full_param_path_table[idx].path_table_index;
        //name = tr069_get_name_from_index(path_table_idx, &name_len); 
        name = tr069_index_to_path_name(path_table_idx);
        NSDL2_TR069(NULL, NULL, "idx = %d, name = [%s], path_type_flags = [0x%x]", idx, name, 
                          param_path_table_ptr[path_table_idx].path_type_flags);

        if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_STRING) {
          value = tr069_get_value_from_big_buf(param_path_table_ptr[path_table_idx].data.value, &value_len);
          tr069_CB(name, "string", value, cur_ptr, num_params);
        } else if (param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_UINT) { 
          value_len = sprintf(value_string, "%ld", param_path_table_ptr[path_table_idx].data.value->value_idx);
          tr069_CB(name, "unsignedInt", value_string, cur_ptr, num_params);
        } else if (param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_BOOLEAN) {
          value_len = sprintf(value_string, "%ld", param_path_table_ptr[path_table_idx].data.value->value_idx);
          tr069_CB(name, "boolean", value_string, cur_ptr, num_params);
        } else {
          NSDL2_TR069(NULL, NULL, "Parameter not found [%*.*s]", param_name_len, param_name_len, param_name);
          tr069_CB("NetStormParameter", "string", "NotFound", cur_ptr, num_params);
        }
      }
    }
    return NS_TR069_DATA_GET_SUCCESS;
  } else {
    NSDL2_TR069(NULL, NULL, "Parameter not found [%*.*s]", param_name_len, param_name_len, param_name);
    tr069_CB("NetStormParameter", "string", "NotFound", cur_ptr, num_params);
    return NS_TR069_DATA_INSERT_INVALID_PARAM; 
  }
}

// TODO
int tr069_get_param_attributes_with_cb(VUser *vptr, char *param_name, int param_name_len,
                                       TR069GPAblockCB tr069_CB, char **cur_ptr, int *num_params) {

  char accessList[1024];
  int path_table_idx;
  char name_string[512];

  TR069ParamPath_t *param_path_table_ptr = &tr069_param_path_table[(vptr->user_index * total_param_path_entries)];

  NSDL2_TR069(NULL, NULL, "Method called, param_name = [%*.*s] ", 
                           param_name_len, param_name_len, param_name);

  if(param_name_len < 0) {
    param_name_len = strlen(param_name);
  }

  path_table_idx = tr069_path_name_to_index(param_name, param_name_len);
  strcpy(accessList, "Subscriber");

  if(path_table_idx >= 0) {
     NSDL2_TR069(NULL, NULL, "path_table_idx = %d",path_table_idx);
     strncpy(name_string, param_name, param_name_len);
     name_string[param_name_len] = '\0';
    if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_FULL_PATH) {
      tr069_CB(name_string, param_path_table_ptr[path_table_idx].data.value->notification,
               accessList, cur_ptr, num_params);
      return NS_TR069_DATA_GET_SUCCESS;
    } else {
      NSDL2_TR069(NULL, NULL, "Only Full path is supported", param_name_len, param_name_len, param_name);
      tr069_CB("NetStormNotaFullPathParam", 0, accessList, cur_ptr, num_params);
      return NS_TR069_DATA_INSERT_INVALID_PARAM; 
    }
  } else {
    NSDL2_TR069(NULL, NULL, "Parameter not found [%*.*s]", param_name_len, param_name_len, param_name);
    tr069_CB("NetStormParameter", 0, accessList, cur_ptr, num_params);
    return NS_TR069_DATA_INSERT_INVALID_PARAM; 
  }
}

// TODO
int tr069_get_param_names_with_cb(VUser *vptr, char *param_name, int param_name_len, TR069GPNblockCB tr069_CB,
                               char **cur_ptr, int *num_params) {
 
  int path_table_idx;
  char *name;
  int start_idx, num_entries, idx;
  //int name_len;
  char name_string[256];

  TR069ParamPath_t *param_path_table_ptr = &tr069_param_path_table[vptr->user_index * total_param_path_entries];
  NSDL2_TR069(NULL, NULL, "Method called, param_name = [%*.*s], param_name_len = %d", 
                           param_name_len, param_name_len, param_name, param_name_len);

  if(param_name_len < 0) {
    param_name_len = strlen(param_name);
  }

  path_table_idx = tr069_path_name_to_index(param_name, param_name_len);

  if(path_table_idx >= 0) {
    strncpy(name_string, param_name, param_name_len);
    name_string[param_name_len] = '\0';
    if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_FULL_PATH) {
      tr069_CB(name_string, "1", cur_ptr, num_params); // TODO writable
    } else {  // Its a partial path
      start_idx   =  param_path_table_ptr[path_table_idx].data.full_path.start_index;
      num_entries =  param_path_table_ptr[path_table_idx].data.full_path.num_indexes;

      for(idx = start_idx; idx < start_idx + num_entries; idx++) {
        path_table_idx = tr069_full_param_path_table[idx].path_table_index;
        //name = tr069_get_name_from_index(path_table_idx, &name_len); 
        name = tr069_index_to_path_name(path_table_idx);
        tr069_CB(name, "1", cur_ptr, num_params); // TODO writable
      }
    }
    return NS_TR069_DATA_GET_SUCCESS;
  } else {
    tr069_CB("NetStormParameter", "1", cur_ptr, num_params);
    NSDL2_TR069(NULL, NULL, "Parameter not found [%*.*s]", param_name_len, param_name_len, param_name);
    return NS_TR069_DATA_INSERT_INVALID_PARAM; 
  }

  return NS_TR069_DATA_GET_SUCCESS;
}

/* Set paramater values returns
   0     - Succcess 
   1     - Failure
*/
int tr069_set_param_values(VUser *vptr, char *param_name, int param_name_len, 
                                        char *param_key, int param_key_len) {

  char name_string[256];
  int path_table_idx;
  TR069ParamPath_t *param_path_table_ptr = &tr069_param_path_table[vptr->user_index * total_param_path_entries];

  if(param_name_len < 0) {
    param_name_len = strlen(param_name);
  }

  if(param_key_len < 0) {
    param_key_len = strlen(param_key);
  }

  if(param_key_len > TR069_MAX_PARAMETER_VALUE_LEN) {
    param_key_len =  TR069_MAX_PARAMETER_VALUE_LEN - 1;
  }

  NSDL2_TR069(NULL, NULL, "Method called, param_name = [%*.*s] " 
                                          "param_key = [%*.*s]",
                                           param_name_len, param_name_len, param_name,
                                           param_key_len, param_key_len, param_key);

  path_table_idx = tr069_path_name_to_index(param_name, param_name_len);

  if(path_table_idx >= 0) {
    strncpy(name_string, param_name, param_name_len);
    name_string[param_name_len] = '\0';
    if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_FULL_PATH) {
      // Return if not writable
      if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_RONLY) { 
        NSDL2_TR069(NULL, NULL, "Can not set value, Parameter [%*.*s] is read only.",
                                 param_name_len, param_name_len, param_name);
        return NS_TR069_DATA_INSERT_RONLY;
      }
      if(IGDMgSvrURLidx == path_table_idx) {
        NSDL2_TR069(NULL, NULL, "Setting bit for remapping of url as"
                                " InternetGatewayDevice.ManagementServer.URL is going to change.");
        vptr->httpData->flags |= NS_TR069_REMAP_SERVER;
      }
      if(param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_STRING) {
        tr069_update_value_in_big_buf(param_path_table_ptr[path_table_idx].data.value, param_key, param_key_len);
      } else if (param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_UINT) { 
        param_path_table_ptr[path_table_idx].data.value->value_idx = atoi(param_key);
      } else if (param_path_table_ptr[path_table_idx].path_type_flags & NS_TR069_PARAM_BOOLEAN) {
        param_path_table_ptr[path_table_idx].data.value->value_idx = atoi(param_key)? 1 : 0;
      }
      // if means we have to take inform parameter by treversing the tree 
      // not the array index which we have taken for optimization
      if(param_path_table_ptr[path_table_idx].data.value->notification == 1) {
        vptr->httpData->flags |= NS_TR069_EVENT_VALUE_CHANGE_PASSIVE;
        param_path_table_ptr[path_table_idx].path_type_flags |= NS_TR069_INFORM_PARAMETER_RUN_TIME;
      } else if (param_path_table_ptr[path_table_idx].data.value->notification == 2) {
        param_path_table_ptr[path_table_idx].path_type_flags |= NS_TR069_INFORM_PARAMETER_RUN_TIME;
        vptr->httpData->flags |= NS_TR069_EVENT_VALUE_CHANGE_ACTIVE;
      }
      return NS_TR069_DATA_INSERT_SUCCESS;
    } else {
      NSDL2_TR069(NULL, NULL, "Can not set value of partial path = [%*.s]", param_name_len, param_name_len, param_name);
      return NS_TR069_DATA_INSERT_INVALID_PARAM;
    }
  } else {  // A partial parameter values can not be set
    NSDL2_TR069(NULL, NULL, "Parameter not found [%*.*s]", param_name_len, param_name_len, param_name);
    return NS_TR069_DATA_INSERT_INVALID_PARAM; 
  }
}

int tr069_set_param_attributes_with_cb(VUser *vptr, char *param_name, int param_name_len, 
                                        char *attribute, int attribute_len) {

  NSDL2_TR069(NULL, NULL, "Method called, param_name = [%*.*s], attribute = [%*.*s] ",
                           param_name_len, param_name_len, param_name,
                           attribute_len, attribute_len, attribute);

  return NS_TR069_DATA_INSERT_SUCCESS;
}

// Return NULL if first time, else pointer to the new url string 
char* tr069_is_cpe_new(VUser *vptr) {

  char *value;
  int value_len;

  TR069ParamPath_t *param_path_table_ptr = &tr069_param_path_table[vptr->user_index * total_param_path_entries];
  NSDL2_TR069(vptr, NULL, "Method Called");

  if(IGDMgSvrURLidx >= 0 ) {
    param_path_table_ptr = &tr069_param_path_table[vptr->user_index * total_param_path_entries];
    value = tr069_get_value_from_big_buf(param_path_table_ptr[IGDMgSvrURLidx].data.value, &value_len);
    if(value && value[0] != 'N' && value [1] != 'A') {
      return value;
    } else {
      return NULL;
    }
  } else {
    return NULL;
  }
}
