# CD dynamic-value transport ABI decision

Date: 2026-08-03  
Status: decision complete; implementation is a separate follow-on slice

## Decision summary

The first cross-function dynamic-value boundary will use explicit function ABI
attributes together with the existing address-space-zero opaque-pointer shape:

```text
"cd.value.params"="0,2"
"cd.value.return"
```

`cd.value.params` is a function string attribute containing a sorted,
comma-separated list of zero-based parameter indexes whose LLVM type is an
address-space-zero `ptr`. `cd.value.return` is a marker function attribute
which is valid only on a function returning an address-space-zero `ptr`.

These attributes declare a CD value boundary; they do not turn an arbitrary
pointer into a CD value. The shared CD ABI validator must still prove every
value entering or leaving the boundary from one of the accepted producers.
Every address-space-zero pointer parameter must be listed, and every
address-space-zero pointer return must carry `cd.value.return`. Ordinary
pointer parameters and returns are rejected rather than assigned a second
meaning.

The first implementation slice is defined function parameter/return transport.
It reuses the existing `Call` and `Return` artifact operations, the existing
function parameter metadata, and the Rust VM's current `Value` call-frame
behavior. It adds no `.cdbc 0.1` opcode, field, or version change. PHI/select,
one-slot local storage, and function-value callback transport remain separate
decisions and implementation slices.

## Why a boundary decision is required

The current LLVM value ABI intentionally treats `ptr addrspace(0)` as an
opaque token, not as a native address. `isCDValue` currently recognizes the
address-space-zero nil token and results of explicit `llvm.cd.*` producers.
Function arguments, ordinary call results, PHI/select results, loads, stores,
allocas, and pointer operations are not implicit CD provenance.

Both emitters currently enforce the same conservative boundary:

- function parameters are scalar-only;
- non-scalar function returns are rejected except for an all-nil pointer
  return shape;
- ordinary function calls accept scalar values and nil only;
- PHI/select and one-slot storage are scalar-only;
- explicit CD values can flow through local CD consumers and printing, but not
  through ordinary function boundaries.

The artifact and VM already have the runtime shape needed for this boundary.
`CDBody` carries parameter names and arity, and `Call`/`Return` carry registers
without a static scalar-versus-dynamic tag. The Rust VM stores each argument in
its own parameter cell, clones a returned `Value` into the caller register, and
shares the reference-counted storage behind mutable array, map, and struct
handles. The missing contract is therefore LLVM-side provenance, not a new
bytecode representation.

## Provenance matrix

The matrix uses “CD token” for an LLVM `ptr` value whose provenance has been
validated as a dynamic VM `Value`. The pointer itself is never dereferenced by
the target or VM.

| Edge | Accepted producers and LLVM shape | Nil behavior | Ordinary-pointer rule | Aliasing and mutation | Status |
| --- | --- | --- | --- | --- | --- |
| Explicit intrinsic result -> defined function parameter | A direct `ptr` result of a validated explicit CD intrinsic, a marked CD-return call result, a marked CD parameter, or an address-space-zero `ptr null` may be passed to a parameter listed by `cd.value.params`. Mixed scalar and CD parameters are allowed. | Address-space-zero `ptr null` is CD `nil`. Other address spaces are not nil for this ABI. | The call-site argument must have proven CD provenance. A global, alloca address, GEP, bitcast, addrspacecast, ordinary load, or other unproven pointer is rejected. | Passing a scalar copies it. Strings are immutable values. Array, map, and struct handles retain shared VM storage, so `assign_index`/`assign_field` in the callee is visible to the caller. Rebinding the parameter cell is local. | First slice |
| Defined function return -> caller result | A function with `cd.value.return` may return `ptr null`, a marked CD parameter, an explicit CD producer, or a call result from another marked CD-return function. The return type must be address-space-zero `ptr`. | `ptr null` returns VM `nil`; a non-null CD handle is returned as the same dynamic value capability. | Every pointer return must be explicitly marked and proven. A non-CD pointer return is a target error, not a nil conversion. | The caller receives a cloned `Value`; mutable handle identity remains shared, so mutations remain observable across the return boundary. Strings remain immutable. | First slice |
| Explicit value -> PHI/select | Future PHI/select propagation must accept only incoming values with one common proven capability. A `select` condition remains scalar `i1`; pointer PHI/select values remain address-space-zero `ptr`. `ptr null` is one valid incoming capability. | A selected or merged nil remains VM `nil`. Mixed nil/non-nil CD values are valid when all inputs are proven CD values. | Every incoming edge must be proven. `undef`, poison, ordinary pointers, and pointer arithmetic are rejected. | The operation has `Move` semantics: scalar/string values copy, while mutable handles retain shared storage. A later mutation through either alias is observable. | Deferred until parameter/return parity |
| Explicit value -> one-slot local storage | Future storage support is limited to one direct, non-volatile, non-atomic alloca whose stored element is an address-space-zero `ptr`; only direct `load`/`store` use is admitted. Stores must carry a proven CD value, and a load becomes a CD token only when the slot's store history is proven. The alloca address is storage, never a CD token. | Storing/loading address-space-zero `ptr null` stores/loads VM `nil`. | Indirect pointer aliases, GEPs, escaped allocas, arbitrary globals, and unproven `load ptr` values are rejected. | A store replaces the slot's value. A loaded array/map/struct handle still shares VM storage with the source; mutating the handle is visible, replacing the slot is not a mutation of the old handle. | Deferred until parameter/return parity |
| Function value -> callback native argument | A separate function-value ABI must explicitly materialize a VM `Value::Function` and declare its callback signature. An ordinary LLVM function pointer or opaque `ptr` is not such a value. | `ptr null` remains CD `nil` for the general value ABI, but is not a valid callback unless a future native capability explicitly admits an optional callback. | Current callback names and ordinary external calls remain rejected. No callback is inferred from a function symbol, `ptr`, or `llvm.cd.native` name alone. | The Rust VM's function identity and captured environment semantics must be audited per callback helper; callback transport is not implied by ordinary dynamic-value transport. | Separate ABI decision |

The first slice therefore recognizes only these new provenance sources in
`CDValueABI`:

1. a function `Argument` whose index is listed by `cd.value.params`;
2. a direct defined-function `CallBase` whose callee has `cd.value.return`;
3. the existing explicit intrinsic results and address-space-zero nil token.

PHI/select, load/store, and function-value classification must not be added to
this helper as incidental conveniences. Each gets its own source-shape rule,
fixture, and parity gate.

## Boundary mechanism comparison

### Explicit boundary intrinsic or attribute

An identity intrinsic such as `llvm.cd.param` or `llvm.cd.return` would make
the source spelling visible, but an intrinsic that simply accepts `ptr` would
also certify an ordinary pointer. Making it safe would require the intrinsic
to carry the same function-interface and call-site proof as an ABI attribute.
A pair of call/return intrinsics would additionally duplicate the existing
`Call`/`Return` artifact operations and create a second lowering path for
function control flow.

The chosen function attributes provide the proof at the actual interface:
the callee declares which pointer parameters and result are CD values, while
the common validator checks the value at every call and return. The attributes
are explicit LLVM IR ABI metadata, require no new intrinsic or pseudo, and are
available to both direct and machine paths through the same `Function` and
`CallBase` APIs.

### Provenance-checked address-space-zero pointer convention without a marker

Address space zero is necessary for the current opaque token representation but
is not sufficient as provenance. A convention that treats every `ptr` parameter
or return as a CD value would admit ordinary pointers. Inferring the meaning
only from observed call operands is also incomplete: uncalled functions,
recursive call graphs, declarations, and future LLVM transformations would
have no stable interface contract. Rejecting every ambiguous function avoids
the unsoundness but does not provide a usable dynamic-value boundary.

The chosen attributes retain AS0 as a shape check, but add the missing explicit
interface declaration. AS0 alone is never accepted as proof.

### New artifact-level value type

The Rust VM already executes every register as a dynamic `Value`, and the
existing artifact verifier already validates register references. Adding a
static value-kind field or a new register class to `cdbc 0.1` would change the
wire contract without solving the LLVM-side question of whether a `ptr` came
from a CD producer or an ordinary pointer operation. It would also require
synchronized parser, formatter, verifier, VM, direct-emitter, machine-emitter,
and parity changes. This is larger than the needed boundary and does not
improve the alias semantics already supplied by the VM.

## Exact first-slice contract

### Function attributes

The canonical LLVM IR spelling is:

```llvm
define ptr @identity(ptr %value) #0 {
entry:
  ret ptr %value
}

attributes #0 = { "cd.value.params"="0" "cd.value.return" }
```

For a mutating helper with a scalar argument and no dynamic return:

```llvm
define void @touch(ptr %object, i64 %replacement) #1 {
entry:
  ; an explicit llvm.cd.assign.field or llvm.cd.assign.index uses %object
  ret void
}

attributes #1 = { "cd.value.params"="0" }
```

The target must reject the following attribute errors before emission:

- a non-decimal, duplicate, unsorted, out-of-range, or empty parameter list;
- an index that names a non-pointer parameter;
- a pointer parameter omitted from `cd.value.params`;
- `cd.value.return` on a non-pointer return or absence of the marker on a
  pointer return;
- an address-space other than zero on a marked parameter or return;
- inconsistent attributes on a declaration/definition pair when that pair is
  present in the input module.

The first slice supports direct calls to defined functions only. Declarations,
indirect calls, function-pointer casts, ordinary external calls, and callback
natives remain rejected. `@main` continues to have no parameters. A marked
parameter may be consumed by an existing explicit CD intrinsic, passed to
another marked function, printed through the existing `ptr` print boundary,
or returned when the function carries `cd.value.return`. It may not be used by
pointer arithmetic, ordinary loads/stores, or PHI/select in this slice.

### Artifact mapping

No artifact change is required:

```text
rF = make_function f0
rD = call rF [rArg0, ...]
return rValue
```

The existing function `param` metadata continues to name the parameter cells.
The direct emitter keeps `Call` and `Return`; the machine bridge keeps
`CD_CALL` and `CD_RETURN`. The shared verifier remains responsible for
register and arity validity, and the Rust VM remains responsible for runtime
values, resource budgets, cancellation, and diagnostics.

### Fixture grammar and oracle checks

The implementation plan must include one positive direct/machine fixture with:

1. `llvm.cd.string` or `llvm.cd.struct` creating a local CD value;
2. an identity helper using `cd.value.params` and `cd.value.return`;
3. a mutating helper using a marked CD parameter and an explicit
   `assign_field` or `assign_index` operation;
4. caller-side observation after the helper returns;
5. a nil argument/return path; and
6. mixed scalar and CD parameters.

The negative corpus must cover an unmarked pointer parameter/return, an invalid
attribute index/list, a marked parameter called with an ordinary alloca/global
pointer, an ordinary pointer returned from a marked function, and direct versus
machine diagnostic parity. The positive artifact must pass Rust `dump`, and
`run` must prove both return transport and mutation visibility.

The Rust VM oracle checks are:

- each call argument is copied into a fresh parameter cell;
- returning a value copies it into the caller register;
- mutable array/map/struct handles remain shared across the call and return;
- `assign_field`/`assign_index` in the callee is visible to the caller;
- rebinding a callee parameter cell does not rebind the caller's register or
  local cell;
- nil remains nil, and no native pointer identity is exposed; and
- call-depth, instruction-budget, cancellation, and runtime-error behavior
  remain the existing VM behavior.

No nested VM change is required for this slice. A Rust test may be added only
if the outer fixture exposes a missing existing oracle behavior; it must remain
a separately reviewed nested-checkout action.

## Implementation boundary after this decision

The next implementation plan must first add a shared function-interface
validator and provenance classifier, invoked before either emitter lowers a
function. It must validate the attribute grammar once, classify arguments and
direct defined-function results consistently, and feed the same capability
checks into native/collection/record/variant consumers. Direct and machine
fixtures must use the same positive and malformed IR.

That implementation must not modify `CDBytecodeFormat`, the Rust VM, or the
artifact version merely to represent the first function-boundary slice. The
following remain explicitly out of scope until that slice has direct/machine
artifact and behavior parity:

- PHI/select propagation;
- one-slot dynamic local storage;
- function-value materialization and callback natives;
- arbitrary pointer or aggregate lowering; and
- any new `.cdbc` opcode or static register-kind section.

The baseline commands for the decision commit are the existing LLVM lit,
parity-unit, module-link-unit, Rust VM, whitespace, and nested-checkout status
gates in the active development plan. The decision record itself must be
committed separately from the later implementation.
