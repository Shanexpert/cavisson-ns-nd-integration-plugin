#include <stdio.h>


static int table_a_to_x[255] = {
	-1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, //48 1,
	2, 3, 4, 5, 6, 7, 8, 9, 10, /*58 - :*/ 0,
	0, 0, 0, 0, 11, /*64 - @*/ 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
	27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
	37, /*90 -  Z*/ 0, 0, 0, 0, 0, 0, 38, /*97 - a*/ 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
	51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
	61, /*120 -d*/ 62, 63 /*122 - z*/ 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0 }; //250-255

static char table_64x_to_a[64] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
	':', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 
	'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 
	'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 
	'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 
	'w', 'x', 'y', 'z' };

unsigned int 
convert_ato64x(char *stnum)
{
unsigned int fnum=0;
int num, idx =0;

	while ((num = table_a_to_64x[stnum[idx++]]) != -1) {
	    fnum = fnum << 6;
	    fnum += num;
	}
	return fnum;
}

char *
convert_64xtoa(unsigned int num)
{
static stnum[16];
int idx=15;
int left; 

	stnum[idx--] = '\0';

	while (num) {
	    left = num%64;
	    stnum[idx--] = table_64x_to_a[left];
	    num /= 64;
	}
	idx++;
	return (&stnum[idx]);
}

