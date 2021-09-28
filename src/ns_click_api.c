int ns_click_api(int page_id, int clickaction_id) {

  int ret = 0;
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method Called, page_id = %d, clickaction_id = %d", page_id, clickaction_id);
  if(IS_NS_SCRIPT_MODE_USER_CONTEXT) /* Non Thread Mode */ 
  {
    ret = ns_click_action(vptr, page_id, clickaction_id);
    if(ret < 0) {
      return -1;
    }
  }
  else  /* Thread Mode */
  {
    /* Making Request to send thread to run click action api */
    Ns_click_action_req click_action;
 
    click_action.opcode = NS_API_CLICK_ACTION_REQ;
    click_action.page_id = page_id;
    click_action.click_action_id = clickaction_id;
 
    ret = vutd_send_msg_to_nvm(VUT_CLICK_ACTION, (char *)(&click_action), sizeof(Ns_click_action_req));
 
/*    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
      ret = ns_rbu_click_execute_page(vptr, page_id, clickaction_id); */
  }
  return ret;
}
