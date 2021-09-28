#include<stdio.h>
#include<string.h>
#include <stdlib.h>
#include <openssl/des.h>


// Array of tripletes used for kohls to decode
static char *cipher_arr[] = {
        "ZZE", "i2x", "aEG", "qbJ", "qIz", "ao2", "82y", "EoV", "qXg", "FWD", 
        "swG", "IGU", "K7N", "lDK", "MpG", "Q2T", "qYr", "OCB", "GUs", "I2i", 
        "dvO", "4dw", "ls5", "BMY", "Z4F", "Vxt", "jjB", "L9N", "NCV", "7Ul", 
        "iwv", "pdl", "JwL", "ugs", "vDJ", "Ujz", "Leh", "zrt", "Hul", "HAq", 
        "Roz", "DMM", "s3h", "Eg9", "56K", "NAO", "B88", "JAJ", "LD5", "AQR", 
        "eiZ", "TiW", "Qwe", "N9E", "LLu", "EM8", "vTW", "14E", "4S8", "mBj", 
        "HMX", "Nbp", "btT", "Wcy", "py6", "iZj", "SuR", "E2Q", "Eep", "fAf", 
        "dPL", "lSc", "nQX", "Cbl", "KjA", "gi2", "xfY", "lJb", "Vdg", "BB3", 
        "ZwE", "8yf", "Auh", "gje", "Tq3", "qCX", "jt0", "cX9", "ucV", "CMi", 
        "DWV", "yPX", "7ZF", "IEj", "DbJ", "kTN", "LBO", "PPj", "J0S", "0uD", 
        "Ubz", "aBx", "atR", "o7Q", "RFi", "417", "yuC", "yNg", "bpl", "Hea", 
        "huL", "v6A", "OTX", "EmM", "Rfd", "pCN", "PS6", "TfO", "6Wf", "j3U", 
        "WIw", "GMP", "kRP", "Nb5", "2E5", "iVS", "bAc", "Zus", "z1k", "XM1", 
        "isb", "bu0", "fJ0", "GXR", "hVj", "xB0", "35M", "pdQ", "gNW", "cf2", 
        "OQd", "fR9", "fbt", "wvt", "lU4", "Y9u", "1K6", "XHF", "ooE", "g2i", 
        "YNk", "5Kw", "LYR", "LZ6", "3XE", "JD6", "Vit", "OM2", "bro", "OeD", 
        "kgx", "0HF", "1Ki", "Cpl", "b81", "Ph2", "E1I", "Dpf", "v6C", "9MK", 
        "pvA", "szv", "HoN", "c0c", "djR", "8IG", "9DW", "wV3", "Hzb", "fQV", 
        "Lf9", "3SQ", "kWy", "YEg", "YAI", "PNY", "4km", "NcB", "s7X", "WQe", 
        "CCs", "k2Y", "h70", "9Mp", "Ah2", "fRS", "Rs5", "8jW", "oBF", "ERX", 
        "8Iv", "4jw", "qoc", "vt0", "FId", "7Jm", "YEM", "54c", "yke", "KeT", 
        "Xbu", "PTS", "Y5U", "V5n", "C9l", "uBl", "Rmz", "xHa", "OPh", "z40", 
        "jTT", "t51", "36d", "9Mz", "MY3", "6cu", "Zfv", "wK3", "gWm", "GNt", 
        "ls3", "r8b", "dA2", "xL0", "orB", "d61", "IHS", "Jr7", "UtW", "SSR", 
        "oqx", "b0E", "umK", "0wI", "4eZ", "75o", "9yp", "Ck2", "qBi", "v3r", 
        "ie0", "vaV", "9RS", "DMc", "AS8", "kw0"
    };

int get_index_in_array(char *str){

  int i;
  NSDL2_API(NULL, NULL, "Method called. str = [%s]", str);

  for(i = 0; i < 256; i++){
    if(!strcmp(str,  cipher_arr[i]))
      return i;
  }
  NSDL2_API(NULL, NULL, "Index not found in array. Exiting from captcha decrypt");
  return(-1);
}  

int triplete_decode_bytes(char *str, char *return_str, int *out_len){

  char split_arr[16];
  char *tmp;
  char *start_ptr;
  int j = 0, break_loop = 0, idx;
  start_ptr = str;

  NSDL2_API(NULL, NULL, "Method called. input = [%s]", str);

  while(1){

    NSDL2_API(NULL, NULL, "start_ptr = [%s]", start_ptr);

    tmp = strchr(start_ptr, '-');

    if(tmp){
      strncpy(split_arr, start_ptr, tmp - start_ptr);
      split_arr[tmp - start_ptr] = '\0';		
      NSDL2_API(NULL, NULL, "string = [%s]", split_arr);
      start_ptr = tmp +1; 
    } else {
      strcpy(split_arr, start_ptr);
      NSDL2_API(NULL, NULL, "last_string = [%s]", split_arr);
      break_loop = 1;
    }
    idx = get_index_in_array(split_arr);
    NSDL2_API(NULL, NULL, "idx hex = [%02x]", idx);
    if(idx == -1)
      return -1; 
    if(idx < 127)
      idx -= 256; 
    return_str[j] = (char)idx;
    j++; 
    if(break_loop) 
      break;
  }
 
  *out_len = j; // Set len, it will be used in next method

  return 0;
}

int des_decrypt(char *in, int in_len, char *out, int out_len, char *key)
{
  int i;
  DES_cblock key1, key2, key3;
  DES_key_schedule ks1, ks2, ks3;

  NSDL2_API(NULL, NULL, "Method called. in = [%s], out = [%s], key = [%s]", in, out, key);

  if(out_len < in_len){
    NSDL2_API(NULL, NULL, "Error: Out len %d is not sufficient to save decrypted of len %d captcha", out_len, in_len);
    return -1;
  }

  // set out to 0
  memset(out, 0, out_len);

  if(strlen(key) != 24){
    fprintf(stderr, "Invalid key %s. Exiting.\n", key);
    return (-1);
  }

  memcpy(&key1, key, 8);
  memcpy(&key2, key + 8, 8);
  memcpy(&key3, key + 16, 8);

  NSDL2_API(NULL, NULL, "key1 = %*.*s\n", 8, 8, key1);
  NSDL2_API(NULL, NULL, "key2 = %*.*s\n", 8, 8, key2);
  NSDL2_API(NULL, NULL, "key3 = %*.*s\n", 8, 8, key3);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  DES_set_key((C_Block *)key1, &ks1);
  DES_set_key((C_Block *)key2, &ks2);
  DES_set_key((C_Block *)key3, &ks3);
#else
  DES_set_key((DES_cblock *)key1, &ks1);
  DES_set_key((DES_cblock *)key2, &ks2);
  DES_set_key((DES_cblock *)key3, &ks3);
#endif

  // Decrypt data in chunk of 8 bytes 
  for (i = 0; i < in_len; i += 8) {
   #if OPENSSL_VERSION_NUMBER < 0x10100000L
    DES_ecb3_encrypt((C_Block *)(in + i),(C_Block *)(out + i), &ks1, &ks2, &ks3, DES_DECRYPT);
   #else 
    DES_ecb3_encrypt((DES_cblock *)(in + i),(DES_cblock *)(out + i), &ks1, &ks2, &ks3, DES_DECRYPT);
   #endif
  }
 
  NSDL2_API(NULL, NULL, "Decryptrd_text= [%s]", out);

  return 0; 
}

void remove_padding(char *in){

  int i;
  int len = strlen(in);
  char c = in[len-1];
  int index = len -1;

  NSDL2_API(NULL, NULL, "Method called, in= [%s], len = [%d]", in, len);

  for(i = len - 2; i >= 0; i--){
    NSDL2_API(NULL, NULL, "char = [%c], in[i] = [%c], i = [%d]", c, in[i], i);
    if(c == in[i]){
      NSDL2_API(NULL, NULL, "char = [%c], in[i]= [%c], i = [%d]", c, in[i], i);
      index = i;
    } else {
      break;
    }
    c = in[i];
  }

  if(index < len){
    NSDL2_API(NULL, NULL, "index = [%d]", index);
 
    in[index] = '\0';
  } else if( index == len && c & 0x00000001) {
    in[index] = '\0';

  }
}

