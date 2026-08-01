# CD TableGen Machine Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a concrete TableGen description for the CD bytecode pseudo-machine model and make LLVM generate its instruction, register, calling-convention, and subtarget descriptors without changing the existing direct emitter.

**Architecture:** The current typed `CDArtifact` serializer and direct `ModulePass` remain the default path. TableGen describes pre-register-allocation CD pseudo-instructions whose operands are dynamic VM values and table indices; it does not define an object encoding or pretend that CD VM registers are native physical registers. A later M3 slice will lower `MachineInstr` objects to the existing artifact model behind an explicit machine-backend option.

**Tech Stack:** LLVM TableGen, CMake `tablegen()`/`add_public_tablegen_target()`, generated MC descriptors, C++ target registration, FileCheck/lit, and the existing CD unit-test/build gates.

---

## File map

- Create `docs/cd-bytecode-machine-backend.md`: the M3 design gate and ownership rules.
- Create `llvm/lib/Target/CD/CD.td`: the target-level TableGen entry point.
- Create `llvm/lib/Target/CD/CDInstrInfo.td`: stable pseudo-machine operations matching `CDOpcode`.
- Create `llvm/lib/Target/CD/CDRegisterInfo.td`: the opaque CD value register class and reserved placeholder registers.
- Create `llvm/lib/Target/CD/CDCallingConv.td`: the initial VM convention for the future machine lowering.
- Create `llvm/lib/Target/CD/CDSubtarget.td`: the generic CD processor and feature list.
- Modify `llvm/lib/Target/CD/CMakeLists.txt`: generate and expose CD descriptor includes through `CDCommonTableGen`.
- Modify `llvm/lib/Target/CD/MCTargetDesc/CDMCTargetDesc.cpp`: initialize MC descriptors from generated TableGen data.
- Modify `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`: link the design gate and mark only the generated-foundation work complete.

## Task 1: Write and review the design gate

**Files:**
- Create: `docs/cd-bytecode-machine-backend.md`
- Modify: `docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md`

- [x] **Step 1: Establish the missing generated-target gate.**

Run:

```bash
ninja -C build-cd CDCommonTableGen
```

Expected before this slice: `ninja: error: unknown target 'CDCommonTableGen'`.

- [x] **Step 2: Write the design decision.**

The document must state these exact invariants:

```text
The direct ModulePass path remains the default and is the compatibility oracle.
The machine path is opt-in and may emit only CD pseudo-instructions with a
stable CDOpcode mapping. CDValue registers are VM register identities, not
native CPU registers; the first machine slice runs before physical register
allocation and never emits an object file. The module owns constants, names,
functions, and function indexes; each MachineFunction owns only its body,
parameter metadata, and branch labels. Calls carry a callee VM register and a
variadic list of CDValue operands. A later artifact bridge resolves block
labels to instruction offsets and validates through CDBytecodeFormat before
serialization.
```

Add a table mapping `CD_CONSTANT`, `CD_MOVE`, `CD_NEGATE`, `CD_NOT`, the scalar
binary operations, `CD_LOAD_VAR`, `CD_STORE_VAR`, `CD_MAKE_FUNCTION`,
`CD_CALL`, `CD_PRINT`, `CD_RETURN`, `CD_JUMP`, and `CD_JUMP_IF_FALSE` to the
existing `CDOpcode` values. Explicitly defer `JumpIfTrue`, aggregate values,
native calls, object emission, and register allocation until their bridge
contracts exist.

- [x] **Step 3: Run documentation hygiene.**

Run `git diff --check` and confirm the design document does not claim that a
machine artifact path exists yet.

## Task 2: Define the TableGen model

**Files:**
- Create: `llvm/lib/Target/CD/CD.td`
- Create: `llvm/lib/Target/CD/CDInstrInfo.td`
- Create: `llvm/lib/Target/CD/CDRegisterInfo.td`
- Create: `llvm/lib/Target/CD/CDCallingConv.td`
- Create: `llvm/lib/Target/CD/CDSubtarget.td`

- [x] **Step 1: Define the opaque value registers.**

`CDRegisterInfo.td` must define `R0` through `R31` in namespace `CD` and a
`CDValue` carrier class with `i64` type and 64-bit width.  The class remains a
descriptor-only register class in this foundation; the future
`TargetRegisterInfo` reserves its registers before any host allocation:

```tablegen
class CDReg<bits<8> Num, string Name> : Register<Name> {
  let Namespace = "CD";
  let HWEncoding{7-0} = Num;
}

foreach I = 0-31 in
  def R#I : CDReg<I, "r"#I>;

def CDValue : RegisterClass<"CD", [i64], 64,
                             (sequence "R%u", 0, 31)> {
}
```

- [x] **Step 2: Define the VM calling convention.**

`CDCallingConv.td` must assign the first eight `i64` carrier values to `R0`
through `R7` and return one value in `R0`, making the future machine bridge
deterministic without claiming that the VM uses native ABI registers:

```tablegen
def RetCC_CD : CallingConv<[
  CCIfType<[i64], CCAssignToReg<[R0]>>
]>;

def CC_CD : CallingConv<[
  CCIfType<[i64], CCAssignToReg<[R0, R1, R2, R3, R4, R5, R6, R7]>>
]>;
```

- [x] **Step 3: Define the generic processor.**

`CDSubtarget.td` must define the `generic` processor with no target-specific
features and a `CD` target feature namespace reserved for later additions:

```tablegen
class Proc<string Name, list<SubtargetFeature> Features>
    : Processor<Name, NoItineraries, Features>;

def : Proc<"generic", []>;
```

- [x] **Step 4: Define only stable pseudo-operations.**

`CDInstrInfo.td` must define a `CDPseudo` instruction base with
`isPseudo = 1`, `isCodeGenOnly = 1`, and no native assembly spelling. Define
these instruction shapes using `CDValue` for dynamic values and `i64imm` for
module/name/function indexes: `CD_CONSTANT`, `CD_MAKE_FUNCTION`, `CD_MOVE`,
`CD_LOAD_VAR`, `CD_STORE_VAR`, `CD_CALL` with `variable_ops`, `CD_PRINT`,
`CD_RETURN`, `CD_NEGATE`, `CD_NOT`, `CD_ADD`, `CD_SUBTRACT`, `CD_MULTIPLY`,
`CD_DIVIDE`, `CD_EQUAL`, `CD_NOT_EQUAL`, `CD_GREATER`, `CD_GREATER_EQUAL`,
`CD_LESS`, `CD_LESS_EQUAL`, `CD_JUMP`, and `CD_JUMP_IF_FALSE`.

Each opcode must have a direct comment naming its `CDOpcode` mapping. No
instruction may use a pointer, aggregate, native-call, or object-file operand.

- [x] **Step 5: Assemble the target entry point.**

`CD.td` must include `llvm/Target/Target.td`, the four component descriptions,
define `CDInstrInfo : InstrInfo`, and declare:

```tablegen
def CD : Target {
  let InstructionSet = CDInstrInfo;
}
```

## Task 3: Generate and initialize descriptors

**Files:**
- Modify: `llvm/lib/Target/CD/CMakeLists.txt`
- Modify: `llvm/lib/Target/CD/MCTargetDesc/CDMCTargetDesc.cpp`

- [x] **Step 1: Add the CMake generation rules.**

Set `LLVM_TARGET_DEFINITIONS CD.td`, generate `CDGenInstrInfo.inc` with
`-gen-instr-info`, `CDGenRegisterInfo.inc` with `-gen-register-info`, and
`CDGenSubtargetInfo.inc` with `-gen-subtarget`, then expose them through
`add_public_tablegen_target(CDCommonTableGen)`. Add `CDCommonTableGen` as a
dependency of `CDCodeGen` before the source list is compiled.

- [x] **Step 2: Initialize the MC descriptors from generated data.**

Include the generated files with these macro sections in
`CDMCTargetDesc.cpp`:

```cpp
#define GET_INSTRINFO_MC_DESC
#include "CDGenInstrInfo.inc"
#define GET_SUBTARGETINFO_MC_DESC
#include "CDGenSubtargetInfo.inc"
#define GET_REGINFO_MC_DESC
#include "CDGenRegisterInfo.inc"
```

Change the factory functions to call `InitCDMCInstrInfo`,
`InitCDMCRegisterInfo(X, CD::R0)`, and `createCDMCSubtargetInfoImpl(TT, CPU,
CPU, FS)`. Keep the existing text-only `MCAsmInfo` and do not register an
object streamer.

- [x] **Step 3: Reconfigure and build the generated target.**

Run:

```bash
cmake -S llvm -B build-cd -G Ninja
ninja -C build-cd CDCommonTableGen CDTests llc
```

Expected: the generated `.inc` files are created, `CDTests` links, and `llc`
still builds without changing direct emission.

## Task 4: Verify the foundation and commit it

- [x] **Step 1: Run the generated descriptor and direct-path gates.**

Run:

```bash
build-cd/unittests/Target/CD/CDTests
build-cd/bin/llvm-lit -j1 llvm/test/CodeGen/CD
git diff --check
```

Expected: `9/9` CD unit tests and `12/12` CD lit tests pass; the lit run is
single-process because the repository's lit timing file must not be written by
concurrent runners.

- [ ] **Step 2: Inspect the staged scope.**

Stage only the design document, five TableGen files, the two CD build/MC files,
and the roadmap. Do not stage the nested `cd-compiler/` checkout.

- [ ] **Step 3: Commit the foundation.**

```bash
git add docs/cd-bytecode-machine-backend.md \
  docs/superpowers/plans/2026-08-01-cd-tablegen-foundation.md \
  docs/superpowers/plans/2026-07-31-cd-bytecode-roadmap.md \
  llvm/lib/Target/CD/CD.td \
  llvm/lib/Target/CD/CDInstrInfo.td \
  llvm/lib/Target/CD/CDRegisterInfo.td \
  llvm/lib/Target/CD/CDCallingConv.td \
  llvm/lib/Target/CD/CDSubtarget.td \
  llvm/lib/Target/CD/CMakeLists.txt \
  llvm/lib/Target/CD/MCTargetDesc/CDMCTargetDesc.cpp
git diff --cached --check
git commit -m "feat(cd): add TableGen machine descriptors"
```
