#ifndef NS_BODY_ENCRYPT_H  
#define NS_BODY_ENCRYPT_H  


#define ENC_ERR_BUF_SIZE	512
#define ENC_KEY_IVEC_SIZE	32
#define AES_DATA_LEN		512
#define KEY_IVEC_BUF_SIZE 	44

#define AES_NONE 	0
#define AES_128_CBC	1
#define AES_128_CTR	2
#define AES_192_CBC	3
#define AES_192_CTR	4
#define AES_192_ECB	5
#define AES_256_CBC	6
#define AES_256_CTR	7


/*---------------------------------------------------------------------------------------------------------------- 
 * This structure is for mapping of keyIvec size with different encryption algorithm
 *
 *------------------------------------------------------------------------------------------------------------------*/

typedef struct 
{
  char raw_size;
  char encoded_size; 
  char block_size; 
} keyIvecSize;

typedef struct 
{ 
  char encryption_algo;
  char base64_encode_option;
  char key_size;
  char ivec_size; 
  char key[ENC_KEY_IVEC_SIZE + 1];
  char ivec[ENC_KEY_IVEC_SIZE + 1]; 
} BodyEncryption;

extern int ns_do_encrypt(unsigned char *buffer, int buffer_len, char encryption_algo, char base64_encode_option, char *key, char *ivec, unsigned char *encrypted_buffer, int encrypted_buffer_size, char *enc_err_msg);
extern int nslb_do_crypt(unsigned char *buffer, int buff_length, char encryption_algo, char do_base64_encoding, char *key, char *iv, unsigned char *out_buffer, int out_buffer_size, char do_encrypt, char *err_msg);
extern unsigned char *ns_aes_crypt(unsigned char *buffer, int buffer_len, int encryption_algo, char base64_encode_option, char *key, int key_len, char *ivec, int ivec_len, int do_encrypt, char **err_msg);
#endif
