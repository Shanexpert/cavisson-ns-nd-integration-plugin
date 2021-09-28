/*****************************************************************************************************
 * Name	     : ns_body_encrypt.c
 * Purpose   : This file contains all the functions related to AES Encryption/Decryption. 
 * Code Flow : 
 * Author(s) : Devendar Jain, Deepika
 * Date      : 15 Jun 2018
 * Copyright : (c) Cavisson Systems
 * Modification History :
 *
 *****************************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ns_body_encrypt.h"
#include "util.h"
#include "ns_string.h"
#include "ns_script_parse.h"
#include "ns_trace_level.h"
#include "nslb_encode_decode.h"
#include "ns_iovec.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

static keyIvecSize keyIvecSizeMap[AES_256_CTR + 1] = 
{
  {0,0,0},        /*NONE*/
  {16,24,16},     /*AES_128_CBC*/
  {16,24,0},      /*AES_128_CTR*/
  {24,32,16},     /*AES_192_CBC*/
  {24,32,0},      /*AES_192_CTR*/
  {24,32,16},      /*AES_192_ECB*/
  {32,44,16},     /*AES_256_CBC*/
  {32,44,0}       /*AES_256_CTR*/
};

static int get_base64_encode_option(char *str)
{
  NSDL2_HTTP(NULL, NULL, "str = %s", str);
  if(!strcasecmp(str,"NONE"))
    return NONE;
  if(!strcasecmp(str,"KEY_IVEC"))
    return KEY_IVEC;
  if(!strcasecmp(str,"BODY"))
    return BODY;
  if(!strcasecmp(str,"KEY_IVEC_BODY"))
    return KEY_IVEC_BODY;

  return -1;
}

static int get_aes_algo(char *str)
{
  NSDL2_HTTP(NULL, NULL, "str = %s", str);

  if(!strcasecmp(str,"NONE"))
    return AES_NONE;
  if(!strcasecmp(str,"AES_128_CBC"))
    return AES_128_CBC;
  if(!strcasecmp(str,"AES_128_CTR"))
    return AES_128_CTR;
  if(!strcasecmp(str,"AES_192_CBC"))
    return AES_192_CBC;
  if(!strcasecmp(str,"AES_192_CTR"))
    return AES_192_CTR;
  if(!strcasecmp(str,"AES_256_CBC"))
    return AES_256_CBC;
  if(!strcasecmp(str,"AES_256_CTR"))
    return AES_256_CTR; 

  return -1; 
}


/*---------------------------------------------------------------------------------------------------------------- 
 * Fun Name  : kw_g_body_encryption 
 *
 * Purpose   : This function will parse keyword G_BODY_ENCRYPTION and fill user provided inputs
 *             into DS GroupSettings  
 *
 * Input     : buf = G_BODY_ENCRYPTION <Group> <algo> <is_base64_encoded> <Key> <IVec> 
 *
 * Output    : On error     -1
 *             On success    0
 *        
 * Build_v   : 4.1.12 #9 
 *------------------------------------------------------------------------------------------------------------------*/
int kw_g_body_encryption(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[AES_DATA_LEN + 1] = "";
  char group_name[AES_DATA_LEN + 1] = "";
  char encryption_algo[AES_DATA_LEN + 1] = "";
  char base64_encode_option[AES_DATA_LEN + 1] = "";
  char key[ENC_KEY_IVEC_SIZE + 1] = "";
  char ivec[ENC_KEY_IVEC_SIZE + 1] = "";
  char key_len = 0, ivec_len = 0, base64_encode_opt;
  char enc_algo = 0;
  int key_ivec_size;

  NSDL1_PARSING(NULL, NULL, "Method Called, buf = [%s], gset = [%p]", buf, gset);

  int count_arg = sscanf(buf, "%s %s %s %s %s %s", keyword, group_name, encryption_algo, base64_encode_option, key, ivec);

  NSDL1_PARSING(NULL, NULL, "keyword = %s, group_name = %s, encryption_algo = %s, base64_encode_option = %s, key = %s, ivec = %s",
                             keyword, group_name, encryption_algo, base64_encode_option, key, ivec);

  enc_algo = get_aes_algo(encryption_algo);

  NSDL4_PARSING(NULL, NULL, "enc_algo = %d", enc_algo);
  if(enc_algo < 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_BODY_ENCRYPTION_USAGE, CAV_ERR_1011107, CAV_ERR_MSG_3);
  }
  gset->body_encryption.encryption_algo = enc_algo;

  if(enc_algo == AES_NONE)
  {
    NSDL2_PARSING(NULL, NULL, "In G_BODY_ENCRYPTION: Encryption algo is NONE and don't need to parse other attributes, hence returning.");
    return 0;
  }

  if(count_arg != 6)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_BODY_ENCRYPTION_USAGE, CAV_ERR_1011107, CAV_ERR_MSG_1);
  }
  
  base64_encode_opt = get_base64_encode_option(base64_encode_option);
  if(base64_encode_opt < 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_BODY_ENCRYPTION_USAGE, CAV_ERR_1011107, CAV_ERR_MSG_3);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  NSDL4_HTTP(NULL, NULL, "base64_encode_opt = %d", base64_encode_opt);
  key_ivec_size = (base64_encode_opt == KEY_IVEC || base64_encode_opt == KEY_IVEC_BODY)?
                   keyIvecSizeMap[(int)enc_algo].encoded_size: keyIvecSizeMap[(int)enc_algo].raw_size;

  key_len = strlen(key); 
  ivec_len = strlen(ivec);

  NSDL2_HTTP(NULL, NULL, "key_ivec_size = %d, key_len = %d, ivec_len = %d", key_ivec_size, key_len, ivec_len);
  if((key_len != key_ivec_size) || (ivec_len != key_ivec_size))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_BODY_ENCRYPTION_USAGE, CAV_ERR_1011108, "");
  }

  strcpy(gset->body_encryption.key, key);
  strcpy(gset->body_encryption.ivec, ivec);

  gset->body_encryption.base64_encode_option = base64_encode_opt;
  gset->body_encryption.key_size = key_len;
  gset->body_encryption.ivec_size = ivec_len;

  NSDL1_PARSING(NULL, NULL, "Method end, encryption_algo  = [%d], base64_encode_option = [%d], key = [%s], key_size = [%d], "
                            "ivec = [%s], ivec_size = [%d]",
                             gset->body_encryption.encryption_algo, gset->body_encryption.base64_encode_option, 
			     gset->body_encryption.key, gset->body_encryption.key_size, gset->body_encryption.ivec,
                             gset->body_encryption.ivec_size);

  return 0;
}

/*
ns_web_url( “Page_name”,
            “URL=<url_name>”,
	    “BODY_ENCRYPTION=<encryption_algo>,<base64_encode_option>,<key>,<ivec>”,
	    “BODY=<body>”);
*/

int set_body_encryption_param(char *attribute_value, char *flow_file, int url_idx, int sess_idx, int script_ln_no)
{
  int num_tok = 0; 
  char *attr = NULL;
  char *fields[AES_DATA_LEN + 1];
  char key_len = 0, ivec_len = 0, key_param = 0, ivec_param = 0;

  BodyEncryptionArgs *body_encrypt = &requests[url_idx].proto.http.body_encryption_args;

  NSDL2_PARSING(NULL, NULL, "Method called, attribute_value = [%s], flow_file = [%s], url_idx = [%d], sess_idx = [%d], script_ln_no = [%d]",
                             attribute_value, flow_file, url_idx, sess_idx, script_ln_no);
  if(!attribute_value[0])
  {
    fprintf(stderr, "Error: BODY_ENCRYPTION has no attribute value.\n");
    return NS_PARSE_SCRIPT_ERROR;
  }

  attr = attribute_value;
  num_tok = get_tokens(attr, fields, "," , 5);

  if((num_tok != 4) && (strcasecmp(fields[0], "NONE")))
  {
    fprintf(stderr, "Error: BODY_ENCRYPTION either should have 4 attributes or NONE in encryption_algo.\n");
    return NS_PARSE_SCRIPT_ERROR;
  }

  NSDL4_PARSING(NULL, NULL, "Encryption_algo = %s, key_ivec_base64_encoded = %s, key = %s, ivec = %s", 
                             fields[0], fields[1], fields[2], fields[3]);

  body_encrypt->encryption_algo = get_aes_algo(fields[0]);
  NSDL4_PARSING(NULL, NULL, "enc_algo = %d", body_encrypt->encryption_algo);

  if(body_encrypt->encryption_algo < 0)
  {
    fprintf(stderr, "Error: BODY_ENCRYPTION has wrong value [%s] in encryption_algo attribute.\n", fields[0]);
    return NS_PARSE_SCRIPT_ERROR;
  }
  
  if(body_encrypt->encryption_algo == AES_NONE)
  {
    NSDL2_PARSING(NULL, NULL, "In BODY_ENCRYPTION: Encryption algo is NONE and don't need to parse other attributes, hence returning.");
    //body_encrypt->body_encryption_none_flag = 1;
    return 0;
  }

  body_encrypt->base64_encode_option = get_base64_encode_option(fields[1]);
  if(body_encrypt->base64_encode_option < 0)
  {
    fprintf(stderr, "Error: BODY_ENCRYPTION has wrong value [%s] in key_ivec_base64_encoded attribute.\n", fields[1]);
    return NS_PARSE_SCRIPT_ERROR;
  }

  //TODO: Need to discuss if key has { at start and } at end, as we are using these {} as parameters
  char key_ivec_size;
  key_len = strlen(fields[2]); 
  ivec_len = strlen(fields[3]);
  //Check length of key based on encryption_algo
  if((fields[2][0] == '{') && (fields[2][key_len - 1] == '}'))
    key_param = 1;
  if((fields[3][0] == '{') && (fields[3][ivec_len - 1] == '}'))
    ivec_param = 1;

  NSDL2_HTTP(NULL, NULL, "key_param = %d, ivec_param = %d, fields[2][0] = %c, fields[2][key_len - 1] = %c", 
                          key_param, ivec_param, fields[2][0], fields[2][key_len - 1]);

  if(!key_param || !ivec_param)
  {
    key_ivec_size = (body_encrypt->base64_encode_option == KEY_IVEC || body_encrypt->base64_encode_option == KEY_IVEC_BODY)?
                 keyIvecSizeMap[(int)body_encrypt->encryption_algo].encoded_size : keyIvecSizeMap[(int)body_encrypt->encryption_algo].raw_size;
  
    NSDL2_HTTP(NULL, NULL, "key_ivec_size = %d, key_len = %d, ivec_len = %d", key_ivec_size, key_len, ivec_len);
    
    if(!key_param && (key_len != key_ivec_size))
    {
      fprintf(stderr, "Error: BODY_ENCRYPTION, key length should be [%d].\n", key_ivec_size);
      return NS_PARSE_SCRIPT_ERROR;
    }
   
    if(!ivec_param && (ivec_len != key_ivec_size)) 
    {
      fprintf(stderr, "Error: BODY_ENCRYPTION, ivec length should be [%d].\n", key_ivec_size);
      return NS_PARSE_SCRIPT_ERROR;
    }    
  }

  //TODO: If {} inside the key and ivec value then need to disscuss.
  NSDL2_PARSING(NULL, NULL, "Segmenting BODY_ENCRYPTION key = [%s]", fields[2]);
  segment_line(&(body_encrypt->key), fields[2], 0, script_ln_no, sess_idx, flow_file);

  NSDL2_PARSING(NULL, NULL, "Segmenting BODY_ENCRYPTION ivec = [%s]", fields[3]);
  segment_line(&(body_encrypt->ivec), fields[3], 0, script_ln_no, sess_idx, flow_file);

  NSDL2_PARSING(NULL, NULL, "encryption_algo = %d, base64_encode_option = %d, key_seg_start = %d, ivec_seg_start = %d",
                             body_encrypt->encryption_algo, body_encrypt->base64_encode_option, 
                             body_encrypt->key.seg_start, body_encrypt->ivec.seg_start); 
  return 0;
}

int ns_do_crypt(unsigned char *buffer, int buffer_len, char encryption_algo, char base64_encode_option, char *key,
                  char *ivec, unsigned char *encrypted_buffer, int encrypted_buffer_size, char do_encrypt, char *enc_err_msg)
{
  char key_new[ENC_KEY_IVEC_SIZE] = "";
  char iv_new[ENC_KEY_IVEC_SIZE] = "";
  char do_base64_encoding;
  int len;

  NSDL2_HTTP(NULL, NULL, "buffer = %s, buffer_len = %d, encryption_algo = %d, base64_encode_option = %d, key = %s, "
                         "ivec = %s, encrypted_buffer = %s, encrypted_buffer_size = %d", buffer, buffer_len, encryption_algo, 
                         base64_encode_option, key, ivec, encrypted_buffer, encrypted_buffer_size);

  if(base64_encode_option == KEY_IVEC || base64_encode_option == KEY_IVEC_BODY)
  {
    NSDL2_HTTP(NULL, NULL, "base64_encode_option = %d", base64_encode_option);
    nslb_decode_base64_ex((unsigned char *)key, strlen(key), (unsigned char *)key_new, (int)keyIvecSizeMap[(int) encryption_algo].encoded_size);
    key = key_new;
    
    if (ivec)
    {
      nslb_decode_base64_ex((unsigned char *)ivec, strlen(ivec), (unsigned char *)iv_new, (int)keyIvecSizeMap[(int) encryption_algo].encoded_size);
      ivec = iv_new;
    }
    NSDL2_HTTP(NULL, NULL, "key = %s, ivec = %s", key, ivec);
  }

  do_base64_encoding = (base64_encode_option == BODY || base64_encode_option == KEY_IVEC_BODY) ? 1 : 0;

  if((len = nslb_do_crypt(buffer, buffer_len, encryption_algo, do_base64_encoding, key, ivec, encrypted_buffer, encrypted_buffer_size, do_encrypt, enc_err_msg)) < 0)
  {
    NSTL1(NULL, NULL, "Error: In nslb_do_crypt = [%d]", encryption_algo);
    return -1;
  }

  encrypted_buffer[len] = '\0';
  return len;
}
 

unsigned char* make_encrypted_body(NSIOVector *ns_io_data_vec, char encryption_algo, char base64_encode_option, 
                                   char *key, int key_len, char *ivec, int ivec_len, int *content_length)
{
  int encrypted_buffer_length = 0;
  static int  encrypted_buffer_size = 0;
  static unsigned char *encrypted_buffer = NULL; 

  NSDL2_HTTP(NULL, NULL, "encryption_algo = %d, base64_encode_option = %d, "
                         "key = %s, key_len = %d, ivec = %s, ivec_len = %d, content_length = %d",
                          encryption_algo, base64_encode_option, key, key_len, 
                          ivec, ivec_len, *content_length);

  int key_ivec_size = (base64_encode_option == KEY_IVEC || base64_encode_option == KEY_IVEC_BODY)? 
                  keyIvecSizeMap[(int)encryption_algo].encoded_size: keyIvecSizeMap[(int)encryption_algo].raw_size;
  if(key_len != key_ivec_size || ivec_len != key_ivec_size)
  {
    NSTL1(NULL, NULL, "Error: In ns_aes_encrypt API, key and ivec length should be as per encryption_algo");
    return NULL;
  } 
  char block_size = keyIvecSizeMap[(int)encryption_algo].block_size;

  if(block_size)
    encrypted_buffer_length = *content_length + block_size - (*content_length % block_size);
  else
    encrypted_buffer_length = *content_length;

  if(base64_encode_option == KEY_IVEC_BODY || base64_encode_option == BODY)
    encrypted_buffer_length = 4*((encrypted_buffer_length + 2)/3);

  if (encrypted_buffer_size < encrypted_buffer_length)
  {
    MY_REALLOC(encrypted_buffer, encrypted_buffer_length + 1, "reallocating for BODY_ENCRYPTION ", -1);
    encrypted_buffer_size = encrypted_buffer_length;
  }
  NSDL2_HTTP(NULL, NULL, "After encrypted_buffer_size = %d, encrypted_buffer_length = %d", encrypted_buffer_size, encrypted_buffer_length);

  if(ns_nvm_scratch_buf_size < encrypted_buffer_length)
  {
    MY_REALLOC(ns_nvm_scratch_buf, encrypted_buffer_length + 1, "reallocating for BODY_ENCRYPTION ", -1);
    ns_nvm_scratch_buf_size = encrypted_buffer_length;
  }
  NSDL2_HTTP(NULL, NULL, "After ns_nvm_scratch_buf_size = %d, encrypted_buffer_length = %d", ns_nvm_scratch_buf_size, encrypted_buffer_length);

  unsigned char *body_buffer = (unsigned char *)ns_nvm_scratch_buf;
  //Copy body into ns_nvm_scratch_buf
  for(int i = 0 ; i < NS_GET_IOVEC_CUR_IDX(*ns_io_data_vec); i++)
  {
    memcpy(body_buffer, NS_GET_IOVEC_VAL(*ns_io_data_vec, i), NS_GET_IOVEC_LEN(*ns_io_data_vec, i));
    body_buffer += NS_GET_IOVEC_LEN(*ns_io_data_vec, i);
  }
  NS_FREE_RESET_IOVEC(*ns_io_data_vec);
  body_buffer = (unsigned char *)ns_nvm_scratch_buf;

  char err_msg[ENC_ERR_BUF_SIZE + 1] = "";
  if(ns_do_crypt(body_buffer, *content_length, encryption_algo, base64_encode_option, key, ivec, encrypted_buffer, encrypted_buffer_size, 1, err_msg) < 0)
  {
    NSTL1(NULL, NULL, "Error: %s", err_msg);  
    return NULL;
  }
 
  NSDL2_PARSING(NULL, NULL, "out_buffer = %*.*s", encrypted_buffer_length, encrypted_buffer_length, encrypted_buffer);  
  *content_length = encrypted_buffer_length;  
  return encrypted_buffer;
}   

unsigned char *ns_aes_crypt(unsigned char *buffer, int buffer_len, int encryption_algo, char base64_encode_option, char *key, int key_len, char *ivec, int ivec_len, int do_encrypt, char **err_msg)
{  
  int output_buffer_length;
  int key_ivec_size;
  char base64_encode;
  char block_size;
  static int loc_scratch_buffer_size = 0;
  static unsigned char *loc_scratch_buffer = NULL;
  static unsigned char *output_buffer = NULL;
  static __thread char out_err_msg[MAX_PARAM_SIZE + 1] = "";
  static int output_buffer_size = 0;
  char key_local_buf[KEY_IVEC_BUF_SIZE + 1];
  char ivec_local_buf[KEY_IVEC_BUF_SIZE + 1];
  
  NSDL2_PARSING(NULL, NULL, "Method called, buffer = %s, buffer_len = %d, encryption_algo = %d, base64_encode_option = %d, key = %s, "
                            "key_len = %d, ivec = %s, ivec_len = %d", 
                             buffer, buffer_len, encryption_algo, base64_encode_option, key, key_len, ivec, ivec_len);
  if(err_msg) 
    *err_msg = out_err_msg;

  if(encryption_algo <= AES_NONE  || encryption_algo > AES_256_CTR)
  {
    sprintf(out_err_msg, "Error: In ns_aes_encrypt/decrypt() API, has passed invalid encryption_algo = [%d]", encryption_algo);
    return NULL;
  } 

  if(base64_encode_option < NONE || base64_encode_option > KEY_IVEC_BODY)
  {
    sprintf(out_err_msg, "Error: In ns_aes_encrypt/decrypt() API, has passed invalid base64_encode_option = [%d]", base64_encode_option);
    return NULL;
  }

  //Handling of parameter and find the buffer_len
  if(key && (key[0] == '{' && key[strlen(key) - 1] == '}'))
  {
    strncpy(key_local_buf, ns_eval_string(key),KEY_IVEC_BUF_SIZE);
    key_len = strlen(key_local_buf);
    key = key_local_buf;
  }
  NSDL2_PARSING(NULL, NULL, "key = %s, key_len = %d", key, key_len);

  //Handling of parameter and find the buffer_len
  if(ivec && (ivec[0] == '{' && ivec[strlen(ivec) - 1] == '}'))
  {
    strncpy(ivec_local_buf, ns_eval_string(ivec),KEY_IVEC_BUF_SIZE);
    ivec_len = strlen(ivec_local_buf);
    ivec = ivec_local_buf;
  }
  NSDL2_PARSING(NULL, NULL, "ivec = %s, ivec_len = %d", ivec, ivec_len);
 
  //Handling of parameter and find the buffer_len
  if(buffer && (buffer[0] == '{' && buffer[strlen((char *)buffer) - 1] == '}'))
  {
    buffer = (unsigned char *)ns_eval_string((char *)buffer);
    buffer_len = strlen((char *)buffer);
  }
  NSDL2_PARSING(NULL, NULL, "buffer = %s, buffer_len = %d", buffer, buffer_len);
 
  //Verify all the paramters should not be NULL
  if(!key || !key[0] || !key_len || !buffer || !buffer[0] || !buffer_len)
  {
    sprintf(out_err_msg, "Error: In ns_aes_encrypt/decrypt() API, Invalid arguments are provided, buffer = %s, buffer_len = %d, "
                         "key = %s, key_len = %d, ivec = %s, ivec_len = %d", buffer, buffer_len, key, key_len, ivec, ivec_len);
    return NULL;
  }

  base64_encode = (base64_encode_option == KEY_IVEC || base64_encode_option == KEY_IVEC_BODY)?1:0;
  key_ivec_size = base64_encode ? keyIvecSizeMap[encryption_algo].encoded_size: keyIvecSizeMap[encryption_algo].raw_size;
  //if(key_len != key_ivec_size || ivec_len != key_ivec_size)
  if(key_len != key_ivec_size)
  {
    sprintf(out_err_msg, "Error: In ns_aes_encrypt/decrypt() API, key and ivec length should be [%d], as per encryption_algo [%d]\n",
                          key_ivec_size, encryption_algo);
    return NULL;
  } 

  base64_encode = (base64_encode_option == KEY_IVEC_BODY || base64_encode_option == BODY) ? 1:0;
  if(do_encrypt)
  {
    //Here block_size is of 128 bits (16 bytes) in case of AES-CBC algorithm
    block_size = keyIvecSizeMap[(int)encryption_algo].block_size;
        
    if(block_size)
      output_buffer_length = buffer_len + block_size - (buffer_len % block_size);
    else
      output_buffer_length = buffer_len;
 
    //If encrypted_buffer is encoded then it will increase size by 4*(encrypted_buffer + 2)/3  
    if(base64_encode)
      output_buffer_length = 4*((output_buffer_length + 2)/3);
  }
  else
    output_buffer_length = buffer_len;
  
  if (output_buffer_size < output_buffer_length)
  {
    MY_REALLOC(output_buffer, output_buffer_length + 1, "reallocating for BODY_ENCRYPTION ", -1);
    output_buffer_size = output_buffer_length;
  }

  //In case of base64 we need a temporary buffer "loc_scratch_buffer" for encoding the encrypted buffer.
  if(base64_encode)
  {
    if (loc_scratch_buffer_size < output_buffer_length)
    {
      MY_REALLOC(loc_scratch_buffer, output_buffer_length + 1, "reallocating for BODY_ENCRYPTION ", -1);
      loc_scratch_buffer_size = output_buffer_length;
    }
    memcpy(loc_scratch_buffer, buffer, buffer_len);
    buffer = loc_scratch_buffer;
  }

  if(ns_do_crypt(buffer, buffer_len, encryption_algo, base64_encode_option, key, ivec, output_buffer, output_buffer_size, do_encrypt, out_err_msg) < 0)
    return NULL;

  NSDL2_PARSING(NULL, NULL, "out_buffer = %s, out_err_msg = %s", output_buffer, out_err_msg); 
 
  return output_buffer;
}

void get_key_ivec_buf(int consumed_vector, NSIOVector *ns_iovec, StrEnt_Shr *key_ivec, char *key_ivec_local_ptr, int key_ivec_size)
{
  int i, key_ivec_buf_len = 0;

  NSDL2_HTTP(NULL, NULL, "consumed_vector = %d", consumed_vector);

  if (consumed_vector == 0)  
  {
    key_ivec_local_ptr = key_ivec->seg_start->seg_ptr.str_ptr->big_buf_pointer;
    key_ivec_buf_len = strlen(key_ivec->seg_start->seg_ptr.str_ptr->big_buf_pointer);
  } 
  else //(consumed_vector > 0)
  {
    // Combine all vectors in one big buf and malloc
    for (i = 0; i < consumed_vector; i++) 
    {
      if(NS_GET_IOVEC_LEN(*ns_iovec, i) != 0) 
      {
        /* abort filling it since it will go out of bounds. The resultant key will be truncated. */
        if ((key_ivec_buf_len + NS_GET_IOVEC_LEN(*ns_iovec, i)) > key_ivec_size) 
        {
          memcpy(key_ivec_local_ptr + key_ivec_buf_len, NS_GET_IOVEC_VAL(*ns_iovec, i), key_ivec_size - key_ivec_buf_len); 
          key_ivec_buf_len = key_ivec_size;
          NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR, "Data is truncated due to bigger size( > %d), key_ivec[%s]",  
                               key_ivec_size, key_ivec_local_ptr);
          break;  
        } 
        else 
        {
          memcpy(key_ivec_local_ptr + key_ivec_buf_len, NS_GET_IOVEC_VAL(*ns_iovec, i), NS_GET_IOVEC_LEN(*ns_iovec, i));
          key_ivec_buf_len += NS_GET_IOVEC_LEN(*ns_iovec, i);
        }
      }
    }
    key_ivec_local_ptr[key_ivec_buf_len] = '\0';
    NS_FREE_RESET_IOVEC((*ns_iovec));
  }
  NSDL2_HTTP(NULL, NULL, "key_ivec_local_buf = %s, key_ivec_buf_len = %d", key_ivec_local_ptr, key_ivec_buf_len);
  return;
}
 
int ns_do_decrypt(unsigned char *encrypted_buffer, int encrypted_buffer_len, char decryption_algo, char base64_encode_option, char *key,
                  char *ivec, unsigned char *decrypted_buffer, int decrypted_buffer_size, char *enc_err_msg)
{
  char key_new[ENC_KEY_IVEC_SIZE] = "";
  char iv_new[ENC_KEY_IVEC_SIZE] = "";
  char do_base64_encoding;

  NSDL2_HTTP(NULL, NULL, "encrypted_buffer = %s, encrypted_buffer_len = %d, decryption_algo = %d, base64_encode_option = %d, key = %s, "
                         "ivec = %s, decrypted_buffer = %s, decrypted_buffer_size = %d", encrypted_buffer, encrypted_buffer_len, 
                          decryption_algo, base64_encode_option, key, ivec, decrypted_buffer, decrypted_buffer_size);

  if(base64_encode_option == KEY_IVEC || base64_encode_option == KEY_IVEC_BODY)
  {
    NSDL2_HTTP(NULL, NULL, "base64_encode_option = %d", base64_encode_option);
    nslb_decode_base64_ex((unsigned char *)key, strlen(key), (unsigned char *)key_new, (int)keyIvecSizeMap[(int) decryption_algo].encoded_size);
    nslb_decode_base64_ex((unsigned char *)ivec, strlen(ivec), (unsigned char *)iv_new, (int)keyIvecSizeMap[(int) decryption_algo].encoded_size);
    key = key_new;
    ivec = iv_new;
    NSDL2_HTTP(NULL, NULL, "key = %s, ivec = %s", key, ivec);
  }

  do_base64_encoding = (base64_encode_option == BODY || base64_encode_option == KEY_IVEC_BODY) ? 1 : 0;

  if(nslb_do_crypt(encrypted_buffer, encrypted_buffer_len, decryption_algo, do_base64_encoding, key, ivec, decrypted_buffer, decrypted_buffer_size, 0, enc_err_msg) < 0)
  {
    NSTL1(NULL, NULL, "Error: In nslb_do_crypt = [%d]", decryption_algo);
    return -1;
  }
  
  NSDL2_HTTP(NULL, NULL, "enc_err_msg = %s", enc_err_msg);
  return 0;
}

char *ns_aes_decrypt_int(unsigned char *encrypted_buffer, int encrypted_buffer_len, int decryption_algo, char base64_encode_option, char *key , int key_len, char *ivec, int ivec_len, char **err_msg)
{  
  int decrypted_buffer_length = 0;
  int key_ivec_size;
  char base64_encode;
  //char block_size;
  static int loc_scratch_buffer_size = 0;
  static unsigned char *loc_scratch_buffer = NULL;
  static unsigned char *decrypted_buffer = NULL;
  static char enc_err_msg[MAX_PARAM_SIZE + 1] = "";
  //static int decrypted_buffer_size = 0;
  char key_local_buf[KEY_IVEC_BUF_SIZE + 1];
  char ivec_local_buf[KEY_IVEC_BUF_SIZE + 1];
  
  NSDL2_PARSING(NULL, NULL, "Method called, encrypted_buffer = %s, encrypted_buffer_len = %d, decryption_algo = %d, "
                            "base64_encode_option = %d, key = %s, key_len = %d, ivec = %s, ivec_len = %d", 
                             encrypted_buffer, encrypted_buffer_len, decryption_algo, base64_encode_option, key, key_len, ivec, ivec_len);
  if(err_msg) 
    *err_msg = enc_err_msg;

  if(decryption_algo <= AES_NONE  || decryption_algo > AES_256_CTR)
  {
    sprintf(enc_err_msg, "Error: In ns_aes_decrypt() API, has passed invalid decryption_algo = [%d]", decryption_algo);
    return NULL;
  } 

  if(base64_encode_option < NONE || base64_encode_option > KEY_IVEC_BODY)
  {
    sprintf(enc_err_msg, "Error: In ns_aes_decrypt() API, has passed invalid base64_encode_option = [%d]", base64_encode_option);
    return NULL;
  }

  //Handling of parameter and find the buffer_len
  if(key && (key[0] == '{' && key[strlen(key) - 1] == '}'))
  {
    strncpy(key_local_buf, ns_eval_string(key),KEY_IVEC_BUF_SIZE);
    key_len = strlen(key_local_buf);
    key = key_local_buf;
  }
  NSDL2_PARSING(NULL, NULL, "key = %s, key_len = %d", key, key_len);

  //Handling of parameter and find the buffer_len
  if(ivec && (ivec[0] == '{' && ivec[strlen(ivec) - 1] == '}'))
  {
    strncpy(ivec_local_buf, ns_eval_string(ivec),KEY_IVEC_BUF_SIZE);
    ivec_len = strlen(ivec_local_buf);
    ivec = ivec_local_buf;
  }
  NSDL2_PARSING(NULL, NULL, "ivec = %s, ivec_len = %d", ivec, ivec_len);
 
  //Handling of parameter and find the buffer_len
  if(encrypted_buffer && (encrypted_buffer[0] == '{' && encrypted_buffer[strlen((char *)encrypted_buffer) - 1] == '}'))
  {
    encrypted_buffer = (unsigned char *)ns_eval_string((char *)encrypted_buffer);
    encrypted_buffer_len = strlen((char *)encrypted_buffer);
  }
  NSDL2_PARSING(NULL, NULL, "encrypted_buffer = %s, encrypted_buffer_len = %d", encrypted_buffer, encrypted_buffer_len);
 
  //Verify all the paramters should not be NULL 
  if(!key || !key[0] || !key_len || !ivec || !ivec[0] || !ivec_len || !encrypted_buffer || !encrypted_buffer[0] || !encrypted_buffer_len)
  {
    sprintf(enc_err_msg, "Error: In ns_aes_decrypt() API, Invalid arguments are provided, encrypted_buffer = %s, encrypted_buffer_len = %d, "
                         "key = %s, key_len = %d, ivec = %s, ivec_len = %d", 
                          encrypted_buffer, encrypted_buffer_len, key, key_len, ivec, ivec_len);
    return NULL;
  }

  base64_encode = (base64_encode_option == KEY_IVEC || base64_encode_option == KEY_IVEC_BODY)?1:0;
  key_ivec_size = base64_encode ? keyIvecSizeMap[decryption_algo].encoded_size: keyIvecSizeMap[decryption_algo].raw_size;
  if(key_len != key_ivec_size || ivec_len != key_ivec_size)
  {
    sprintf(enc_err_msg, "Error: In ns_aes_decrypt() API, key and ivec length should be [%d], as per decryption_algo [%d]\n",
                          key_ivec_size, decryption_algo);
    return NULL;
  } 

  //In case of base64 we need a temporary encrypted_buffer "loc_scratch_buffer" for encoding the decrypted buffer.
  if(base64_encode)
  {
    if (loc_scratch_buffer_size < decrypted_buffer_length)
    {
      MY_REALLOC(loc_scratch_buffer, decrypted_buffer_length + 1, "reallocating for BODY_DECRYPTION ", -1);
      loc_scratch_buffer_size = decrypted_buffer_length;
    }
    memcpy(loc_scratch_buffer, encrypted_buffer, encrypted_buffer_len);
    encrypted_buffer = loc_scratch_buffer;
  }

  if(ns_do_decrypt(encrypted_buffer, encrypted_buffer_len, decryption_algo, base64_encode_option, key, ivec, decrypted_buffer, decrypted_buffer_length, enc_err_msg) < 0)
    return NULL;

  NSDL2_PARSING(NULL, NULL, "out_buffer = %*.*s", decrypted_buffer_length, decrypted_buffer_length, decrypted_buffer); 
 
  return(char *) decrypted_buffer;
}
