#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>
#include "ram_memory.h"

typedef enum
{
   RAM_NONE = 0,
   RAM_READ,  //����� ����� �� �������� � ������
   RAM_WRITE,  //�������� ����� � �������
   RAM_LOAD,    //�������� ������� � �������
   RAM_STORE,   //�������� ����� � �������� � ������
   RAM_ADD,    //���� ������� �� ��������
   RAM_NEG,    //����� ���� ����� � ������
   RAM_HALF,   //�������� � ������ �����/2
   RAM_JUMP,  //������� �� ������� �� ������� �����
   RAM_JGTZ,  //������� �� ������� �� ������� �����, ���� ���� �������� >0
   RAM_HALT  //ʳ���� ��������� ��������
}
RAM_InstructionType;

typedef enum
{
   RAM_NO_PARAMETER, RAM_CONSTANT, RAM_POINTER, RAM_INDIRECT_POINTER,
   RAM_INSTRUCTION
}
RAM_ParameterType;

typedef struct
{
   RAM_InstructionType instruction;
   RAM_ParameterType parameter_type;
   mpz_t parameter;
}
RAM_Instruction;

typedef struct
{
   RAM_Instruction **instructions;
   unsigned int n;
}
RAM_Program;

typedef struct
{
   RAM_Program *program;

   FILE *input, *output;

   RAM_Memory *memory;

   unsigned int current_instruction;
   
   mpz_t instructions_done, time_consumed;
}
RAM;


RAM_Instruction *ram_instruction_new ();
void ram_instruction_delete (RAM_Instruction *);

RAM_Program *ram_program_new ();
void ram_program_delete (RAM_Program *);

RAM *ram_new ();

void ram_clear (RAM *);
void ram_delete (RAM *);

void ram_reset (RAM *);

RAM_Program *ram_program_parse (FILE *f, RAM_Text *text);
RAM *ram_new_by_program (RAM_Program *);



int ram_do_instruction (RAM *);
inline int ram_is_running (RAM *);
