# AngelScript Bytecode Complete Reference Guide

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [Bytecode Binary Format](#bytecode-binary-format)
3. [Instruction Type System](#instruction-type-system)
4. [Complete Instruction Reference](#complete-instruction-reference)
5. [VM Register Model & Execution](#vm-register-model--execution)
6. [Accessing Bytecode via API](#accessing-bytecode-via-api)
7. [Writing a Bytecode Disassembler](#writing-a-bytecode-disassembler)
8. [JIT Compiler Interface](#jit-compiler-interface)
9. [Debug Output](#debug-output)
10. [Meta / Temporary Instructions](#meta--temporary-instructions)

---

## Architecture Overview

AngelScript compiles script source into a **stack-based virtual machine bytecode**. The VM has:
- A **program pointer** (`l_bc`) pointing to the current bytecode instruction
- A **stack pointer** (`l_sp`) for the operand stack
- A **frame pointer** (`l_fp`) pointing to the current function's local variable space
- A **value register** (`m_regs.valueRegister`) — a 64-bit register for integer/float intermediates
- An **object register** (`m_regs.objectRegister`) — a pointer for object and handle intermediates
- An **object type** (`m_regs.objectType`) — tracks the type in the object register

The bytecode is composed of **DWORD-aligned instructions**. Each instruction is 1-4 DWORDs (4-16 bytes on 32-bit, 4-20 bytes on 64-bit with 8-byte pointers).

---

## Bytecode Binary Format

The bytecode is stored as a flat array of `asDWORD` values. Instructions are laid out sequentially.

### Instruction Layout

```
Byte 0:      opcode (asBYTE = asEBCInstr enum value)
Byte 1:      always 0 (padding)
Bytes 2-N:   arguments (varies by instruction type)
```

### Argument Access Macros (from `angelscript.h`)

```c
#define asBC_DWORDARG(x)  (*(((asDWORD*)x)+1))    // 2nd DWORD: 32-bit unsigned
#define asBC_INTARG(x)    (*(int*)(((asDWORD*)x)+1)) // 2nd DWORD: 32-bit signed
#define asBC_QWORDARG(x)  (*(asQWORD*)(((asDWORD*)x)+1)) // 2nd-3rd DWORDs: 64-bit
#define asBC_FLOATARG(x)  (*(float*)(((asDWORD*)x)+1))   // 2nd DWORD: float
#define asBC_PTRARG(x)    (*(asPWORD*)(((asDWORD*)x)+1)) // 2nd DWORD(s): pointer-sized

#define asBC_WORDARG0(x)  (*(((asWORD*)x)+1))   // 16-bit arg0
#define asBC_WORDARG1(x)  (*(((asWORD*)x)+2))   // 16-bit arg1
#define asBC_SWORDARG0(x) (*(((short*)x)+1))    // signed 16-bit arg0
#define asBC_SWORDARG1(x) (*(((short*)x)+2))    // signed 16-bit arg1
#define asBC_SWORDARG2(x) (*(((short*)x)+3))    // signed 16-bit arg2
```

### Key Type Sizes

```c
#define AS_PTR_SIZE 1    // 32-bit: pointer = 1 DWORD, 64-bit: pointer = 2 DWORDs
#define BYTECODE_SIZE 4  // opcode + padding = 4 bytes (1 DWORD header)
#define MAX_DATA_SIZE 8  // max additional data = 8 bytes (2 DWORDs)
#define MAX_INSTR_SIZE 12 // 4 + 8 = 12 bytes max
```

---

## Instruction Type System

Each instruction belongs to one of 21 types (`asEBCType`), defining its argument layout:

| Type | Value | DWORDs | Arg Layout |
|------|-------|--------|------------|
| `asBCTYPE_INFO` | 0 | 0 | No code emitted (debug info only) |
| `asBCTYPE_NO_ARG` | 1 | 1 | Opcode only, zero padding |
| `asBCTYPE_W_ARG` | 2 | 1 | 16-bit argument in word[0] |
| `asBCTYPE_wW_ARG` | 3 | 1 | 16-bit variable offset in word[0] |
| `asBCTYPE_DW_ARG` | 4 | 2 | 32-bit argument in DWORD[1] |
| `asBCTYPE_rW_DW_ARG` | 5 | 2 | 16-bit variable offset + 32-bit argument |
| `asBCTYPE_QW_ARG` | 6 | 3 | 64-bit argument in DWORD[1..2] |
| `asBCTYPE_DW_DW_ARG` | 7 | 3 | Two 32-bit arguments in DWORD[1] and DWORD[2] |
| `asBCTYPE_wW_rW_rW_ARG` | 8 | 2 | Three 16-bit variable offsets |
| `asBCTYPE_wW_QW_ARG` | 9 | 3 | 16-bit variable offset + 64-bit argument |
| `asBCTYPE_wW_rW_ARG` | 10 | 2 | Two 16-bit variable offsets |
| `asBCTYPE_rW_ARG` | 11 | 1 | 16-bit variable offset (signed) |
| `asBCTYPE_wW_DW_ARG` | 12 | 2 | 16-bit variable offset + 32-bit argument |
| `asBCTYPE_wW_rW_DW_ARG` | 13 | 3 | Two 16-bit offsets + 32-bit argument |
| `asBCTYPE_rW_rW_ARG` | 14 | 2 | Two 16-bit variable offsets (both rW) |
| `asBCTYPE_wW_W_ARG` | 15 | 2 | 16-bit variable offset + 16-bit word |
| `asBCTYPE_QW_DW_ARG` | 16 | 4 | 64-bit argument + 32-bit argument |
| `asBCTYPE_rW_QW_ARG` | 17 | 3 | 16-bit variable offset + 64-bit argument |
| `asBCTYPE_W_DW_ARG` | 18 | 2 | 16-bit word + 32-bit argument |
| `asBCTYPE_rW_W_DW_ARG` | 19 | 3 | 16-bit offset + 16-bit word + 32-bit argument |
| `asBCTYPE_rW_DW_DW_ARG` | 20 | 3 | 16-bit offset + two 32-bit arguments |

**Pointer-adaptive types:** On 32-bit, `asBCTYPE_PTR_ARG = asBCTYPE_DW_ARG`; on 64-bit, `asBCTYPE_PTR_ARG = asBCTYPE_QW_ARG`. Same mapping applies to `asBCTYPE_PTR_DW_ARG`, `asBCTYPE_wW_PTR_ARG`, `asBCTYPE_rW_PTR_ARG`.

---

## Complete Instruction Reference

Instructions 0-201 are "real" instructions emitted to final bytecode. Instructions 250-255 are temporary/compiler-only.

### Stack Operations (0-5, 47, 48, 58, 59, 73)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 0 | `PopPtr` | NO_ARG | — | **-1 ptr** | Pop a pointer-sized value off the stack |
| 1 | `PshGPtr` | PTR_ARG | ptr: globalAddr | **+1 ptr** | Push a pointer to a global variable (combined PGA + RDSPtr) |
| 2 | `PshC4` | DW_ARG | val: 32-bit const | **+1 dword** | Push a 32-bit constant onto the stack |
| 3 | `PshV4` | rW_ARG | v: varOffset | **+1 dword** | Push the 32-bit value of a variable onto the stack |
| 4 | `PSF` | rW_ARG | v: varOffset | **+1 ptr** | Push the **address** of a variable onto the stack (Push Stack Frame offset) |
| 5 | `SwapPtr` | NO_ARG | — | 0 | Swap the top two pointer-sized values on the stack |
| 47 | `PshC8` | QW_ARG | val: 64-bit const | **+2 dwords** | Push a 64-bit constant onto the stack |
| 48 | `PshVPtr` | rW_ARG | v: varOffset | **+1 ptr** | Push a pointer-sized variable value onto the stack |
| 58 | `PopRPtr` | NO_ARG | — | **-1 ptr** | Pop a pointer from the register stack (pop into object register) |
| 59 | `PshRPtr` | NO_ARG | — | **+1 ptr** | Push the object register onto the stack |
| 73 | `PshNull` | NO_ARG | — | **+1 ptr** | Push a null handle (zero pointer) onto the stack |

### Variable Access: Push Global (7, 8, 179, 96, 97, 98)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 7 | `PshG4` | PTR_ARG | ptr: globalVarAddr | **+1 dword** | Push a 32-bit global variable value |
| 8 | `LdGRdR4` | wW_PTR_ARG | v: destVar, ptr: globalVarAddr | 0 | Load global address into register, then copy its value to a local variable |
| 179 | `PshV8` | rW_ARG | v: varOffset | **+2 dwords** | Push 64-bit variable value onto the stack |
| 96 | `LDG` | PTR_ARG | ptr: globalAddr | 0 | Load global address into the value register (address only, no dereference) |
| 97 | `LDV` | rW_ARG | v: varOffset | 0 | Load address of a local variable into the value register |
| 98 | `PGA` | PTR_ARG | ptr: globalVarAddr | **+1 ptr** | Push global address onto the stack |

### Copy/Move Operations (46, 80-87, 183-186)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 46 | `COPY` | W_DW_ARG | w: size, dw: srcOffset | **-1 ptr** | Copy `w` dwords from stack-top-ptr to destination at `(fp - dw)` |
| 80 | `CpyVtoV4` | wW_rW_ARG | dest: varOff, src: varOff | 0 | Copy 4 bytes between two local variables |
| 81 | `CpyVtoV8` | wW_rW_ARG | dest: varOff, src: varOff | 0 | Copy 8 bytes between two local variables |
| 82 | `CpyVtoR4` | rW_ARG | src: varOff | 0 | Copy 4-byte variable value into valueRegister |
| 83 | `CpyVtoR8` | rW_ARG | src: varOff | 0 | Copy 8-byte variable value into valueRegister |
| 84 | `CpyVtoG4` | rW_PTR_ARG | src: varOff, ptr: globalAddr | 0 | Copy 4-byte variable value to a global variable |
| 85 | `CpyRtoV4` | wW_ARG | dest: varOff | 0 | Copy 4 bytes from valueRegister to variable |
| 86 | `CpyRtoV8` | wW_ARG | dest: varOff | 0 | Copy 8 bytes from valueRegister to variable |
| 87 | `CpyGtoV4` | wW_PTR_ARG | dest: varOff, ptr: globalAddr | 0 | Copy 4 bytes from global to local variable |
| 183 | `LoadRObjR` | rW_W_DW_ARG | v: dest, w: offset, dw: typeId | 0 | Load object from valueRegister+offset into dest variable via register |
| 184 | `LoadVObjR` | rW_W_DW_ARG | v: dest, w: offset, dw: typeId | 0 | Load object from stack variable+offset into dest variable via register |
| 185 | `RefCpyV` | wW_PTR_ARG | v: varOff, ptr: typePtr | 0 | Reference-copy a handle-compatible value into a variable |

### Variable Set Operations (77, 78, 142, 143, 88-95, 136, 159)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 77 | `SetV4` | wW_DW_ARG | v: varOff, val: 32-bit | 0 | Set a 32-bit variable to a constant value |
| 78 | `SetV8` | wW_QW_ARG | v: varOff, val: 64-bit | 0 | Set a 64-bit variable to a constant value |
| 142 | `SetV1` | wW_DW_ARG | v: varOff, val: 8-bit | 0 | Set a 1-byte variable to a constant (stores high byte) |
| 143 | `SetV2` | wW_DW_ARG | v: varOff, val: 16-bit | 0 | Set a 2-byte variable to a constant (stores high word) |
| 136 | `SetG4` | PTR_DW_ARG | ptr: globalAddr, val: 32-bit | 0 | Set a 32-bit global variable to a constant value |
| 88 | `WRTV1` | rW_ARG | v: varOff | 0 | Write 1 byte from valueRegister to variable |
| 89 | `WRTV2` | rW_ARG | v: varOff | 0 | Write 2 bytes from valueRegister to variable |
| 90 | `WRTV4` | rW_ARG | v: varOff | 0 | Write 4 bytes from valueRegister to variable |
| 91 | `WRTV8` | rW_ARG | v: varOff | 0 | Write 8 bytes from valueRegister to variable |
| 92 | `RDR1` | wW_ARG | dest: varOff | 0 | Read 1 byte from address in valueRegister into variable |
| 93 | `RDR2` | wW_ARG | dest: varOff | 0 | Read 2 bytes from address in valueRegister into variable |
| 94 | `RDR4` | wW_ARG | dest: varOff | 0 | Read 4 bytes from address in valueRegister into variable |
| 95 | `RDR8` | wW_ARG | dest: varOff | 0 | Read 8 bytes from address in valueRegister into variable |

### Pointer/Handle/Reference Operations (49, 60, 64-72, 74, 99, 100, 137, 138, 173)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 49 | `RDSPtr` | NO_ARG | — | 0 | Dereference: replace stack-top pointer with the value it points to |
| 60 | `STR` | W_ARG | w: alignment | **+1 dword + ptr** | Store value from register to address on stack |
| 64 | `ALLOC` | PTR_DW_ARG | ptr: objType, dw: funcId | variable | Allocate an object on the stack (calls constructor if funcId != 0) |
| 65 | `FREE` | wW_PTR_ARG | v: varOff, ptr: typePtr | 0 | Free a handle/object reference (calls Release if handle) |
| 66 | `LOADOBJ` | rW_ARG | v: varOff | 0 | Load object from variable into objectRegister |
| 67 | `STOREOBJ` | wW_ARG | v: varOff | 0 | Store object from objectRegister into variable |
| 68 | `GETOBJ` | W_ARG | w: offset | 0 | Get child object at offset from current objectRegister |
| 69 | `REFCPY` | PTR_ARG | ptr: typePtr | **-1 ptr** | Copy a reference: pop handle, addref, push copy |
| 70 | `CHKREF` | NO_ARG | — | 0 | Check if objectRegister is null, throw exception if so |
| 71 | `GETOBJREF` | W_ARG | w: offset | 0 | Get a handle/reference to child at offset from objectRegister |
| 72 | `GETREF` | W_ARG | w: offset | 0 | Get a reference at offset from objectRegister |
| 74 | `ClrVPtr` | wW_ARG | v: varOff | 0 | Clear a pointer/handle in a variable (set to null) |
| 99 | `CmpPtr` | rW_rW_ARG | a: varOff, b: varOff | 0 | Compare two pointers/handles for equality |
| 100 | `VAR` | rW_ARG | v: varOff | **+1 ptr** | Push the address of a local variable (initialization) |
| 137 | `ChkRefS` | NO_ARG | — | 0 | Check if stack-top handle needs release (for store operations) |
| 138 | `ChkNullV` | rW_ARG | v: varOff | 0 | Check if a handle variable is null |
| 173 | `ChkNullS` | W_ARG | w: offset | 0 | Check if object+offset is a null handle |

### List Initialization (189-192)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 189 | `AllocMem` | wW_DW_ARG | v: dest, dw: size | 0 | Allocate memory block, store address in variable |
| 190 | `SetListSize` | rW_DW_DW_ARG | v: varOff, typeId, size | 0 | Initialize list/array size |
| 191 | `PshListElmnt` | rW_DW_ARG | v: listVar, typeId | **+1 ptr** | Push pointer to a list element for initialization |
| 192 | `SetListType` | rW_DW_DW_ARG | v: varOff, typeId, subTypeId | 0 | Set list's element type info |

### Call & Return (9, 10, 61, 62, 139, 176, 177, 200, 163)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 9 | `CALL` | DW_ARG | funcId | variable | Call a script function by ID |
| 10 | `RET` | W_ARG | popSize | variable | Return from current function, pop `popSize` bytes of arguments |
| 61 | `CALLSYS` | DW_ARG | funcId | variable | Call a registered native (system) function by ID |
| 62 | `CALLBND` | DW_ARG | funcId | variable | Call a bound function (delegate via object) |
| 139 | `CALLINTF` | DW_ARG | funcId | variable | Call an interface method by ID |
| 176 | `CallPtr` | rW_ARG | funcPtrVar | variable | Call a function pointer stored in a variable |
| 177 | `FuncPtr` | PTR_ARG | ptr: funcPtr | **+1 ptr** | Push a function pointer onto the stack |
| 200 | `Thiscall1` | DW_ARG | funcId | **-ptr-1** | Call with 'this' — pops object register as first arg |
| 163 | `SUSPEND` | NO_ARG | — | 0 | Suspend execution (yield for coroutines) |

### Jump Instructions (11-17, 57, 187, 188)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 11 | `JMP` | DW_ARG | relativeOffset | 0 | Unconditional jump |
| 12 | `JZ` | DW_ARG | relativeOffset | 0 | Jump if valueRegister == 0 |
| 13 | `JNZ` | DW_ARG | relativeOffset | 0 | Jump if valueRegister != 0 |
| 14 | `JS` | DW_ARG | relativeOffset | 0 | Jump if valueRegister < 0 (sign flag) |
| 15 | `JNS` | DW_ARG | relativeOffset | 0 | Jump if valueRegister >= 0 |
| 16 | `JP` | DW_ARG | relativeOffset | 0 | Jump if valueRegister > 0 (positive) |
| 17 | `JNP` | DW_ARG | relativeOffset | 0 | Jump if valueRegister <= 0 |
| 57 | `JMPP` | rW_ARG | v: varOff | 0 | Jump through a function pointer variable |
| 187 | `JLowZ` | DW_ARG | relativeOffset | 0 | Jump if low 32-bits of valueRegister == 0 |
| 188 | `JLowNZ` | DW_ARG | relativeOffset | 0 | Jump if low 32-bits of valueRegister != 0 |

### Test Instructions (18-23) — Set valueRegister without branching

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 18 | `TZ` | NO_ARG | — | 0 | Test zero: `valueRegister = (valueRegister == 0)` |
| 19 | `TNZ` | NO_ARG | — | 0 | Test non-zero: `valueRegister = (valueRegister != 0)` |
| 20 | `TS` | NO_ARG | — | 0 | Test sign: `valueRegister = (valueRegister < 0)` |
| 21 | `TNS` | NO_ARG | — | 0 | Test not sign: `valueRegister = (valueRegister >= 0)` |
| 22 | `TP` | NO_ARG | — | 0 | Test positive: `valueRegister = (valueRegister > 0)` |
| 23 | `TNP` | NO_ARG | — | 0 | Test not positive: `valueRegister = (valueRegister <= 0)` |

### Boolean NOT (6)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 6 | `NOT` | rW_ARG | v: varOff | 0 | Boolean NOT on a variable: `v = (v == 0 ? true : false)` |

### Negation (24-26, 156)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 24 | `NEGi` | rW_ARG | v: varOff | 0 | Negate int32 in variable `v = -v` |
| 25 | `NEGf` | rW_ARG | v: varOff | 0 | Negate float in variable |
| 26 | `NEGd` | rW_ARG | v: varOff | 0 | Negate double in variable |
| 156 | `NEGi64` | rW_ARG | v: varOff | 0 | Negate int64 in variable |

### Bitwise NOT (39, 159)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 39 | `BNOT` | rW_ARG | v: varOff | 0 | Bitwise NOT int32 in variable |
| 159 | `BNOT64` | rW_ARG | v: varOff | 0 | Bitwise NOT int64 in variable |

### Bitwise Operations on 32-bit (40-45)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 40 | `BAND` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a & b` (32-bit) |
| 41 | `BOR` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a \| b` |
| 42 | `BXOR` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a ^ b` |
| 43 | `BSLL` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a << b` |
| 44 | `BSRL` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a >> b` (logical) |
| 45 | `BSRA` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a >> b` (arithmetic) |

### Bitwise Operations on 64-bit (165-170)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 165 | `BAND64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a & b` (64-bit) |
| 166 | `BOR64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a \| b` |
| 167 | `BXOR64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a ^ b` |
| 168 | `BSLL64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a << b` |
| 169 | `BSRL64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a >> b` (logical) |
| 170 | `BSRA64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a >> b` (arithmetic) |

### Integer Arithmetic 32-bit (115-119, 79, 37, 38, 27-32, 180, 181)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 115 | `ADDi` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a + b` (signed int32) |
| 116 | `SUBi` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a - b` |
| 117 | `MULi` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a * b` |
| 118 | `DIVi` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a / b` (signed) |
| 119 | `MODi` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a % b` (signed) |
| 180 | `DIVu` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a / b` (unsigned) |
| 181 | `MODu` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a % b` (unsigned) |
| 79 | `ADDSi` | W_DW_ARG | w: offset, dw: delta | 0 | `*(fp - offset) += delta` (add signed immediate to stack variable) |
| 37 | `IncVi` | rW_ARG | v: varOff | 0 | Pre-increment int32 variable |
| 38 | `DecVi` | rW_ARG | v: varOff | 0 | Pre-decrement int32 variable |
| 27 | `INCi16` | NO_ARG | — | 0 | Increment int16 at stack top |
| 28 | `INCi8` | NO_ARG | — | 0 | Increment int8 at stack top |
| 29 | `DECi16` | NO_ARG | — | 0 | Decrement int16 at stack top |
| 30 | `DECi8` | NO_ARG | — | 0 | Decrement int8 at stack top |
| 31 | `INCi` | NO_ARG | — | 0 | Increment int32 at stack top |
| 32 | `DECi` | NO_ARG | — | 0 | Decrement int32 at stack top |

### Integer Arithmetic 64-bit (160-164, 182, 183, 157, 158)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 160 | `ADDi64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a + b` (int64) |
| 161 | `SUBi64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a - b` |
| 162 | `MULi64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a * b` |
| 163 | `DIVi64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a / b` (signed) |
| 164 | `MODi64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a % b` (signed) |
| 182 | `DIVu64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a / b` (unsigned) |
| 183 | `MODu64` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a % b` (unsigned) |
| 157 | `INCi64` | NO_ARG | — | 0 | Increment int64 at stack top |
| 158 | `DECi64` | NO_ARG | — | 0 | Decrement int64 at stack top |

### Float Arithmetic (120-124, 33, 34)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 120 | `ADDf` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a + b` (float) |
| 121 | `SUBf` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a - b` |
| 122 | `MULf` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a * b` |
| 123 | `DIVf` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a / b` |
| 124 | `MODf` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = fmod(a, b)` |
| 33 | `INCf` | NO_ARG | — | 0 | Increment float at stack top |
| 34 | `DECf` | NO_ARG | — | 0 | Decrement float at stack top |

### Double Arithmetic (125-129, 35, 36)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 125 | `ADDd` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a + b` (double) |
| 126 | `SUBd` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a - b` |
| 127 | `MULd` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a * b` |
| 128 | `DIVd` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = a / b` |
| 129 | `MODd` | wW_rW_rW_ARG | dest, a, b | 0 | `dest = fmod(a, b)` |
| 35 | `INCd` | NO_ARG | — | 0 | Increment double at stack top |
| 36 | `DECd` | NO_ARG | — | 0 | Decrement double at stack top |

### Power Operations (193-199)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 193 | `POWi` | wW_rW_rW_ARG | dest, base, exp | 0 | `dest = pow(base, exp)` (int32) |
| 194 | `POWu` | wW_rW_rW_ARG | dest, base, exp | 0 | `dest = pow(base, exp)` (uint32) |
| 195 | `POWf` | wW_rW_rW_ARG | dest, base, exp | 0 | `dest = pow(base, exp)` (float) |
| 196 | `POWd` | wW_rW_rW_ARG | dest, base, exp | 0 | `dest = pow(base, exp)` (double) |
| 197 | `POWdi` | wW_rW_rW_ARG | dest, base, exp | 0 | `dest = pow(base, exp)` (double base, int exp) |
| 198 | `POWi64` | wW_rW_rW_ARG | dest, base, exp | 0 | `dest = pow(base, exp)` (int64) |
| 199 | `POWu64` | wW_rW_rW_ARG | dest, base, exp | 0 | `dest = pow(base, exp)` (uint64) |

### Immediate Arithmetic (130-135)

Operate on a variable with a 32-bit immediate:

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 130 | `ADDIi` | wW_rW_DW_ARG | dest, a, imm | 0 | `dest = a + imm` (int32) |
| 131 | `SUBIi` | wW_rW_DW_ARG | dest, a, imm | 0 | `dest = a - imm` (int32) |
| 132 | `MULIi` | wW_rW_DW_ARG | dest, a, imm | 0 | `dest = a * imm` (int32) |
| 133 | `ADDIf` | wW_rW_DW_ARG | dest, a, imm | 0 | `dest = a + imm` (float) |
| 134 | `SUBIf` | wW_rW_DW_ARG | dest, a, imm | 0 | `dest = a - imm` (float) |
| 135 | `MULIf` | wW_rW_DW_ARG | dest, a, imm | 0 | `dest = a * imm` (float) |

### Comparison Instructions (50-56, 171, 172)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 50 | `CMPd` | rW_rW_ARG | a, b | 0 | Compare doubles; result in valueRegister (-1, 0, 1) |
| 51 | `CMPu` | rW_rW_ARG | a, b | 0 | Compare uint32 values |
| 52 | `CMPf` | rW_rW_ARG | a, b | 0 | Compare floats |
| 53 | `CMPi` | rW_rW_ARG | a, b | 0 | Compare int32 values |
| 54 | `CMPIi` | rW_DW_ARG | a, imm32 | 0 | Compare int32 variable with immediate |
| 55 | `CMPIf` | rW_DW_ARG | a, immFloat | 0 | Compare float variable with immediate |
| 56 | `CMPIu` | rW_DW_ARG | a, immU32 | 0 | Compare uint32 variable with immediate |
| 171 | `CMPi64` | rW_rW_ARG | a, b | 0 | Compare int64 values |
| 172 | `CMPu64` | rW_rW_ARG | a, b | 0 | Compare uint64 values |
| 99 | `CmpPtr` | rW_rW_ARG | a, b | 0 | Compare two pointers (equality) |

### Type Casting & Conversion (101-114, 140, 141, 144, 145-155, 174)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 101 | `iTOf` | rW_ARG | v | 0 | int32 → float |
| 102 | `fTOi` | rW_ARG | v | 0 | float → int32 |
| 103 | `uTOf` | rW_ARG | v | 0 | uint32 → float |
| 104 | `fTOu` | rW_ARG | v | 0 | float → uint32 |
| 105 | `sbTOi` | rW_ARG | v | 0 | int8 → int32 (sign extend) |
| 106 | `swTOi` | rW_ARG | v | 0 | int16 → int32 (sign extend) |
| 107 | `ubTOi` | rW_ARG | v | 0 | uint8 → int32 (zero extend) |
| 108 | `uwTOi` | rW_ARG | v | 0 | uint16 → int32 (zero extend) |
| 109 | `dTOi` | wW_rW_ARG | dest, src | 0 | double → int32 |
| 110 | `dTOu` | wW_rW_ARG | dest, src | 0 | double → uint32 |
| 111 | `dTOf` | wW_rW_ARG | dest, src | 0 | double → float |
| 112 | `iTOd` | wW_rW_ARG | dest, src | 0 | int32 → double |
| 113 | `uTOd` | wW_rW_ARG | dest, src | 0 | uint32 → double |
| 114 | `fTOd` | wW_rW_ARG | dest, src | 0 | float → double |
| 140 | `iTOb` | rW_ARG | v | 0 | int32 → int8 (truncate) |
| 141 | `iTOw` | rW_ARG | v | 0 | int32 → int16 (truncate) |
| 144 | `Cast` | DW_ARG | typeId | **-1 ptr** | Cast handle/object to another type |
| 145 | `i64TOi` | wW_rW_ARG | dest, src | 0 | int64 → int32 |
| 146 | `uTOi64` | wW_rW_ARG | dest, src | 0 | uint32 → int64 |
| 147 | `iTOi64` | wW_rW_ARG | dest, src | 0 | int32 → int64 (sign extend) |
| 148 | `fTOi64` | wW_rW_ARG | dest, src | 0 | float → int64 |
| 149 | `dTOi64` | rW_ARG | v | 0 | double → int64 (2 src → 1 dest) |
| 150 | `fTOu64` | wW_rW_ARG | dest, src | 0 | float → uint64 |
| 151 | `dTOu64` | rW_ARG | v | 0 | double → uint64 |
| 152 | `i64TOf` | wW_rW_ARG | dest, src | 0 | int64 → float |
| 153 | `u64TOf` | wW_rW_ARG | dest, src | 0 | uint64 → float |
| 154 | `i64TOd` | rW_ARG | v | 0 | int64 → double |
| 155 | `u64TOd` | rW_ARG | v | 0 | uint64 → double |
| 174 | `ClrHi` | NO_ARG | — | 0 | Clear upper 32 bits of valueRegister |

### Type Info / Object Type (75, 76)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 75 | `OBJTYPE` | PTR_ARG | ptr: typePtr | **+1 ptr** | Push the object type pointer (for `is` operator) |
| 76 | `TYPEID` | DW_ARG | typeId | **+1 dword** | Push a type ID onto the stack |

### JIT Entry Point (175)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 175 | `JitEntry` | PTR_ARG | ptr: jitFunc | 0 | JIT entry marker — jumps to native compiled code if available |

### Load This (178)

| # | Mnemonic | Type | Args | Stack | Description |
|---|----------|------|------|-------|-------------|
| 178 | `LoadThisR` | W_DW_ARG | w: offset, dw: typeId | 0 | Load `this` from call stack into objectRegister |

---

## VM Register Model & Execution

### VM Registers Structure (`asSVMRegisters`)

```c
struct asSVMRegisters {
    asDWORD    *programPointer;     // current bytecode instruction
    asDWORD    *stackFramePointer;  // function stack frame (local vars)
    asDWORD    *stackPointer;       // top of stack (grows DOWNWARD)
    asQWORD     valueRegister;      // 64-bit register for primitives
    void       *objectRegister;     // pointer register for objects/handles
    asITypeInfo *objectType;        // type of object in objectRegister
    bool        doProcessSuspend;   // JIT: break on suspend?
    asIScriptContext *ctx;          // owning context
};
```

### Execution Model

The VM uses a **computed-goto dispatch loop** (when `AS_USE_COMPUTED_GOTOS` is defined) or a `switch` statement. The loop is in `asCContext::ExecuteNext()`.

Key points:
- **Stack grows downward**: `l_sp -= N` allocates N DWORDs; `l_sp += N` frees them
- Variables are accessed relative to the frame pointer: `*(l_fp - varOffset)`
- The **valueRegister** (`asQWORD`) holds int32, int64, float (32-bit), or double results of comparisons/arithmetic
- The **objectRegister** holds pointers to objects, handles, or type info
- Most arithmetic takes 3 variable offsets: `dest, operandA, operandB` and uses `wW_rW_rW_ARG` type
- Comparison results store -1, 0, 1 in valueRegister (used by conditional jumps)
- `CALL`/`CALLSYS`/`CALLBND`/`CALLINTF` push a call frame; `RET` pops it

### Variable Offsets

Variables are referenced as signed 16-bit offsets from the frame pointer (`l_fp`):
- `asBC_SWORDARG0(l_bc)` → `short` offset
- `l_fp - offset` → address of the variable
- The offset is positive downward (stack grows down)

---

## Accessing Bytecode via API

### Getting Raw Bytecode from a Function

```cpp
asIScriptFunction *func = /* ... */;
asUINT bytecodeLength = 0;
asDWORD *bytecode = func->GetByteCode(&bytecodeLength);

// bytecode is an array of bytecodeLength DWORDs
// The opcode is the first BYTE of each instruction
```

### Iterating Instructions

```cpp
asDWORD *bc = bytecode;
asDWORD *end = bc + bytecodeLength;

while (bc < end) {
    asBYTE op = *(asBYTE*)bc;
    const asSBCInfo &info = asBCInfo[op];
    
    int instrSize = asBCTypeSize[info.type];  // size in DWORDs
    int stackInc = info.stackInc;
    const char *name = info.name;
    
    // Access arguments based on info.type using macros
    // e.g., asBC_INTARG(bc), asBC_SWORDARG0(bc), etc.
    
    bc += instrSize;
}
```

### Retrieving Variable Information

```cpp
// Get declared variables of a function
asUINT varCount = func->GetVarCount();
for (asUINT i = 0; i < varCount; i++) {
    const char *name;
    int typeId;
    func->GetVar(i, &name, &typeId);
}

// Get parameter info
asUINT paramCount = func->GetParamCount();
for (asUINT i = 0; i < paramCount; i++) {
    int typeId;
    asDWORD flags;
    const char *name, *defaultArg;
    func->GetParam(i, &typeId, &flags, &name, &defaultArg);
}
```

### Getting Bytecode for Debugging

Enable AS_DEBUG in the build and the engine will output bytecode to `AS_DEBUG/` directory. Or use the `GetByteCode()` API manually.

---

## Writing a Bytecode Disassembler

Here's a minimal disassembler skeleton using the AngelScript API:

```cpp
#include <angelscript.h>
#include <stdio.h>

void DisassembleFunction(asIScriptFunction *func) {
    asUINT length = 0;
    asDWORD *bc = func->GetByteCode(&length);
    if (!bc) return;
    
    printf("Function: %s\n", func->GetDeclaration(true, true, true));
    
    // Print variables with their stack offsets
    asUINT varCount = func->GetVarCount();
    for (asUINT i = 0; i < varCount; i++) {
        const char *name;
        int typeId;
        func->GetVar(i, &name, &typeId);
        // Note: actual offset requires internal knowledge or debug build
        printf("  var %d: %s (typeId=%d)\n", i, name, typeId);
    }
    
    printf("\nBytecode (%u DWORDs):\n", length);
    
    asDWORD *pos = bc;
    asDWORD *end = bc + length;
    int offset = 0;
    
    while (pos < end) {
        asEBCInstr op = (asEBCInstr)*(asBYTE*)pos;
        const asSBCInfo &info = asBCInfo[op];
        int size = asBCTypeSize[info.type];
        
        printf("%04d: %-12s", offset, info.name);
        
        switch (info.type) {
        case asBCTYPE_NO_ARG:
            printf("\n");
            break;
        case asBCTYPE_W_ARG:
        case asBCTYPE_rW_ARG:
        case asBCTYPE_wW_ARG:
            printf(" v%d\n", asBC_SWORDARG0(pos));
            break;
        case asBCTYPE_DW_ARG:
            printf(" %d\n", asBC_INTARG(pos));
            break;
        case asBCTYPE_rW_DW_ARG:
        case asBCTYPE_wW_DW_ARG:
            printf(" v%d, %d\n", asBC_SWORDARG0(pos), (int)asBC_DWORDARG(pos));
            break;
        case asBCTYPE_QW_ARG:
            printf(" %lld\n", (long long)asBC_QWORDARG(pos));
            break;
        case asBCTYPE_wW_rW_rW_ARG:
            printf(" v%d, v%d, v%d\n", 
                   asBC_SWORDARG0(pos), asBC_SWORDARG1(pos), asBC_SWORDARG2(pos));
            break;
        case asBCTYPE_wW_rW_ARG:
        case asBCTYPE_rW_rW_ARG:
            printf(" v%d, v%d\n", asBC_SWORDARG0(pos), asBC_SWORDARG1(pos));
            break;
        case asBCTYPE_W_DW_ARG:
            printf(" %d, %d\n", asBC_WORDARG0(pos), (int)asBC_DWORDARG(pos));
            break;
        case asBCTYPE_wW_rW_DW_ARG:
        case asBCTYPE_rW_W_DW_ARG:
            printf(" v%d, v%d, %d\n", 
                   asBC_SWORDARG0(pos), asBC_SWORDARG1(pos), (int)asBC_DWORDARG(pos));
            break;
        case asBCTYPE_PTR_ARG:
            printf(" 0x%p\n", (void*)asBC_PTRARG(pos));
            break;
        case asBCTYPE_PTR_DW_ARG:
            printf(" 0x%p, %d\n", (void*)asBC_PTRARG(pos), (int)((asDWORD*)&asBC_PTRARG(pos))[AS_PTR_SIZE]);
            break;
        case asBCTYPE_wW_PTR_ARG:
        case asBCTYPE_rW_PTR_ARG:
            printf(" v%d, 0x%p\n", asBC_SWORDARG0(pos), (void*)asBC_PTRARG(pos));
            break;
        case asBCTYPE_DW_DW_ARG:
            printf(" %u, %d\n", (unsigned)asBC_DWORDARG(pos), (int)*(((asDWORD*)pos)+2));
            break;
        case asBCTYPE_rW_DW_DW_ARG:
            printf(" v%d, %u, %u\n", asBC_SWORDARG0(pos), 
                   (unsigned)asBC_DWORDARG(pos), (unsigned)*(((asDWORD*)pos)+2));
            break;
        case asBCTYPE_wW_W_ARG:
            printf(" v%d, %d\n", asBC_SWORDARG0(pos), asBC_WORDARG1(pos));
            break;
        case asBCTYPE_INFO:
            if (op == asBC_LABEL)
                printf(" %d:\n", asBC_WORDARG0(pos));
            else
                printf("\n");
            break;
        default:
            printf(" (unknown type %d)\n", info.type);
            break;
        }
        
        pos += size;
        offset += size;
    }
}
```

---

## JIT Compiler Interface

AngelScript supports JIT compilation through two interfaces:

### Version 1 (`asIJITCompiler`)
```cpp
class asIJITCompiler {
    virtual int  CompileFunction(asIScriptFunction *function, asJITFunction *output) = 0;
    virtual void ReleaseJITFunction(asJITFunction func) = 0;
};
```

### Version 2 (`asIJITCompilerV2`)
```cpp
class asIJITCompilerV2 {
    virtual void NewFunction(asIScriptFunction* scriptFunc) = 0;
    virtual void CleanFunction(asIScriptFunction *scriptFunc, asJITFunction jitFunc) = 0;
};
```

When a JIT compiler is registered, functions get a `JitEntry` instruction at the start. At runtime, if a JIT-compiled version exists, execution jumps to native code. Otherwise it falls through to the interpreted bytecode.

The `asSVMRegisters` struct is the API between JIT-compiled code and the VM — JIT code reads/writes these registers directly.

---

## Debug Output

When compiled with `AS_DEBUG` defined, the engine generates human-readable bytecode dumps in `AS_DEBUG/` directory.

**Output format per line:**
```
bytecodeOffset stackDepth* instructionName arguments
```

Example:
```
     0   0   SUSPEND
     1   0   PshC4   0x0          (i:0, f:0.000000)
     3   1   SetV4   v0, 0x0          (i:0, f:0.000000)
```

- `*` after stack depth = instruction is "marked" (reachable path)
- `vN` = variable at offset N (signed) from frame pointer
- Jump targets show `relative+offset` and `(d:absoluteAddr)`
- Labels show as `N:`
- Lines show as `- line,col -`

---

## Meta / Temporary Instructions

These are only used during compilation and are **never emitted** to the final bytecode array:

| # | Mnemonic | Type | Purpose |
|---|----------|------|---------|
| 250 | `TryBlock` | DW_ARG | Marks beginning of try-catch block with catch label |
| 251 | `VarDecl` | W_ARG | Variable declaration with index |
| 252 | `Block` | INFO | Marks `{` (arg=1) or `}` (arg=0) for scope tracking |
| 253 | `ObjInfo` | rW_DW_ARG | Associates offset+info metadata with an object variable |
| 254 | `LINE` | INFO | Source line/column marker for debugging |
| 255 | `LABEL` | INFO | Jump label target with numeric ID |

These are stripped during `Finalize()`, `ExtractLineNumbers()`, and `ExtractObjectVariableInfo()`.

---

## Quick Reference: Common Patterns

### Variable to Variable Copy (32-bit)
```
PshV4  src      ; push src var value
SetV4  dest, 0  ; pop into dest var (optimized as CpyVtoV4)
```

### Function Call
```
; Push arguments onto stack (in reverse order)
PshC4  arg2     ; (if constant)
PshV4  arg1Var  ; (if variable)
PshC8  arg0     ; (if 64-bit)
CALL   funcId   ; call function
```

### Conditional Branch
```
CMPi   a, b     ; compare two int32 vars → valueRegister
JZ     +N       ; jump if equal (valueRegister == 0)
```

### Object Method Call
```
PSF    objVar   ; push address of object
RDSPtr          ; dereference to get object pointer
CALLBND funcId  ; call bound method
```

### Loop Counter
```
SetV4  i, 0     ; i = 0
:label
; ... loop body ...
IncVi  i         ; ++i
CMPIi  i, 10    ; compare i with 10
JS     -N        ; jump back if i < 10
```
