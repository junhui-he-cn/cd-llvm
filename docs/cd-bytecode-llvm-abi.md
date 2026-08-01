# LLVM CD value ABI

Status: M4 string-constant and array-constructor slices implemented, 2026-08-01.

This document defines the boundary between LLVM IR values and the dynamic
values consumed by the `cdbc 0.1` Rust VM.  It is intentionally target-specific:
ordinary LLVM aggregates, pointers, globals, and calls are not silently
reinterpreted as CD arrays, maps, structs, or native values.

## Scope and invariants

The supported pipeline remains:

```text
LLVM IR --llc -mtriple=cd-unknown-unknown--> cdbc 0.1 --> Rust VM
```

The direct `ModulePass` emitter remains the compatibility path and the
TableGen machine emitter remains opt-in.  Both paths must recognize the same
`llvm.cd.*` operations and lower them through `CDBytecodeFormat`; neither path
may write a second wire representation.

The ABI follows these rules:

1. A CD value that has no faithful native LLVM type is carried in LLVM IR as an
   opaque `ptr` SSA token only at a defined `llvm.cd.*` boundary.  It is not a
   native address and it must not be dereferenced, compared, indexed, passed
   to an ordinary external call, or used by an ordinary pointer operation.
2. A target-specific intrinsic is the proof that an opaque pointer is a CD
   value.  An arbitrary LLVM `ptr` remains unsupported, including a pointer
   produced by `alloca`, `load`, `getelementptr`, `inttoptr`, or a global whose
   contents are not consumed by a defined CD intrinsic.
3. Immutable constants are module-owned and are interned by their complete CD
   value.  The generated instruction still materializes a VM register with
   the existing `constant cN` operation.
4. Intrinsic validation is performed during LLVM lowering.  Invalid ABI shapes
   are compile-time target errors; they are never emitted as a best-effort
   `nil`, number, or native pointer.
5. The Rust VM's `cdbc 0.1` parser, validator, and executor remain the runtime
   oracle.  An intrinsic is not enabled until `dump` and `run` accept its
   artifact and direct/machine parity covers it.

## Intrinsic naming and registration

CD intrinsics are registered in `llvm/include/llvm/IR/IntrinsicsCD.td`, which
is included by `llvm/include/llvm/IR/Intrinsics.td`.  The first intrinsic is:

```tablegen
def int_cd_string : DefaultAttrsIntrinsic<[llvm_ptr_ty], [llvm_ptr_ty],
                                          [IntrNoMem]>;
```

Its canonical IR spelling is `llvm.cd.string`.  The return type is an opaque CD
string token represented by `ptr`; the argument is a pointer to the immutable
LLVM string global described below.  The intrinsic has no runtime memory
access: the target reads the initializer while lowering and emits a constant
table entry.

The first collection intrinsic is:

```tablegen
def int_cd_array : DefaultAttrsIntrinsic<
    [llvm_ptr_ty], [llvm_i32_ty, llvm_vararg_ty],
    [ImmArg<ArgIndex<0>>]>;
```

Its canonical IR spelling is `llvm.cd.array`.  It returns an opaque CD array
token represented by `ptr` and accepts an immediate `i32` element count followed
by a variadic list of element operands.  The count must exactly match the
number of following operands; zero is valid.  Since this is a variadic LLVM
intrinsic, textual IR spells the call type explicitly, for example
`call ptr (i32, ...) @llvm.cd.array(i32 0)`.  The intrinsic deliberately has no
`IntrNoMem` property: array construction creates a fresh mutable VM object and
must not be treated as a pure, freely CSE-able value by LLVM optimization.

The intrinsic must be used with the registered declaration and exact `ptr`
signature.  A manually declared function with a similar name is not a CD ABI
operation and remains an ordinary unsupported declaration.

## String constants: first ABI group

### Accepted source shape

The operand of `llvm.cd.string` must be a direct pointer to a private-linkage,
constant, address-space-zero `GlobalVariable` with an array of 8-bit
elements.  The array must end in exactly one zero byte.  LLVM canonicalizes a
one-byte all-zero initializer to `zeroinitializer`; that representation is
accepted for the empty string.  The emitted CD string is every byte before
that terminator.

For example:

```llvm
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @message)
  call void @cd_print(ptr %value)
  ret i32 0
}
```

The first slice rejects a GEP or bitcast around the global, a mutable or
externally defined global, a non-byte array, a missing terminator, more than
one trailing zero, embedded zero bytes, non-UTF-8 bytes, and a non-global
operand.  These restrictions keep the initial contract deterministic and avoid
silently turning runtime pointer arithmetic into a string operation.  A later
ABI revision may accept canonical constant GEPs without changing the wire
format.

### Wire and runtime behavior

The global initializer becomes a `CDConstant::String` entry:

```text
constants:
  c0 = string "hello"
```

The intrinsic result materializes that entry through the existing `constant`
instruction.  Identical UTF-8 payloads share one constant-table entry across
all functions.  The value is immutable; no intrinsic in this group mutates or
aliases the LLVM global.  The Rust VM treats it as its existing immutable
string value, and `cd_print`/`print` observes the same escaped text used by the
`cdbc 0.1` formatter.

The direct and machine paths may assign different VM register numbers, but
they must emit the same string constant payload and produce identical `dump`
validation and `run` output.  No new wire opcode is introduced for this group.

### Allowed consumers in the first slice

The resulting CD string token may currently be:

- passed to `cd_print` or `print` when the declaration has one `ptr` argument
  and returns `void`;

Ordinary scalar operations, pointer comparisons, loads/stores, ordinary calls,
and native calls do not accept this token in the first slice.  Function
returns, PHI/select propagation, and parameters carrying CD strings are
deferred until the function-value ABI is specified; the implementation
restricts string tokens to local materialization and printing.

## Future ABI groups

The following signatures are reserved conceptually but are not enabled by this
document:

| Group | Required operation shape | Wire operations | Design dependency |
| --- | --- | --- | --- |
| Arrays/maps | explicit `llvm.cd.*` constructors and mutation operations | `array`, `map`, `index`, `assign_index`, `len`, `assert_array` | ownership, aliasing, mutation failure, element capability matrix |
| Records | explicit field/type-name operands | `struct`, `field`, `assign_field` | nominal type-name encoding and field order |
| Variants | explicit enum/variant names and ordered payload operands | `variant`, `variant_tag`, `variant_field` | payload layout and invalid-tag behavior |
| Native calls | allowlisted name-table identity and typed capability matrix | `native_call` | Rust VM allowlist and argument/result validation |

Each group requires its own intrinsic signatures, malformed-input fixtures,
Rust parser/validator coverage, and direct/machine parity before it can be
implemented.  Existing `cdbc 0.1` opcodes are not permission to map arbitrary
LLVM IR instructions to those operations.

## Array constructor: first collection ABI group

### Accepted source shape

The operands after the immediate count of `llvm.cd.array` are the complete array
payload.  Each payload operand must be one of:

- an integer or floating-point scalar, using the existing CD number/bool
  conversion rules;
- a `ptr null` constant, which becomes CD `nil`;
- the result of `llvm.cd.string`; or
- the result of `llvm.cd.array`.

An arbitrary pointer, aggregate, vector, function value, `undef`, poison, or
ordinary pointer operation is not an array element.  The target validates every
operand during lowering, in both direct and machine paths, and reports a
target error instead of emitting a partially typed array.

The result is local to the call in this first slice.  It may be passed to
`cd_print`/`print`, or used as an element of another `llvm.cd.array` call.  It
may not yet be returned from a function, passed to an ordinary function,
stored in an ordinary alloca, selected by `select`, or used by pointer
operations.  `index`, `assign_index`, `len`, and `assert_array` are separate
intrinsics and are not inferred from ordinary LLVM operations.

### Ownership, aliasing, and failure behavior

The `array` bytecode instruction allocates a new mutable array and copies the
element values into its slots.  Scalar, nil, and string elements have value
semantics for this purpose.  Nested arrays are copied as dynamic value handles:
the nested array storage is shared, so a later explicit mutation of that
nested value is observable through every alias.  The outer array itself is
never an alias of an input array.

Construction does not validate element types at runtime because LLVM lowering
has already proven the capability of each operand.  Runtime allocation can
still fail under the VM resource budget; that failure is a runtime error and
must not be converted to `nil` or a compile-time diagnostic.  The existing Rust
VM parser, verifier, and executor own the `array` wire spelling and its
allocation behavior.

### Wire and runtime behavior

An array constructor lowers to the existing instruction form:

```text
rD = array [r0, r1, ...]
```

No new artifact version or native pointer representation is introduced.  The
direct and machine paths share the typed artifact model, and their only
permitted difference is virtual/register numbering.  Printing the result uses
the Rust VM's existing array formatting, including nested values.

## Verification contract

The string group is complete only when all of the following are true:

- `llvm.cd.string` is generated by TableGen and accepted by LLVM IR parsing;
- direct and machine `llc` paths emit a `string` constant and reject invalid
  global/operand shapes with CD-target diagnostics;
- the generated artifact passes Rust `dump` and executes through Rust `run`;
- ASCII, multi-byte UTF-8, escape characters, deduplication, and empty strings
  are covered;
- direct/machine parity includes the positive and negative fixtures; and
- ordinary pointer/global use remains rejected.

The array-constructor group is complete only when `llvm.cd.array` accepts the
documented operand capabilities, emits `array`, rejects ordinary pointer and
aggregate substitutes, passes Rust `dump` and `run`, and has direct/machine
artifact and runtime parity for empty, mixed, and nested values.

The sibling `cd-compiler` checkout already defines `string` constants in the
`cdbc 0.1` parser, formatter, and VM.  This first group therefore changes the
LLVM artifact model and lowering only; it does not add a new Rust opcode or
alter the artifact version.
