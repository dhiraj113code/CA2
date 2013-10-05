
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

register_rename(instr, state, IQ_index);

//Updating the Conflict Queue(CQ)
//If memory instruction
CQ_tail = state->CQ_tail;
CQ_head = state->CQ_head;
state->CQ[CQ_tail].instr = instr;
state->CQ[CQ_tail].issued = FALSE;
state->CQ[CQ_tail].ROB_index = ROB_tail;
CQ_index = CQ_tail;
CQ_tail = (CQ_tail + 1)%CQ_SIZE;
state->CQ_tail = CQ_tail;

//Call to register_rename function 
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


void register_rename(int instr, state_t *state, int IQ_index)
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
      read_int_reg(r1, state, IQ_index, 1);
      read_int_reg(r2, state, IQ_index, 2);
   }
   else if(use_imm == 1)
   {
      r1 = FIELD_R1(instr);
      rd = FIELD_R2(instr);
      imm = FIELD_IMM(instr);
      read_int_reg(r1, state, IQ_index, 1);
   }
   break;
case FU_GROUP_ADD:
case FU_GROUP_MULT:
case FU_GROUP_DIV:
   //assert : use_imm == 0
   r1 = FIELD_R1(instr);
   r2 = FIELD_R2(instr);
   rd = FIELD_R3(instr);
   read_fp_reg(r1, state, IQ_index, 1);
   read_fp_reg(r2, state, IQ_index, 2);
   break;
case FU_GROUP_BRANCH:
   switch(op_info->operation)
   {
   case OPERATION_J:
   case OPERATION_JAL:
      break;
   case OPERATION_JR:
   case OPERATION_JALR:
      r1 = FIELD_R1(instr);
      read_int_reg(r1, state, IQ_index, 1);
      break;
   case OPERATION_BEQZ:
   case OPERATION_BNEZ:
      r1 = FIELD_R1(instr);
      imm = FIELD_IMM(instr);
      read_int_reg(r1, state, IQ_index, 1);
      break; 
   }
   break;
case FU_GROUP_MEM:
   switch(op_info->operation)
   {
   case OPERATION_LOAD:
      switch(op_info->data_type)
      {
      case DATA_TYPE_W:
         break;
      case DATA_TYPE_F:
         break; 
      }
      break;
   case OPERATION_STORE:
      switch(op_info->data_type)
      {
      case DATA_TYPE_W:
         break;
      case DATA_TYPE_F:
         break;
      }
      break;
   }
   break;
}
}


void read_int_reg(int r, state_t *state, int IQ_index, int op)
{
if(state->rf_int.tag[r] == -1)
{
   state->IQ[IQ_index].tag1 = -1;
   switch(op)
   {
   case 1:
      state->IQ[IQ_index].tag1 = -1;
      state->IQ[IQ_index].operand1.integer = state->rf_int.reg_int.integer[r];
      break;
   case 2:
      state->IQ[IQ_index].tag2 = -1;
      state->IQ[IQ_index].operand2.integer = state->rf_int.reg_int.integer[r];
      break;
   }
}
else
{
   switch(op)
   {
   case 1:
      state->IQ[IQ_index].tag1 = state->rf_int.tag[r];
      break;
   case 2:
      state->IQ[IQ_index].tag2 = state->rf_int.tag[r];
      break;
   }
}
}


void read_fp_reg(int r, state_t *state, int IQ_index, int op)
{
if(state->rf_fp.tag[r] == -1)
{
   switch(op)
   {
   case 1:
      state->IQ[IQ_index].tag1 = -1;
      state->IQ[IQ_index].operand1.flt = state->rf_fp.reg_fp.flt[r];
      break;
   case 2:
      state->IQ[IQ_index].tag2 = -1;
      state->IQ[IQ_index].operand2.flt = state->rf_fp.reg_fp.flt[r];
      break;
   }
}
else
{
   switch(op)
   {
   case 1:
      state->IQ[IQ_index].tag1 = state->rf_fp.tag[r];
      break;
   case 2:
      state->IQ[IQ_index].tag2 = state->rf_fp.tag[r];
      break;
   }
}
}
