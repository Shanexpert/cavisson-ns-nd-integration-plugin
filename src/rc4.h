#ifndef RC4_H
#define RC4_H
#define ARES_ID_KEY_LEN 31
typedef struct rc4_key
{
  unsigned char state[256];
  unsigned char x;
  unsigned char y;
} rc4_key;

extern rc4_key dns_global_id_key;

#define ARES_SWAP_BYTE(a,b) \
  { unsigned char swapByte = *(a);  *(a) = *(b);  *(b) = swapByte; }

int init_id_key(rc4_key* key,int key_data_len);
unsigned short ares__generate_new_id(rc4_key* key);
void ares__rc4(rc4_key* key, unsigned char *buffer_ptr, int buffer_len);
#endif
