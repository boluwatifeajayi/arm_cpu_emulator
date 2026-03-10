# ARM CPU Emulator

An **ARM instruction decoder / disassembler** that reads 32-bit ARM machine code (from memory or a text file), decodes each instruction using bitwise operations, and prints the equivalent assembly mnemonic.

**Module:** Software and Systems Development (CRN: 11550) — Component 1

---

## What it does

- **Decodes** ARM machine code into human-readable assembly
- **Supports:** Data processing (ALU), data transfer (LDR/STR), branch (B/BL), and SWI
- **Runs** with built-in test instructions or with instructions loaded from a file
- **Output:** Address, op-code (hex), and assembly mnemonic for each instruction

---

## Step 1 — Install GCC on your Mac

Open **Terminal** and run:

```bash
xcode-select --install
```

A popup will appear — click **Install**. This gives you GCC (and other dev tools). Verify:

```bash
gcc --version
```

You should see something like `Apple clang version 15...` — that’s fine; it works as a C compiler.

---

## Step 2 — Set up the project in Cursor

1. Open **Cursor**
2. **File → Open Folder** → open (or create) a folder named `arm_emulator`
3. Put these in that folder:
   - `emulator.c` — main source
   - `test_all.txt` — comprehensive test instructions
   - `prog1.txt` … `prog5.txt` — module test programs (if you have them)

---

## Step 3 — Compile and run

In Cursor’s terminal (**Ctrl+`**) or Mac Terminal, go into the project folder, then:

```bash
# Compile
gcc emulator.c -o emulator -Wall -Wextra -std=c11

# Run with hardcoded instructions (built-in test)
./emulator

# Run with a test file
./emulator test_all.txt

# Run with the module's provided files
./emulator prog1.txt
./emulator prog2.txt
./emulator prog3.txt
./emulator prog4.txt
./emulator prog5.txt
```

---

## Step 4 — What you’ll see

Example output:

```
Address      Op-Code       Assembly Mnemonic
--------------------------------------------------
00000000 : E3A00001   MOV r0, #1
00000004 : E3A01002   MOV r1, #2
00000008 : E0802001   ADD r2, r0, r1
0000000C : E2822005   ADD r2, r2, #5

[End of program]
```

- **Address** — byte address of the instruction (PC)
- **Op-Code** — 32-bit instruction in hex
- **Assembly Mnemonic** — decoded instruction (e.g. MOV, ADD, LDR, STR, B, BL, SWI)

---

## Input file format

Instruction files are plain text, one **32-bit hex** value per line. Comments are allowed.

- Use `0x` prefix or plain hex: `0xE3A00001` or `E3A00001`
- Lines starting with `//` or `#` are ignored
- Blank lines are ignored
- The emulator stops when it fetches a **zero** word (or end of loaded instructions)

Example (`prog1.txt` style):

```
0xE3A00001  // MOV r0, #1
0xE3A01002  // MOV r1, #2
0xE0802001  // ADD r2, r0, r1
0xE2822005  // ADD r2, r2, #5
```

---

## Supported instruction types

| Category        | Examples                          |
|----------------|------------------------------------|
| **Data processing (ALU)** | MOV, ADD, SUB, AND, ORR, EOR, CMP, TST, etc. |
| **Data transfer**        | LDR, STR (with immediate or register offset) |
| **Branch**               | B, BL                              |
| **Software interrupt**   | SWI                                |

Condition codes (EQ, NE, AL, etc.) and Operand2 forms (immediate, register, shifts) are decoded and shown in the assembly output.

---

## Project layout

```
arm_emulator/
├── emulator.c      # Source code
├── test_all.txt    # Full test suite (ALU, LDR/STR, branch, etc.)
├── prog1.txt       # Simple MOV/ADD test
├── prog2.txt       # (module test)
├── prog3.txt       # (module test)
├── prog4.txt       # Data transfer (LDR/STR) test
├── prog5.txt       # (module test)
└── README.md       # This file
```

---

## License

For educational use as part of the Software and Systems Development module.
