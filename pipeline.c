
/*
 * pipeline.c
 * 
 * Donald Yeung
 */


#include <stdlib.h>
#include "fu.h"
#include "pipeline.h"


int
commit(state_t *state) {
}


void
writeback(state_t *state) {
}


void
execute(state_t *state) {
}


int
memory_disambiguation(state_t *state) {
}


int
issue(state_t *state) {
}


int
dispatch(state_t *state) {
//Input state variables - if_id
//Output state variables - IQ, CQ, ROB

int instr;
int  ROB_tail, ROB_head, ROB_full, IQ_tail, IQ_head, IQ_full, CQ_tail, CQ_head, CQ_full;
int ROB_index, IQ_index, CQ_index;
unsigned long pc;
instr = state->if_id.instr;
pc = state->if_id.pc;

const op_info_t *op_info;
int use_imm;
op_info = decode_instr(instr, &use_imm);

if(FIELD_OPCODE(instr) == 0x0) return 0; //Drop the NOP instruction

//Check if all of ROB, IQ and CQ have atleast one empty-slot
//This checking should also be instruction dependent.
//For example, an HALT instruction need not need space in IQ and CQ.
//Similarly, a non-memory instruction needs space only in ROB and IQ.

//Updating the Re-Order Buffer(ROB)
ROB_tail = state->ROB_tail;
ROB_head = state->ROB_head;
state->ROB[ROB_tail].instr = instr;
if(FIELD_OPCODE(instr) == 0x3f) //HALT instruction
{
   state->ROB[ROB_tail].completed = TRUE;
   return 0;
}
else
   state->ROB[ROB_tail].completed = FALSE;
ROB_index = ROB_tail;
ROB_tail = (ROB_tail + 1)%ROB_SIZE;
state->ROB_tail = ROB_tail;

//Updating the Instruction Queue(IQ)
IQ_tail = state->IQ_tail;
IQ_head = state->IQ_head;
state->IQ[IQ_tail].instr = instr;
state->IQ[IQ_tail].pc = pc;
state->IQ[IQ_tail].issued = FALSE;
state->IQ[IQ_tail].ROB_index = ROB_index;
IQ_index = IQ_tail;
IQ_tail = (IQ_tail + 1)%IQ_SIZE;
state->IQ_tail = IQ_tail;

//Updating the Conflict Queue(CQ)
if(op_info->fu_group_num == FU_GROUP_MEM)
{ 
   CQ_tail = state->CQ_tail;
   CQ_head = state->CQ_head;
   state->CQ[CQ_tail].instr = instr;
   state->CQ[CQ_tail].issued = FALSE;
   state->CQ[CQ_tail].ROB_index = ROB_tail;
   CQ_index = CQ_tail;
   CQ_tail = (CQ_tail + 1)%CQ_SIZE;
   state->CQ_tail = CQ_tail;
}
register_rename(instr, state, IQ_index, CQ_index);
}


void
fetch(state_t *state) {
//Input state variables - pc, fetch_lock
//Output state variables - if_id

unsigned long pc = state->pc;
int i, instr;
int machine_type, big_endian = 1, little_endian = 0;
machine_type = little_endian;
if(machine_type == big_endian)
{
   instr = (state->mem[pc] << 24) | (state->mem[pc+1] << 16) | (state->mem[pc+2] << 8) | (state->mem[pc+3]);
}
else if(machine_type == little_endian)
{
   instr = (state->mem[pc]) | (state->mem[pc+1] << 8) | (state->mem[pc+2] << 16) | (state->mem[pc+3] << 24);
}
state->if_id.instr = instr;
state->if_id.pc = pc;

state->pc = state->pc + 4;
}


void register_rename(int instr, state_t *state, int IQ_index, int CQ_index)
{
const op_info_t *op_info;
int use_imm;
op_info = decode_instr(instr, &use_imm);

int r1, r2, rd, imm;
switch(op_info->fu_group_num)
{
case FU_GROUP_INT:
   if(use_imm == 0)
   {
      r1 = FIELD_R1(instr);
      r2 = FIELD_R2(instr);
      rd = FIELD_R3(instr);
      read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
      read_reg(r2, state, IQ_index, CQ_index, TRUE, TRUE, 2);
      write_reg(rd, state, IQ_index, TRUE);
   }
   else if(use_imm == 1)
   {
      r1 = FIELD_R1(instr);
      rd = FIELD_R2(instr);
      imm = FIELD_IMM(instr);
      read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
      set_imm_operand(imm, state, IQ_index, 2);
      write_reg(rd, state, IQ_index, TRUE);
   }
   break;
case FU_GROUP_ADD:
case FU_GROUP_MULT:
case FU_GROUP_DIV:
   //assert : use_imm == 0
   r1 = FIELD_R1(instr);
   r2 = FIELD_R2(instr);
   rd = FIELD_R3(instr);
   read_reg(r1, state, IQ_index, CQ_index, FALSE, TRUE, 1);
   read_reg(r2, state, IQ_index, CQ_index, FALSE, TRUE, 2);
   write_reg(rd, state, IQ_index, FALSE);
   break;
case FU_GROUP_BRANCH:
   switch(op_info->operation)
   {
   case OPERATION_J:
      break;
   case OPERATION_JAL:
      write_reg(31, state, IQ_index, TRUE);
      break;
   case OPERATION_JR:
      r1 = FIELD_R1(instr);
      read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
      break;
   case OPERATION_JALR:
      r1 = FIELD_R1(instr);
      read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
      write_reg(31, state, IQ_index, TRUE);
      break;
   case OPERATION_BEQZ:
   case OPERATION_BNEZ:
      r1 = FIELD_R1(instr);
      imm = FIELD_IMM(instr);
      read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
      set_imm_operand(imm, state, IQ_index, 2);
      break; 
   }
   break;
case FU_GROUP_MEM:
   r1 = FIELD_R1(instr);
   imm = FIELD_IMM(instr);
   read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
   set_imm_operand(imm, state, IQ_index, 2);
   //setting the Conflict Queue(CQ) tag
   state->CQ[CQ_index].tag1 = IQ_index; 
   switch(op_info->operation)
   {
   case OPERATION_LOAD:
      rd = FIELD_R2(instr);
      switch(op_info->data_type)
      {
      case DATA_TYPE_W:
         write_reg(rd, state, IQ_index, TRUE);
         break;
      case DATA_TYPE_F:
         write_reg(rd, state, IQ_index, FALSE);
         break; 
      }
      break;
   case OPERATION_STORE:
      r2 = FIELD_R2(instr);
      switch(op_info->data_type)
      {
      case DATA_TYPE_W:
         //Read an int reg into CQ
         read_reg(r2, state, IQ_index, CQ_index, TRUE, FALSE, 2);
         break;
      case DATA_TYPE_F:
         //Read an fp reg into CQ
         read_reg(r2, state, IQ_index, CQ_index, FALSE, FALSE, 2);
         break;
      }
      break;
   }
   break;
}
}


void read_reg(int r, state_t *state, int IQ_index, int CQ_index, int isint, int inIQ, int op)
{
int r_tag, ROB_completed;
operand_t r_value;
int op_tag;


r_tag = isint ? state->rf_int.tag[r] : state->rf_fp.tag[r];
ROB_completed = state->ROB[r_tag].completed;

//Reading register tag and value from register file or ROB
if(r_tag == -1)
{
   op_tag = -1;
   if(isint)
      r_value.integer = state->rf_int.reg_int.integer[r];
   else
      r_value.flt = state->rf_fp.reg_fp.flt[r];
}
else if(state->ROB[r_tag].completed == TRUE)
{
   op_tag = -1;
   r_value = state->ROB[r_tag].result;
}
else
{
   op_tag = r_tag;
}

//Copying the tag and register value into IQ or CQ, operand 1 or operand2.
if(inIQ)
{
   switch(op)
   {
   case 1:
      state->IQ[IQ_index].tag1 = op_tag;
      state->IQ[IQ_index].operand1 = r_value;
      break;
   case 2:
      state->IQ[IQ_index].tag2 = op_tag;
      state->IQ[IQ_index].operand2 = r_value;
      break;
   }
}
else
{
   switch(op)
   {
   case 1:
      printf("error_info : trying to read register value into address field of CQ in dispatch\n");
      break;
   case 2:
      state->CQ[CQ_index].tag2 = op_tag;
      state->CQ[CQ_index].result = r_value;
      break;
   }
}
}



void set_imm_operand(int imm, state_t *state, int IQ_index, int op)
{
switch(op)
{
   case 1:
      printf("error_info : operand 1 is being set with immediate value in IQ during dispatch\n");
      break;
   case 2:
      state->IQ[IQ_index].tag2 = -1;
      state->IQ[IQ_index].operand2.integer.w = imm;
      //Is immediate instruction signed long or unsigned long. Something to think about
      break;
}
}


void write_reg(int r, state_t *state, int IQ_index, int isint)
{
int ROB_index = state->IQ[IQ_index].ROB_index;
if(isint)
{
   if(r != 0)
      state->rf_int.tag[r] = ROB_index;
   else
      printf("error_info : Instruction trying to write to integer R0\n");
}
else
{
   state->rf_fp.tag[r] = ROB_index;
}
}
