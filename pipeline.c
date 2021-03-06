
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
int ROB_head, instr;
ROB_head = state->ROB_head;
const op_info_t *op_info;
int use_imm, isFloatMem = FALSE, commitThisCycle = TRUE;

//Examine the ROB head for completion
if(!state->ROB[ROB_head].completed)
{
   return 0;
}
else
{
   instr = state->ROB[ROB_head].instr;
   if(FIELD_OPCODE(instr) == 0x3f)
      return -1; //Return -1 to signal the end of simulation

   commitThisCycle = TRUE; 
   op_info = decode_instr(instr, &use_imm);
   if(op_info->operation == OPERATION_STORE)
   {
      isFloatMem = (op_info->data_type == DATA_TYPE_F) ? TRUE : FALSE;
      if(issue_fu_mem(state->fu_mem_list, ROB_head, isFloatMem, TRUE) != -1)
      {
         commitThisCycle = TRUE;
         if(DEBUG) printf("debug_info : Store issued into fu_mem in commit stage");
      }
      else
      {
         commitThisCycle = FALSE;
         return 0;
      }
   }

   if(commitThisCycle)
   {
      write_regfile(ROB_head, state);
      ROB_head = (ROB_head + 1)%ROB_SIZE;
      state->ROB_head = ROB_head;
      return 1;
   }
}
printf("error_info : commit not returning as expected\n");
}


void
writeback(state_t *state) {
int i, tag, isTaken;
//Integer writeback
for(i = 0; i < state->wb_port_int_num; i++)
{
   tag = state->wb_port_int[i].tag;
   if(tag != -1)
   {
      //Perform Writeback
      register_writeback(state, tag);
      state->wb_port_int[i].tag = -1; //Freeing wbport
   }
}
//Floating point writeback
for(i = 0; i < state->wb_port_fp_num; i++)
{
   tag = state->wb_port_fp[i].tag;
   if(tag != -1)
   {
      //Perform Writeback
      register_writeback(state, tag);
      state->wb_port_fp[i].tag = -1; 
   }
}

//Control instructions writeback
if(state->branch_tag != -1)
{
   tag = state->branch_tag;
   state->ROB[tag].completed = TRUE;
   state->fetch_lock = FALSE;
   //If branch is taken
   isTaken = state->ROB[tag].result.integer.w;
   if(isTaken)
   {
      state->pc = state->ROB[tag].target.integer.w; //Copying target to pc
      state->if_id.instr = 0; //Squashing the instruction in if_id and setting it equal to NOP
   }
   state->branch_tag = -1;
}
}


void
execute(state_t *state) {
//Advancing functional units by one stage
advance_fu_int(state->fu_int_list, state->wb_port_int, state->wb_port_int_num, &state->branch_tag);
advance_fu_fp(state->fu_add_list, state->wb_port_fp, state->wb_port_fp_num);
advance_fu_fp(state->fu_mult_list, state->wb_port_fp, state->wb_port_fp_num);
advance_fu_fp(state->fu_div_list, state->wb_port_fp, state->wb_port_fp_num);
advance_fu_mem(state->fu_mem_list, state->wb_port_int, state->wb_port_int_num, state->wb_port_fp, state->wb_port_fp_num); 
}


int
memory_disambiguation(state_t *state) {
int CQ_head, CQ_tail, CQ_curr, ROB_index;
CQ_head = state->CQ_head;
CQ_tail = state->CQ_tail;
CQ_curr = state->CQ_head;
while(CQ_curr != CQ_tail)
{
   if(!state->CQ[CQ_curr].issued)
   {
      if(state->CQ[CQ_curr].store)
      {
         if(state->CQ[CQ_curr].tag1 == -1)
         {
            if(state->CQ[CQ_curr].tag2 == -1)
            {
               //Issue the store memory operation.
               state->CQ[CQ_curr].issued = TRUE;
               ROB_index = state->CQ[CQ_curr].ROB_index;
               state->ROB[ROB_index].completed = TRUE; //bcos stores complete in the same cycle as they are issued.
               func_mem_exec(CQ_curr, state); 
               break;
            }
         }
         else
            break; //Cannot issue
      }
      else
      {
         if(state->CQ[CQ_curr].tag1 == -1)
         {
            if(disambigaute_mem(state, CQ_curr, CQ_head))
            {
               if(issue_mem_instr(CQ_curr, state) != -1)
               {
                  state->CQ[CQ_curr].issued = TRUE;
                  func_mem_exec(CQ_curr, state);
                  break;
               }
               else
                  if(DEBUG) printf("debug_info : Unable to issue Load instr bcos structural hazard for CQ_entry = %d\n", CQ_curr);
            }
            else
               if(DEBUG) printf("debug_info : Memory disambiguation failed for CQ_entry = %d\n", CQ_curr);
         }
      }
   }
   CQ_curr++;
}
//Removing issued instructions from the top of CQ
while(CQ_head != CQ_tail)
{
   if(!(state->CQ[CQ_head].issued))
      break;
   else
      CQ_head = (CQ_head + 1)%CQ_SIZE;
}
state->CQ_head = CQ_head;
}


int
issue(state_t *state) {
//Input state variables - IQ
//Output state variables - fu_int_list, fu_add_list, fu_mult_div, fu_div_list
int IQ_head, IQ_tail, IQ_curr;
IQ_head = state->IQ_head;
IQ_tail = state->IQ_tail;
IQ_curr = IQ_head;

while(IQ_curr != IQ_tail)
{
   if(!(state->IQ[IQ_curr].issued) && state->IQ[IQ_curr].tag1 == -1 && state->IQ[IQ_curr].tag2 == -1)
   {
      //Check and issue the instruction if it can be issued
      if(issue_instr(IQ_curr, state) != -1)
      {
         state->IQ[IQ_curr].issued = TRUE;
         func_exec(IQ_curr, state);
         break;
      }
      else
         IQ_curr = (IQ_curr + 1)%IQ_SIZE;   
   }
   else
      IQ_curr = (IQ_curr + 1)%IQ_SIZE;
}
//Removing issued instructions from the top of IQ
while(IQ_head != IQ_tail)
{
   if(!(state->IQ[IQ_head].issued))
      break;
   else
      IQ_head = (IQ_head + 1)%IQ_SIZE;
}
state->IQ_head = IQ_head;
}


int
dispatch(state_t *state) {
//Strictly Input state variables - if_id
//Both Input and Output state variables :  IQ, CQ, ROB, rf_int, rf_fp
//Strictly Output state variables - pc 

int instr;
int  ROB_tail, ROB_head, IQ_tail, IQ_head, CQ_tail, CQ_head;
int ROB_index, IQ_index, CQ_index;
int ROB_full = FALSE, IQ_full = FALSE, CQ_full = FALSE;
unsigned long pc;
int isHALT = FALSE, isNOP = FALSE, isCONTROL = FALSE, isMEMORY = FALSE, isSTORE = FALSE;
int buffersFull = FALSE;

instr = state->if_id.instr;
pc = state->if_id.pc;

const op_info_t *op_info;
int use_imm;
op_info = decode_instr(instr, &use_imm);


if(instr == 0x0) isNOP = TRUE; // NOP instruction
else if(FIELD_OPCODE(instr) == 0x3f) isHALT = TRUE; //HALT instruction
else if(op_info->fu_group_num == FU_GROUP_BRANCH) isCONTROL = TRUE;
else if(op_info->fu_group_num == FU_GROUP_MEM)
{
   isMEMORY = TRUE;
   if(op_info->operation == OPERATION_STORE) isSTORE = TRUE;
}

if(isNOP) return 0; //Drop the NOP instruction.

ROB_tail = state->ROB_tail;
ROB_head = state->ROB_head;
IQ_tail = state->IQ_tail;
IQ_head = state->IQ_head;
CQ_tail = state->CQ_tail;
CQ_head = state->CQ_head;
if( ROB_head == (ROB_tail + 1)%ROB_SIZE ) ROB_full = TRUE;
if( IQ_head == (IQ_tail + 1)%IQ_SIZE ) IQ_full = TRUE;
if( CQ_head == (CQ_tail + 1)%CQ_SIZE ) CQ_full = TRUE;

//Need to stall the pipeline if ROB, IQ or CQ are full.
//This is no longer happening.
if(ROB_full) buffersFull = TRUE;
else if(!ROB_full && IQ_full)
{
   if(!isHALT) buffersFull = TRUE;
}
else if(!ROB_full && !IQ_full && CQ_full)
{
   if(isMEMORY) buffersFull = TRUE;
}

if(buffersFull)
{
   if(DEBUG) printf("debug_info : One of ROB, IQ, CQ is full, locking fetch\n");
   return -1;
}

//Updating the Re-Order Buffer(ROB)
ROB_index = ROB_tail;
state->ROB[ROB_index].instr = instr;
if(!isHALT) state->ROB[ROB_index].completed = FALSE;
ROB_tail = (ROB_tail + 1)%ROB_SIZE;
state->ROB_tail = ROB_tail;

if(isHALT)
{
   state->ROB[ROB_index].completed = TRUE;
   state->fetch_lock = TRUE;
   return 0; //Exit dispatch if HALT instruction
}
   

//Updating the Instruction Queue(IQ)
IQ_index = IQ_tail;
state->IQ[IQ_index].instr = instr;
state->IQ[IQ_index].pc = pc;
state->IQ[IQ_index].issued = FALSE;
state->IQ[IQ_index].ROB_index = ROB_index;
IQ_tail = (IQ_tail + 1)%IQ_SIZE;
state->IQ_tail = IQ_tail;

//Updating the Conflict Queue(CQ)
if(isMEMORY)
{
   CQ_index = CQ_tail;
   state->CQ[CQ_index].instr = instr;
   state->CQ[CQ_index].issued = FALSE;
   state->CQ[CQ_index].ROB_index = ROB_index;
   state->CQ[CQ_index].store = isSTORE ? TRUE : FALSE;
   CQ_tail = (CQ_tail + 1)%CQ_SIZE;
   state->CQ_tail = CQ_tail;
}

register_rename(instr, state, IQ_index, CQ_index);

if(isCONTROL) state->fetch_lock = TRUE;
return 0;
}


void
fetch(state_t *state) {
//Input state variables - pc, fetch_lock
//Output state variables - if_id

unsigned long pc = state->pc;
int i, instr;
int machine_type = LITTLE_ENDIAN_MACHINE;
if(pc%4 != 0) printf("error_info : pc not a multiple of 4, pc = %lu\n", pc);
instr = memory_fetch_int(pc, state);
state->if_id.instr = instr;
state->if_id.pc = pc;
state->pc = pc + 4;
}


void register_rename(int instr, state_t *state, int IQ_index, int CQ_index)
{
const op_info_t *op_info;
int use_imm;
op_info = decode_instr(instr, &use_imm);

int r1, r2, rd, imm;
int ROB_index = state->IQ[IQ_index].ROB_index;;
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
      set_operand_ready(state, IQ_index, 1);
      set_operand_ready(state, IQ_index, 2);
      break;
   case OPERATION_JAL:
      set_operand_ready(state, IQ_index, 1);
      set_operand_ready(state, IQ_index, 2);
      write_reg(31, state, IQ_index, TRUE);
      break;
   case OPERATION_JR:
      r1 = FIELD_R1(instr);
      read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
      set_operand_ready(state, IQ_index, 2);
      break;
   case OPERATION_JALR:
      r1 = FIELD_R1(instr);
      read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
      set_operand_ready(state, IQ_index, 2);
      write_reg(31, state, IQ_index, TRUE);
      break;
   case OPERATION_BEQZ:
   case OPERATION_BNEZ:
      r1 = FIELD_R1(instr);
      //imm = FIELD_IMM(instr);
      read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
      set_operand_ready(state, IQ_index, 2);
      //set_imm_operand(imm, state, IQ_index, 2);
      break; 
   }
   break;
case FU_GROUP_MEM:
   r1 = FIELD_R1(instr);
   imm = FIELD_IMM(instr);
   read_reg(r1, state, IQ_index, CQ_index, TRUE, TRUE, 1);
   set_imm_operand(imm, state, IQ_index, 2);
   //setting the Conflict Queue(CQ) tag
   state->CQ[CQ_index].tag1 = ROB_index + ROB_SIZE;
   switch(op_info->operation)
   {
   case OPERATION_LOAD:
      rd = FIELD_R2(instr);
      state->CQ[CQ_index].tag2 = -1; //Set second operand in CQ to always ready for LOAD
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


int issue_instr(int IQ_entry, state_t *state)
{
//To this function : IQ buffer entry is read_only

int instr = state->IQ[IQ_entry].instr;
unsigned long pc = state->IQ[IQ_entry].pc;
const op_info_t *op_info;
int use_imm;
op_info = decode_instr(instr, &use_imm);

int tag = state->IQ[IQ_entry].ROB_index;
int isbranch = FALSE, islink = FALSE;

switch(op_info->fu_group_num)
{
case FU_GROUP_INT:
   isbranch = FALSE;
   islink = FALSE;
   if(issue_fu_int(state->fu_int_list, tag, isbranch, islink) != -1) return 0;
   break;
case FU_GROUP_MEM:
   tag = tag + ROB_SIZE;
   isbranch = FALSE;
   islink = FALSE;
   if(issue_fu_int(state->fu_int_list, tag, isbranch, islink) != -1) return 0;
   break;
case FU_GROUP_BRANCH:
   isbranch = TRUE;
   switch(op_info->operation)
   {
   case OPERATION_JAL:
   case OPERATION_JALR:
      islink = TRUE;
      break;
   default:
      islink = FALSE;
      break;
   }
   if(issue_fu_int(state->fu_int_list, tag, isbranch, islink) != -1) return 0;
   break;
case FU_GROUP_ADD:
   if(issue_fu_fp(state->fu_add_list, tag) != -1) return 0;
   break;
case FU_GROUP_MULT:
   if(issue_fu_fp(state->fu_mult_list, tag) != -1) return 0;
   break;
case FU_GROUP_DIV:
   if(issue_fu_fp(state->fu_div_list, tag) != -1) return 0;
   break;
default:
   printf("error_info : Instruction group is not one among INT, MEM, BRANCH, ADD, MUL, DIV in issue stage\n");
   break;
}
return -1;
}


void func_exec(int IQ_entry, state_t *state)
{
int instr = state->IQ[IQ_entry].instr;
const op_info_t *op_info;
int use_imm;
op_info = decode_instr(instr, &use_imm);

operand_t operand1, operand2, *result, *target;
unsigned long pc = state->IQ[IQ_entry].pc;
int ROB_index = state->IQ[IQ_entry].ROB_index; 
operand1 = state->IQ[IQ_entry].operand1;
operand2 = state->IQ[IQ_entry].operand2;
result = &state->ROB[ROB_index].result;
target = &state->ROB[ROB_index].target;

int offset = FIELD_OFFSET(instr); 
int imm;

switch(op_info->fu_group_num)
{
case FU_GROUP_INT:
  switch(op_info->operation)
  {
  case OPERATION_ADD:
     (*result).integer.w = operand1.integer.w + operand2.integer.w;
     break; 
  case OPERATION_ADDU:
     (*result).integer.wu = operand1.integer.wu + operand2.integer.wu;
     break;
  case OPERATION_SUB:
     (*result).integer.w = operand1.integer.w - operand2.integer.w;
     break;
  case OPERATION_SUBU:
     (*result).integer.wu = operand1.integer.wu - operand2.integer.wu;
     break;
  case OPERATION_SLL:
     (*result).integer.w = operand1.integer.wu <<  operand2.integer.w;
     break;
  case OPERATION_SRL:
     (*result).integer.w = operand1.integer.w >> operand2.integer.w;
     break;
  case OPERATION_AND:
     (*result).integer.w = operand1.integer.w & operand2.integer.w;
     break;
  case OPERATION_OR:
     (*result).integer.w = operand1.integer.w | operand2.integer.w;
     break;
  case OPERATION_XOR:
     (*result).integer.w = operand1.integer.w ^ operand2.integer.w;
     break;
  case OPERATION_SLT:
     (*result).integer.w = (operand1.integer.w < operand2.integer.w) ? TRUE : FALSE;
  case OPERATION_SLTU:
     (*result).integer.wu = (operand1.integer.wu < operand2.integer.wu) ? TRUE : FALSE;
     break;
  case OPERATION_SGT:
     (*result).integer.w = (operand1.integer.w > operand2.integer.w) ? TRUE : FALSE;
  case OPERATION_SGTU:
     (*result).integer.wu = (operand1.integer.wu > operand2.integer.wu) ? TRUE : FALSE;
     break;
  default:
     printf("error_info : unknown operation type in fu_group_int during functional execution\n");
     break;
  }
  break;
case FU_GROUP_ADD:
  switch(op_info->operation)
  {
  case OPERATION_ADD: 
     (*result).flt = operand1.flt + operand2.flt;
     break;
  case OPERATION_SUB:
     (*result).flt = operand1.flt - operand2.flt;
     break;
  }
  break;
case FU_GROUP_MULT:
  (*result).flt = operand1.flt * operand2.flt;
  break;
case FU_GROUP_DIV:
  (*result).flt = operand1.flt / operand2.flt;
  break;
case FU_GROUP_MEM:
  (*target).integer.w = operand1.integer.w + operand2.integer.w;
  break;
case FU_GROUP_BRANCH:
  switch(op_info->operation)
  {
     case OPERATION_J:
        (*result).integer.w = TRUE; //3; //bullshit
        (*target).integer.w = pc + offset + 4;
        break;
     case OPERATION_JR:
        (*result).integer.w = TRUE;
        (*target).integer.w = operand1.integer.w;
        break;
     case OPERATION_JAL:
        (*result).integer.w = TRUE;
        (*target).integer.w = pc + offset + 4;
        //What about storing the value of pc in r31
        break;
     case OPERATION_JALR:
        (*result).integer.w = TRUE;
        (*target).integer.w = operand1.integer.w;
        //What about storing the value of pc in r31.
        break;
     case OPERATION_BEQZ:
        imm = FIELD_IMM(instr);
        (*target).integer.w = pc + imm + 4;
        (*result).integer.w = (operand1.integer.w == 0) ? TRUE : FALSE;
        //(*target).integer.w = (operand1.integer.w == 0) ? pc + operand2.integer.w + 4 : pc + 4;
        //(*target).integer.w = pc + operand2.integer.w + 4;
        break;
     case OPERATION_BNEZ:
        imm = FIELD_IMM(instr);
        (*target).integer.w = pc + imm + 4;
        (*result).integer.w = (operand1.integer.w != 0) ? TRUE : FALSE;
        //(*target).integer.w = (operand1.integer.w != 0) ? pc + operand2.integer.w + 4 : pc + 4;
        //(*target).integer.w = pc + operand2.integer.w + 4;
        break;
  }
  break;
default :
  printf("error_info : Does not belong to one among FU_INT, ADD, MULT, DIV, BRANCH, MEM groups in function execution\n");
  break;
}
}


void register_writeback(state_t *state, int tag)
{
if( tag < 0 ) printf("error_info : register_writeback called with tag < 0  or tag > 64\n");

int ROB_index, CQ_head, CQ_tail, CQ_curr, IQ_curr, IQ_head, IQ_tail;
CQ_head = state->CQ_head;
CQ_tail = state->CQ_tail;
IQ_head = state->IQ_head;
IQ_tail = state->IQ_tail;

if(tag < ROB_SIZE)
{
   //Writing to ROB
   state->ROB[tag].completed = TRUE;

   //Forwarding the results to IQ and CQ
   IQ_curr = IQ_head;
   while(IQ_curr != IQ_tail)
   {
      if(state->IQ[IQ_curr].tag1 == tag)
      {
         state->IQ[IQ_curr].tag1 = -1;
         state->IQ[IQ_curr].operand1 = state->ROB[tag].result;
         
      }
      if(state->IQ[IQ_curr].tag2 == tag)
      {
         state->IQ[IQ_curr].tag2 = -1;
         state->IQ[IQ_curr].operand2 = state->ROB[tag].result;
      }
      IQ_curr = (IQ_curr + 1)%IQ_SIZE;
   }

   CQ_curr = CQ_head;
   while(CQ_curr != CQ_tail)
   {
      if(state->CQ[CQ_curr].tag2 == tag)
      {
         state->CQ[CQ_curr].tag2 = -1;
         state->CQ[CQ_curr].result = state->ROB[tag].result;
      }
      CQ_curr = (CQ_curr + 1)%CQ_SIZE;
   }
}
else
{
    //Writing the results to CQ operand 1
    ROB_index = tag - ROB_SIZE;
    CQ_curr = CQ_head;
    while(CQ_curr != CQ_tail)
    {
       if(state->CQ[CQ_curr].ROB_index == ROB_index)
       {
          if(DEBUG) printf("debug_info : CQ entry which matchces the tag = %d is CQ_curr = %d\n", tag, CQ_curr);
          state->CQ[CQ_curr].tag1 = -1;
          state->CQ[CQ_curr].address = state->ROB[ROB_index].target;
          break;
       }
       CQ_curr = (CQ_curr + 1)%CQ_SIZE;
    }
}
}

int disambigaute_mem(state_t *state, int CQ_curr, int CQ_head)
{
int i;
for(i = CQ_head; i < CQ_curr; i++)
{
   if(state->CQ[i].store && !(state->CQ[i].issued))
   {
      if(state->CQ[i].tag1 != -1) printf("error_info : tyring disambiguating a store without address");
      if(state->CQ[i].address.integer.w == state->CQ[CQ_curr].address.integer.w)
         return FALSE;
   }
   i++;
}
return TRUE;
}


int issue_mem_instr(int CQ_index, state_t *state)
{
int ROB_index, isStore = FALSE, isFloatMem = FALSE;
int instr, use_imm;
const op_info_t *op_info;
instr = state->CQ[CQ_index].instr;
op_info = decode_instr(instr, &use_imm);

ROB_index = state->CQ[CQ_index].ROB_index;
isStore = state->CQ[CQ_index].store;
isFloatMem = (op_info->data_type == DATA_TYPE_F) ? TRUE : FALSE;
//Issue the Load instruction after checking for available memory units
if(issue_fu_mem(state->fu_mem_list, ROB_index, isFloatMem, isStore) != -1)
   return 0;
else
   return -1;
}


void func_mem_exec(int CQ_entry, state_t *state)
{
int instr = state->CQ[CQ_entry].instr;
const op_info_t *op_info, *local_op_info;
int use_imm;
op_info = decode_instr(instr, &use_imm);

int ROB_index = state->CQ[CQ_entry].ROB_index;
int rd = FIELD_R2(instr);
int address = state->CQ[CQ_entry].address.integer.w;
int i, local_instr, local_use_imm, search = FALSE;


//address = state->ROB[ROB_index].target.integer.w;

switch(op_info->operation)
{
case OPERATION_LOAD:
   //Go and check if any issued store is present in the reorder buffer before going to memory to fetch
   for(i = 0; i < ROB_index; i++)
   {
      local_op_info = decode_instr(local_instr, &local_use_imm);
      if(local_op_info->operation == OPERATION_STORE)
      {
         if(state->ROB[i].completed)
         {
            if(state->ROB[i].target.integer.w == address)
            {
               search = TRUE;
               state->ROB[ROB_index].result = state->ROB[i].result;
            }
             
         }
         else
            printf("error_info : Load executing before a prior incomplete store\n"); 
      }
   }
   if(!search)  state->ROB[ROB_index].result.integer.w = memory_fetch_int(address, state); 
   break;
case OPERATION_STORE:
   state->ROB[ROB_index].result = state->CQ[CQ_entry].result;
   break;
}
}

void write_regfile(int ROB_entry, state_t *state)
{
int instr = state->ROB[ROB_entry].instr;
const op_info_t *op_info;
int use_imm, rd, isINT, mem_addr;
op_info = decode_instr(instr, &use_imm);
operand_t result = state->ROB[ROB_entry].result;
switch(op_info->fu_group_num)
{
case FU_GROUP_INT:
   rd = use_imm ? FIELD_R2(instr) : FIELD_R3(instr);
   update_reg(rd, ROB_entry, result, state, TRUE);
   break;
case FU_GROUP_ADD:
case FU_GROUP_MULT:
case FU_GROUP_DIV:
  rd = FIELD_R3(instr);
  update_reg(rd, ROB_entry, result, state, FALSE);
  break;
case FU_GROUP_MEM:
  switch(op_info->operation)
  {
  case OPERATION_LOAD:
     rd = FIELD_R2(instr);
     isINT = (op_info->data_type == DATA_TYPE_W) ? TRUE : FALSE;
     update_reg(rd, ROB_entry, result, state, isINT);
     break;
  case OPERATION_STORE:
     mem_addr = state->ROB[ROB_entry].target.integer.w;
     write_mem_int(mem_addr, result.integer.w, state);
     break;
  }
  break;
case FU_GROUP_BRANCH:
  if(op_info->operation == OPERATION_JAL || op_info->operation == OPERATION_JALR)
  {
     rd = 31;
     //What about storing the value of pc in r31.
  } 
  break;
}
return;
}


void update_reg(int rd, int ROB_entry, operand_t result,  state_t *state, int isINT)
{
if(isINT)
{
   state->rf_int.reg_int.integer[rd].w = result.integer.w; //Writing to integer register file
   if(state->rf_int.tag[rd] == ROB_entry)
      state->rf_int.tag[rd] = -1; //Register is ready
}
else
{
   state->rf_fp.reg_fp.flt[rd] = result.flt; //Writing to floating point register file
   if(state->rf_fp.tag[rd] == ROB_entry)
     state->rf_fp.tag[rd] = -1; //Register is ready
}
return ;
}

int memory_fetch_int(unsigned long pc, state_t *state)
{
int data;
if(MACHINE_TYPE == BIG_ENDIAN_MACHINE)
{
   data = (state->mem[pc] << 24) | (state->mem[pc+1] << 16) | (state->mem[pc+2] << 8) | (state->mem[pc+3]);
}
else if(MACHINE_TYPE == LITTLE_ENDIAN_MACHINE)
{
   data = (state->mem[pc]) | (state->mem[pc+1] << 8) | (state->mem[pc+2] << 16) | (state->mem[pc+3] << 24);
}
return data;
}

void write_mem_int(int mem_addr, int result, state_t *state)
{
if(MACHINE_TYPE == BIG_ENDIAN_MACHINE)
{
   state->mem[mem_addr] = (char)((result & 0xff000000) >> 24);
   state->mem[mem_addr + 1] = (char)((result & 0x00ff0000) >> 16);
   state->mem[mem_addr + 2] = (char)((result & 0x0000ff00) >> 8);
   state->mem[mem_addr + 3] = (char)(result & 0x000000ff) ;

}
else if(MACHINE_TYPE == LITTLE_ENDIAN_MACHINE)
{
   state->mem[mem_addr + 3] = (char)((result & 0xff000000) >> 24);
   state->mem[mem_addr + 2] = (char)((result & 0x00ff0000) >> 16);
   state->mem[mem_addr + 1] = (char)((result & 0x0000ff00) >> 8);
   state->mem[mem_addr] = (char)(result & 0x000000ff) ;
}
}


void set_operand_ready(state_t *state, int IQ_index, int op)
{
switch(op)
{
case 1:
   state->IQ[IQ_index].tag1 = -1;
   //state->IQ[IQ_index].operand1.integer.w = 0;
   break;
case 2:
   state->IQ[IQ_index].tag2 = -1;
   //state->IQ[IQ_index].operand2.integer.w = 0;
   break;
default:
   printf("error_info : op values to set_operand_ready should only be 1 or 2\n");
   break;
}
return;
}
