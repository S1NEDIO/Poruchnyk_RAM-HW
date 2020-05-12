#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gmp.h>
#include "ram.h"

static InstructionName instruction_names[] =
{
   {"read", RAM_READ},   //Зчитує число та заносить в регістр
   {"write", RAM_WRITE},  //Виводить число з регістра
   {"load", RAM_LOAD},   //Заносить операнд в суматор
   {"store", RAM_STORE},  //Заносить число зі суматора в регістр
   {"add", RAM_ADD},    //Додає операнд до суматора
   {"neg", RAM_NEG},    //Змінює знак числа в регістрі
   {"half", RAM_HALF},   //Заносить в регістр число/2
   {"jump", RAM_JUMP},  //Перехід на команду із заданою міткою
   {"jgtz", RAM_JGTZ},   //Перехід на команду із заданою міткою, якщо вміст суматора >0
   {"halt", RAM_HALT},   //Кінець виконання програми
   {NULL, RAM_NONE}
};


typedef struct _TextLine
{
   char *line;

   struct _TextLine *next;
}
TextLine;

typedef struct _Label
{
   char *name;
   unsigned int operator;

   struct _Label *next;
}
Label;

typedef struct _Instruction
{
   RAM_Instruction *instruction;
   const char *reference;	
   char *text_reference;	
   unsigned int line;		
   struct _Instruction *next;
}
Instruction;

typedef struct
{
   RAM_Program *program;
   Label *labels;		
   Instruction *instructions;
   TextLine *text_start, *text_end;
   unsigned int line_no, instruction;	
}
ParserData;

typedef struct
{
   const char *name;
   RAM_InstructionType instruction;
}
InstructionName;

static const char *INPUT_DELIMITERS = " \t\n";

static const int CONSTANT_PARAMETER = 0x01;
static const int INSTRUCTION_PARAMETER = 0x02;
static const int POINTER_PARAMETER = 0x04;
static const int INDIRECT_POINTER_PARAMETER = 0x08;
}
static RAM_InstructionType get_instruction (const char *s)
{
   InstructionName *in;

   in = instruction_names;
   while (in -> name)
   {
      if (! strcasecmp ((in -> name), s))
	 return (in -> instruction);

      in ++;
   }

   return RAM_NONE;
}

static int parse_argument (char *argument, Instruction *i, int allowed)
{
   int brackets = 0;
   char old;
   char *end;
  
   if (! argument)
      return 0;
   
   while (*argument == '[')
   {
      brackets ++;
      do
	 argument ++;
      while (isspace (*argument));
   }

   end = argument;
   while (*end == '-' || *end == '_' || isalnum (*end))
      end ++;
   old = *end;
   (*end) = '\0';

   if (brackets &&
	(allowed & (INDIRECT_POINTER_PARAMETER | POINTER_PARAMETER)))
   {
      mpz_init (i -> instruction -> parameter);
      if (gmp_sscanf
	(argument, "%Zd", &(i -> instruction -> parameter)) != 1)
      {
         mpz_clear (i -> instruction -> parameter);
	 goto error;
      }
   }
  
   if (brackets == 0 && (allowed & INSTRUCTION_PARAMETER))
   {
      mpz_init (i -> instruction -> parameter);
      if (gmp_sscanf
	(argument, "%Zd", &(i -> instruction -> parameter)) !=
		      1)
      {
         mpz_clear (i -> instruction -> parameter);
         (i -> reference) = strdup (argument);
      }
      else if (mpz_sgn (i -> instruction -> parameter) <= 0)
      {
         mpz_clear (i -> instruction -> parameter);
         goto error;
      }
      
      (i -> instruction -> parameter_type) = RAM_INSTRUCTION;
   }
   else if (brackets == 2 && (allowed & INDIRECT_POINTER_PARAMETER))
      (i -> instruction -> parameter_type) = RAM_INDIRECT_POINTER;
   else if (brackets == 1 && (allowed & POINTER_PARAMETER))
      (i -> instruction -> parameter_type) = RAM_POINTER;
   else if (brackets == 0 && (allowed & CONSTANT_PARAMETER))
   {
      (i -> instruction -> parameter_type) = RAM_CONSTANT;
      if (mpz_init_set_str ((i -> instruction -> parameter),
		      argument, 10))
	 goto error;
   }
   else
      return 0;

   (*end) = old;
   while (brackets)
   {
      if (!(*end))
	 return 0;

      if ((*end) == ']')
	 brackets --;

      end ++;
   }
   
   return 1;
   
error:
   (*end) = old;
   return 0;
}

static int parse_instruction (ParserData *pd, char *token,
				char **line, char *delimiter,
				char *line_ref)
{
   RAM_InstructionType type;

   (pd -> instruction) ++;
   
   type = get_instruction (token);
   if (type == RAM_NONE)
   {
      Label *l;

      l = label_new ();
      (l -> operator) = (pd -> instruction);
      (l -> name) = strdup (token);

      (l -> next) = (pd -> labels);
      (pd -> labels) = l;

      type = get_instruction (token);
      if (type == RAM_NONE)
      {
	 parse_error ((pd -> line_no), "no instruction found <2>");
	 return 0;
      }
   }
   {
      Instruction *i;
      int parse_result = 1;

      i = instruction_new ();
      (i -> line) = (pd -> line_no);
      (i -> next) = (pd -> instructions);
      (pd -> instructions) = i;

      (i -> instruction) = ram_instruction_new ();
      (i -> instruction -> instruction) = type;
      
      token = get_token (line, INPUT_DELIMITERS, delimiter);
      while (get_token (line, "\n", delimiter));

      switch (type)
      {
      case RAM_LOAD:
      case RAM_ADD:
         parse_result = parse_argument (token, i, CONSTANT_PARAMETER |
			    POINTER_PARAMETER |
			    INDIRECT_POINTER_PARAMETER);
	 break;
      case RAM_STORE:
	 parse_result = parse_argument (token, i, POINTER_PARAMETER |
			    INDIRECT_POINTER_PARAMETER);
	 break;
      case RAM_JUMP:
      case RAM_JGTZ:
	 parse_result = parse_argument (token, i, INSTRUCTION_PARAMETER);
	 break;
      default:
	 break;
      }

      if (! parse_result)
      {
         parse_error ((pd -> line_no), "error in argument");
	 return 0;
      }

      (i -> text_reference) = line_ref;
   }

   return 1;
}

