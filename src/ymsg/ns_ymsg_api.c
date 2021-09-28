#include "ns_ymsg_client.h" 

int ns_ymsg_login(char *yahoo_id, char *password)
{
  NSDL2_API(NULL, NULL, "Method Called: yahoo_id = %s, password = %s", yahoo_id, password);

  return(ns_ymsg_login_ext(yahoo_id, password, 0, 6));
}

int ns_ymsg_send_chat(char *my_yahoo_id, char *dest_yahoo_id, char *chat_msg)
{
  NSDL2_API(NULL, NULL, "Method Called: my_yahoo_id = %s, dest_yahoo_id = %s, chat_msg = %s", my_yahoo_id, dest_yahoo_id, chat_msg);
  return (ns_ymsg_send_chat_ext(my_yahoo_id, dest_yahoo_id, chat_msg));
}

int ns_ymsg_logout()
{
  NSDL2_API(NULL, NULL, "Method Called:");
  return (ns_ymsg_logout_ext());
}


void *ns_ymsg_get_local_host()
{
  return (ns_ymsg_get_local_host_ext());
}

void *ns_ymsg_get_ylad()
{
  return (ns_ymsg_get_ylad_ext());
}

void *ns_ymsg_get_buddies()
{
  return (ns_ymsg_get_buddies_ext());
}

int ns_ymsg_get_connection_tags()
{
  return (ns_ymsg_get_connection_tags_ext());
}


void ns_ymsg_set_local_host(void *ptr)
{
  return (ns_ymsg_set_local_host_ext(ptr));
}

void ns_ymsg_set_ylad(void *ptr)
{
  return (ns_ymsg_set_ylad_ext(ptr));
}

void ns_ymsg_set_buddies(void *ptr)
{
  return (ns_ymsg_set_buddies_ext(ptr));
}

void ns_ymsg_set_connection_tags(int con_ptr)
{
  return (ns_ymsg_set_connection_tags_ext(con_ptr));
}



/*
char *ns_ymsg_init_ylad()
{
  yahoo_local_account *ns_local_ylad = NULL;

  NSDL2_API(NULL, NULL, "Method Called:");
  ns_local_ylad = (yahoo_local_account *)calloc(1, sizeof(yahoo_local_account));
  NSDL2_API(NULL, NULL, "ns_local_ylad = %p", ns_local_ylad);

  return ((char *)ns_local_ylad);
}
*/

/*
void ns_ymsg_set_globals(char *ns_local_host, char *ns_ylad, int *ns_poll_loop)
{
  NSDL2_API(NULL, NULL, "Method Called:");
  return (ymsg_set_globals(ns_local_host, ns_ylad, ns_poll_loop));
}

*/
