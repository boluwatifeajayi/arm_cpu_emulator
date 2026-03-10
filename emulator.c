/*
 * emulator.c
 *
 * ARM CPU Emulator - Component 1: Instruction Decoder / Disassembler
 *
 * This program reads 32-bit ARM machine code instructions (either hardcoded
 * or loaded from a text file), decodes each instruction into its component
 * parts using bitwise operations, and displays the equivalent Assembly mnemonic.
 *
 * Covers: Data processing (ALU), Data transfer (LDR/STR), Branch (B/BL), SWI
 *
 * Usage:
 *   ./emulator              -- runs with hardcoded test instructions
 *   ./emulator prog1.txt    -- loads instructions from a file
 *
 * Author: [Your Name]
 * Date:   [Today's Date]
 * Module: Software and Systems Development CRN:11550
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ===========================================================================
 * TYPE DEFINITIONS
 * Use platform-independent fixed-width types from <stdint.h> so the sizes
 * are guaranteed regardless of the platform this runs on.
 * =========================================================================== */

typedef uint32_t REGISTER;  /* CPU registers are 32 bits wide */
typedef uint32_t WORD;      /* A memory word is also 32 bits */
typedef uint8_t  BYTE;      /* A byte is 8 bits */

/* ===========================================================================
 * MACHINE STATE
 * These globals represent the internal state of the simulated CPU.
 * =========================================================================== */

BYTE     sr          = 0;         /* 8-bit Status Register (N Z C V I F S1 S0) */
REGISTER registers[16];           /* 16 general-purpose registers R0-R15 */
WORD     memory[1024];            /* Virtual RAM: 1024 words = 4KB */

/* ===========================================================================
 * CONDITION CODE CONSTANTS
 * Bits [31:28] of every ARM instruction hold a condition code.
 * The CPU checks the status register flags against this code before executing.
 * =========================================================================== */

#define COND_CODE_POS  28    /* Bit position of the condition code field */

#define CC_EQ  0x0   /* Equal:            Z set */
#define CC_NE  0x1   /* Not Equal:        Z clear */
#define CC_CS  0x2   /* Carry Set:        C set */
#define CC_CC  0x3   /* Carry Clear:      C clear */
#define CC_MI  0x4   /* Minus/Negative:   N set */
#define CC_PL  0x5   /* Plus/Positive:    N clear */
#define CC_VS  0x6   /* Overflow Set:     V set */
#define CC_VC  0x7   /* Overflow Clear:   V clear */
#define CC_HI  0x8   /* Higher:           C set AND Z clear */
#define CC_LS  0x9   /* Lower or Same:    C clear OR Z set */
#define CC_GE  0xa   /* Greater or Equal: N==V */
#define CC_LT  0xb   /* Less Than:        N!=V */
#define CC_GT  0xc   /* Greater Than:     Z clear AND N==V */
#define CC_LE  0xd   /* Less or Equal:    Z set OR N!=V */
#define CC_AL  0xe   /* Always:           (unconditional) */
#define CC_NV  0xf   /* Never:            never executes */

/* Human-readable labels for each condition code, indexed by value 0-15.
 * AL is shown as empty string (conventional to omit it in assembly output). */
char *condition_labels[] = {
    "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
    "HI", "LS", "GE", "LT", "GT", "LE", "",   "NV"
};

/* ===========================================================================
 * STATUS REGISTER FLAG CONSTANTS
 * The 8-bit SR has flags in bits 7-4. We use bitmasks to test/set them.
 * =========================================================================== */

#define STAT_N  (1<<7)  /* Negative flag: set if result was negative */
#define STAT_Z  (1<<6)  /* Zero flag:     set if result was zero */
#define STAT_C  (1<<5)  /* Carry flag:    set if operation produced a carry */
#define STAT_V  (1<<4)  /* Overflow flag: set if signed overflow occurred */

/* ===========================================================================
 * DATA PROCESSING (ALU) INSTRUCTION CONSTANTS
 * Layout: | Cond[31:28] | 00 | I[25] | OpCode[24:21] | S[20] | Rn[19:16] | Rd[15:12] | Op2[11:0] |
 * =========================================================================== */

#define ALU_OP_CODE_POS   21        /* Bit position of the ALU opcode field */
#define ALU_OP_CODE_MASK  0xf       /* 4-bit mask to extract the opcode */
#define ALU_REG_N_POS     16        /* Bit position of the Rn (first operand) register field */
#define ALU_REG_N_MASK    0xf       /* 4-bit mask */
#define ALU_REG_DEST_POS  12        /* Bit position of the Rd (destination) register field */
#define ALU_REG_DEST_MASK 0xf       /* 4-bit mask */

#define ALU_S_BIT (1<<20)  /* Set Status bit: if set, instruction updates the SR flags */
#define ALU_I_BIT (1<<25)  /* Immediate bit:  if set, Operand2 is an immediate value */

/* ALU opcodes (bits 24:21) — identify which data-processing instruction to perform */
#define AND  0   /* Rd = Rn & Op2 */
#define EOR  1   /* Rd = Rn ^ Op2 */
#define SUB  2   /* Rd = Rn - Op2 */
#define RSB  3   /* Rd = Op2 - Rn */
#define ADD  4   /* Rd = Rn + Op2 */
#define ADC  5   /* Rd = Rn + Op2 + Carry */
#define SBC  6   /* Rd = Rn - Op2 - !Carry */
#define RSC  7   /* Rd = Op2 - Rn - !Carry */
#define TST  8   /* flags from Rn & Op2  (no result stored) */
#define TEQ  9   /* flags from Rn ^ Op2  (no result stored) */
#define CMP  10  /* flags from Rn - Op2  (no result stored) */
#define CMN  11  /* flags from Rn + Op2  (no result stored) */
#define ORR  12  /* Rd = Rn | Op2 */
#define MOV  13  /* Rd = Op2 (Rn ignored) */
#define BIC  14  /* Rd = Rn & ~Op2 */
#define MVN  15  /* Rd = ~Op2 (Rn ignored) */

/* Display labels for ALU opcodes, indexed 0-15 */
char *alu_op_labels[] = {
    "AND", "EOR", "SUB", "RSB", "ADD", "ADC", "SBC", "RSC",
    "TST", "TEQ", "CMP", "CMN", "ORR", "MOV", "BIC", "MVN"
};

/* ===========================================================================
 * OPERAND 2 CONSTANTS
 * Op2 occupies bits [11:0] and its meaning depends on the I (immediate) bit.
 *
 * When I=1 (immediate):
 *   bits[11:8] = rotate amount (×2), bits[7:0] = 8-bit immediate value
 *
 * When I=0 (register):
 *   bits[3:0]  = register number
 *   bits[6:5]  = shift type (LSL/LSR/ASR/ROR)
 *   bits[11:7] = shift amount (or register if bit4=1)
 * =========================================================================== */

#define OP2_I_VALUE_MASK      0xff   /* 8-bit immediate value mask */
#define OP2_I_SHIFT_POS       8      /* Position of rotate amount in immediate form */
#define OP2_I_SHIFT_MASK      0xf    /* 4-bit rotate amount mask */

#define OP2_REG_VALUE_MASK    0xf    /* Mask to extract the register number */
#define OP2_SHIFT_AMOUNT_POS  7      /* Position of the shift amount */
#define OP2_SHIFT_AMOUNT_MASK 0x1f   /* 5-bit shift amount mask */
#define OP2_SHIFT_TYPE_POS    5      /* Position of shift type bits */
#define OP2_SHIFT_TYPE_MASK   0x3    /* 2-bit shift type mask */
#define OP2_REG_SHIFT_POS     8      /* Position of register-controlled shift register number */
#define OP2_REG_SHIFT_MASK    0xf    /* Mask for register-controlled shift */

/* Shift type values */
#define SHIFT_LSL  0x0   /* Logical Shift Left */
#define SHIFT_LSR  0x1   /* Logical Shift Right */
#define SHIFT_ASR  0x2   /* Arithmetic Shift Right */
#define SHIFT_ROR  0x3   /* Rotate Right */
#define SHIFT_RRX  0x4   /* Rotate Right with Extend (special case: shift amount = 0, type = ROR) */

char *shift_labels[] = { "LSL", "LSR", "ASR", "ROR", "RRX" };

/* ===========================================================================
 * DATA TRANSFER (LDR / STR) CONSTANTS
 * Layout: | Cond[31:28] | 01 | I[25] | P[24] | U[23] | B[22] | W[21] | L[20] | Rn[19:16] | Rd[15:12] | Offset[11:0] |
 * =========================================================================== */

#define DT_OFFSET_MASK  0xfff    /* 12-bit offset mask */
#define DT_L_BIT (1<<20)         /* Load/Store: 1=LDR (load), 0=STR (store) */
#define DT_W_BIT (1<<21)         /* Write-back: 1=write updated address back to Rn */
#define DT_B_BIT (1<<22)         /* Byte/Word:  1=byte transfer, 0=word transfer */
#define DT_U_BIT (1<<23)         /* Up/Down:    1=add offset, 0=subtract offset */
#define DT_P_BIT (1<<24)         /* Pre/Post:   1=pre-index (offset before), 0=post-index */
#define DT_I_BIT (1<<25)         /* Immediate:  1=offset is a shifted register, 0=immediate */

/* ===========================================================================
 * BRANCH INSTRUCTION CONSTANTS
 * Layout: | Cond[31:28] | 101 | L[24] | Offset[23:0] |
 * =========================================================================== */

#define BRANCH_OFFSET_MASK  0xffffff   /* 24-bit signed offset mask */
#define BRANCH_NEG_MASK     0xff000000 /* Used to sign-extend a negative offset */
#define BRANCH_N_BIT        (1<<23)    /* If set, offset is negative */
#define BRANCH_L_BIT        (1<<24)    /* Link bit: 1=BL (save return addr to R14), 0=B */

/* ===========================================================================
 * SWI (SOFTWARE INTERRUPT) CONSTANTS
 * Layout: | Cond[31:28] | 1111 | SWI_code[23:0] |
 * =========================================================================== */

#define SWI_CODE_MASK     0xffffff  /* 24-bit interrupt code mask */

/* ===========================================================================
 * INSTRUCTION TYPE DETECTION BIT MASKS
 * Bits 27, 26, 25 identify the broad category of instruction.
 * =========================================================================== */

#define BIT_4  (1<<4)
#define BIT_20 (1<<20)
#define BIT_24 (1<<24)
#define BIT_25 (1<<25)
#define BIT_26 (1<<26)
#define BIT_27 (1<<27)
#define BIT_31 (1<<31)

/* ===========================================================================
 * STATUS REGISTER HELPER FUNCTIONS
 * =========================================================================== */

/* Returns 1 if at least one of the given flag bits is set in the SR */
int isSet(int flag)   { return (sr & flag) ? 1 : 0; }

/* Returns 1 if all of the given flag bits are clear in the SR */
int isClear(int flag) { return (sr & flag) ? 0 : 1; }

/* Sets specified flag bit(s) in the SR */
void setFlag(int flag)  { sr = sr | flag; }

/* Clears specified flag bit(s) in the SR */
void clearFlag(int flag) { sr = sr & (~flag); }

/* Sets or clears a flag depending on the 'set' parameter (non-zero = set) */
void updateFlag(int flag, int set) {
    if (set) sr = sr | flag;
    else     sr = sr & (~flag);
}

/* ===========================================================================
 * REGISTER NAME HELPER
 * ARM has conventional names for some registers (sp, lr, pc).
 * Returns the conventional name string for registers 13, 14, 15,
 * or "rN" for registers 0-12. Also handles "sl" (r10) and "fp" (r11).
 * =========================================================================== */
const char* regName(int r) {
    /* Static array of register name strings */
    static const char *names[] = {
        "r0","r1","r2","r3","r4","r5","r6","r7",
        "r8","r9","sl","fp","ip","sp","lr","pc"
    };
    if (r >= 0 && r < 16) return names[r];
    return "??";
}

/* ===========================================================================
 * OPERAND 2 DECODER
 * Decodes Operand 2 and writes a human-readable string into 'buf'.
 *
 * Two forms:
 *   Immediate (I=1): rotate a small 8-bit value left by (rotate*2) bits
 *   Register  (I=0): a register optionally shifted by an amount or another register
 * =========================================================================== */
void decodeOperand2(WORD inst, int isBit, char *buf) {

    if (isBit) {
        /* --- IMMEDIATE FORM ---
         * bits[7:0]  = 8-bit immediate value
         * bits[11:8] = rotate amount (rotated RIGHT by rotate*2 positions) */
        int imm    = inst & OP2_I_VALUE_MASK;
        int rotate = (inst >> OP2_I_SHIFT_POS) & OP2_I_SHIFT_MASK;

        /* Apply the rotate: rotate RIGHT by rotate*2 bits (circular) */
        int shift_amount = rotate * 2;
        uint32_t value;
        if (shift_amount == 0) {
            value = imm;
        } else {
            /* ROR: bits shifted off the right wrap to the top */
            value = (imm >> shift_amount) | (imm << (32 - shift_amount));
        }
        sprintf(buf, "#%d", value);

    } else {
        /* --- REGISTER FORM ---
         * bits[3:0]  = source register number
         * bits[6:5]  = shift type
         * bit[4]     = 0 means shift by immediate amount, 1 means shift by register */
        int regNum    = inst & OP2_REG_VALUE_MASK;
        int shiftType = (inst >> OP2_SHIFT_TYPE_POS) & OP2_SHIFT_TYPE_MASK;

        if (inst & BIT_4) {
            /* Shift amount is stored in another register (bits[11:8]) */
            int shiftReg = (inst >> OP2_REG_SHIFT_POS) & OP2_REG_SHIFT_MASK;
            sprintf(buf, "%s, %s %s", regName(regNum), shift_labels[shiftType], regName(shiftReg));
        } else {
            /* Shift amount is an immediate 5-bit value in bits[11:7] */
            int shiftAmt = (inst >> OP2_SHIFT_AMOUNT_POS) & OP2_SHIFT_AMOUNT_MASK;

            if (shiftAmt == 0 && shiftType == SHIFT_ROR) {
                /* Special case: ROR with shift amount 0 means RRX (rotate right with extend) */
                sprintf(buf, "%s, RRX", regName(regNum));
            } else if (shiftAmt == 0) {
                /* No shift: just output the register name alone */
                sprintf(buf, "%s", regName(regNum));
            } else {
                sprintf(buf, "%s, %s #%d", regName(regNum), shift_labels[shiftType], shiftAmt);
            }
        }
    }
}

/* ===========================================================================
 * DATA PROCESSING (ALU) INSTRUCTION DECODER
 * Extracts and prints the mnemonic for instructions in the format:
 *   MNEMONIC{cond} Rd, Rn, Op2    (most instructions)
 *   MNEMONIC{cond} Rd, Op2        (MOV, MVN — no Rn)
 *   MNEMONIC{cond} Rn, Op2        (TST, TEQ, CMP, CMN — no Rd, result not stored)
 * =========================================================================== */
void decodeALU(WORD inst) {
    /* Extract each field using bit shifts and masks */
    int opcode  = (inst >> ALU_OP_CODE_POS)  & ALU_OP_CODE_MASK;
    int regN    = (inst >> ALU_REG_N_POS)    & ALU_REG_N_MASK;
    int regDest = (inst >> ALU_REG_DEST_POS) & ALU_REG_DEST_MASK;
    int iBit    = (inst & ALU_I_BIT) ? 1 : 0;   /* 1 if Op2 is immediate */
    int sBit    = (inst & ALU_S_BIT) ? 1 : 0;   /* 1 if instruction sets SR flags */
    int condCode = inst >> COND_CODE_POS;

    /* Decode the Operand 2 field into a readable string */
    char op2str[64];
    decodeOperand2(inst & 0xfff, iBit, op2str);

    /* Build the condition suffix (e.g. "EQ", "NE", "" for AL) */
    const char *cond = condition_labels[condCode];

    /* Build the S suffix ("S" if the S bit is set, meaning flags get updated) */
    const char *s = sBit ? "S" : "";

    /* Print the mnemonic based on opcode type:
     *
     * MOV/MVN: only use Rd and Op2 (Rn is always 0000 in the encoding)
     * TST/TEQ/CMP/CMN: only Rn and Op2 (Rd is 0000, no destination register)
     * All others: use Rd, Rn, Op2
     */
    switch (opcode) {
        case MOV:
        case MVN:
            /* No Rn for MOV/MVN */
            printf("%s%s%s %s, %s",
                alu_op_labels[opcode], cond, s,
                regName(regDest), op2str);
            break;

        case TST:
        case TEQ:
        case CMP:
        case CMN:
            /* No Rd for comparison instructions — result only affects flags */
            printf("%s%s %s, %s",
                alu_op_labels[opcode], cond,
                regName(regN), op2str);
            break;

        default:
            /* Standard 3-operand form: Rd = Rn op Op2 */
            printf("%s%s%s %s, %s, %s",
                alu_op_labels[opcode], cond, s,
                regName(regDest), regName(regN), op2str);
            break;
    }
}

/* ===========================================================================
 * DATA TRANSFER (LDR / STR) INSTRUCTION DECODER
 * Extracts and prints the mnemonic for single-register memory access.
 *
 * Addressing modes:
 *   [Rn]           — simple, no offset
 *   [Rn, #offset]  — pre-indexed immediate offset
 *   [Rn, Rm]       — pre-indexed register offset
 *   [Rn, #offset]! — pre-indexed with write-back
 *   [Rn], #offset  — post-indexed immediate
 *   [Rn], Rm       — post-indexed register
 * =========================================================================== */
void decodeDT(WORD inst) {
    int condCode = inst >> COND_CODE_POS;
    int regN     = (inst >> ALU_REG_N_POS)    & ALU_REG_N_MASK;    /* Base register */
    int regDest  = (inst >> ALU_REG_DEST_POS) & ALU_REG_DEST_MASK; /* Src/Dst register */

    /* Extract control bits */
    int lBit = (inst & DT_L_BIT) ? 1 : 0;  /* 1=LDR (load), 0=STR (store) */
    int wBit = (inst & DT_W_BIT) ? 1 : 0;  /* 1=write updated address back to Rn */
    int bBit = (inst & DT_B_BIT) ? 1 : 0;  /* 1=byte transfer */
    int uBit = (inst & DT_U_BIT) ? 1 : 0;  /* 1=add offset, 0=subtract */
    int pBit = (inst & DT_P_BIT) ? 1 : 0;  /* 1=pre-index, 0=post-index */
    int iBit = (inst & DT_I_BIT) ? 1 : 0;  /* 1=offset is shifted register */

    const char *cond = condition_labels[condCode];
    const char *mnem = lBit ? "LDR" : "STR";
    const char *bsuf = bBit ? "B"   : "";   /* "B" suffix for byte transfers */
    const char *sign = uBit ? ""    : "-";  /* "-" prefix if subtracting offset */

    /* Decode the offset/operand field (bits 11:0) */
    char offsetStr[64];
    if (iBit) {
        /* Offset is a (possibly shifted) register — reuse Op2 decoder on bits[11:0] */
        decodeOperand2(inst & 0xfff, 0, offsetStr);
    } else {
        /* Offset is a 12-bit immediate value */
        int offset = inst & DT_OFFSET_MASK;
        if (offset == 0)
            offsetStr[0] = '\0';   /* No offset to show */
        else
            sprintf(offsetStr, "#%s%d", sign, offset);
    }

    /* Print the full mnemonic with addressing mode */
    if (pBit) {
        /* Pre-indexed: address calculated before transfer */
        if (strlen(offsetStr) == 0) {
            /* No offset: [Rn] */
            printf("%s%s%s %s, [%s]",
                mnem, cond, bsuf, regName(regDest), regName(regN));
        } else if (wBit) {
            /* Pre-indexed with write-back: [Rn, offset]! */
            printf("%s%s%s %s, [%s, %s]!",
                mnem, cond, bsuf, regName(regDest), regName(regN),
                iBit ? offsetStr : offsetStr);
        } else {
            /* Pre-indexed no write-back: [Rn, offset] */
            printf("%s%s%s %s, [%s, %s]",
                mnem, cond, bsuf, regName(regDest), regName(regN), offsetStr);
        }
    } else {
        /* Post-indexed: transfer happens first, then address updated */
        if (strlen(offsetStr) == 0) {
            printf("%s%s%s %s, [%s]",
                mnem, cond, bsuf, regName(regDest), regName(regN));
        } else {
            printf("%s%s%s %s, [%s], %s",
                mnem, cond, bsuf, regName(regDest), regName(regN), offsetStr);
        }
    }
}

/* ===========================================================================
 * BRANCH (B / BL) INSTRUCTION DECODER
 * Layout: | Cond[31:28] | 101 | L[24] | Offset[23:0] |
 *
 * The 24-bit offset is a signed two's complement value in WORDS.
 * Actual byte offset = (offset << 2) + 8 (ARM pipeline prefetch adds 8).
 * =========================================================================== */
void decodeBranch(WORD inst) {
    int condCode = inst >> COND_CODE_POS;
    int lBit     = (inst & BRANCH_L_BIT) ? 1 : 0;  /* 1=BL (branch with link) */

    /* Extract the 24-bit offset and sign-extend it to 32 bits */
    int offset = inst & BRANCH_OFFSET_MASK;
    if (inst & BRANCH_N_BIT) {
        /* Bit 23 set means negative — fill upper 8 bits with 1s to sign extend */
        offset |= BRANCH_NEG_MASK;
    }

    /* Convert word offset to byte offset, accounting for ARM pipeline (+8) */
    int byteOffset = (offset << 2) + 8;

    const char *cond = condition_labels[condCode];
    const char *mnem = lBit ? "BL" : "B";

    printf("%s%s %d", mnem, cond, byteOffset);
}

/* ===========================================================================
 * SWI (SOFTWARE INTERRUPT) INSTRUCTION DECODER
 * Layout: | Cond[31:28] | 1111 | Code[23:0] |
 * =========================================================================== */
void decodeSWI(WORD inst) {
    int condCode  = inst >> COND_CODE_POS;
    int swiCode   = inst & SWI_CODE_MASK;  /* 24-bit interrupt code */
    const char *cond = condition_labels[condCode];
    printf("SWI%s %d", cond, swiCode);
}

/* ===========================================================================
 * MAIN DECODE DISPATCHER
 * Called once per instruction. Uses bits [27:26] (and sometimes [25] and [4])
 * to determine the broad instruction category, then dispatches to the
 * appropriate decoder function.
 *
 * Category detection (bits 27:26):
 *   00 = Data processing (ALU)
 *   01 = Data transfer (LDR/STR)
 *   10 = Branch (B/BL)
 *   11 = SWI
 * =========================================================================== */
void decodeInstruction(WORD inst, WORD address) {

    /* Print the address and raw op-code hex before the mnemonic */
    printf("%08X : %08X   ", address, inst);

    /* Extract bits 27 and 26 to determine instruction category */
    int bit27 = (inst & BIT_27) ? 1 : 0;
    int bit26 = (inst & BIT_26) ? 1 : 0;

    if (!bit27 && !bit26) {
        /* -------------------------------------------------------
         * Bits 27:26 = 00 → Data Processing (ALU) instruction
         * But we must distinguish from multiply instructions (bit4=1, bit7=1)
         * For this assignment we only need to handle standard ALU instructions.
         * ------------------------------------------------------- */
        decodeALU(inst);

    } else if (!bit27 && bit26) {
        /* -------------------------------------------------------
         * Bits 27:26 = 01 → Single Data Transfer (LDR / STR)
         * ------------------------------------------------------- */
        decodeDT(inst);

    } else if (bit27 && !bit26) {
        /* -------------------------------------------------------
         * Bits 27:26 = 10 → Branch (B or BL)
         * Confirm bit 25 is also set (101 = branch encoding)
         * ------------------------------------------------------- */
        if (inst & BIT_25) {
            decodeBranch(inst);
        } else {
            printf("[Unknown instruction: %08X]", inst);
        }

    } else {
        /* -------------------------------------------------------
         * Bits 27:26 = 11 → SWI (Software Interrupt)
         * The full encoding for SWI is bits 27:24 = 1111
         * ------------------------------------------------------- */
        decodeSWI(inst);
    }

    printf("\n");
}

/* ===========================================================================
 * FILE LOADER
 * Reads ARM instructions from a text file.
 * Each line should contain a hex number (with or without "0x" prefix),
 * optionally followed by a comment. Example:
 *
 *   0xe3a00001  // MOV r0, #1
 *   0xe3a01002  // MOV r1, #2
 *
 * Valid instructions are stored in the global memory[] array starting at index 0.
 * Returns the number of instructions loaded, or 0 on error.
 * =========================================================================== */
int parseFile(char *path) {
    FILE *fp = fopen(path, "r");

    if (fp != NULL) {
        const unsigned MAX_LENGTH = 256;
        char buffer[MAX_LENGTH];
        size_t n = 0;

        printf("Loading file: %s\n\n", path);

        while (fgets(buffer, MAX_LENGTH, fp)) {
            char *end_ptr;

            /* strtol with base 16 parses both "0xe3a00001" and "e3a00001" */
            long num = strtol(buffer, &end_ptr, 16);

            /* Only store values that fit in a 32-bit unsigned word */
            if (num > 0 && num <= UINT32_MAX) {
                memory[n++] = (WORD)num;
            } else if (num != 0) {
                fprintf(stderr, "Ignoring out-of-range value: %lX\n", num);
            }
        }

        /* Terminate the instruction list with a zero word */
        memory[n++] = 0;

        fclose(fp);

        printf("%zu instruction(s) loaded.\n\n", n - 1);
        return (int)n;
    }

    perror("Could not open file");
    return 0;
}

/* ===========================================================================
 * MAIN ENTRY POINT
 * Sets up the virtual machine state, loads or hardcodes instructions,
 * then runs the fetch-decode loop printing each instruction.
 * =========================================================================== */
int main(int argc, char *argv[]) {

    /* --- Initialise all registers to 0 --- */
    for (int i = 0; i < 16; i++)
        registers[i] = 0;

    /* R13 is conventionally the stack pointer — set to top of our virtual RAM.
     * 1024 words × 4 bytes/word = 4096 bytes = 0x1000 */
    registers[13] = 0x1000;

    /* --- Load instructions into virtual memory --- */
    if (argc > 1) {
        /* A filename was provided as a command-line argument — load from file */
        if (parseFile(argv[1]) == 0) {
            fprintf(stderr, "Failed to load instructions from file.\n");
            return EXIT_FAILURE;
        }
    } else {
        /* No file provided — use hardcoded test instructions */
        int n = 0;
        memory[n++] = 0xE3A00001;  /* MOV r0, #1   */
        memory[n++] = 0xE3A01002;  /* MOV r1, #2   */
        memory[n++] = 0xE0802001;  /* ADD r2, r0, r1 */
        memory[n++] = 0xE2822005;  /* ADD r2, r2, #5 */
        memory[n++] = 0x0;         /* Sentinel: marks end of program */
    }

    /* --- Print header --- */
    printf("%-12s %-12s  %s\n", "Address", "Op-Code", "Assembly Mnemonic");
    printf("--------------------------------------------------\n");

    /* --- Fetch-decode loop ---
     * The PC (R15) starts at 0 and advances by 4 bytes per instruction.
     * We divide the PC by 4 to get a word index into memory[]. */
    int done = 0;

    while (!done) {
        WORD address = registers[15];       /* Current instruction address (byte address) */
        WORD inst    = memory[address >> 2]; /* Fetch: divide by 4 to get word index */
        registers[15] += 4;                  /* Advance PC to next instruction */

        if (inst != 0) {
            decodeInstruction(inst, address);
        } else {
            printf("\n[End of program]\n");
            done = 1;
        }
    }

    return EXIT_SUCCESS;
}
