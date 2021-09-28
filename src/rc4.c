#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include "rc4.h"
#include "ns_log.h"

static void randomize_key(unsigned char* key,int key_data_len);
unsigned short ares__generate_new_id(rc4_key* key);

/* initialize an rc4 key. If possible a cryptographically secure random key
   is generated using a suitable function (for example win32's RtlGenRandom as
   described in
   http://blogs.msdn.com/michael_howard/archive/2005/01/14/353379.aspx
   otherwise the code defaults to cross-platform albeit less secure mechanism
   using rand
*/
static void randomize_key(unsigned char* key,int key_data_len)
{
  int randomized = 0;
  int counter=0;
#ifdef WIN32
  BOOLEAN res;
  if (ares_fpSystemFunction036)
    {
      res = (*ares_fpSystemFunction036) (key, key_data_len);
      if (res)
        randomized = 1;
    }
#else /* !WIN32 */
#ifdef RANDOM_FILE
  FILE *f = fopen(RANDOM_FILE, "rb");
  if(f) {
    counter = fread(key, 1, key_data_len, f);
    fclose(f);
  }
#endif
#endif /* WIN32 */

  if ( !randomized ) {
    for (;counter<key_data_len;counter++)
      key[counter]=(unsigned char)(rand() % 256);
  }
}

int init_id_key(rc4_key* key,int key_data_len)
{
  unsigned char index1;
  unsigned char index2;
  unsigned char* state;
  short counter;
  unsigned char *key_data_ptr = 0;

  key_data_ptr = calloc(1,key_data_len);
  if (!key_data_ptr) {
    NSDL2_DNS(NULL, NULL, "error in calloc for rc4 id key");
    return(-1);
  } 
  state = &key->state[0];
  for(counter = 0; counter < 256; counter++)
    /* unnecessary AND but it keeps some compilers happier */
    state[counter] = (unsigned char)(counter & 0xff);
  randomize_key(key->state,key_data_len);
  key->x = 0;
  key->y = 0;
  index1 = 0;
  index2 = 0;
  for(counter = 0; counter < 256; counter++)
  {
    index2 = (unsigned char)((key_data_ptr[index1] + state[counter] +
                              index2) % 256);
    ARES_SWAP_BYTE(&state[counter], &state[index2]);

    index1 = (unsigned char)((index1 + 1) % key_data_len);
  }
  free(key_data_ptr);
  return (0);
}

unsigned short ares__generate_new_id(rc4_key* key)
{
  unsigned short r=0;
  ares__rc4(key, (unsigned char *)&r, sizeof(r));
  return r;
}


void ares__rc4(rc4_key* key, unsigned char *buffer_ptr, int buffer_len)
{
  unsigned char x;
  unsigned char y;
  unsigned char* state;
  unsigned char xorIndex;
  short counter;

  x = key->x;
  y = key->y;

  state = &key->state[0];
  for(counter = 0; counter < buffer_len; counter ++)
  {
    x = (unsigned char)((x + 1) % 256);
    y = (unsigned char)((state[x] + y) % 256);
    ARES_SWAP_BYTE(&state[x], &state[y]);

    xorIndex = (unsigned char)((state[x] + state[y]) % 256);

    buffer_ptr[counter] = (unsigned char)(buffer_ptr[counter]^state[xorIndex]);
  }
  key->x = x;
  key->y = y;
}

/* a unique query id is generated using an rc4 key. Since the id may already
   be used by a running query (as infrequent as it may be), a lookup is
   performed per id generation. In practice this search should happen only
   once per newly generated id
*/
/* assuming that the ID will not repeat in the life of 1 netstorm run */
#if 0
static unsigned short generate_unique_id(ares_channel channel)
{
  unsigned short id;

  do {
    id = ares__generate_new_id(&channel->id_key);
  } while (find_query_by_id(channel, id));

  return (unsigned short)id;
}
#endif

#if 0 // use this for testing 
main()
{
	unsigned short id;
  	unsigned short r=0;
	int status,i ;

//	rc4_key *id_key = malloc(sizeof (rc4_key));
	rc4_key id_key;
	printf("id_key %x\n",&id_key);
 	status = init_id_key(&id_key, ARES_ID_KEY_LEN);
    if (!status)
	for (i=0; i< 1000000; i++) {
  		ares__rc4(&id_key, (unsigned char *)&r, sizeof(r));
		printf("id 0x%x \n",r);
	}
//	free(id_key);	
	
}
#endif
