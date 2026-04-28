# ARM CPU Emulator

A software-based implementation of an ARM processor written in C. This emulator can fetch, decode, and execute ARM machine code instructions — essentially simulating what a real ARM CPU chip does in hardware, but in software.

Built as part of a Software and Systems Development module at Leeds Beckett University.

---

## What it does

At its core this emulator does two things:

**Disassembler** — Takes raw ARM machine code (hex opcodes) and converts them into human readable assembly language. So `0xE3A00001` becomes `mov r0, #1`.

**Emulator** — Goes further than just translating. It actually executes those instructions, updating registers and status flags after each one — exactly like a real ARM processor would.

---

## How a CPU works (the short version)

Every CPU in the world runs a loop called the **fetch-decode-execute cycle**:

1. **Fetch** — grab the next instruction from memory
2. **Decode** — figure out what that instruction means
3. **Execute** — carry it out and update the CPU state

This emulator replicates that loop in software. The virtual CPU has 16 registers, 4KB of virtual RAM, and 4 status flags (N, Z, C, V) — all represented as variables in C.

---

## Features

- Full disassembler for all ARM data processing instructions
- Execution of all ALU instructions — ADD, SUB, MOV, CMP, AND, ORR, EOR, TST, TEQ, MVN, BIC, RSB, RSC, ADC, SBC
- Barrel shifter support — LSL, LSR, ASR, ROR, RRX
- Branch instructions — B and BL with correct offset calculation and link register saving
- Memory instructions — LDR and STR with all addressing modes including pre/post index, writeback, register offsets, shifted register offsets, and byte mode
- Full conditional execution — all 16 ARM condition codes (EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV)
- Status flag updates — N, Z, C, V set correctly after each operation
- Software interrupt (SWI) decoding
- Multiply instruction (MUL) support
- Instruction cycle counting using real ARM timing values
- File loading — reads instruction files directly, handles inline comments and semicolons
- Step-through execution — pauses after each instruction so you can inspect register state
- Hardcoded fallback — runs a basic test program if no file is provided

---

## Getting started

### Requirements

- GCC compiler
- Any Unix-like terminal (Mac, Linux, or WSL on Windows)

### Compile

```bash
gcc -o arm_emulator arm_emulator.c
```

### Run with a test file

```bash
./arm_emulator prog1.txt
```

### Run with hardcoded test instructions

```bash
./arm_emulator
```

Press **Enter** to step through each instruction one at a time.

---

## Test files

Five test files are included, each testing different parts of the instruction set:

| File | Instructions | What it tests |
|------|-------------|---------------|
| prog1.txt | 4 | Basic MOV and ADD — simplest possible test |
| prog2.txt | 21 | Full range of data processing instructions |
| prog3.txt | 9 | Barrel shifter — LSL, LSR, ASR, ROR, RRX |
| prog4.txt | 6 | Memory instructions — LDR and STR |
| prog5.txt | 49 | Everything — all instruction types including branches, conditionals, SWI |

---

## Example output

```
Loaded 4 instructions from prog1.txt

Op-Code       Assembly Mnemonic
-
  E3A00001    mov r0, #1
R0: 00000001  R1: 00000000  R2: 00000000  R3: 00000000
R4: 00000000  R5: 00000000  R6: 00000000  R7: 00000000
R8: 00000000  R9: 00000000  R10:00000000  R11:00000000
R12:00000000  R13:00010000  R14:00000000  R15:00000004
Flags: N=0 Z=0 C=0 V=0
Cycles so far: 1

Press <enter> for next instruction to run

  E3A01002    mov r1, #2
...

  E0802001    add r2, r0, r1
R0: 00000001  R1: 00000002  R2: 00000003  ...
Cycles so far: 3

  E2822005    add r2, r2, #5
R0: 00000001  R1: 00000002  R2: 00000008  ...
Cycles so far: 4

Program end. Terminating.
Total cycles: 4
```

---

## Code structure

```
arm_emulator.c
├── Type definitions         — INSTRUCTION, REGISTER, BYTE
├── Global state             — register_array, ram_memory_array, flags, PC, SP, LR
├── Condition codes          — 16 #define constants for all ARM conditions
├── check_cond()             — decides if an instruction should run or be skipped
├── cond_to_str()            — converts condition codes to text for printing
├── update_nz()              — updates N and Z flags
├── update_flags_add()       — updates all flags after addition
├── update_flags_sub()       — updates all flags after subtraction
├── do_shift()               — applies LSL, LSR, ASR, ROR barrel shifts
├── shift_name()             — converts shift type to string for printing
├── reg_name()               — converts register number to name (sp, lr, pc etc)
├── decode_operand2()        — extracts and computes the second operand
├── print_registers()        — prints all 16 registers, flags, and cycle count
├── reset_state()            — initialises all CPU state to zero
├── load_from_file()         — reads hex instructions from a text file into RAM
├── do_data_processing()     — executes all ALU instructions
├── do_branch()              — executes B and BL branch instructions
├── do_mem_transfer()        — executes LDR and STR memory instructions
├── do_swi()                 — handles software interrupt instructions
├── decode_and_run()         — main decoder, routes each instruction to the right handler
└── main()                   — entry point, runs the fetch-decode-execute loop
```

---

## How the decoding works

Every ARM instruction is exactly 32 bits — an 8 digit hex number. Different groups of bits mean different things:

```
Bits 31-28  →  Condition code (should this instruction run?)
Bits 27-25  →  Instruction type (branch? memory? ALU?)
Bits 24-20  →  Various flags (S bit, link bit, load/store etc)
Bits 19-16  →  First source register (rn)
Bits 15-12  →  Destination register (rd)
Bits 11-0   →  Second operand (immediate value or register with shift)
```

The `decode_and_run` function uses bit shifting (`>>`) and masking (`&`) to extract each field and route the instruction to the correct handler.

---

## Cycle counting

ARM instructions have different costs based on complexity:

| Instruction type | Cycles |
|-----------------|--------|
| ALU (ADD, SUB, MOV etc) | 1 |
| Multiply | 2 |
| Branch | 3 |
| Memory (LDR, STR) | 3 |
| Software interrupt | 3 |

Branches and memory cost more because on a real pipelined processor they cause stalls — branches flush the pipeline and memory access involves waiting for RAM.

---

## ARM condition codes

Every ARM instruction carries a 4-bit condition code in its top bits. The emulator checks these before executing anything:

| Code | Meaning | Flag checked |
|------|---------|-------------|
| EQ | Equal | Z = 1 |
| NE | Not equal | Z = 0 |
| CS | Carry set | C = 1 |
| CC | Carry clear | C = 0 |
| MI | Minus / negative | N = 1 |
| PL | Plus / positive | N = 0 |
| VS | Overflow | V = 1 |
| VC | No overflow | V = 0 |
| HI | Unsigned higher | C=1 and Z=0 |
| LS | Unsigned lower or same | C=0 or Z=1 |
| GE | Signed >= | N == V |
| LT | Signed < | N != V |
| GT | Signed > | Z=0 and N==V |
| LE | Signed <= | Z=1 or N!=V |
| AL | Always | — |
| NV | Never | — |

---

## Status flags

After each operation the CPU updates four status flags:

- **N (Negative)** — set when the result is negative (top bit is 1)
- **Z (Zero)** — set when the result is exactly zero
- **C (Carry)** — set when addition overflows 32 bits, or subtraction has no borrow
- **V (Overflow)** — set when signed arithmetic produces a result with the wrong sign

---

## What I learned building this

- How a real CPU works at the hardware level — registers, memory, the fetch-decode-execute cycle
- How ARM encodes instructions as 32-bit binary patterns and how to extract fields using bit operations
- Why conditional execution is powerful — every instruction in ARM can be conditionally skipped
- The difference between a disassembler (translating) and an emulator (actually running)
- How the barrel shifter allows ARM to do shifts and operations in a single instruction
- Why pipelined processors take more cycles for branches and memory operations

---

## Built with

- C
- GCC
- ARM Architecture Reference Manual (for instruction encoding specification)

---

## Author

Boluwatife Ajayi — [github.com/boluwatifeajayi](https://github.com/boluwatifeajayi) — [linkedin.com/in/bolu-ajayi](https://linkedin.com/in/bolu-ajayi)