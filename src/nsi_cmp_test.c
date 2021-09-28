#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LEN 1024
typedef struct {
	int grpDispOrder;
	char grpName[MAX_LEN];
	int dataDispOrder;
	char dataName[MAX_LEN];
	char dataValue[MAX_LEN];
	int tnum;
} DataRow;

typedef struct {
	int tnum;
	char *dval;
} TestNumData;

static char *notApp = "-";

#define DELTA_DATA_ENTRIES 250
int total_data_entries;
int max_data_entries;
DataRow *dataTable;

static void 
create_data_table_entry(int *row_num) 
{
  if (total_data_entries == max_data_entries) {
    dataTable = (DataRow *) realloc ((char *)dataTable,
				   (max_data_entries + DELTA_DATA_ENTRIES) * sizeof(DataRow));
    if (!dataTable) {
      fprintf(stderr,"create_data_table_entry(): Error allocating more memory for data entries\n");
      exit(1);
    } else max_data_entries += DELTA_DATA_ENTRIES;
  }
  *row_num = total_data_entries++;
  return; 
}

int 
test_num_comp (const void *r1, const void *r2)
{
   if (((TestNumData *)r1)->tnum > ((TestNumData *)r2)->tnum) {
     return 1;
   }
   else if (((TestNumData *)r1)->tnum < ((TestNumData *)r2)->tnum) {
     return -1;
   }
   else 
     return 0;
}

int 
data_row_comp (const void *r1, const void *r2)
{
int ret;
	if (((DataRow *)r1)->grpDispOrder > ((DataRow *)r2)->grpDispOrder) {
		return 1;
	} else if (((DataRow *)r1)->grpDispOrder < ((DataRow *)r2)->grpDispOrder) {
		return -1;
	} else  {
	    ret = strcmp (((DataRow *)r1)->grpName, ((DataRow *)r2)->grpName);
	    if (ret)
		return ret;
	    //means same
	    if (((DataRow *)r1)->dataDispOrder > ((DataRow *)r2)->dataDispOrder)
		return 1;
	    if (((DataRow *)r1)->dataDispOrder < ((DataRow *)r2)->dataDispOrder)
		return -1;
	    //means same
	    ret = strcmp (((DataRow *)r1)->dataName, ((DataRow *)r2)->dataName);
	    if (ret) return ret;

	    //means same
	    if (((DataRow *)r1)->tnum > ((DataRow *)r2)->tnum) 
		return 1;
	    else if (((DataRow *)r1)->tnum < ((DataRow *)r2)->tnum) 
		return -1;
	    else 
		return 0;
	}
}

int main (int argc, char *argv[])
{
char *ptr, fname[64];
int i, tnum, j;
char buf[4096];
int gdo, ddo, rnum;
char gname[MAX_LEN], dname[MAX_LEN], dval[MAX_LEN];
DataRow *r;
TestNumData testnum_arr[argc-1];
int num_tests=0, fnum;
FILE *fp;
int ret;
char *last_name;

	if (argc < 3) {
	      	printf ("Usage: %s test-run test-run...\n", argv[0]);
		exit(1);
	}

	ptr = getenv("NS_WDIR");
	for (i =1; i < argc; i++) {
	    num_tests++;
	    tnum = atoi (argv[i]);
	    // printf ("loop i=%d tnum=%d\n", i, tnum);
	    testnum_arr[i-1].tnum = tnum;
	    ptr = getenv("NS_WDIR");
	    sprintf (fname, "%s/logs/TR%d/summary.data", ptr?ptr:"/home/cavisson/work", tnum);
	    //printf ("INFO: Opening  test run %d, %s file\n", tnum, fname);
	    fp = fopen (fname , "r"); 
	    if (!fp) {
		printf ("ERR: Ignoring test run %d, %s Unable to open data file\n", tnum, fname);
		exit (1);
	    }
	    if (!fgets (buf, 4096, fp)) {
		printf ("ERR: Ignoring test run %d, %s bad data file\n", tnum, fname);
		exit (1);
	    }
	    if (!strstr (buf, "Group Display Order")) {
		printf ("ERR: Ignoring test run %d, %s bad file format\n", tnum, fname);
		exit (1);
	    }
	    while (fgets (buf, 4096, fp)) {
		buf[strlen(buf)-1]= '\0';

		ptr = strtok (buf, "|"); 
		if (!ptr) continue;
		gdo = atoi(ptr);

		ptr = strtok (NULL, "|"); 
		if (!ptr) continue;
		strncpy(gname, ptr, MAX_LEN);
		gname[MAX_LEN-1] = '\0';

		ptr = strtok (NULL, "|"); 
		if (!ptr) continue;
		ddo = atoi(ptr);

		ptr = strtok (NULL, "|"); 
		if (!ptr) continue;
		strncpy(dname, ptr, MAX_LEN);
		dname[MAX_LEN-1] = '\0';

		ptr = strtok (NULL, "|"); 
		if (!ptr) continue;
		strncpy(dval, ptr, MAX_LEN);
		dval[MAX_LEN-1] = '\0';

		create_data_table_entry (&rnum);
		
		dataTable[rnum].tnum = tnum;
		dataTable[rnum].grpDispOrder = gdo;
		dataTable[rnum].dataDispOrder = ddo;
		strcpy (dataTable[rnum].grpName, gname);
		strcpy (dataTable[rnum].dataName, dname);
		strcpy (dataTable[rnum].dataValue, dval);
	    }
	    fclose(fp);
	}

      //for (i = 0; i < num_tests; i++)
//	printf ("before sort Tnum = %d\n", testnum_arr[i].tnum);
      qsort (testnum_arr, num_tests, sizeof(TestNumData), test_num_comp);
 //     for (i = 0; i < num_tests; i++)
//	printf ("after sort Tnum = %d\n", testnum_arr[i].tnum);

      r = &dataTable[0];
      qsort (r, total_data_entries, sizeof(DataRow), data_row_comp);

      //printf ("Total data_entries=%d\n", total_data_entries);

#if 0
	for (i = 0; i < total_data_entries; i++) {
		printf ("i=%d gdo=%d ddo=%d tnumm=%d gname=%s dname=%s\n", i , 
			dataTable[i].grpDispOrder,
			dataTable[i].dataDispOrder,
			dataTable[i].tnum,
			dataTable[i].grpName,
			dataTable[i].dataName);
	}
#endif
//	exit(1);

      printf ("Field Num|Group Name|Data name");

      for (j = 0; j < num_tests; j++) {
	printf ("|Test Run %d", testnum_arr[j].tnum);
      }
	printf ("\n");

      fnum = 1;
      i = 0;
      while (i < total_data_entries) {
	printf ("%d|%s|%s", fnum, dataTable[i].grpName, dataTable[i].dataName);
        last_name = dataTable[i].dataName;
	for (j = 0; j < num_tests; j++) {
            ret = strcmp(dataTable[i].dataName, last_name);
	    if (testnum_arr[j].tnum == dataTable[i].tnum && (ret == 0)) {
		printf ("|%s", dataTable[i].dataValue);
		testnum_arr[j].dval = dataTable[i].dataValue;
		i++;
	    } else {
		printf ("|%s", notApp);
		testnum_arr[j].dval = notApp;
	    }
	} 
	printf("\n");
	fnum++;
      }
	exit (0); // Added by Neeraj
}
