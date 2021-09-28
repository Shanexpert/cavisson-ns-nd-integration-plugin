
#include <stdio.h>
#include <unistd.h>
#include "amf.h"

#define AMF_BINARY                   0
#define AMF_ASCII_HEX                1

#define AMF_DECODE_BINARY_TO_XML     1
#define AMF_ENCODE_XML_TO_BINARY     2

// Usage

static void Usage(char *err)
{
  fprintf(stderr, "%s\n", err);
  fprintf(stderr, "Usage: \n");
  fprintf(stderr, "nsi_amf -e or -d -i <in_file> -o <out_file> [-H -D <debug level>]\n");
  fprintf(stderr, "Where: \n");
  fprintf(stderr, "  -d to convert AMF Binary to AMF XMl\n");
  fprintf(stderr, "  -e to convert AMF XML to AMF Binary\n");
  fprintf(stderr, "  -H to spcify that input file is in ASCII Hex (To be used later ...)\n");

  exit(1);
}

static FILE *open_in_file(char *in_file)
{
  FILE *fp = fopen (in_file, "r");
  if (!fp) {
    printf ("unable to open input file %s\n", in_file);
    exit (1);
  }
  return(fp);
}

static FILE *open_out_file(char *out_file)
{

  fprintf(stdout, "Opening output file %s\n", out_file);

  FILE *fp = fopen (out_file, "w");
  if (!fp) {
    printf ("unable to open output file %s\n", out_file);
    exit (1);
  }

  return (fp);
}

//TODO: We need to make a check that file size should not be more than 2MB
#define MAX_MSG_LEN  0x200000// 2MB

static void  amf_binary_to_xml(char *in_file, char *out_file)
{
char message[MAX_MSG_LEN];
FILE *fp, *fpout;
int len, cur = 0;

  // Open amf data file (hex dump)
  fp = open_in_file(in_file);

  fprintf(stdout, "Reading input file %s\n", in_file);
  cur = fread(message, 1, MAX_MSG_LEN, fp);
  fprintf(stdout, "Input file size %d bytes\n", cur);

  fclose (fp);

  fpout = open_out_file(out_file);

  fprintf(stdout, "Converting AMF binary to XML\n");

  //if(amf_encode(1, message, cur, msgout, &len, 0, &version) == NULL)
  if ((len = ns_amf_binary_to_xml (message, &cur)) != -1)
  {
    if(amf_asc_ptr != NULL)
       fprintf (fpout, "%s", amf_asc_ptr);
  }
  fclose (fpout);
}


static void  amf_hex_to_xml(char *in_file, char *out_file)
{
char message[MAX_MSG_LEN];
FILE *fp, *fpout;
char buf[1024], *ptr;
int cur = 0, len;
long num;    //SS: Used to store return value of strtol

  // Open amf data file (hex dump)
  fp = open_in_file(in_file);

  //Read Hex bytes encoded message
  while (fgets (buf, 1024, fp)) {
    buf[strlen (buf) - 1] = '\0';
    ptr = strtok (buf, " ");
    while (ptr) {
      num = strtol (ptr, NULL, 16);
      if (num > 256) {
        printf ("Exception got num (%ld) >256\n", num);
        exit (1);
      }
      message[cur++] = num;
      ptr = strtok (NULL, " ");
    }
  }
  printf ("Total bytes = %d\n", cur);

  fclose (fp);

  fpout = open_out_file(out_file);
  fprintf(stdout, "Converting AMF Hex to XML\n");

  if ((len = ns_amf_binary_to_xml (message, &cur)) != -1)
  {
    if(amf_asc_ptr != NULL)
       fprintf (fpout, "%s", amf_asc_ptr);
  }
  fclose (fpout);

}


static void  amf_xml_to_binary(char *in_file, char *out_file)
{
char message[MAX_MSG_LEN];
char msgout[MAX_MSG_LEN];

FILE *fp, *fpout;
int cur = 0, len;

  // Open amf data file (hex dump)
  fp = open_in_file(in_file);

  fprintf(stdout, "Reading input file %s\n", in_file);
  cur = fread(message, 1, MAX_MSG_LEN, fp);
  fprintf(stdout, "Input file size %d bytes\n", cur);

  fclose (fp);

  fprintf(stdout, "Converting AMF Binary to XML\n");

  //amfin_lineno and amf_infp must be initialized before using read_amf and skip amfr+debug
  len = MAX_MSG_LEN;
  int amf_version;

#define AMF_SRC_IS_BUFFER   1
#define AMF_DO_NOT_SEGMENT  0

  // This method reduced the len by the size of binary data
  if (amf_encode (AMF_SRC_IS_BUFFER, message, cur, msgout, &len, AMF_DO_NOT_SEGMENT, 1, &amf_version) == NULL)
    return;
  int amf_bin_len = MAX_MSG_LEN - len;
  fprintf(stdout, "XML to AMF Conversion Successful (size=%d)\n", amf_bin_len);

  fprintf(stdout, "Saving XML in out file (size = %d)\n", amf_bin_len);

  fpout = open_out_file(out_file);
  fwrite(msgout, amf_bin_len, 1, fpout);
  fclose (fpout);

#if 0

  // TODO skip_amf_debug ();
  //show_buf (msgout, 64*1024-len);
  cur = MAX_MSG_LEN - len;
  //show_buf(message, cur);
  for (i = 0; i < cur; i++) {
    if (i != 0 && i % 16 == 0)
      fprintf (fpout, "\n");
      fprintf (fpout, "%02x ", (unsigned char) msgout[i]);
    }
    fprintf (fpout, "\n");
    //cur = 64*1024-len;
    //write_amf (0, stdout, 4, msgout , &cur );
  }
  fclose (fpout);

#endif

}


int main (int argc, char *argv[])
{
int type  = -1;
int format = AMF_BINARY;
char c;

char in_file[1024] = "", out_file[1024] = "";

  /* Parse args. */
  while ((c = getopt(argc, argv, "edHi:o:D:")) != -1)
  {
    switch (c)
    {
      case 'H':
        format = AMF_ASCII_HEX;
        break;

      case 'd':
        type = AMF_DECODE_BINARY_TO_XML;
        break;

      case 'e':
        type = AMF_ENCODE_XML_TO_BINARY;
        break;

      case 'i':
        strcpy(in_file, optarg);
        break;

      case 'o':
        strcpy(out_file, optarg);
        break;

      case 'D':
        amf_set_debug(atoi(optarg));
        break;

      case '?':
        Usage("Invalid arguments");
        break;
    }
  }

  if((type == -1) || (in_file[0] == '\0') || (out_file[0] == '\0'))
    Usage("Invalid arguments");

  if(type == AMF_DECODE_BINARY_TO_XML)
  {
    if(format == AMF_BINARY)
      amf_binary_to_xml(in_file, out_file);
    else
      amf_hex_to_xml(in_file, out_file);
    return 0;
  }

  if(type == AMF_ENCODE_XML_TO_BINARY)
  {
    amf_xml_to_binary(in_file, out_file);
    return 0;
  }

  return 0;
}

