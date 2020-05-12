#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <gmp.h>
#include "ram.h"



RAM_Instruction *ram_instruction_new ()
{
   RAM_Instruction *ri;

   ri = (RAM_Instruction *) malloc (sizeof (RAM_Instruction));
   
   (ri -> parameter_type) = RAM_NO_PARAMETER;

   return ri;
}

static void ram_instruction_clear (RAM_Instruction *ri)
{
   if ((ri -> parameter_type) != RAM_NO_PARAMETER)
      mpz_clear (ri -> parameter);
}

void ram_instruction_delete (RAM_Instruction *ri)
{
   ram_instruction_clear (ri);
   free (ri);
}

RAM_Program *ram_program_new ()
{
   RAM_Program *rp;

   rp = (RAM_Program *) calloc (1, sizeof (RAM_Program));

   return rp;
}

void ram_program_clear (RAM_Program *rp)
{
   if (rp -> instructions)
   {
      int i;
      
      for (i = 0; i < (rp -> n); i ++)
	 if ((rp -> instructions) [i])
	    ram_instruction_clear ((rp -> instructions) [i]);
   }
}

void ram_program_delete (RAM_Program *rp)
{
   ram_program_clear (rp);
   free (rp);
}

RAM *ram_new ()
{
   RAM *rm;

   rm = (RAM *) calloc (1, sizeof (RAM));

   (rm -> program) = NULL;

   (rm -> input) = stdin;
   (rm -> output) = stdout;

   (rm -> memory) = ram_memory_new ();

   mpz_init (rm -> instructions_done);
   mpz_init (rm -> time_consumed);

   return rm;
}

void ram_clear (RAM *rm)
{
   if (!rm)
      return;
	
   if (rm -> program)
   {
      ram_program_delete (rm -> program);
      (rm -> program) = NULL;
   }

   if (rm -> memory)
      ram_memory_delete (rm -> memory);

   (rm -> memory) = NULL;

   mpz_set_ui ((rm -> instructions_done), 0);
   mpz_set_ui ((rm -> time_consumed), 0);
}

void ram_delete (RAM *rm)
{
   ram_clear (rm);

   mpz_clear (rm -> instructions_done);
   mpz_clear (rm -> time_consumed);
   
   free (rm);
}

void ram_reset (RAM *rm)
{
   ram_memory_reset (rm -> memory);
   
   mpz_set_ui ((rm -> instructions_done), 0);
   mpz_set_ui ((rm -> time_consumed), 0);

   rewind (rm -> input);
}
