# CD Bytecode Target Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Add an experimental LLVM CD target that accepts LLVM IR through llc -march=cd and emits a valid cdbc 0.1 text artifact for the cd-compiler Rust VM.

**Architecture:** Treat CD as a software target rather than an ELF CPU. The target registers a normal LLVM TargetMachine, provides the minimal MC objects required by llc's MachineModuleInfo, and installs a module-level legacy pass that lowers LLVM IR directly into CD's virtual-register bytecode text. The first supported lowering surface is scalar constants, arithmetic/comparison, direct calls, local alloca/load/store, conditional/unconditional branches, PHI edge stores, returns, and the cd_print/print intrinsic convention; unsupported LLVM operations fail explicitly instead of emitting invalid .cdbc.

**Tech Stack:** LLVM 24 C++ target infrastructure, CMake experimental targets, legacy ModulePass, LLVM IR APIs, LLVM lit/FileCheck, and the cd-compiler cdbc 0.1 text format.

---

### Task 1: Add the failing CD target regression test

**Files:**
- Create: llvm/test/CodeGen/CD/cdbc-basic.ll

- [x] **Step 1: Write the failing test**

Create one LLVM IR fixture that exercises the bytecode contract without relying on machine registers:

~~~
; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o - | FileCheck %s

declare void @cd_print(double)

define double @add_one(double %value) {
entry:
  %result = fadd double %value, 1.0
  ret double %result
}

define i32 @main() {
entry:
  %value = call double @add_one(double 41.0)
  call void @cd_print(double %value)
  %condition = icmp eq i32 1, 1
  br i1 %condition, label %yes, label %no
yes:
  call void @cd_print(double 1.0)
  br label %done
no:
  call void @cd_print(double 0.0)
  br label %done
done:
  ret i32 0
}

; CHECK: cdbc 0.1
; CHECK: constants:
; CHECK: names:
; CHECK: main registers=
; CHECK: make_function f0
; CHECK: call
; CHECK: print
; CHECK: equal
; CHECK: jump_if_false
; CHECK: jump
; CHECK: function f0 name="add_one" arity=1 registers=
; CHECK: param 0 = "value"
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: return
~~~

- [x] **Step 2: Run it to verify it fails for the missing target**

Run:

~~~
build/bin/llc -mtriple=cd-unknown-unknown -O0 llvm/test/CodeGen/CD/cdbc-basic.ll -o /tmp/cd-bytecode-before.cdbc
~~~

Expected: FAIL before code generation because the existing LLVM build does not register the cd target.

### Task 2: Register the experimental target and satisfy llc's MC prerequisites

**Files:**
- Modify: llvm/CMakeLists.txt
- Modify: llvm/include/llvm/TargetParser/Triple.h
- Modify: llvm/lib/TargetParser/Triple.cpp
- Modify: llvm/lib/TargetParser/TargetDataLayout.cpp
- Create: llvm/lib/Target/CD/CMakeLists.txt
- Create: llvm/lib/Target/CD/TargetInfo/CDTargetInfo.h
- Create: llvm/lib/Target/CD/TargetInfo/CDTargetInfo.cpp
- Create: llvm/lib/Target/CD/MCTargetDesc/CDMCTargetDesc.cpp
- Create: llvm/lib/Target/CD/CDTargetMachine.h
- Create: llvm/lib/Target/CD/CDTargetMachine.cpp

- [x] **Step 1: Add the cd triple spelling and fixed software-target layout**

Add cd immediately before LastArchType in Triple::ArchType; return and parse the spelling cd; make Triple::computeDataLayout() return:

~~~
e-p:64:64-i64:64-n8:16:32:64-S128
~~~

Treat the target as a 64-bit little-endian software target for pointer-width and endianness queries. Give its default object format ELF only so MachineModuleInfo can initialize; the target itself must still reject CodeGenFileType::ObjectFile.

- [x] **Step 2: Add the target CMake component**

Add CD to LLVM_ALL_EXPERIMENTAL_TARGETS. The target CMake file must build CDInfo, CDDesc, and CDCodeGen, add the three subdirectories/components, and link the code generator against Core, MC, Support, Target, and TargetParser.

- [x] **Step 3: Register cd and construct minimal MC descriptions**

CDTargetInfo.cpp must define getTheCDTarget() and register:

~~~
RegisterTarget<Triple::cd, /*HasJIT=*/false>(
    getTheCDTarget(), "cd", "Compiler Design bytecode", "CD");
~~~

CDMCTargetDesc.cpp must register a minimal MCAsmInfo subclass with MCAsmInfo(Options), an empty MCInstrInfo, an empty MCRegisterInfo, and an MCSubtargetInfo constructed with an empty StringTable("\\0") and empty feature/schedule tables. No object writer or assembler printer is required for the text-only output target.

- [x] **Step 4: Add a target machine that accepts assembly output only**

CDTargetMachine must inherit CodeGenTargetMachineImpl, call initAsmInfo(), retain a TargetLoweringObjectFile subclass whose getExplicitSectionGlobal returns nullptr, and implement:

~~~
bool addPassesToEmitFile(PassManagerBase &PM, raw_pwrite_stream &Out,
                         raw_pwrite_stream *DwoOut,
                         CodeGenFileType FileType, bool DisableVerify,
                         MachineModuleInfoWrapperPass *MMIWP) override;
~~~

The method returns true for object/null output and adds the CD bytecode emitter pass for AssemblyFile. LLVMInitializeCDTarget() registers RegisterTargetMachine<CDTargetMachine>.

- [x] **Step 5: Configure and build the target registration layer**

Run:

~~~
cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=CD
ninja -C build llc
~~~

Expected: configuration and the llc target build succeed, with CD included in the generated target initialization code.

### Task 3: Implement the cdbc module emitter

**Files:**
- Create: llvm/lib/Target/CD/CDBytecodeEmitter.h
- Create: llvm/lib/Target/CD/CDBytecodeEmitter.cpp
- Create: llvm/lib/Target/CD/README.md
- Modify: llvm/lib/Target/CD/CMakeLists.txt

- [x] **Step 1: Define the pass factory and output model**

Define ModulePass *createCDBytecodeEmitterPass(raw_ostream &OS) and a pass that preserves the input module. The emitter owns module-wide constant/name/function tables and emits this exact envelope:

~~~
cdbc 0.1

constants:
  cN = nil|number <literal>|bool true|false

names:
  nN = "<escaped name>"

main registers=<count>:
  <instructions>

function fN name="<escaped name>" arity=<arity> registers=<count>:
  param <index> = "<escaped parameter name>"
  <instructions>
~~~

Omit optional debug sections because LLVM IR does not carry the original cd source text. Keep table indexes zero-based and branch targets as instruction offsets.

- [x] **Step 2: Implement deterministic constants, names, and virtual registers**

Use stable first-use order for constants and names. Map i1 integer constants to bool, integer/floating constants to number, and null pointers to nil; reject non-finite floating constants and unsupported aggregate constants. Assign one monotonically increasing CD register to every non-void LLVM instruction result and every function argument, excluding alloca pointer tokens. Name unnamed values as arg<N>, value<N>, or alloca<N>.

- [x] **Step 3: Lower scalar operations and local storage**

Emit the following mappings:

~~~
add/fadd -> add
sub/fsub -> subtract
mul/fmul -> multiply
sdiv/udiv/fdiv -> divide
icmp/fcmp eq/ne/gt/ge/lt/le -> equal/not_equal/greater/greater_equal/less/less_equal
integer/floating/bit/integer-extension casts -> move
alloca + store/load -> store_var/assign_var + load_var
~~~

Only direct alloca pointers are accepted for load/store. Unsupported remainder, bitwise, aggregate, atomic, volatile, pointer arithmetic, and indirect memory operations must call report_fatal_error with the LLVM opcode and CD target name.

- [x] **Step 4: Lower calls, control flow, and returns**

Map direct calls to defined non-entry functions to make_function plus call; map declarations named cd_print or print with one argument to print; reject other declarations and indirect calls. Lower conditional/unconditional br to jump_if_true/jump_if_false/jump, patching basic-block offsets after emission. For each PHI, emit a load_var at the block start and a store_var for each incoming edge before that edge's branch. Emit a return for every LLVM return, materializing nil for ret void; reject unreachable with a target diagnostic because it has no executable CD equivalent.

- [x] **Step 5: Document the target boundary**

In llvm/lib/Target/CD/README.md, document -mtriple=cd-unknown-unknown, the cdbc 0.1 output contract, supported mappings, the cd_print convention, the absence of object/JIT output, and the fact that arrays/maps/structs/variants/native calls/debug source sections remain deferred until an explicit LLVM-to-CD representation is defined.

### Task 4: Run the regression and VM contract checks

**Files:**
- Test: llvm/test/CodeGen/CD/cdbc-basic.ll

- [x] **Step 1: Run the LLVM FileCheck test**

Run:

~~~
build/bin/llc -mtriple=cd-unknown-unknown -O0 llvm/test/CodeGen/CD/cdbc-basic.ll -o /tmp/cd-bytecode.cdbc
build/bin/FileCheck llvm/test/CodeGen/CD/cdbc-basic.ll < /tmp/cd-bytecode.cdbc
~~~

Expected: both commands exit zero and the output contains the checked cdbc, call, arithmetic, branch, and function records.

- [x] **Step 2: Verify the artifact with the cd-compiler VM parser**

Run:

~~~
build/bin/llc -mtriple=cd-unknown-unknown -O0 llvm/test/CodeGen/CD/cdbc-basic.ll -o - \
  | cargo run --manifest-path cd-compiler/vm-rs/Cargo.toml --quiet -- dump /dev/stdin
~~~

Expected: the Rust VM accepts the generated cdbc 0.1 artifact and prints its canonical dump without a parse/validation error.

- [x] **Step 3: Verify explicit object rejection**

Run:

~~~
not build/bin/llc -mtriple=cd-unknown-unknown -filetype=obj \
  llvm/test/CodeGen/CD/cdbc-basic.ll -o /tmp/cd-bytecode.o 2>&1 \
  | FileCheck --check-prefix=NO-OBJECT llvm/test/CodeGen/CD/cdbc-basic.ll
~~~

Add this check to the fixture:

~~~
; NO-OBJECT: target does not support generation of this file type
~~~

### Task 5: Final verification and handoff

**Files:**
- Verify: all modified LLVM target/triple/CMake/test files

- [x] **Step 1: Run focused build and test gates**

Run:

~~~
ninja -C build llc FileCheck
build/bin/llc -mtriple=cd-unknown-unknown -O0 \
  llvm/test/CodeGen/CD/cdbc-basic.ll -o - \
  | build/bin/FileCheck llvm/test/CodeGen/CD/cdbc-basic.ll
~~~

- [x] **Step 2: Run repository hygiene checks**

Run:

~~~
git diff --check
git status --short --branch
git diff --stat
~~~

Expected: no whitespace errors; the pre-existing untracked cd-compiler/ reference tree remains untouched; no commit, push, or merge is performed unless separately requested.
