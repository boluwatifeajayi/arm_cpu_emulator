//imports, give access to certain functions and types,
#include <stdint.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 

// basic type definitions, this for code readability, 
typedef uint32_t INSTRUCTION; // 32 bit instruction type, this is what we read from memory and decode to figure out what to do
typedef uint32_t REGISTER; // 32 bit register type, this is what we use for our 16 registers,
typedef uint8_t BYTE; // 8 bit byte type, used for byte mode in memory transfers


#define NUMBER_OF_REGISTERS 16 // number of registers we have in our arm cpu
#define MEMORY_SIZE 1024 // our memory size for our cpu, basically 4KB of memory
#define STACK_START_ADDRESS 0x00010000 // keep track of start address of stack in the register


REGISTER register_array[NUMBER_OF_REGISTERS]; // simulate a register storage for our array for our 16 registers,
INSTRUCTION ram_memory_array[MEMORY_SIZE]; // simulate our ram memory, Instructions live here before being pulled into registers

// status flags - N Z C V record what happened after each operation, used to know how to update flags after each instruction and also for conditional execution of instructions based on these flags
int flag_NEGATIVE = 0;
int flag_ZERO = 0;
int flag_CARRY = 0;
int flag_OVERFLOW = 0;


#define PROGRAM_COUNTER register_array[15] //tracks where the program is and incremenrs by 4, r15 for arm
#define STACK_POINTER register_array[13] // tracks the top of the stack address r13
#define LINK_REGISTER register_array[14] // used to know where to return after an instruction r14


unsigned long total_cycles = 0;  // just incrementer for anytime program runs an instruction

// 16 possible condition codes that the cpu will underdstand names for numbers (0–15) that represent conditions., its to know if something should be run, used with our status flags
#define COND_EQUAL 0x0       // EQ - Z set
#define COND_NOT_EQUAL 0x1   // NE - Z clear
#define COND_CARRY_SET 0x2   // CS - C set 
#define COND_CARRY_CLEAR 0x3 // CC - C clear
#define COND_MINUS 0x4       // MI - N set
#define COND_PLUS 0x5        // PL - N clear
#define COND_OVERFLOW 0x6    // VS - V set
#define COND_NO_OVERFLOW 0x7 // VC - V clear
#define COND_HI 0x8          // unsigned higher - C set and Z clear
#define COND_LS 0x9          // unsigned lower or same - C clear or Z set
#define COND_GE 0xA          // signed >= - N == V
#define COND_LT 0xB          // signed < - N != V
#define COND_GT 0xC          // signed > - Z clear and N == V
#define COND_LE 0xD          // signed <= - Z set or N != V
#define COND_ALWAYS 0xE      // AL - always run 
#define COND_NEVER 0xF       // NV - never run 

// function that uses CPU flags to decide whether an instruction should execute or be skipped.
int check_cond(int cond) {
  switch (cond) {
  case COND_EQUAL:
    return flag_ZERO == 1;
  case COND_NOT_EQUAL:
    return flag_ZERO == 0;
  case COND_CARRY_SET:
    return flag_CARRY == 1;
  case COND_CARRY_CLEAR:
    return flag_CARRY == 0;
  case COND_MINUS:
    return flag_NEGATIVE == 1;
  case COND_PLUS:
    return flag_NEGATIVE == 0;
  case COND_OVERFLOW:
    return flag_OVERFLOW == 1;
  case COND_NO_OVERFLOW:
    return flag_OVERFLOW == 0;
  case COND_HI:
    return (flag_CARRY == 1 && flag_ZERO == 0);
  case COND_LS:
    return (flag_CARRY == 0 || flag_ZERO == 1);
  case COND_GE:
    return (flag_NEGATIVE == flag_OVERFLOW);
  case COND_LT:
    return (flag_NEGATIVE != flag_OVERFLOW);
  case COND_GT:
    return (flag_ZERO == 0 && flag_NEGATIVE == flag_OVERFLOW);
  case COND_LE:
    return (flag_ZERO == 1 || flag_NEGATIVE != flag_OVERFLOW);
  case COND_ALWAYS:
    return 1;
  case COND_NEVER:
    return 0;
  default:
    return 1;
  }
}

// helper mechanics to make our program behave like a cpu


// converts internal condition codes which are just numbers into the readable suffixes seen in ARM assembly.
const char *cond_to_str(int cond) {
  switch (cond) {
  case COND_EQUAL:
    return "eq";
  case COND_NOT_EQUAL:
    return "ne";
  case COND_CARRY_SET:
    return "cs";
  case COND_CARRY_CLEAR:
    return "cc";
  case COND_MINUS:
    return "mi";
  case COND_PLUS:
    return "pl";
  case COND_OVERFLOW:
    return "vs";
  case COND_NO_OVERFLOW:
    return "vc";
  case COND_HI:
    return "hi";
  case COND_LS:
    return "ls";
  case COND_GE:
    return "ge";
  case COND_LT:
    return "lt";
  case COND_GT:
    return "gt";
  case COND_LE:
    return "le";
  case COND_ALWAYS:
    return "";
  default:
    return "";
  }
}

// update N and Z flags based on a result
void update_nz(uint32_t result) {
  flag_NEGATIVE = (result >> 31) & 1; // set as  negative if the top bit is set as 1
  flag_ZERO = (result == 0) ? 1 : 0; // set as zero if result is zero, otherwise clear
}

// handles the flads after an addition operation, needs to check for carry and overflow which are different things, also updates N and Z
void update_flags_add(uint32_t a, uint32_t b, uint32_t result) {
  update_nz(result);
  // carry if result wrapped around
  flag_CARRY = (result < a) ? 1 : 0;
  // overflow if signs are wrong
  flag_OVERFLOW = (((a ^ result) & (b ^ result)) >> 31) & 1;
}

// handles the flags after a subtraction operation, carry in subtraction means no borrow so its set if a is greater or equal to b, overflow is if the signs are wrong, also updates N and Z
void update_flags_sub(uint32_t a, uint32_t b, uint32_t result) {
  update_nz(result);
  flag_CARRY = (a >= b) ? 1 : 0;
  flag_OVERFLOW = (((a ^ b) & (a ^ result)) >> 31) & 1;
}

 // Applies bit shifting to a value based on the shift type and amount, moves the bits around
uint32_t do_shift(uint32_t val, int shift_type, int amount) {
  if (amount == 0)
    return val;

  switch (shift_type) {
  case 0: // LSL
    return val << amount; // logical shift left
  case 1: // LSR
    return val >> amount; // logical shift right
  case 2: // ASR - arithmetic shift right (sign extend)
    return (uint32_t)((int32_t)val >> amount); // arithmetic shift right same as LSR but the left fills with whatever the top bit was. So negative numbers stay negative.
  case 3: // ROR - rotate right
    amount = amount & 31; 
    if (amount == 0)
      return val;
    return (val >> amount) | (val << (32 - amount)); // rotate right  bits that fall off the right end come back in on the left. Nothing gets lost, it just rotates around.
  default:
    return val;
  }
}

// get shift type name as string for printing assembly
const char *shift_name(int type) {
  switch (type) {
  case 0:
    return "lsl";
  case 1:
    return "lsr";
  case 2:
    return "asr";
  case 3:
    return "ror";
  default:
    return "???";
  }
}

// Turns register numbers into names
const char *reg_name(int r) {
  static char bufs[4][8];
  static int idx = 0;
  char *buf = bufs[idx % 4];
  idx++;
  switch (r) {
  case 10:
    return "sl"; // stack limit
  case 11:
    return "fp"; // frame pointer
  case 12:
    return "ip"; // instruction pointer
  case 13:
    return "sp"; // stack pointer
  case 14:
    return "lr"; // link register
  case 15:
    return "pc"; // program counter
  default:
    sprintf(buf, "r%d", r); 
    return buf; // for r0-r9 just return r0, r1, etc.
  }
}

// Extract operand2 → compute its value → build printable string
uint32_t decode_operand2(INSTRUCTION inst, int is_imm, char *out_str) {
  if (is_imm) {
    // immediate value - 8 bit value rotated right by 2 * rotate field
    int rotate = (inst >> 8) & 0xF;
    uint32_t imm = inst & 0xFF;
    uint32_t val = do_shift(imm, 3, rotate * 2); // ROR
    if (rotate != 0) {
      val = (imm >> (rotate * 2)) | (imm << (32 - rotate * 2));
    } else {
      val = imm;
    }
    sprintf(out_str, "#%u", val);
    return val;
  } else {
    // register operand
    int rm = inst & 0xF;
    int shift_type = (inst >> 5) & 0x3;
    int reg_shift = (inst >> 4) & 1; // 1 = shift amount is in a register

    if (reg_shift) {
      // shift amount comes from a register
      int rs = (inst >> 8) & 0xF;
      int amount = register_array[rs] & 0xFF;
      // special case: rrx
      if (shift_type == 3 && amount == 0) {
        uint32_t result = (register_array[rm] >> 1) | (flag_CARRY << 31);
        sprintf(out_str, "%s, rrx", reg_name(rm));
        return result;
      }
      uint32_t result = do_shift(register_array[rm], shift_type, amount);
      sprintf(out_str, "%s, %s %s", reg_name(rm), shift_name(shift_type),
              reg_name(rs));
      return result;
    } else {
      int amount = (inst >> 7) & 0x1F;
      // special case: RRX (ror with amount 0 is actually RRX)
      if (shift_type == 3 && amount == 0) {
        uint32_t result = (register_array[rm] >> 1) | (flag_CARRY << 31);
        sprintf(out_str, "%s, rrx", reg_name(rm));
        return result;
      }
      // lsl #0 is just the register itself
      if (amount == 0 && shift_type == 0) {
        sprintf(out_str, "%s", reg_name(rm));
        return register_array[rm];
      }
      uint32_t result = do_shift(register_array[rm], shift_type, amount);
      sprintf(out_str, "%s, %s #%d", reg_name(rm), shift_name(shift_type),
              amount);
      return result;
    }
  }
}

// print all 16 registers, flags and cycle count to the terminal after each instruction
void print_registers() {
  printf("R0: %08X  R1: %08X  R2: %08X  R3: %08X\n", register_array[0],
         register_array[1], register_array[2], register_array[3]);
  printf("R4: %08X  R5: %08X  R6: %08X  R7: %08X\n", register_array[4],
         register_array[5], register_array[6], register_array[7]);
  printf("R8: %08X  R9: %08X  R10:%08X  R11:%08X\n", register_array[8],
         register_array[9], register_array[10], register_array[11]);
  printf("R12:%08X  R13:%08X  R14:%08X  R15:%08X\n", register_array[12],
         register_array[13], register_array[14], register_array[15]);
  printf("Flags: N=%d Z=%d C=%d V=%d\n", flag_NEGATIVE, flag_ZERO, flag_CARRY,
         flag_OVERFLOW);
  printf("Cycles so far: %lu\n", total_cycles);
}

// decode and execute one data processing instruction
// Decode -> get operands -> Do operation -> update flags -> print disassembly maybe
void do_data_processing(INSTRUCTION inst, int cond) {

 //breaks down the instruction into its components
  int is_imm = (inst >> 25) & 1; // I bit - determines if operand2 its 1 is immediate or register-based which is 0 (bits 25)
  int opcode = (inst >> 21) & 0xF; // bits 24-21 give us the specific operation (ADD, SUB, AND, ORR, etc.)
  int set_flags = (inst >> 20) & 1; // operate status flags or cpu flags if its 1 (bits 20)
  int rn = (inst >> 16) & 0xF; // operand1 register number (bits 19-16) - this is the first operand for most instructions, but ignored for MOV/MVN
  int rd = (inst >> 12) & 0xF; // destination register number (bits 15-12)  results

    // gets pure operand2 value and also a string for disassembly
  char op2_str[32];
  uint32_t op2 = decode_operand2(inst, is_imm, op2_str); // operand2 is the shifted register or immediate value, and we also get a string for disassembly
  uint32_t op1 = register_array[rn]; // operand1 is always the value in rn the first first source register number
  uint32_t result = 0;

  // figure out the operaton it is 
  const char *mnem = "???"; // will hold the mnemonic for disassembly, default to ??? if we dont recognize it
  int write_result = 1; // most instructions write back to rd, write result to register rd if this is 1, some instructions like TST/TEQ/CMP/CMN dont write to rd, they just update flags based on rn and op2, so for those we set this to 0

  // look up code from encoding chart and do the operation, also update flags if needed, and build the disassembly string

  switch (opcode) {
  case 0x0: // AND
    mnem = "and";
    result = op1 & op2;
    if (set_flags)
    // AND updates N and Z based on the result, but doesnt affect C or V
      update_nz(result);
    break;
  case 0x1: // EOR
    mnem = "eor";
    result = op1 ^ op2;
    if (set_flags)
      update_nz(result);
    break;
  case 0x2: // SUB
    mnem = "sub";
    result = op1 - op2;
    if (set_flags)
      update_flags_sub(op1, op2, result);
    break;
  case 0x3: // RSB - reverse subtract
    mnem = "rsb";
    result = op2 - op1;
    if (set_flags)
      update_flags_sub(op2, op1, result);
    break;
  case 0x4: // ADD
    mnem = "add";
    result = op1 + op2;
    if (set_flags)
      update_flags_add(op1, op2, result);
    break;
  case 0x5: // ADC - add with carry
    mnem = "adc";
    result = op1 + op2 + flag_CARRY;
    if (set_flags)
      update_flags_add(op1, op2, result);
    break;
  case 0x6: // SBC - subtract with carry
    mnem = "sbc";
    result = op1 - op2 - (1 - flag_CARRY);
    if (set_flags)
      update_flags_sub(op1, op2, result);
    break;
  case 0x7: // RSC
    mnem = "rsc";
    result = op2 - op1 - (1 - flag_CARRY);
    if (set_flags)
      update_flags_sub(op2, op1, result);
    break;
  case 0x8: // TST - test, just updates flags, no write
    mnem = "tst";
    result = op1 & op2;
    write_result = 0;
    if (set_flags)
      update_nz(result);
    break;
  case 0x9: // TEQ - test equivalence
    mnem = "teq";
    result = op1 ^ op2;
    write_result = 0;
    if (set_flags)
      update_nz(result);
    break;
  case 0xA: // CMP - compare
    mnem = "cmp";
    result = op1 - op2;
    write_result = 0;
    // CMP always updates flags even without S bit
    update_flags_sub(op1, op2, result);
    break;
  case 0xB: // CMN - compare negative
    mnem = "cmn";
    result = op1 + op2;
    write_result = 0;
    update_flags_add(op1, op2, result);
    break;
  case 0xC: // ORR
    mnem = "orr";
    result = op1 | op2;
    if (set_flags)
      update_nz(result);
    break;
  case 0xD: // MOV - just moves op2 into rd
    mnem = "mov";
    result = op2;
    if (set_flags)
      update_nz(result);
    break;
  case 0xE: // BIC - bit clear
    mnem = "bic";
    result = op1 & ~op2;
    if (set_flags)
      update_nz(result);
    break;
  case 0xF: // MVN - move not
    mnem = "mvn";
    result = ~op2;
    if (set_flags)
      update_nz(result);
    break;
  }

  // figure out what the S suffix should look like
  const char *s_str = set_flags ? "s" : "";
  const char *cond_str = cond_to_str(cond);

  // print the disassembly line
  // MOV and MVN dont use rn so just: mov rd, op2
  if (opcode == 0xD || opcode == 0xF) {
    printf("%s%s%s %s, %s", mnem, cond_str, s_str, reg_name(rd), op2_str);
  }
  // TST, TEQ, CMP, CMN dont write to rd - just: cmp rn, op2
  else if (!write_result) {
    printf("%s%s %s, %s", mnem, cond_str, reg_name(rn), op2_str);
  }
  // normal 3-operand: add rd, rn, op2
  else {
    printf("%s%s%s %s, %s, %s", mnem, cond_str, s_str, reg_name(rd),
           reg_name(rn), op2_str);
  }

  // write result if needed and condition passes
  if (write_result && check_cond(cond)) {
    register_array[rd] = result;
  }
}

// jumping to a different part of the program so It reads a branch instruction, figures out where to jump, and updates the program counter to go there.
void do_branch(INSTRUCTION inst, int cond) {
  int link = (inst >> 24) & 1; // is it branch with linkk which is function call, if set we need to save return address in link register because its a function
  int32_t offset = inst & 0x00FFFFFF; // offset so it knows how far to jump
  // sign extend from 24 bits

  //handle negatuve jumps
  if (offset & 0x800000) {
    offset |= 0xFF000000;
  }
  offset = offset << 2;

  const char *cond_str = cond_to_str(cond);

  if (link) {
    printf("bl%s #%d", cond_str, offset);
  } else {
    printf("b%s #%d", cond_str, offset);
  }

  if (check_cond(cond)) {
    if (link) {
      LINK_REGISTER = PROGRAM_COUNTER; // save return address
    }
    // PROGRAM_COUNTER is already pointing at next instruction (+4), add offset
    // and +4 for pipeline
    PROGRAM_COUNTER = PROGRAM_COUNTER + offset + 4;
  }
}

// decode and handle LDR / STR (single data transfer)
//“Figure out an address, then either read from it or write to it.”
void do_mem_transfer(INSTRUCTION inst, int cond) {

    // bits extraction
  int is_reg_offset = (inst >> 25) & 1; // decode intruction flag so know if register or immidate number
  int pre_index = (inst >> 24) & 1;     // to know qwhen to apply offset 
  int up = (inst >> 23) & 1;            // add or subtract offset
  int byte_mode = (inst >> 22) & 1;     // byte transfer if set the size of the transfer is 1 byte, otherwise its 4 bytes (a word)
  int writeback = (inst >> 21) & 1;     // whether to update base register after transfer, 
  int is_load = (inst >> 20) & 1;       // to know the operation, LDR or STR
  int rn = (inst >> 16) & 0xF; // base address register
  int rd = (inst >> 12) & 0xF; // data register



  uint32_t offset = 0;
  char offset_str[32] = "";

  if (is_reg_offset) {
    // register-based offset with optional shift
    int rm = inst & 0xF;
    int shift_type = (inst >> 5) & 0x3;
    int amount = (inst >> 7) & 0x1F;

    if (shift_type == 3 && amount == 0) {
      // RRX
      offset = (register_array[rm] >> 1) | (flag_CARRY << 31);
      sprintf(offset_str, "%sr%d, rrx", up ? "" : "-", rm);
    } else if (amount == 0) {
      offset = register_array[rm];
      sprintf(offset_str, "%s%s", up ? "" : "-", reg_name(rm));
    } else {
      offset = do_shift(register_array[rm], shift_type, amount);
      sprintf(offset_str, "%s%s, %s #%d", up ? "" : "-", reg_name(rm),
              shift_name(shift_type), amount);
    }
  } else {
    // immediate offset
    offset = inst & 0xFFF;
    if (offset != 0)
      sprintf(offset_str, "#%s%u", up ? "" : "-", offset);
  }

  // apply direction
  uint32_t applied_offset = up ? offset : -offset;

  // work out the actual address
  uint32_t base = register_array[rn];
  uint32_t addr = pre_index ? (base + applied_offset) : base;

  // build the disassembly string
  const char *cond_str = cond_to_str(cond);
  const char *b_str = byte_mode ? "b" : "";
  const char *op = is_load ? "ldr" : "str";

  if (pre_index) {
    if (strlen(offset_str) == 0)
      printf("%s%s%s %s, [%s]", op, cond_str, b_str, reg_name(rd),
             reg_name(rn));
    else if (writeback)
      printf("%s%s%s %s, [%s, %s]!", op, cond_str, b_str, reg_name(rd),
             reg_name(rn), offset_str);
    else
      printf("%s%s%s %s, [%s, %s]", op, cond_str, b_str, reg_name(rd),
             reg_name(rn), offset_str);
  } else {
    if (strlen(offset_str) == 0)
      printf("%s%s%s %s, [%s]", op, cond_str, b_str, reg_name(rd),
             reg_name(rn));
    else
      printf("%s%s%s %s, [%s], %s", op, cond_str, b_str, reg_name(rd),
             reg_name(rn), offset_str);
  }

  if (check_cond(cond)) {
    // convert memory INSTRUCTION address
    uint32_t INSTRUCTION_addr = addr / 4;

    if (INSTRUCTION_addr < MEMORY_SIZE) {
      if (is_load) {
        if (byte_mode)
          register_array[rd] =
              (ram_memory_array[INSTRUCTION_addr] >> ((addr % 4) * 8)) & 0xFF;
        else
          register_array[rd] = ram_memory_array[INSTRUCTION_addr];
      } else {
        if (byte_mode) {
          // write just the byte - keep other bytes
          int shift = (addr % 4) * 8;
          ram_memory_array[INSTRUCTION_addr] =
              (ram_memory_array[INSTRUCTION_addr] & ~(0xFF << shift)) |
              ((register_array[rd] & 0xFF) << shift);
        } else {
          ram_memory_array[INSTRUCTION_addr] = register_array[rd];
        }
      }
    }

    // writeback for post-index always happens, pre-index only if W bit set
    if (!pre_index || writeback) {
      register_array[rn] = base + applied_offset;
    }
  }
}

// handle software interrupt -just print it
void do_swi(INSTRUCTION inst, int cond) {
  uint32_t comment = inst & 0x00FFFFFF;
  const char *cond_str = cond_to_str(cond);
  printf("swi%s %u", cond_str, comment);
  // in a real emulator this would call an OS handler but we just printing here

}

// main decode function its the engine which figures out what kind of
// instruction it is and calls the right handler
void decode_and_run(INSTRUCTION inst) {
  // top 4 bits are the condition code
  int cond = (inst >> 28) & 0xF; //slide right 28, then keep only the bottom 4 bits., this is for the condition code
  int bits27_25 = (inst >> 25) & 0x7; //shift right 25, keep last 3 bits, this is for knowing the intruction type
  int bit4 = (inst >> 4) & 0x1; //shift right 4, keep last 1 bit, determines if its a multiply instruction or not
  int bit7 = (inst >> 7) & 0x1; // shift right 7, keep last 1 bit., also for multiply instruction

  // print the opcode first
  printf("  %08X    ", inst);

  // figure out the instruction type using the standard ARM encoding chart
  if (bits27_25 == 0b101) { //b101 means its branch intruction then do branch and they cost 3 cpu cycles 
    // branch instruction
    do_branch(inst, cond);
    total_cycles += 3; // branches cost 3 cycles

  } else if (bits27_25 == 0b111 && ((inst >> 24) & 1)) { // its a special intruction maybe used to exsit programs, 
    // SWI
    do_swi(inst, cond);
    total_cycles += 3;
  } else if (bits27_25 == 0b010 || bits27_25 == 0b011 || bits27_25 == 0b110 ||
             bits27_25 == 0b111) { //memory type of intructions  so do meme transfer 
    // data transfer (LDR/STR) with register or immediate offset
    do_mem_transfer(inst, cond);
    total_cycles += 3;
  } else if (bits27_25 == 0b000 || bits27_25 == 0b001) { // ALU or multiply intructioins 
    // data processing (ALU) or multiply - check bit 4 and 7 for multiply
    int is_multiply =
        (bits27_25 == 0 && bit7 == 1 && bit4 == 1 && ((inst >> 24) & 0xF) == 0);
    if (is_multiply) { // detects multiply
      // basic multiply - MUL
      //  print it at least
      int rd_m = (inst >> 16) & 0xF;
      int rs = (inst >> 8) & 0xF;
      int rm = inst & 0xF;
      printf("mul %s, %s, %s", reg_name(rd_m), reg_name(rm), reg_name(rs));
      if (check_cond(cond))
        register_array[rd_m] = register_array[rm] * register_array[rs];
      total_cycles += 2;
    } else { // if not any of these then do the normal operations like add, sub, and, orr, eor, tst, teq, cmp, cmn, mov, mvn etc.
      do_data_processing(inst, cond);
      total_cycles += 1;
    }
  } else {
    printf("??? (unrecognised instruction)");
  }
//a case of Take instruction → Look at key bits → Decide type → Run correct handler
  printf("\n");
}

// load instructions from a text file into memory starting at index 0
// the file format is one hex value per line like: 0xE3A00001
// lines starting with // are comments and are ignored
int load_from_file(const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    printf("Could not open file: %s\n", filename);
    return 0;
  }

  int count = 0;
  char line[256];

  while (fgets(line, sizeof(line), f)) {
    // skip lines that start with // (full line comments)
    char *trimmed = line;
    while (*trimmed == ' ' || *trimmed == '\t')
      trimmed++; // skip leading whitespace
    if (trimmed[0] == '/' && trimmed[1] == '/')
      continue;
    if (trimmed[0] == '\n' || trimmed[0] == '\r' || trimmed[0] == '\0')
      continue;

    // strip trailing semicolon if present 
    char *semi = strchr(line, ';');
    if (semi)
      *semi = ' '; // replace with space so sscanf stops there

    // strip inline comments  anything after the hex value on same line
    // we just read the first hex token and stop
    unsigned int val = 0;
    if (sscanf(trimmed, "0x%X", &val) == 1 ||
        sscanf(trimmed, "%X", &val) == 1) {
      if (count < MEMORY_SIZE) {
        ram_memory_array[count++] = val;
      }
    }
  }

  fclose(f);
  printf("Loaded %d instructions from %s\n\n", count, filename);
  return count;
}

// reset everything back to zero per instructions
void reset_state() {
  int i;
  for (i = 0; i < NUMBER_OF_REGISTERS; i++)
    register_array[i] = 0;
  STACK_POINTER = STACK_START_ADDRESS;
  flag_NEGATIVE = flag_ZERO = flag_CARRY = flag_OVERFLOW = 0;
  total_cycles = 0;
  PROGRAM_COUNTER = 0;
}

// entry point - optionally takes a filename to load instructions from,
// otherwise uses hardcoded test
int main(int num_args, char *arg_values[]) {

  // reset everything to a known state before starting
  reset_state();

  // if a filename is given, load instructions from there,
  if (num_args > 1) {
    int loaded = load_from_file(arg_values[1]);
    if (loaded == 0) {
      printf("No instructions loaded, exiting.\n");
      return 1;
    }
    // put a zero at the end to terminate the loop
    ram_memory_array[loaded] = 0;
  } else {
    // hardcoded test the basic example
    printf("No file given so im using hardcoded test instructions\n\n");
    ram_memory_array[0] = 0xE3A00001; // MOV r0,#1
    ram_memory_array[1] = 0xE3A01002; // MOV r1,#2
    ram_memory_array[2] = 0xE0802001; // ADD r2,r0,r1
    ram_memory_array[3] = 0xE2822005; // ADD r2,r2,#5
    ram_memory_array[4] = 0;          // end
  }

  printf("Op-Code       Assembly Mnemonic\n");
  printf("-\n");

  // main fetch-decode-execute loop
  int done = 0;
  while (!done) {
    INSTRUCTION instruct = ram_memory_array[PROGRAM_COUNTER / 4]; 
    PROGRAM_COUNTER += 4;   

    if (instruct == 0) {
      //end the program
      done = 1;
    } else {
      decode_and_run(instruct);
      print_registers();
      printf("\nPress <enter> for next instruction to run");
      getchar();
      printf("\n");
    }
  }

  printf("\nProgram end. Terminating.\n");
  printf("Total cycles: %lu\n", total_cycles);

  return 0;
}