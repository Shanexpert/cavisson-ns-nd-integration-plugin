#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "nslb_util.h"
#include "decomp.h"


#define CHUNK 16384*2

/* #ifdef TEST */
#ifdef FOOTEST
char *orig_buf;

int orig_max_len = CHUNK;
int orig_cur_len = 0;
#endif
char *comp_buf;
int comp_max_len = 0;
int comp_cur_len = 0;
char *uncomp_buf=NULL;
int uncomp_max_len=0;
int uncomp_cur_len=0;

//Output is in static buf comp_buf of size comp_buf_len
int 
ns_comp_do (char *in, int in_len, short comp_type)
{
    int ret, len;
    z_stream strm;
    comp_cur_len = 0;
    //int fdin, fdout;

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, comp_type==1?31:15, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK)
        return ret;

    // Cannot use if globals.debug as this is used by hpd code
    // printf("comp in=%d\n", in_len);

    //fdin = open ("/tmp/compin", O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
    //if (fdin == -1) perror("compin");
    //fdout = open ("/tmp/compout", O_RDWR |O_CREAT|O_TRUNC, S_IRWXU);
    //write(fdin, in, in_len);
    //if (fdout == -1) perror("compin");
    /* compress until end of file */
        strm.avail_in = in_len;
        strm.next_in = (Bytef *)in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            if (comp_cur_len == comp_max_len) {
#ifdef TEST
	        printf("Reallocing for comp at %d\n", comp_max_len);
#endif
	        comp_buf = (char *)realloc (comp_buf, comp_max_len+CHUNK);
	        if (comp_buf == NULL) {
	        	printf ("mem relaoc failed for compression buf\n");
	        	return(-1);
	        }
	        comp_max_len += CHUNK;
    	    }
            strm.avail_out = comp_max_len - comp_cur_len;
            strm.next_out = (Bytef *) comp_buf + comp_cur_len;

            ret = deflate(&strm, Z_FINISH);    /* no bad return value */
            //assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
		
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
            case Z_STREAM_ERROR:
                (void)deflateEnd(&strm);
		printf ("compression error ret=%d\n", ret);
                return -1;
            }
	    len = (comp_max_len - comp_cur_len) - strm.avail_out;
	    comp_cur_len += len;
#ifdef TEST
	    printf("compressed %d bytes total=%d\n", len, comp_cur_len);
#endif
        } while (strm.avail_out == 0);
        if ((strm.avail_in != 0) || (ret != Z_STREAM_END)) {    /* all input will be used */
    		(void)deflateEnd(&strm);
		printf ("decompression error ret=%d\n", ret);
		return -1;
	}
        //assert(strm.avail_in == 0);     /* all input will be used */
        //assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);

    // Cannot use if globals.debug as this is used by hpd code
    // printf("comp out=%d\n", comp_cur_len);

    //write(fdout, comp_buf, comp_cur_len);
    //close(fdin);
    //close(fdout);
    return ret == Z_STREAM_END ? 0 : -1;
    //return Z_OK;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
//Output is in static buf decomp_buf of size decomp_buf_len
int 
ns_decomp_do (char *in, int in_len, short comp_type)
{
    int ret, len;
    z_stream strm;
    //debug_log(1, _FL_, (char*)__FUNCTION__, "method called");

    uncomp_cur_len = 0;
    //fprintf (stderr, "inflating\n");
    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, comp_type==1?31:15);
    //ret = inflateInit2(&strm, 31);
    if (ret != Z_OK) {
	printf ("inflateInit2 failed for comp type =%d\n", comp_type);
        return -1;
    }

    /* decompress until deflate stream ends or end of file */
        strm.avail_in = in_len;
        strm.next_in = (Bytef *)in;

        /* run inflate() on input until output buffer not full */
        do {
            if (uncomp_cur_len == uncomp_max_len) {
#ifdef TEST
	        printf("Reallocing for uncomp at %d\n", uncomp_max_len);
#endif
                //1 extra byte for null character
	        uncomp_buf = (char *)realloc (uncomp_buf, uncomp_max_len+CHUNK + 1);
	        if (uncomp_buf == NULL) {
	        	printf ("mem relaoc failed for decompression buf\n");
	        	return(-1);
	        }
	        uncomp_max_len += CHUNK;
    	    }
            strm.avail_out = uncomp_max_len - uncomp_cur_len;
            strm.next_out = (Bytef *)uncomp_buf + uncomp_cur_len;
            ret = inflate(&strm, Z_NO_FLUSH);
	    //assert (ret != Z_STREAM_ERROR);
		
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
            case Z_STREAM_ERROR:
                (void)inflateEnd(&strm);
		printf ("decompression error ret=%d\n", ret);
                return -1;
            }
	    len = (uncomp_max_len - uncomp_cur_len) - strm.avail_out;
	    uncomp_cur_len += len;
#ifdef TEST
	    printf("uncompressed %d bytes total=%d\n", len, uncomp_cur_len);
#endif
        } while (strm.avail_out == 0);
        if ((strm.avail_in != 0) || (ret != Z_STREAM_END)) {    /* all input will be used */
    		(void)inflateEnd(&strm);
		printf ("decompression error ret=%d\n", ret);
		return -1;
	}

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? 0 : -1;
}

static z_stream strm;

/* report a zlib or i/o error */
void zerr(char *msg, char *err_msg, int ret)
{
  int do_printf = 0;
#ifdef FOOTEST
  do_printf = 1;
#endif

    if (do_printf) fprintf(stderr, "%s: ", msg);
    switch (ret) {
    case Z_ERRNO:
      if (ferror(stdin)) {
        if (do_printf) fputs("error reading stdin\n", stderr);
        sprintf(err_msg, "error reading stdin");
      }
      if (ferror(stdout)) {
        if (do_printf) fputs("error writing stdout\n", stderr);
        sprintf(err_msg, "error writing stdout");
      }
        break;
    case Z_STREAM_ERROR:
        if (do_printf) fputs("invalid compression level\n", stderr);
        sprintf(err_msg, "invalid compression level");
        break;
    case Z_DATA_ERROR:
        if (do_printf) fputs("invalid or incomplete deflate data\n", stderr);
        sprintf(err_msg, "invalid or incomplete deflate data");
        break;
    case Z_MEM_ERROR:
        if (do_printf) fputs("out of memory\n", stderr);
        sprintf(err_msg, "out of memory");
        break;
    case Z_VERSION_ERROR:
        if (do_printf) fputs("zlib version mismatch!\n", stderr);
        sprintf(err_msg, "zlib version mismatch!");
    default: 
        if (do_printf) fputs("Unknown Error!\n", stderr);
        sprintf(err_msg, "Unknown Error!");
    }
}
int 
ns_decomp_do_new (char *in, int in_len, short comp_type, char *err_msg)
{
    int ret, len;
    z_stream strm;
    char *tmp_in = in;

    uncomp_cur_len = 0;
    /* allocate inflate state */

#ifdef FOOTEST
    printf("%s Method called. in_len = %d comp_type = %d\n", __FUNCTION__, in_len, comp_type);
#endif

    while (1) {
      /* decompress until deflate stream ends or end of file */
      strm.zalloc = Z_NULL;
      strm.zfree = Z_NULL;
      strm.opaque = Z_NULL;
      strm.avail_in = 0;
      strm.next_in = Z_NULL;
      
      strm.total_in = 0;

#ifdef FOOTEST
      printf("Inflate initing.\n");
#endif      
      ret = inflateInit2(&strm, comp_type==1?31:15);
      //ret = inflateInit2(&strm, 31);
      if (ret != Z_OK) {
        fprintf (stderr, "inflateInit2 failed for comp type =%d\n", comp_type);
        return -1;
      }

      strm.avail_in = in_len;
      strm.next_in = (Bytef *)tmp_in;

      /* run inflate() on input until output buffer not full */
      do {
        if (uncomp_cur_len == uncomp_max_len) {
#ifdef TEST
          printf("Reallocing for uncomp at %d\n", uncomp_max_len);
#endif
          //Added realloc size +1, for null character.
          uncomp_buf = (char *)realloc (uncomp_buf, uncomp_max_len+CHUNK + 1);
          if (uncomp_buf == NULL) {
            fprintf (stderr, "mem relaoc failed for decompression buf\n");
            return(-1);
          }
          uncomp_max_len += CHUNK;
        }
        strm.avail_out = uncomp_max_len - uncomp_cur_len;
        strm.next_out = (Bytef *)uncomp_buf + uncomp_cur_len;

#ifdef FOOTEST
        printf ("Inflating. next_in = %p, avail_in = %d, total_in = %lu, next_out = %p, avail_out = %d, total_out = %lu\n",
                strm.next_in, strm.avail_in, strm.total_in, strm.next_out, strm.avail_out, strm.total_out);
#endif

        ret = inflate(&strm, Z_NO_FLUSH);
        //assert (ret != Z_STREAM_ERROR);
	
        if (ret != Z_OK && ret != Z_STREAM_END) {
          zerr("decompression", err_msg, ret);
          (void)inflateEnd(&strm);


          return -1;
        }
        len = (uncomp_max_len - uncomp_cur_len) - strm.avail_out;
        uncomp_cur_len += len;
#ifdef TEST
        printf("uncompressed %d bytes total=%d\n", len, uncomp_cur_len);
#endif

#ifdef FOOTEST
        printf ("Inflating done. next_in = %p, avail_in = %d, total_in = %lu, next_out = %p, avail_out = %d, total_out = %lu\n",
               strm.next_in, strm.avail_in, strm.total_in, strm.next_out, strm.avail_out, strm.total_out);
#endif

        if (ret == Z_STREAM_END) { 
#ifdef FOOTEST
          printf("Z_STREAM_END reached\n");
#endif
          break;
        }
      } while (strm.avail_out == 0);
      /*         if ((strm.avail_in != 0) || (ret != Z_STREAM_END)) {    /\* all input will be used *\/ */
      /*     		(void)inflateEnd(&strm); */
      /* 		printf ("decompression error ret=%d\n", ret); */
      /* 		return -1; */
      /* 	} */
      /* clean up and return */

#ifdef FOOTEST
      printf ("Inflating End next_in = %p, avail_in = %d, total_in = %lu, next_out = %p, avail_out = %d, total_out = %lu\n",
              strm.next_in, strm.avail_in, strm.total_in, strm.next_out, strm.avail_out, strm.total_out);
#endif
      (void)inflateEnd(&strm);

      if (in_len == strm.total_in) break; /* we are done */
      tmp_in = tmp_in + strm.total_in;
      in_len = in_len - strm.total_in;
    }

    if(ret == Z_OK)
      return 0;

    return ret == Z_STREAM_END ? 0 : -1;
}

int 
init_ns_decomp_do_continue(short comp_type)
{
  int ret;

  uncomp_cur_len = 0;
  //fprintf (stderr, "inflating\n");
  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  ret = inflateInit2(&strm, comp_type==1?31:15);
  //ret = inflateInit2(&strm, 31);
  if (ret != Z_OK) {
    printf ("inflateInit2 failed for comp type =%d\n", comp_type);
    return -1;
  }
  return 0;
}

int
ns_decomp_do_continue (char *in, int in_len)
{
  int ret;
  int len;

  /* decompress until deflate stream ends or end of file */
  strm.avail_in = in_len;
  strm.next_in = (Bytef *)in;

  do {
    if (uncomp_cur_len == uncomp_max_len) {
#ifdef FOOTEST
      printf("Reallocing for uncomp at %d\n", uncomp_max_len);
#endif
      //1 extra for null character.
      uncomp_buf = (char *)realloc (uncomp_buf, uncomp_max_len+CHUNK + 1);
      if (uncomp_buf == NULL) {
        printf ("mem relaoc failed for decompression buf\n");
        return(-1);
      }
      uncomp_max_len += CHUNK;
    }
    strm.avail_out = uncomp_max_len - uncomp_cur_len;
    strm.next_out = (Bytef *)uncomp_buf + uncomp_cur_len;
    
    ret = inflate(&strm, Z_NO_FLUSH);

    if (ret != Z_OK && ret != Z_STREAM_END) {
      switch (ret) {
      case Z_NEED_DICT:
        ret = Z_DATA_ERROR;     /* and fall through */
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
      case Z_STREAM_ERROR:
      case Z_BUF_ERROR:
      default:
        (void)inflateEnd(&strm);
        printf ("decompression error ret=%d\n", ret);
      }
      return -1;
    }

#ifdef FOOTEST
    printf("ret = %d\n", ret);
#endif

    len = (uncomp_max_len - uncomp_cur_len) - strm.avail_out;
    uncomp_cur_len += len;
  } while (strm.avail_out == 0);
  if (ret == Z_STREAM_END)
    inflateEnd(&strm);

  return ret == Z_OK ? 1 : 0;
}

int 
ns_decomp_init()
{
    uncomp_buf = (char *)malloc (CHUNK);
    if (uncomp_buf) {
	uncomp_max_len = CHUNK;
	uncomp_cur_len = 0;
	return 0;
    } else {
	printf("Unable to alloc for decompression buf\n");
	return -1;
    }
}

#ifdef NEW_MAIN
#define MAX_FILENAME 1024

static int write_file_from_buf (int file_to_write_fd, char *buff_to_write)
{
  unsigned int offset = 0;
  unsigned int bytes_to_write =uncomp_cur_len; 
  int write_bytes = 0;
   while(bytes_to_write){
     write_bytes = write(file_to_write_fd, (buff_to_write + offset), 1);
     if(write_bytes < 0){
       fprintf(stderr, "Error: Error in write file.(%s)\n", nslb_strerror(errno));
       return -1;
     }
     bytes_to_write -= write_bytes;
     offset += write_bytes;
   }
   return offset;
}


int main(int argc,char **argv)
{
  int option = 0, type = 0;
  char inputfile[MAX_FILENAME];
  char outfile[MAX_FILENAME];
  char err_msg[4096];
  
  char c;

 while ((c = getopt(argc, argv, "o:t:i:O:")) != -1) {
    switch(c) {
    case 'o':
     option = atoi(optarg);
     printf("option %d\n",option);
      if(option <= 0 || option >=5 )
      { // valid options 1,2,3,4
        fprintf(stderr, "invalid option '%s'\n", optarg);
        exit(-1);
      }
      break;

    case 't':
      type = atoi(optarg);
      printf("type %d\n",type);
      break;
    case 'i':
      printf("Input file name : %s\n",optarg);
      strcpy(inputfile,optarg);
      break;

    case 'O':
      printf("Output file name :%s\n",optarg);

      strcpy(outfile,optarg);
      break;

    default:
     printf("Incorrect uasge\n");
//./decomp -o 2 -t 1 -i url_rep_0_0_0_0_0_0_0_0_0.dat.gz  -O outcomp.tst.dat
     // usage();
    }
  }

 if (option == 2)
 {
  struct stat stat_st;
  int data_file_size =0; 
  int read_data_fd = 0;
  long read_bytes;
  int write_data_fd = 0;
  int write_bytes = 0;
  char *data_file_buf;
  

 if(stat(inputfile, &stat_st) == -1)
  {
     fprintf(stderr, "File %s does not exists. Exiting.\n", inputfile);
     return -1;
  }
  else
  {
    if(stat_st.st_size == 0)
    {
      fprintf(stderr, "File %s is of zero size. Exiting.\n", inputfile);
      return -1;
    }
  }
  data_file_size = stat_st.st_size;

  
 if ((read_data_fd = open(inputfile, O_RDONLY | O_LARGEFILE | O_CLOEXEC)) < 0){
    fprintf(stderr, "Error: Error in opening file (%s). Error = %s\n", inputfile, nslb_strerror(errno));
    return -1;
  }

  data_file_buf = (char *)malloc(data_file_size + 1);
  if (data_file_buf == NULL) 
  {
    fprintf(stderr, "Error: Out of memory.\n");
    return -1;
  }

  data_file_buf[0] = '\0';
  printf("IN FILE LEN %d\n ",data_file_size);
  read_bytes = nslb_read_file_and_fill_buf (read_data_fd, data_file_buf, data_file_size);
  close(read_data_fd);
  ns_decomp_do_new (data_file_buf, data_file_size, type, err_msg);

  if ((write_data_fd = open(outfile, O_RDWR|O_CLOEXEC|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH )) < 0)
  {
    fprintf(stderr, "Error: Error in opening file (%s). Error = %s\n", outfile, nslb_strerror(errno));
    return -1;
  }

  printf("OUTPUT BUFF LENGTH %d\n", uncomp_cur_len );
  //write_bytes = write_file_from_buf(write_data_fd, data_file_buf);
  write_bytes = write_file_from_buf(write_data_fd, uncomp_buf);
  close(write_data_fd);

  }

}
#endif





#ifdef TEST
/* compress or decompress from stdin to stdout */
int main(int argc, char **argv)
{
    int ret;
    FILE *source;
    int avail_in;
    char *in;
    int finish =0;
    short comp_type;
   
#if 0
    if (ns_decomp_init()) {
	printf("ns_decomp_init() failed\n");
	exit (1);
    }
#endif
    orig_buf = (char *)malloc (CHUNK);
    //comp_buf = (char *)malloc (CHUNK);

    if (argc != 3) {
	printf ("usage: %s: comp-type inp-file\n", argv[0]);
	exit(1);
    }

    comp_type = atoi (argv[1]);
    //0 : default zlib
    //1: gzip
    source = fopen(argv[2], "r");
    if (!source) {
	printf ("unable to open %s file", argv[2]);
	exit(1);
    }

    in = orig_buf;
    do {
        avail_in = fread(in, 1, orig_max_len-orig_cur_len, source);
        if (ferror(source)) {
	    printf ("error reading inputfile\n");
	    exit(1);
        }
        finish = feof(source) ? 1 : 0;
        orig_cur_len += avail_in;
	printf("Read %d bytes total=%d finish=%d\n", avail_in, orig_cur_len, finish);
        if (orig_cur_len == orig_max_len) {
	    printf("Reallocing for orig at %d\n", orig_max_len);
	    orig_buf = (char *)realloc (orig_buf, orig_max_len+CHUNK);
	    if (orig_buf == NULL) {
	        printf ("memrelaoc failed\n");
	        exit(1);
	    }
	    orig_max_len += CHUNK;
    	}
        in = orig_buf + orig_cur_len;
    } while (!finish);
    fclose(source);

    printf("compressing input size = %d\n", orig_cur_len);
    if (ns_comp_do(orig_buf, orig_cur_len, comp_type) != 0) {
	        printf ("def failed\n");
	        exit(1);
    }
    printf("uncompressing input size = %d\n", comp_cur_len);
    if (ns_decomp_do(comp_buf, comp_cur_len, comp_type) != 0) {
	        printf ("inf failed\n");
	        exit(1);
    }
    printf("uncompressed size = %d\n", uncomp_cur_len);
    if (uncomp_cur_len != orig_cur_len) {
	        printf ("comp/decomp not OK\n");
	        exit(1);
    } 
    if (bcmp(orig_buf, uncomp_buf, uncomp_cur_len) != 0) {
	        printf ("comp/decomp bytes not OK\n");
	        exit(1);
    } 
    printf("All OK\n");
}
#endif

#ifdef FOOTEST1
/* compress or decompress from stdin to stdout */
int main(int argc, char **argv)
{
    int ret;
    FILE *source, *dest;
    int avail_in;
    char *in;
    int finish =0;
    short comp_type;
   
#if 0
    if (ns_decomp_init()) {
	printf("ns_decomp_init() failed\n");
	exit (1);
    }
#endif
    orig_buf = (char *)malloc (CHUNK);
    //comp_buf = (char *)malloc (CHUNK);

    if (argc != 4) {
	printf ("usage: %s: comp-type inp-file\n", argv[0]);
	exit(1);
    }

    comp_type = atoi (argv[1]);
    //0 : default zlib
    //1: gzip
    source = fopen(argv[2], "r");
    dest = fopen(argv[3], "w");
    
    if (!source) {
	printf ("unable to open %s file", argv[2]);
	exit(1);
    }

    in = orig_buf;
    do {
        avail_in = fread(in, 1, orig_max_len-orig_cur_len, source);
        if (ferror(source)) {
	    printf ("error reading inputfile\n");
	    exit(1);
        }
        finish = feof(source) ? 1 : 0;
        orig_cur_len += avail_in;
	printf("Read %d bytes total=%d finish=%d\n", avail_in, orig_cur_len, finish);
        if (orig_cur_len == orig_max_len) {
	    printf("Reallocing for orig at %d\n", orig_max_len);
	    orig_buf = (char *)realloc (orig_buf, orig_max_len+CHUNK);
	    if (orig_buf == NULL) {
	        printf ("memrelaoc failed\n");
	        exit(1);
	    }
	    orig_max_len += CHUNK;
    	}
        in = orig_buf + orig_cur_len;
    } while (!finish);
    fclose(source);

    printf("compressing input size = %d\n", orig_cur_len);
    if (ns_comp_do(orig_buf, orig_cur_len, comp_type) != 0) {
	        printf ("def failed\n");
	        exit(1);
    }
    fwrite(comp_buf, comp_cur_len, 1, dest);

    printf("uncompressing input size = %d\n", comp_cur_len);
    if (ns_decomp_do(comp_buf, comp_cur_len, comp_type) != 0) {
	        printf ("inf failed\n");
	        exit(1);
    }
    printf("uncompressed size = %d\n", uncomp_cur_len);
    if (uncomp_cur_len != orig_cur_len) {
	        printf ("comp/decomp not OK\n");
	        exit(1);
    } 
    if (bcmp(orig_buf, uncomp_buf, uncomp_cur_len) != 0) {
	        printf ("comp/decomp bytes not OK\n");
	        exit(1);
    } 
    printf("All OK\n");
}
#endif /* FOOTEST */
#ifdef FOOTEST
/* compress or decompress from stdin to stdout */
int main(int argc, char **argv)
{
    int ret;
    FILE *source, *dest;
    int avail_in;
    char *in;
    int finish =0;
    short comp_type;
    int inited = 0;
   
#if 0
    if (ns_decomp_init()) {
	printf("ns_decomp_init() failed\n");
	exit (1);
    }
#endif
    orig_buf = (char *)malloc (CHUNK);
    //comp_buf = (char *)malloc (CHUNK);

    if (argc != 3) {
	printf ("usage: %s: comp-type out-file\n", argv[0]);
	exit(1);
    }

    comp_type = atoi (argv[1]);
    //0 : default zlib
    //1: gzip
    dest = fopen(argv[2], "w");
    
    char infile[1024];
    
    source = NULL;
    while (1) {
      printf("Input file : ");
      gets(infile);
      source = fopen(infile, "r");
      if (!source) {
	printf ("unable to open %s file", infile);
        break;
      }

      in = orig_buf;
      orig_cur_len  = 0;

      do {
        avail_in = fread(in, 1, orig_max_len-orig_cur_len, source);
        if (ferror(source)) {
          printf ("error reading inputfile\n");
          exit(1);
        }
        finish = feof(source) ? 1 : 0;
        orig_cur_len += avail_in;
	printf("Read %d bytes total=%d finish=%d\n", avail_in, orig_cur_len, finish);
        if (orig_cur_len == orig_max_len) {
          printf("Reallocing for orig at %d\n", orig_max_len);
          orig_buf = (char *)realloc (orig_buf, orig_max_len+CHUNK);
          if (orig_buf == NULL) {
            printf ("memrelaoc failed\n");
            exit(1);
          }
          orig_max_len += CHUNK;
    	}
        in = orig_buf + orig_cur_len;
      } while (!finish);
      fclose(source);

      /*     printf("compressing input size = %d\n", orig_cur_len); */
      /*     if (ns_comp_do(orig_buf, orig_cur_len, comp_type) != 0) { */
      /* 	        printf ("def failed\n"); */
      /* 	        exit(1); */
      /*     } */
      /*     fwrite(comp_buf, comp_cur_len, 1, dest); */

      printf("uncompressing input size = %d\n", orig_cur_len);
      if (inited == 0) {
        init_ns_decomp_do_continue(comp_type);
        inited = 1;
      }

      if ((ret = ns_decomp_do_continue(orig_buf, orig_cur_len)) == 0)
        break;
      if (ret == -1) break;
      
    }

    fwrite(uncomp_buf, uncomp_cur_len, 1, dest);
    printf("uncompressed size = %d\n", uncomp_cur_len);

/*     if (uncomp_cur_len != orig_cur_len) { */
/* 	        printf ("comp/decomp not OK\n"); */
/* 	        exit(1); */
/*     }  */
/*     if (bcmp(orig_buf, uncomp_buf, uncomp_cur_len) != 0) { */
/* 	        printf ("comp/decomp bytes not OK\n"); */
/* 	        exit(1); */
/*     }  */
    printf("All OK\n");
}
#endif /* FOOTEST1 */
