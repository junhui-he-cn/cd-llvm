# CD bytecode target

The experimental `CD` target is a software target for the Compiler Design
bytecode VM.  It is selected with:

```text
llc -mtriple=cd-unknown-unknown input.ll -o output.cdbc
```

The target emits the text `cdbc 0.2` artifact format consumed by the Rust VM in
the sibling `cd-compiler` project.  The output contains deterministic constant
and name tables, a `main` body, and function bodies.  The current capability
reference is [docs/cd-bytecode-features.md](../../../../docs/cd-bytecode-features.md).
LLVM function arguments
are represented by `param` metadata and an initial numeric `load_local`; direct calls
materialize a function value with `make_function` before using `call`.

The initial lowering supports scalar integer and floating-point constants,
arithmetic, comparisons, scalar casts as `move`, direct single-slot `alloca`
storage with scalar or proven dynamic-CD `load`/`store`, direct calls to defined
functions, conditional and unconditional branches, PHI edge stores, returns, and declarations named
`cd_print` or `print` with one argument.  `fneg` lowers to `negate`.  A scalar
or dynamic CD `select` lowers to a small conditional control-flow sequence using
0.2 `br_if`, `move`, and `br` block instructions.  The only supported `xor` form is boolean
inversion: an `i1` value XORed with the literal `true` (in either operand order)
lowers to `not`; other XOR operations remain unsupported.  Unsupported LLVM
instructions fail with a CD-target diagnostic rather than producing an invalid
artifact. Address-space-zero `ptr null` remains the CD `nil` value. Pointer
function parameters and returns require the explicit `cd.value.params` and
`cd.value.return` attributes described below; ordinary pointer interfaces remain
unsupported. A dynamic CD `select` uses the same sequence when its `i1`
condition selects between two proven address-space-zero CD tokens. A dynamic
CD PHI uses existing edge stores and block-entry loads when every incoming
address-space-zero `ptr` is proven. A dynamic storage slot is limited to one
non-volatile, non-atomic `alloca ptr` with direct load/store users; every store
must carry proven CD provenance and every load must follow a proven store on
all control-flow paths.

The emitter builds a typed `llvm::cd::CDArtifact` before writing anything. Its
`CDBytecodeFormat` validator checks table references, register operands, branch
targets, instruction shapes, function parameter records, and finite constants;
the canonical serializer is the only component that spells the `cdbc 0.2`
wire format. This boundary is also the input contract for the later TableGen-
backed machine path.

The opt-in TableGen-backed path is selected with `-cd-backend=machine`. It
lowers the same scalar subset into CD virtual-value `MachineInstr`
pseudo-operations, including branches and PHI edge stores, and then reuses
`CDBytecodeFormat` for artifact validation and serialization. A conditional
edge gets a synthetic machine block when it needs PHI stores, and machine
block operands are patched to final bytecode instruction offsets. The direct
emitter remains the default compatibility path until direct/machine Rust VM
parity is gated.

## Module artifacts

Program mode is the default. Use `-cd-artifact=module` to emit the existing
Rust VM `cdbc 0.2` module envelope through either the direct or machine path:

```text
llc -mtriple=cd-unknown-unknown -cd-artifact=module input.ll -o module.cdbc
llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module input.ll -o module-machine.cdbc
```

Module mode requires exactly one `!cd.module` record with positional
`identity`, `path`, `canonical_path`, `i1 entry`, and optional `i64
entry_order` operands. An optional `!cd.dependencies` list contains ordered
`kind`, target identity, local lowering anchor, and requested path records; the
anchor is used to place an `init_module mN` in the initializer and is not part
of the 0.2 wire record.
The shared typed artifact validator enforces non-empty UTF-8 identities and
paths, entry-order consistency, ordered in-range offsets, and the allowlisted
dependency kinds. The serializer writes the VM's existing `artifact: module`
header before the ordinary sections. Program mode rejects these metadata nodes
instead of silently dropping them.

`llvm-link` does not combine module products: because it concatenates named
metadata, the target rejects a linked IR module with multiple `!cd.module`
records. The Rust VM's module-aware `link` command remains responsible for
resolving dependency identities and expanding products. Non-entry module
products keep top-level work in their initializer function; the emitted module
`main` is an empty executable stub.
The opt-in `llvm/utils/cd_module_link.py` harness covers valid direct/machine
products, linked execution, unlinked-run rejection, and missing-dependency,
cycle runtime rejection, duplicate-identity, entry-order, and invalid
initializer-reference failures.
With explicit source metadata, the same harness compares linked divide-by-zero
diagnostics and confirms the debugger error pause retains the dependency's
module identity.
Run it explicitly with the LLVM build and sibling VM paths, for example:

```text
python3 llvm/utils/cd_module_link.py --llc build-cd/bin/llc \
  --vm /path/to/cd-compiler/vm-rs
```

The VM integration test is opt-in and requires an explicit checkout root:

```text
CD_COMPILER_ROOT="$PWD/cd-compiler" \
  build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-vm-integration.py
```

Without CD_COMPILER_ROOT, the integration test is unsupported and the
LLVM-only CD suite does not inspect or mutate a sibling checkout. The
driver/output boundary, including the upstream `-g` rejection, is covered by
`llvm/test/CodeGen/CD/cdbc-driver-options.ll`. The reproducible verification
matrix is recorded in
docs/cd-bytecode-verification.md.

Integer constants are accepted only when their signed value is exactly
representable as an IEEE-754 double; otherwise the target reports a diagnostic
instead of silently changing the value. Number constants are serialized with a
canonical `%.17g` spelling after finite-value validation.

The target does not reinterpret unsigned integer division or unsigned ordering
predicates as floating-point `number` operations: `udiv` and unsigned `icmp`
ordering predicates are rejected. Integer equality and inequality remain
supported because they do not select a signed or unsigned ordering.

The first M4 value-ABI operation is `llvm.cd.string(ptr)`. It accepts only a
private-linkage, constant, address-space-zero byte global with exactly one
trailing NUL and valid UTF-8 before that terminator; LLVM's canonical
one-byte-zero initializer represents the empty string. The result is lowered
to a deduplicated `string` constant and may currently be passed only to a
one-argument `cd_print` or `print` declaration, or across the marked
function-boundary ABI below. Ordinary pointer operations, comparisons, storage,
unmarked function parameters/returns, and non-string globals remain rejected.

The first dynamic-value transport boundary uses the function attributes
`"cd.value.params"="0,2"` and `"cd.value.return"`. The parameter attribute is
a sorted, comma-separated list of pointer parameter indexes; every listed and
every pointer parameter must be an address-space-zero `ptr`. The return marker
is required for an address-space-zero pointer return. The shared validator
accepts only CD nil, explicit `llvm.cd.*` results, marked CD parameters, and
direct calls to defined marked-return functions as boundary values. Alloca or
global addresses, pointer arithmetic/casts, indirect calls, and unmarked
pointer interfaces fail in both direct and machine paths. The existing
`param`, `call`, and `return` artifact operations carry the values, so no new
wire operation or artifact version is needed.

The Rust VM copies arguments into fresh parameter cells and copies return
values into the caller register. Mutable array, map, and struct handles keep
shared backing storage across the call, so explicit callee mutation is visible;
rebinding a parameter is local. Dynamic CD `select`, PHI propagation, and
one-slot storage are covered by `cdbc-dynamic-select.ll`,
`cdbc-dynamic-phi.ll`, and `cdbc-dynamic-storage.ll`; arbitrary pointer
storage remains rejected. The positive and malformed function boundaries are
covered by `cdbc-function-values.ll` and `cdbc-function-value-errors.ll`.

The first M4 collection operation is `llvm.cd.array(i32 count, ...)`. The
immediate count must equal the number of following scalar, address-space-zero
`nil`, string-token, or array-token operands. The variadic textual IR call type
must be explicit (`call ptr (i32, ...) @llvm.cd.array(...)`). The target lowers
the payload to the existing `array` instruction; array results may be printed
or nested in another constructor, while ordinary pointer, aggregate, and
arbitrary storage,
function-parameter/return, arbitrary storage, and ordinary `select`/PHI uses
remain rejected. A dynamic CD `select` or PHI is accepted only for proven
address-space-zero tokens. The direct and
machine paths validate the same capability matrix and share the same artifact
bridge.

The first M4 array-access operations use `llvm.cd.index(ptr, double)` and
`llvm.cd.len(ptr)`. Their collection operands must be explicit CD dynamic-value
tokens (or the CD nil token), not arbitrary LLVM pointers. They lower to the
0.2 `index` and `len` instructions. `llvm.cd.assert.array(ptr)` has no 0.2
opcode and is rejected during lowering. Index results remain dynamic-value
tokens so scalar and nested array elements can share one ABI; `len` returns a
CD number as `double`. The direct and machine paths share validation, artifact
serialization, and Rust VM output parity. These results remain local and
cannot cross ordinary function, pointer, or arbitrary storage boundaries; the
supported one-slot dynamic storage rule may carry proven results through
numeric local/global cell operations. Dynamic CD `select` and PHI may choose or
merge proven results.

`llvm.cd.assign.index` is the explicit array/map mutation boundary. Its
collection and index must be a CD dynamic-value token and `double`; its value
and result use the same scalar-or-CD-token overload capability. It lowers to
the existing `assign_index` instruction, mutates the VM handle in place, and
returns the assigned value. Ordinary pointers, aggregates, and LLVM stores do
not imply mutation, and the result remains local in this slice. Direct and
machine lowering share the validator and the Rust VM runtime-error/parity
gate.

`llvm.cd.map(i32 count, ...)` is the explicit map-constructor boundary. The
count is an immediate entry count and the variadic payload is an even
key/value list. Keys are scalar, CD nil, or string tokens; values use the
existing scalar, nil, string, array, map, and local CD-token capability matrix.
It lowers to `map [rKey: rValue, ...]` through both direct and opt-in machine
paths. Duplicate keys retain first insertion position while the last value
wins, and map lookup/resource failures remain Rust VM runtime behavior.

The first record-value boundary is the `llvm.cd.struct` /
`llvm.cd.field` / `llvm.cd.assign.field` intrinsic group. `struct` takes an
optional anonymous `ptr null` or private UTF-8 type-name global, an immediate
field count, and alternating private UTF-8 field-name globals and scalar, nil,
or explicit CD-value operands. It lowers to `struct`, preserving field order
and name-table identity. `field` reads a named field and `assign.field` mutates
the struct in place while returning the assigned value; their overloaded
results are limited to scalar values or address-space-zero CD tokens. Ordinary
pointers, aggregates, and name globals used outside this explicit ABI remain
unsupported, and the direct/machine paths share the same validator and artifact
bridge.

The enum-variant boundary is the `llvm.cd.variant` /
`llvm.cd.variant.tag` / `llvm.cd.variant.field` intrinsic group. `variant`
takes private, constant, non-empty UTF-8 enum and variant name globals, an
immediate payload count, and scalar, nil, or explicit CD-value payloads. It
lowers to `make_variant tType, vVariant [rPayload0, ...]`; `is_variant` compares
the numeric type/variant IDs and returns a boolean, while `variant_get` reads an
immediate positional payload index. These map to the existing Rust VM operations and
preserve its non-variant and out-of-bounds runtime diagnostics. Names remain
name-table metadata, ordinary pointers and aggregates remain unsupported, and
variant values stay local to explicit CD intrinsic consumers in this slice.

The bounded native-call slice implements `llvm.cd.native(ptr name, ...)` for
the allowlisted names `floor`, `ceil`, `sqrt`, `str`, `typeOf`, `hash`, `contains`,
`slice`, `copy`, `concat`, `push`, `pop`, `remove`, `clear`, `merge`, `keys`, `values`, `range`, `substr`, `charAt`, and the callback helpers `map`, `filter`, `flatMap`, `reduce`,
`any`, `all`, `count`, `find`, and `findIndex`.
The name must be a private constant UTF-8 global; each name is interned into the
0.2 `native_imports` table and has an exact
scalar/CD-value argument
and result signature recorded in `docs/cd-bytecode-llvm-abi.md`. `substr` and
`charAt` accept explicit CD dynamic-value tokens and leave string type, Unicode
scalar boundaries, integer-valuedness, and bounds errors to the Rust VM.
`contains` accepts a proven CD dynamic-value collection and a scalar or proven
CD dynamic-value needle, returns exact `i1`, and leaves array membership, map
key validation, range integer membership, and runtime type errors to the Rust
VM. Ordinary pointer substitutes remain rejected. `slice` accepts a proven CD
dynamic-value token, two `double` operands, and an address-space-zero `ptr`
result; the VM owns the runtime array check, integer-valuedness, start/length
bounds, snapshot, and fresh shallow-array semantics.
`copy` accepts a proven CD dynamic-value token and an address-space-zero `ptr`
result; the VM owns the runtime array check, snapshot, and fresh shallow-array
semantics.
`concat` accepts two proven CD dynamic-value tokens and an address-space-zero
`ptr` result; the VM owns both runtime array checks, snapshot order, and fresh
shallow-array semantics.
`push` accepts a proven CD dynamic-value array, a scalar or proven CD
dynamic-value value, and an address-space-zero `ptr` result; the VM owns the
runtime array check, append mutation, resource checkpoint, and nil result.
`pop` accepts a proven CD dynamic-value array and an address-space-zero `ptr`
result; the VM owns the runtime array check, last-element mutation, removed
value return, and empty-array diagnostic.
`remove` accepts a proven CD dynamic-value map, a scalar or proven CD
dynamic-value key, and an address-space-zero `ptr` result; the VM owns map/key
validation, first-match mutation, removed-value return, and missing-key errors.
`clear` accepts a proven CD dynamic-value map and an address-space-zero `ptr`
result; the VM owns the runtime map check, shared map mutation, and nil result.
`merge` accepts two proven CD dynamic-value maps and an address-space-zero `ptr`
result; the VM owns fresh allocation, left-to-right insertion order, and
right-side duplicate replacement while retaining the first key position.
`keys` accepts a proven CD dynamic-value token and an address-space-zero `ptr`
result; the VM owns the runtime map check, insertion-order snapshot, and fresh
array semantics.
`values` accepts a proven CD dynamic-value token and an address-space-zero `ptr`
result; the VM owns the runtime map check, insertion-order snapshot, and fresh
array semantics.
`map` accepts one CD token and a direct defined callback with the explicit
`cd.value.params="0"`/`cd.value.return` ABI markers; `filter` accepts one CD
token and a direct defined predicate with `cd.value.params="0"` and an `i1`
result. `any` and `all` accept the same direct predicate shape, return exact
`i1`, and use the empty-array and short-circuit behavior owned by the Rust VM.
`count` accepts the same direct predicate shape, returns an exact `double`, and
counts every true predicate result, including zero for an empty array.
`find` accepts the same direct predicate shape, returns an exact address-space-zero
`ptr`, and returns the first matching element or `nil` for an empty/no-match
array. A non-array input retains the VM error `find expects array as first
argument`; callback arity, predicate result, snapshot iteration, budget, and
cancellation remain VM-owned.
`findIndex` accepts the same direct predicate shape, returns an exact `double`,
and returns the zero-based first matching index or `-1` for an empty/no-match
array. A non-array input retains the VM error `findIndex expects array as first
argument`; callback arity, predicate result, snapshot iteration, budget, and
cancellation remain VM-owned.
`flatMap` accepts the same direct callback shape as `map`, returns an exact
address-space-zero `ptr`, and performs one-level callback-array flattening. A
non-array input retains the VM error `flatMap expects array as first argument`;
callback result type, snapshot iteration, checkpoints, budget, cancellation,
and callback failures remain VM-owned.
`reduce` accepts a proven CD array token, a scalar or CD initial value, and a
direct two-parameter callback with `cd.value.params="0,1"` and a marked CD
pointer return. It returns an exact address-space-zero `ptr`; the VM owns empty
input identity, left-to-right accumulator threading, callback frames, native
checkpoints, budget, cancellation, and the `reduce expects array as first
argument` runtime diagnostic.
All callback values are materialized with `make_function` before `call_native`.
Direct and opt-in machine lowering share the `call_native iN` artifact bridge and
parity coverage. Unsupported names, incomplete callback markers, and ordinary
pointer arguments remain rejected.

The M5A debug-source-table foundation accepts an explicit `!cd.sources` named
metadata node with `path,text` or `module,path,text` string records. It validates
UTF-8, non-empty identities, and duplicate `(module,path)` entries, then emits
the optional `debug_sources` section in both artifact paths. When a source
table is present, matching non-zero `DILocation` metadata is emitted as sparse
`debug_locations` entries through both paths; `-g` and ordinary `llvm.dbg.*`
metadata still do not supply source bytes. Unmatched locations are omitted,
ambiguous source-path matches are rejected. The Rust VM parity fixture covers
the resulting source line, caret, and nested call stack for divide-by-zero;
the bounded `sqrt(-1)` native failure has the same source-backed main call
stack, and an out-of-range array index has the same source-backed main call
stack. An explicit `!cd.ranges` named metadata table can add validated
source-local half-open byte ranges keyed by `DILocation`; ranges are never
inferred from line/column metadata. The direct and machine paths share the
same `debug_ranges` serializer and parity coverage. Opt-in direct/machine
parity covers range and metadata-free observability, step/next and their short
aliases, help output, line-breakpoint deletion, and source-backed runtime-error
pauses; broader interactive debugger behavior remains deferred.

`cdbc-optimization.ll` checks direct `llc` emission at `-O0` and `-O2`, and
also runs LLVM's explicit `default<O2>` middle-end pipeline before emission.
The optimized artifact exercises alloca promotion, constant folding, dead-code
elimination, and scalar-select lowering; its Rust VM `dump` and `run` results
must remain canonical and observable-equivalent to the `-O0` artifact.

This target deliberately has no object-file, assembler, JIT, or arbitrary
native-call output. `-filetype=obj` is rejected. Native calls beyond the
bounded group and non-string globals remain deferred until separate LLVM-to-CD
representations and parity gates are defined.
