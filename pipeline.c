
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

int instr, ROB_tail, ROB_head, ROB_full, IQ_tail, IQ_head, IQ_full, CQ_full, ROB_index;
unsigned long pc;
instr = state->if_id.instr;
pc = state->if_id.pc;
if(FIELD_OPCODE(instr) == 0x0) return 0; //Drop the NOP instruction

//Updating the Re-Order Buffer(ROB)
ROB_tail = state->ROB_tail;
ROB_head = state->ROB_head;
ROB_full = FALSE;
state->ROB[ROB_tail].instr = instr;
if(FIELD_OPCODE(instr) == 0x3f) //HALT instruction
   state->ROB[ROB_tail].completed = TRUE;
else
   state->ROB[ROB_tail].completed = FALSE;
ROB_index = ROB_tail;
ROB_tail = (ROB_tail + 1)%ROB_SIZE;
state->ROB_tail = ROB_tail;

//Updating the Instruction Queue(IQ)
IQ_tail = state->IQ_tail;
IQ_head = state->IQ_head;
IQ_full = FALSE;
state->IQ[IQ_tail].instr = instr;
state->IQ[IQ_tail].pc = pc;
state->IQ[IQ_tail].issued = FALSE;
state->IQ[IQ_tail].ROB_index = ROB_index;

//Updating the Conflict Queue(CQ)
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
