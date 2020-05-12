#include <stdarg.h>
#include "ram.h"

RAM *ram_new_by_program (RAM_Program *program)
{
   RAM *machine;

   machine = ram_new ();
   (machine -> program) = program;
   (machine -> current_instruction) = 0;

   return machine;
}

typedef int (InstructionHandler)(RAM *, RAM_Instruction *);

static const InstructionHandler ram_read, ram_write, ram_load, ram_store,
		ram_add, ram_neg, ram_half, ram_jump, ram_jgtz, ram_halt;

static const InstructionHandler *instruction [] =
{
   NULL,
   ram_read, ram_write,
   ram_load, ram_store,
   ram_add, ram_neg, ram_half,
   ram_jump, ram_jgtz,
   ram_halt
};

int ram_do_instruction (RAM *machine)
{
   RAM_Instruction *i;

   if (!ram_is_running (machine))
      return 0;

   i = (machine -> program -> instructions) [machine -> current_instruction];

   if ((instruction [i -> instruction]) (machine, i))
   {
      mpz_add_ui ((machine -> instructions_done),
		      (machine -> instructions_done), 1);
      return ram_is_running (machine);
   }

   return 0;
}

inline int ram_is_running (RAM *machine)
{
   if ((machine -> current_instruction) >= (machine -> program -> n))
      return 0;

   return ((machine -> program -> instructions)
	 [machine -> current_instruction] -> instruction) != RAM_HALT;
}


static int ram_halt(RAM* machine, RAM_Instruction* i)
{
    return 0;
}


static int ram_read (RAM *machine, RAM_Instruction *i)
{
   char *line;
   
   io_prompt (machine, 1);
   
   line = read_line (machine -> input);
   
   free (line);

   (machine -> current_instruction) ++;

   add_register_0_cost (machine);

   return 1;
}

static int ram_write (RAM *machine, RAM_Instruction *i)
{
   io_prompt (machine, 0);
   
   
   fprintf ((machine -> output), "\n");

   (machine -> current_instruction) ++;

   return 1;
}

static int ram_load (RAM *machine, RAM_Instruction *i)
{
   mpz_t *n;

   n = get_parameter_ptr (machine, i);
   
      
   mpz_set (*(ram_get_register_0 (machine -> memory)), *n);

   (machine -> current_instruction) ++;

   return 1;
}

static int ram_store (RAM *machine, RAM_Instruction *i)
{
   mpz_t *n;

   n = get_parameter_ptr (machine, i);
  
   
   mpz_set (*n, *(ram_get_register_0 (machine -> memory)));

   (machine -> current_instruction) ++;


   return 1;
}

static int ram_add (RAM *machine, RAM_Instruction *i)
{
   mpz_t *n, *reg_0 = ram_get_register_0 (machine -> memory);

   n = get_parameter_ptr (machine, i);
   
      
   mpz_add (*reg_0, *reg_0, *n);

   (machine -> current_instruction) ++;

   return 1;
}

static int ram_neg (RAM *machine, RAM_Instruction *i)
{
   mpz_t *reg_0 = ram_get_register_0 (machine -> memory);
   
   mpz_neg (*reg_0, *reg_0);

   (machine -> current_instruction) ++;

   return 1;
}

static int ram_half (RAM *machine, RAM_Instruction *i)
{
   mpz_t *reg_0 = ram_get_register_0 (machine -> memory);
   
   mpz_tdiv_q_2exp (*reg_0, *reg_0, 1);

   (machine -> current_instruction) ++;

   return 1;
}

static int ram_jump (RAM *machine, RAM_Instruction *i)
{
   (machine -> current_instruction) = mpz_get_ui (i -> parameter) - 1;


   return 1;
}

static int ram_jgtz (RAM *machine, RAM_Instruction *i)
{
   if (mpz_sgn (*ram_get_register_0 (machine -> memory)) > 0)
   {
      (machine -> current_instruction) =
	      mpz_get_ui (i -> parameter) - 1;

   }
   else
   {
      (machine -> current_instruction) ++;
   }

   return 1;
}
