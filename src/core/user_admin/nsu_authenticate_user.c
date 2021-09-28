/*----------------------------------------------------------------------
Name    : nsu_authenticate_user.c 
Author  : Archana
Purpose : This file to Check Authentication of User name and password
nsu_authenticate_user command used by Login GUI screen
This command only can run by root.
Usage   : nsu_authenticate_user -u <User Name> -p <Password>
Return  : This command return following status:
return 0 for Active user i.e. valid  user.
return 1 for Inactive user  i.e. valid  user but not authorized to do anything
return 2 for not netstorm user  i.e. Invalid  user.
return 3 for other errors if any.
To Compile: gcc -o nsu_authenticate_user nsu_authenticate_user.c -lcrypt
Modification History:
01/27/09:  Archana - Initial Version
----------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <shadow.h>
#include <pwd.h>
#include <ctype.h>
//#include <security/pam_appl.h>
#include <ldap.h>
#define _XOPEN_SOURCE       /* See feature_test_macros(7) */
#include <unistd.h>

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <crypt.h>

#include <openssl/des.h>

#define NAME_LENGTH 500

int file_line_num = 0;
char *clear_white_space(char *in_str, char mode)
{
#define START             1  //00000001
#define MID               2  //00000010
#define END               4  //00000100
#define ALL               7  //00000111

  int len = 0 ;
  int len_diff = 0;
  char *tmp_ptr = NULL;
  char *end_ptr = NULL;
  char *start_ptr = NULL;
  char *out_str_ptr = NULL;
  char *out_str = NULL;

  start_ptr = tmp_ptr = in_str;

  if(!in_str || *in_str == '\0')
  {
    return NULL;
  }

  len = strlen(in_str);
  end_ptr = in_str + len - 1;

  //if mode is MID then only output buffer is required
  if(mode & MID)
    out_str = (char *)malloc(len + 1);

  while(isspace(*start_ptr))
    start_ptr++;

  //remove spaces from start of string
  if((mode & START) && (start_ptr > in_str))
  {
    strcpy(in_str, start_ptr);
    len_diff = (start_ptr - in_str);
    len = len - len_diff;
    end_ptr -= len_diff;
    start_ptr = in_str;
  }
  while(isspace(*end_ptr))
    end_ptr--;

  //remove spaces from end of string
  if(mode & END)
    end_ptr[1] = 0;

 //remove spaces between words  
  if(mode & MID)
  {
    out_str_ptr = out_str;
    for(tmp_ptr = in_str; *tmp_ptr != '\0'; tmp_ptr++)
    {
      //if spaces are present in between words of string then only it skip, 
      //other spaces like space at start and end of string are copied to output buffer
      if(isspace(*tmp_ptr) && (tmp_ptr > start_ptr) && (tmp_ptr <= end_ptr))
        continue;

      *out_str_ptr = *tmp_ptr;
      out_str_ptr++;
    }
    *out_str_ptr = '\0';
    strcpy(in_str, out_str);
  }

  if(out_str)
    free(out_str);

  return in_str;
}


void get_ldap_info(char *host, char *binddn, char *domain, char *password, char *search_key)
{
  FILE *fp = NULL;
  char conf_line[1024 +1] = "";
  char *val_ptr = NULL;
  int len = 0;
  short host_count = 0;
  short binddn_count = 0;
  short domain_count = 0;

  fp = fopen("/home/cavisson/etc/ldap.conf", "r");
  if (!fp)
  {
    printf("ldap file doesn't exist on the path /home/cavisson/etc/\n");
    exit(1);
  }


  while(fgets(conf_line, 512 , fp)!= NULL)
  {
    len = strlen(conf_line);
    conf_line[len - 1] = '\0';

    clear_white_space(conf_line, 5);
    
    if ((conf_line[0] == '\0') || ( conf_line[0] == '#' ))
      continue;

    val_ptr = strchr(conf_line, '=');
    
    if (strstr(conf_line,"HOST"))
    { 
      host_count = 1;
      val_ptr++;
      strcpy(host, val_ptr);
    }
    else if (strstr(conf_line,"BIND_DN"))
    {
      binddn_count = 1;
      val_ptr++;
      strcpy(binddn, val_ptr);
    }
    else if (strstr(conf_line,"BASE_DN"))
    {
      domain_count = 1;
      val_ptr++;
      strcpy(domain, val_ptr);
    }
    else if (strstr(conf_line,"BIND_PW"))
    {
      val_ptr++;
      strcpy(password, val_ptr);
    }
    else if (strstr(conf_line,"SEARCH_KEY"))
    {
      val_ptr++;
      strcpy(search_key, val_ptr);
    }

    else
    {
      printf("Invalid format of file\n");
      exit(-1);
    }
  }
  if(!host_count || !binddn_count || !domain_count)
  {
    printf("Invalid format of file\n");
    exit(-1);
  }
   
  fclose(fp);
}

int authenticate_ldap_user(const char *username, const char *password)
{
  LDAP *ld=NULL;
  LDAP *ld2=NULL;
  int rc;
  int ldap_version = LDAP_VERSION3;
  char host[256] ;// "ldap://172.18.152.63:1389";
  char binddn[512];// = "uid=rkumarw8,ou=Internal,ou=People,dc=Walgreens,dc=com";
  char domain[512];// = "dc=walgreens,dc=com";
  char passwd[512]="";// = "dsdfdgh";
  char search_key[512] = "uid";
  char filter[512];
  char *attrs[]       = {"memberOf", NULL};
  int  attrsonly      = 0;
  char *dn;
  LDAPMessage *answer, *entry;
 
  get_ldap_info(host, binddn, domain, passwd, search_key);
  if(ldap_initialize(&ld, host))
  {
    perror( "ldap_initialize" );
    return( 1 );
  }
  ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &ldap_version);

  if(!passwd[0])
    rc = ldap_simple_bind_s( ld, NULL, NULL );
  else
    rc = ldap_simple_bind_s( ld, binddn, passwd );

  if( rc != LDAP_SUCCESS )
  {
    fprintf(stderr, "ldap_simple_bind_s: %s\n", ldap_err2string(rc));
    return( 1 );
  }

  sprintf(filter,"%s=%s",search_key,username);
  rc = ldap_search_s(ld, domain, LDAP_SCOPE_SUBTREE, filter, attrs, attrsonly, &answer);
  if ( rc != LDAP_SUCCESS ) 
  {
    fprintf(stderr, "ldap_search_ext_s: %s\n", ldap_err2string(rc));
    ldap_unbind(ld);
    return( 1 );
  }
  rc = ldap_count_entries(ld, answer);
  if ( rc == 0 ) 
  {
    fprintf(stderr, "LDAP search did not return any data.\n");
    ldap_unbind(ld);
    return( 1 );
  } 
  entry = ldap_first_entry(ld, answer);
  dn = ldap_get_dn(ld, entry);
  if(ldap_initialize(&ld2, host))
  {
    perror( "ldap_initialize" );
    return( 1 );
  }
  ldap_set_option(ld2, LDAP_OPT_PROTOCOL_VERSION, &ldap_version);
  rc = ldap_simple_bind_s(ld2, dn, password);
  if( rc != LDAP_SUCCESS )
  {
    fprintf(stderr, " ldap_simple_bind_s: %s\n", ldap_err2string(rc));
    return( 1 );
  }

  ldap_memfree( dn );
  ldap_msgfree(answer);
  ldap_unbind(ld);
  return rc;
}

int confirm_cavisson_uid()
{
  struct passwd *pw;
  pw = getpwuid(getuid());
  if (pw == NULL)
  {
    printf("Error: Unable to get the real user name.\n");
    return 3;
  }
  if (strcmp(pw->pw_name, "cavisson"))
  {
    printf("Error: This command must be run as cavisson user only. Currently being run as %s.\n", pw->pw_name);
    return 3;
  }
  return 0;
}


int display_help_and_exit()
{
  printf("Usage: nsu_authenticate_user -u <User Name> -p <Password>\n");
  exit(-1);//return 3;
}

int check_options(int uflag, int pflag)
{
  if ((uflag == 0) || (pflag == 0))
  {
    printf("nsu_authenticate_user: All options are mandatory.\n");
    display_help_and_exit();
    return 3;
  }
  return 0;
}

int validate_username(char *username, char *password)
{
  struct spwd *spwd = getspnam (username);
  if (spwd)
  {
    strcpy(password, spwd->sp_pwdp);
    return 1;
  } 
  return 0;
}


int validate_password(char* password, char* password_given, int eflag)
{
  char *result;
  int ok;

  if(eflag){
    printf("encrytpted not implemented\n");
    return  0;
  } else {
    result = crypt(password_given, password);
  }
  
  if(result)
  { 
    ok = strcmp (result, password);
    return ok ? 0 : 1;
  }
  return 0;

}

int main(int argc,char **argv)
{
  char password_given[NAME_LENGTH]; 
  char username[NAME_LENGTH];
  char c;
  int uflag = 0, pflag = 0, eflag = 0;

  if(confirm_cavisson_uid() == 3)
    return 3;

  while ((c = getopt(argc, argv, "u:p:P:")) != -1) 
  {
    switch (c) 
    {
      case 'u': 
        if (uflag) 
        {
          printf("-u option cannot be specified more than once.\n");
          exit(-1);
        }
        uflag++;
        strcpy(username, optarg);
        break;
      case 'p':
        if (pflag) {
          printf("-p option cannot be specified more than once.\n");
          exit(-1);
        }
        pflag++;
        strcpy(password_given, optarg);
        break;
      case 'P':
        if (pflag) {
          printf("-p option cannot be specified more than once.\n");
          exit(-1);
        }
        pflag++;
        eflag++;
        strcpy(password_given, optarg);
        break;
      case ':':
      case '?':
        display_help_and_exit();
    }
  }

  if(check_options(uflag, pflag) == 3)
    return 3;
  /* 
  if(validate_username(username, password)) 
  {
    if(validate_password(password, password_given, eflag))
    {
      printf("User %s is a valid user.\n", username);
      return 0; //Active valid user
    }
    else
    {
      if(authenticate_ldap_user(username, password_given) == LDAP_SUCCESS)
      {
        printf("User %s is a valid user.\n", username);
        return 0; //Active valid user
      }
    }
  }
  */

  if(authenticate_ldap_user(username, password_given) == LDAP_SUCCESS)
  {
    printf("User %s is a valid user.\n", username);
    return 0; //Active valid user
  }
  printf ("Incorrect userid or password.\n");
  return 1;
}    

