#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>


typedef struct _RAM_AVL_Node
{
   mpz_t begin, end;
   unsigned int size;	

   mpz_t *segment;

   int balance;	

   struct _RAM_AVL_Node *left, *right, *up;
}
RAM_AVL_Node;

typedef struct
{
   unsigned int block_size;	
   RAM_AVL_Node *root, *begin,	*cache;		
   unsigned int allocated,	segment_count;	
}
RAM_Memory;

RAM_Memory *ram_memory_new ();
void ram_memory_delete (RAM_Memory *);

void ram_memory_reset (RAM_Memory *);

inline mpz_t *ram_try_to_get_register (RAM_Memory *memory, mpz_t *addr);

inline mpz_t *ram_get_register_0 (RAM_Memory *memory);

mpz_t *ram_get_register (RAM_Memory *memory, mpz_t *addr);
inline mpz_t *ram_get_register_by_pointer (RAM_Memory *memory, mpz_t *addr);
inline mpz_t *ram_get_register_by_indirect_pointer (RAM_Memory *memory,
						mpz_t *addr);
void ram_set_block_size (unsigned int);	
				
unsigned int ram_memory_tree_height (RAM_Memory *);


