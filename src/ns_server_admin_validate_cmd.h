#define BLACKLIST 0
#define WHITELIST 1

#define CHECK_IF_COMMENTED_OR_BLANK_LINE(ptr, flag)   \
{   \
  while(*ptr)     \
  {               \
    if('#' == *ptr)     \
    {                   \
      flag = 1;         \
      break;            \
    }                 \
    else if((' ' != *ptr) && ('\t' != *ptr) && ('\n' != *ptr))      \
      break;              \
    ptr++;                \
  }                       \
}

extern int validate_command(char *cmd_args);
