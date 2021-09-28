/******************************************************************
 Name: nsu_validate_ip
 Author: Sanjay/Achint

 Modification History:
  01 May 06: Sanjay/Achint - First version

 *****************************************************************/

#include <stdio.h>

int
ns_ip_addr (char * addr, unsigned int * out);

#define IS_INCLUSIVE(A,B, X, Y) ((A >= X && A <= Y) && (B >= X && B <= Y))

int main(int argc, char *argv[])
{
   unsigned int A;
   unsigned int B;
   unsigned int X;
   unsigned int Y;
   
   if (argc != 5)
   {
      printf("Error: Invalid no. of arguments\n");
      return -1;
   }

   ns_ip_addr( argv[1], &A );
   ns_ip_addr( argv[2], &B );
   ns_ip_addr( argv[3], &X );
   ns_ip_addr( argv[4], &Y );

   if (IS_INCLUSIVE (A, B, X, Y))
	return 1;
    else
	return 0;
}

int
ns_ip_addr (char * addr, unsigned int * out)
{
int a,b,c,d;
//Read all 4 octects, make sure each octet is
sscanf(addr, "%d.%d.%d.%d", &a, &b, &c, &d); 
//printf("%d.%d.%d.%d",a,b,c,d);
//between 0-255. get these ocetets values in a,b,c,d
if ( !((a >= 0 && a <= 255) && (b >= 0 && b <= 255) && (c >= 0 && c <= 255) && (d >= 0 && d <= 255)) ){
    printf("Error: Invalid IP");
    return -1;
  }
//a is first octet and d is last.
    *out = d+ 256*c + 256*256*b + 256*256*256*a;
    return 0;
}
