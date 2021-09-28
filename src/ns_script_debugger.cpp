#include <map>
#include <iostream>
extern "C" 
{
  using namespace std; 
  map<string,string> ns_param_map;
  void insert_into_map(char* name, char* val)
  {
    ns_param_map[name] = val;
  }
  string get_value_from_map(char *name)
  {
    return ns_param_map[name];
  }
}
