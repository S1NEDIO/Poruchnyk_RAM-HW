#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>



static unsigned int block_size = 4;

void ram_set_block_size (unsigned int size)
{
   block_size = size;
}

static void inline align_size (RAM_Memory *memory, unsigned int *size)
{
   if (*size % (memory -> block_size))
      (*size) = (*size) + (memory -> block_size) -
	      (*size) % (memory -> block_size);
}

static void inline align_block_start (RAM_Memory *memory, mpz_t *start)
{
   mpz_t r;

   mpz_init (r);

   mpz_mod_ui (r, *start, (memory -> block_size));
   if (mpz_sgn (r))
   {
      mpz_ui_sub (r, (memory -> block_size), r);
      mpz_sub (*start, *start, r);
   }
   
   mpz_clear (r);
}

static RAM_AVL_Node *avl_node_new ()
{
   RAM_AVL_Node *node = (RAM_AVL_Node *) calloc (1, sizeof (RAM_AVL_Node));
   if (!node)
      err_fatal_perror ("calloc",
		      "could not allocate RAM_AVL_Node structure");

   mpz_init (node -> begin);
   mpz_init (node -> end);
   
   return node;
}


static RAM_AVL_Node *avl_node_new_for_segment (RAM_Memory *memory,
					mpz_t base, unsigned int size)
{
   RAM_AVL_Node *node = avl_node_new ();
   
   align_size (memory, &size);
   mpz_set ((node -> begin), base);
   mpz_add_ui ((node -> end), (node -> begin), size - 1);
   (node -> size) = size;

   (node -> segment) = (mpz_t *) malloc (size * sizeof (mpz_t));
   if (!(node -> segment))
      err_fatal_perror ("malloc",
		"could not allocate memory for %d registers segment", size);

   {
      unsigned int i;
      for (i = 0; i < size; i ++)
	 mpz_init ((node -> segment) [i]);
   }

   return node;
}

static inline void segment_delete (RAM_AVL_Node *node)
{
   unsigned int i;
   for (i = 0;  i < (node -> size);  i ++)
      mpz_clear ((node -> segment) [i]);

   free (node -> segment);
}

static void avl_node_delete (RAM_AVL_Node *node)
{
   if (node -> size)
      segment_delete (node);

   mpz_clear (node -> begin);
   mpz_clear (node -> end);
   
   free (node);
}

static void avl_tree_delete (RAM_AVL_Node *tree)
{
   while (tree)
   {
      if (tree -> left)
	 tree = (tree -> left);
      else if (tree -> right)
	 tree = (tree -> right);
      else
      {
	 RAM_AVL_Node *node = (tree -> up);

	 avl_node_delete (tree);
	
	 if (node)
	 {
	    if (node -> left)
	       (node -> left) = NULL;
	    else
	       (node -> right) = NULL;
	 }

	 tree = node;
      }
   }
}

void ram_memory_delete (RAM_Memory *rm)
{
   avl_tree_delete (rm -> root);
   free (rm);
}

RAM_Memory *ram_memory_new ()
{
   RAM_Memory *rm = (RAM_Memory *) calloc (1, sizeof (RAM_Memory));
   if (!rm)
      err_fatal_perror ("calloc", 
		      "could not allocate memory for RAM_Memory structure");

   (rm -> block_size) = block_size;

   {
      mpz_t base;

      mpz_init_set_ui (base, 0);
      (rm -> root) = avl_node_new_for_segment (rm, base, 1);
      (rm -> begin) = (rm -> root);
      (rm -> allocated) = (rm -> root -> size);
      (rm -> segment_count) = 1;

      mpz_clear (base);
   }

   return rm;
}


static void inline do_resize_segment (RAM_AVL_Node *node, unsigned int size)
{
   (node -> segment) = (mpz_t *) realloc ((node -> segment),
					  sizeof (mpz_t) * size);
   if (! (node -> segment))
      err_fatal_perror ("realloc",
		"could not resize memory block (old size %d, new size %d)",
		(node -> size), size);

   (node -> size) = size;
   mpz_add_ui ((node -> end), (node -> begin), size - 1);
}

static void shrink_segment (RAM_AVL_Node *node, unsigned int size)
{
   unsigned int i;

   for (i = size; i < (node -> size); i ++)
      mpz_clear ((node -> segment) [i]);
   
   do_resize_segment (node, size);
}

static inline void expand_segment (RAM_AVL_Node *node, unsigned int size)
{
   unsigned int old_size = (node -> size);

   do_resize_segment (node, size);
   {
      unsigned int i;

      for (i = old_size; i < size; i ++)
	 mpz_init ((node -> segment) [i]);
   }
}

static inline void expand_segment_backwards (RAM_AVL_Node *node,
						unsigned int size)
{
   unsigned int diff = size - (node -> size);

   mpz_sub_ui ((node -> begin), (node -> begin), diff);
   do_resize_segment (node, size);
   memmove ((node -> segment) + diff, (node -> segment),
		   (size - diff) * sizeof (mpz_t));
   {
      unsigned int i;

      for (i = 0; i < diff; i ++)
	 mpz_init ((node -> segment) [i]);
   }
}

static inline mpz_t *find_register (RAM_AVL_Node *node, mpz_t *addr)
{
   mpz_t offset, *r;

   mpz_init_set (offset, *addr);
   mpz_sub (offset, offset, (node -> begin));

   r = (node -> segment) + mpz_get_ui (offset);

   mpz_clear (offset);

   return r;
}

static inline int segment_contains (RAM_AVL_Node *node, mpz_t *addr)
{
   return (mpz_cmp (*addr, (node -> begin)) < 0 ||
		   mpz_cmp (*addr, (node -> end)) > 0) ? 0 : 1;
}


static void remove_all_but_begin (RAM_Memory *rm)
{
   RAM_AVL_Node *tree = (rm -> root);

   while (tree)
   {
      if (tree -> left)
	 tree = (tree -> left);
      else if (tree -> right)
	 tree = (tree -> right);
      else
      {
	 RAM_AVL_Node *node = (tree -> up);

	 if (tree != (rm -> begin))
	    avl_node_delete (tree);
	
	 if (node)
	 {
	    if (node -> left)
	       (node -> left) = NULL;
	    else
	       (node -> right) = NULL;
	 }

	 tree = node;
      }
   }

   (rm -> root) = (rm -> begin);
   (rm -> begin -> up) = NULL;

   (rm -> cache) = (rm -> begin);
}

static RAM_AVL_Node *find_segment (RAM_Memory *rm, mpz_t *addr)
{
   RAM_AVL_Node *node;
   
   if (rm -> cache)
      if (segment_contains ((rm -> cache), addr))
	 return (rm -> cache);
   
   node = (rm -> root);
   while (node)
      if (mpz_cmp ((node -> begin), *addr) > 0)
         node = (node -> left);
      else if (mpz_cmp ((node -> end), *addr) < 0)
	 node = (node -> right);
      else
      {
	 (rm -> cache) = node;
	 return node;
      }

   return NULL;	
}


static RAM_AVL_Node *try_to_find_segment (RAM_Memory *rm, mpz_t *addr,
					int *position)
{
   RAM_AVL_Node *node;
   
   if (rm -> cache)
      if (segment_contains ((rm -> cache), addr))
      {
	 *position = 0;
	 return (rm -> cache);
      }

   node = (rm -> root);
   while (node)
      if (mpz_cmp ((node -> begin), *addr) > 0)
      {
	 if (! (node -> left))
	 {
	    (*position) = -1;
	    return node;
	 }
	 
         node = (node -> left);
      }
      else if (mpz_cmp ((node -> end), *addr) < 0)
      {
	 if (! (node -> right))
	 {
	    (*position) = 1;
	    return node;
	 }
	 node = (node -> right);
      }
      else
      {
	 (*position) = 0;
	 (rm -> cache) = node;
	 return node;
      }

   return NULL;	
}

static inline int segment_is_left_to (RAM_AVL_Node *node, mpz_t *addr,
						unsigned int size)
{
   mpz_t one_block_after;
   int result;

   mpz_init (one_block_after);
   mpz_add_ui (one_block_after, (node -> end), size);
   result = (mpz_cmp (*addr, one_block_after) <= 0);
   mpz_clear (one_block_after);

   return result;
}


static inline int segment_is_right_to (RAM_AVL_Node *node, mpz_t *addr,
						unsigned int size)
{
   mpz_t one_block_before;
   int result;

   mpz_init (one_block_before);
   mpz_sub_ui (one_block_before, (node -> begin), size);
   result = (mpz_cmp (*addr, one_block_before) >= 0);
   mpz_clear (one_block_before);

   return result;
}

static inline RAM_AVL_Node *try_to_find_prev_segment
		(RAM_AVL_Node *node, mpz_t *addr, unsigned int size)
{
   if (node -> left)
      return segment_is_left_to ((node -> left), addr, size) ?
	 (node -> left) : NULL;

   if (mpz_cmp ((node -> end), *addr) < 0)
      if (segment_is_left_to (node, addr, size))
         return node;

   while (node -> up)
   {
      if ((node -> up -> right) == node)
	 return segment_is_left_to ((node -> up), addr, size) ?
		 (node -> up) : NULL;

      node = (node -> up);
   }
   
   return NULL;
}


static inline RAM_AVL_Node *try_to_find_next_segment
		(RAM_AVL_Node *node, mpz_t *addr, unsigned int size)
{
   if (node -> right)
      return segment_is_right_to ((node -> left), addr, size) ?
	 (node -> right) : NULL;

   if (mpz_cmp ((node -> begin), *addr) > 0)
      if (segment_is_right_to (node, addr, size))
         return node;

   while (node -> up)
   {
      if ((node -> up -> left) == node)
	 return segment_is_right_to ((node -> up), addr, size) ?
		 (node -> up) : NULL;

      node = (node -> up);
   }
   
   return NULL;
}

static inline RAM_AVL_Node *avl_balance (RAM_AVL_Node **root,
						RAM_AVL_Node *node)
{
   RAM_AVL_Node *next = ((node -> balance) > 0) ? 
	   		(node -> right) : (node -> left);
      
   if ((node -> balance) * (next -> balance) > 0)
   {
      (next -> up) = (node -> up);
      (node -> up) = next;
  
      if ((node -> balance) > 0)
      {
	 if (next -> left)
            (next -> left -> up) = node;
         (node -> right) = (next -> left);

	 (next -> left) = node;
      }
      else
      {
	 if (next -> right)
            (next -> right -> up) = node;
	 (node -> left) = (next -> right);

	 (next -> right) = node;
      }

      if (next -> up)
      {
         RAM_AVL_Node **this = ((next -> up -> left) == node) ?
		       		&(next -> up -> left) : &(next -> up -> right);
	 *this = next;
      }

      (node -> balance) = 0;
      (next -> balance) = 0;

      if (node == *root)
         *root = next;

      return next;
   }
   else
   {
      RAM_AVL_Node *third = ((next -> balance) > 0) ?
	      (next -> right) : (next -> left);

      (third -> up) = (node -> up);
      if (node -> up)
      {
         RAM_AVL_Node **this = ((node -> up -> left) == node) ?
		       		&(node -> up -> left) : &(node -> up -> right);
	 *this = third;
      }


      if ((node -> balance) > 0)
      {
	 if (third -> left)
	    (third -> left -> up) = node;
	 (node -> right) = (third -> left);

	 if (third -> right)
	    (third -> right -> up) = next;
	 (next -> left) = (third -> right);

	 (third -> left) = node;
	 (third -> right) = next;

	 (node -> balance) = ((third -> balance) <= 0) ? 0 : -1;
	 (next -> balance) = ((third -> balance) >= 0) ? 0 : -1;
      }
      else
      {
	 if (third -> left)
            (third -> left -> up) = next;
	 (next -> right) = (third -> left);

	 if (third -> right)
	    (third -> right -> up) = node;
	 (node -> left) = (third -> right);

	 (third -> left) = next;
	 (third -> right) = node;

     	 (next -> balance) = ((third -> balance) <= 0) ? 0 : -1;
	 (node -> balance) = ((third -> balance) >= 0) ? 0 : -1;
      }

      (node -> up) = third;
      (next -> up) = third;

      (third -> balance) = 0;

      if (node == *root)
	 *root = third;

      return third;
   }
}


static void avl_insert (RAM_AVL_Node **root,
		RAM_AVL_Node *node, RAM_AVL_Node *new, int position)
{
   RAM_AVL_Node **to;
	
   to = (position < 0) ? (&(node -> left)) : (&(node -> right));

   (new -> up) = node;
   *to = new;

   (node -> balance) = (node -> balance) ? 0 : position;

   if (node -> balance)
   {
      int side; 
	 
      while (node -> up)
      {
         side = ((node -> up -> left) == node) ? -1 : 1;
         node = (node -> up);

         if ((node -> balance) * side > 0)
         {
            avl_balance (root, node);
            return;
         }
         else if ((node -> balance) == 0)
            (node -> balance) = side;
         else
         {
            (node -> balance) = 0;
            return;
         }
      }
   }
}

static void avl_delete (RAM_AVL_Node **root, RAM_AVL_Node *node)
{
   RAM_AVL_Node *prev, *dp;
   int side = 0;	

   if ((node -> left) && (node -> right))
   {
      if ((node -> balance) >= 0)
      {
         prev = (node -> right);
	 if (prev -> left)
	 {
	    while (prev -> left)
	       prev = (prev -> left);

	    (prev -> left) = (node -> left);
	    (node -> left -> up) = prev;

	    (prev -> right) = (node -> right);
	    (node -> right -> up) = prev;

	    (prev -> up -> left) = (prev -> right);
	    if (prev -> right)
	       (prev -> right -> up) = (prev -> up);

	    dp = (prev -> up);
	    side = -1;
	 }
	 else
	 {
	    (prev -> left) = (node -> left);
	    if (node -> left)
	       (node -> left -> up) = prev;
	   
	    dp = prev;
	    side = 1;
	 }
      }
      else
      {
	 prev = (node -> left);
	 if (prev -> right)
	 {
	    while (prev -> right)
	       prev = (prev -> right);

	    (prev -> right) = (node -> right);
	    (node -> right -> up) = prev;

	    (prev -> left) = (node -> left);
	    (node -> left -> up) = prev;

	    (prev -> up -> right) = (prev -> left);
	    if (prev -> left)
	       (prev -> left -> up) = (prev -> up);

	    dp = (prev -> up);
	    side = 1;
	 }
	 else
	 {
	    (prev -> right) = (node -> right);
	    if (node -> right)
	       (node -> right -> up) = prev;
	   
	    dp = prev;
	    side = -1;
	 }
      }
   }
   else
   {
      dp = (node -> up);
      if (dp)
         side = ((dp -> left) == node) ? -1 : 1;

      if (node -> left)
         prev = (node -> left);
      else if (node -> right)
         prev = (node -> right);
      else
         prev = NULL;
   }
   
   if (prev)
      (prev -> up) = (node -> up);

   if (node -> up)
   {
      RAM_AVL_Node **up = ((node -> up -> left) == node) ?
	      		&(node -> up -> left) : &(node -> up -> right);
      *up = prev;
   }
   else
      *root = prev;

   avl_node_delete (node);

   while (dp)
   {
      int factor = (dp -> balance) * side;
      if (factor > 0)
      {
	 (dp -> balance) = 0;
	 break;
      }
      else if (factor < 0)
	 dp = avl_balance (root, dp);
      else
      {
	 (dp -> balance) = -side;
	 break;
      }

      if (dp -> up)
	 side = ((dp -> up -> left) == dp) ? -1 : 1;

      dp = (dp -> up);
   }
}


static void merge_segments (RAM_Memory *memory,
		RAM_AVL_Node *left, RAM_AVL_Node *right)
{
   unsigned int left_size = (left -> size);

   do_resize_segment (left, left_size + (right -> size) +
		   		(memory -> block_size));
   memcpy ((left -> segment) + left_size + (memory -> block_size),
		   (right -> segment), (right -> size) * sizeof (mpz_t));
   free (right -> segment);
   (right -> size) = 0;
   (right -> segment) = 0;
   avl_delete (&(memory -> root), right);

   {
      unsigned int i;
      
      for (i = 0; i < (memory -> block_size); i ++)
	 mpz_init ((left -> segment) [i + left_size]);
   }

   (memory -> segment_count) --;
   (memory -> allocated) += (memory -> block_size);
}


void ram_memory_reset (RAM_Memory *rm)
{
   if (rm -> allocated)
   {
      shrink_segment ((rm -> begin), (rm -> block_size));
      remove_all_but_begin (rm);
      (rm -> allocated) = (rm -> block_size);
      (rm -> segment_count) = 1;
      {
	 unsigned int i;
	 for (i = 0; i < (rm -> block_size); i ++)
	    mpz_set_ui ((rm -> begin -> segment) [i], 0);
      }
   }
}

inline mpz_t *ram_try_to_get_register (RAM_Memory *memory, mpz_t *addr)
{
   RAM_AVL_Node *node = find_segment (memory, addr);
   
   if (node)
      return find_register (node, addr);

   return NULL;
}

inline mpz_t *ram_get_register_0 (RAM_Memory *memory)
{
   return (memory -> begin -> segment);
}


mpz_t *ram_get_register (RAM_Memory *memory, mpz_t *addr)
{
   int position;
   RAM_AVL_Node *node = try_to_find_segment (memory, addr, &position);

   if (position)
   {
      RAM_AVL_Node	*prev = try_to_find_prev_segment (node, addr,
			 				(memory -> block_size)),
      			*next = try_to_find_next_segment (node, addr,
							(memory -> block_size));
      mpz_t address, *ret;

      mpz_init_set (address, *addr);

      if (prev && next)
      {
         merge_segments (memory, prev, next);

	 (memory -> cache) = prev;

         ret = find_register (prev, &address);
      }
      else if (prev)
      {
	 expand_segment (prev, (prev -> size) + (memory -> block_size));
	 (memory -> allocated) += (memory -> block_size);

	 (memory -> cache) = prev;

         ret = find_register (prev, &address);
      }
      else if (next)
      {
	 expand_segment_backwards
		 (next, (next -> size) + (memory -> block_size));
	 (memory -> allocated) += (memory -> block_size);

	 (memory -> cache) = next;

	 ret = find_register (next, &address);
      }
      else
      {
         RAM_AVL_Node *new;
         mpz_t base;

         mpz_init_set (base, *addr);
         align_block_start (memory, &base);
      
         new = avl_node_new_for_segment (memory, base, block_size);
         avl_insert (&(memory -> root), node, new, position);
         (memory -> allocated) += (memory -> block_size);
         (memory -> segment_count) ++;

         mpz_clear (base);

         (memory -> cache) = new;
	 
         ret = find_register (new, &address);
      }
    
      mpz_clear (address);

      return ret;
   }
   
   return find_register (node, addr);
}

inline mpz_t *ram_get_register_by_pointer (RAM_Memory *memory, mpz_t *addr)
{
   mpz_t *p = ram_get_register (memory, addr);
   
   if (mpz_sgn (*p) < 0)
      return NULL;
   
   return ram_get_register (memory, p);
}

inline mpz_t *ram_get_register_by_indirect_pointer
					(RAM_Memory *memory, mpz_t *addr)
{
   mpz_t *p = ram_get_register_by_pointer (memory, addr);

   if (!p)
      return NULL;
   
   if (mpz_sgn (*p) < 0)
      return NULL;
   
   return ram_get_register (memory, p);
}

static inline unsigned int avl_tree_height (RAM_AVL_Node *node)
{
   unsigned int height = 0;

   while (node)
   {
      if ((node -> balance) >= 0)
	 node = (node -> right);
      else
	 node = (node -> left);

      height ++;
   }

   return height;
}

unsigned int ram_memory_tree_height (RAM_Memory *memory)
{
   return avl_tree_height (memory -> root);
}
