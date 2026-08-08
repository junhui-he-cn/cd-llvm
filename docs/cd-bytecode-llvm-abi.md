# LLVM CD value ABI

Status: M4 string-constant, array-constructor, array-access, array-mutation,
map-constructor, record-value, enum-variant, bounded native-call, and `map` /
`filter` / `flatMap` / `reduce` / `any` / `all` / `count` / `find` / `findIndex`
callback-native slices implemented; M5 explicit debug-source-table,
instruction-location, source-backed runtime-diagnostic, and debug-range slices
implemented; M6 module-envelope and opt-in linker integration implemented; the
first M8 dynamic-value function parameter/return transport slice is
implemented, 2026-08-08. PHI/select and dynamic local storage remain deferred;
future callback helpers remain outside the selected
`map`/`filter`/`flatMap`/`reduce`/`any`/`all`/`count`/`find`/`findIndex` slices.

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
   opaque `ptr` SSA token only at a defined `llvm.cd.*` boundary or an explicit
   marked function boundary. It is not a native address and it must not be
   dereferenced, compared, indexed, passed to an ordinary external call, or
   used by an ordinary pointer operation.
2. A target-specific intrinsic or a validated `cd.value.params` /
   `cd.value.return` function ABI marker is the proof that an opaque pointer is
   a CD value. An arbitrary LLVM `ptr` remains unsupported, including a pointer
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

The first array-access operations use these signatures:

```tablegen
def int_cd_index : DefaultAttrsIntrinsic<[llvm_ptr_ty],
                                         [llvm_ptr_ty, llvm_double_ty]>;
def int_cd_len : DefaultAttrsIntrinsic<[llvm_double_ty], [llvm_ptr_ty]>;
def int_cd_assert_array : DefaultAttrsIntrinsic<[llvm_ptr_ty], [llvm_ptr_ty]>;
```

`llvm.cd.index` takes a CD dynamic-value token and a CD number index. Its
result is another dynamic-value token because an array slot may contain any
value capability admitted by `llvm.cd.array`. `llvm.cd.len` returns the CD
number representation as `double`. `llvm.cd.assert.array` preserves the
existing VM assertion/conversion operation: it returns an array-compatible
dynamic value or raises the VM's normal runtime error. These operations are
not pure in the LLVM sense; their declarations intentionally retain the
default memory/side-effect properties so an optimizer cannot erase a bounds
or type failure or duplicate a mutable-value observation.

The first record-value group uses these signatures:

```tablegen
def int_cd_struct : DefaultAttrsIntrinsic<
    [llvm_ptr_ty], [llvm_ptr_ty, llvm_i32_ty, llvm_vararg_ty],
    [ImmArg<ArgIndex<1>>]>;
def int_cd_field : DefaultAttrsIntrinsic<[llvm_any_ty],
                                         [llvm_ptr_ty, llvm_ptr_ty]>;
def int_cd_assign_field : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_ptr_ty, LLVMMatchType<0>]>;
```

`llvm.cd.struct` returns an address-space-zero opaque CD struct token. Its first
operand is either `ptr null` for an anonymous record or a direct private,
constant, non-empty UTF-8 string global for the nominal type name. The second
operand is an immediate `i32` field count, followed by exactly one field-name
and value pair per field. Field names use the same private string-global shape,
but are name-table metadata rather than CD string values. `llvm.cd.field` reads
one named field, and `llvm.cd.assign.field` mutates one named field and returns
the assigned value. Their overloaded LLVM results are restricted during target
lowering to scalar values or address-space-zero CD value pointers; assignment
results must have exactly the assigned value type.

The enum-variant group uses these signatures:

```tablegen
def int_cd_variant : DefaultAttrsIntrinsic<
    [llvm_ptr_ty], [llvm_ptr_ty, llvm_ptr_ty, llvm_i32_ty, llvm_vararg_ty],
    [ImmArg<ArgIndex<2>>]>;
def int_cd_variant_tag : DefaultAttrsIntrinsic<
    [llvm_i1_ty], [llvm_ptr_ty, llvm_ptr_ty, llvm_ptr_ty]>;
def int_cd_variant_field : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_i32_ty],
    [ImmArg<ArgIndex<1>>]>;
```

`llvm.cd.variant` takes private, constant, non-empty UTF-8 enum and variant
name globals, an immediate `i32` payload count, and exactly one scalar, CD
nil, or explicit CD-value operand per payload field. `llvm.cd.variant.tag`
returns `i1` after checking the value against the two name-table identities.
`llvm.cd.variant.field` takes an immediate `i32` payload index and has the
same scalar-or-address-space-zero CD-value result restriction as record field
access. Names are metadata, not CD string values.

The bounded native-call group uses this signature:

```tablegen
def int_cd_native : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_vararg_ty]>;
```

Its first operand is a private, constant, non-empty UTF-8 name global. The
remaining operands and result are checked against the name-specific capability
matrix in the native-call section below. This intrinsic is the only LLVM
boundary that can emit a `native_call`; ordinary external declarations remain
unsupported.

Each operation must use its registered intrinsic declaration and declared
signature. A manually declared function with a similar name is not a CD ABI
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

Ordinary scalar operations, pointer comparisons, loads/stores, unmarked calls,
and native calls do not accept this token. Marked function parameters and
returns are covered by the function-boundary slice below; PHI/select
propagation and dynamic local storage remain deferred.

## Function-boundary dynamic values: first transport slice

The first cross-function transport boundary uses explicit function attributes
and reuses the existing `Call` and `Return` artifact operations:

```llvm
define ptr @identity(ptr %value) #0 {
entry:
  ret ptr %value
}

attributes #0 = { "cd.value.params"="0" "cd.value.return" }
```

`cd.value.params` is a non-empty, strictly increasing, comma-separated list of
zero-based parameter indexes. Every listed parameter must be an
address-space-zero `ptr`, and every address-space-zero pointer parameter must
be listed. `cd.value.return` is a marker valid only on a function returning an
address-space-zero `ptr`; every pointer-returning function must carry it.
Malformed lists, duplicate or unsorted indexes, out-of-range indexes, foreign
address spaces, and markers on scalar or aggregate types are target errors.

A marked parameter or pointer return accepts only a proven CD value: an
address-space-zero `ptr null`, a direct explicit `llvm.cd.*` producer, a marked
CD parameter, or a direct call to a defined function carrying
`cd.value.return`. Ordinary pointers from allocas, globals, loads, GEPs,
bitcasts, address-space casts, indirect calls, declarations, and function-value
casts are rejected. Direct and machine emitters run the same function ABI and
call validators before lowering.

Mixed scalar and marked CD parameters are allowed. A marked parameter may be
printed, consumed by an explicit CD intrinsic, passed to another marked
function, or returned through a marked pointer-return ABI. The first slice does
not infer CD provenance for PHI/select values or ordinary load/store results.

The artifact representation is unchanged:

```text
param 0 = "value"
rCall = call rFunction [rArgument]
return rValue
```

The Rust VM copies each argument into the callee parameter cell and copies the
returned value into the caller register. Array, map, and struct handles retain
shared backing storage, so explicit callee mutation is visible to the caller;
rebinding the callee parameter cell is local. Nil remains VM `nil`, and no LLVM
pointer address is exposed. Positive, malformed, mutation, nil, and
direct/machine parity coverage is in `cdbc-function-values.ll` and
`cdbc-function-value-errors.ll`.

PHI/select propagation, one-slot dynamic local storage, and function-value
callback transport remain separate ABI decisions. They must not be inferred
from these function attributes.

## Future ABI groups

The array/map and record groups are specified above and below. The bounded
native-call group is implemented below; broader native capabilities remain
reserved for separate ABI decisions:

| Group | Required operation shape | Wire operations | Design dependency |
| --- | --- | --- | --- |
| Broader native calls | allowlisted name-table identity and typed capability matrix | `native_call` | separate Rust VM capability and argument/result validation |

Each future group requires its own intrinsic signatures, malformed-input
fixtures, Rust parser/validator coverage, and direct/machine parity before it
can be implemented. Existing `cdbc 0.1` opcodes are not permission to map
arbitrary LLVM IR instructions to those operations.

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

### Array access and assertion

The access group maps directly to existing `cdbc 0.1` operations:

```text
rD = index rArray, rIndex
rD = len rArray
rD = assert_array rValue
```

The collection operand must be a CD dynamic-value token produced by an
explicit `llvm.cd.*` intrinsic or the `ptr null` CD nil token. An arbitrary
LLVM pointer, alloca, global, aggregate, or pointer operation is rejected
during lowering. `index` uses the Rust VM's array index rules: the index must
be a finite, non-negative integer-valued number and the position must be in
range. The VM owns the exact runtime error text. `len` observes the current
length of an array-compatible value and returns a number. `assert_array`
passes arrays through and applies the VM's existing iterable-to-array
conversion for future map/range producers; a nil or other incompatible value
is a runtime error rather than a compile-time reinterpretation.

Array handles retain the existing ownership contract: `index` returns a value
handle, and `assert_array` returns either the original array handle or a fresh
conversion-owned array as defined by the VM. Neither operation mutates the
source array. `len` is an observation only. Resource-budget failures and
runtime type/bounds failures remain VM errors and are never lowered to `nil`.

The first LLVM access slice permits these results only as local dynamic values:
they may be printed, used as array-constructor elements, or fed to another
explicit access/assertion intrinsic. Ordinary pointer operations, external
calls, function parameters/returns, and PHI/select propagation remain outside
this slice. Mutation is enabled only through the separate explicit
`llvm.cd.assign.index` intrinsic below.

### Array mutation

The mutation operation is the intrinsic ID `llvm.cd.assign.index`. Its
value/result type is overloaded so that the VM's returned assigned value keeps
the LLVM scalar or CD-token type:

```tablegen
def int_cd_assign_index : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_double_ty, LLVMMatchType<0>]>;
```

The first operand is a CD dynamic-value collection token (or the CD nil token),
the second is a `double` index, and the third is exactly one supported element
capability: an integer or floating scalar, `ptr null`, or a value produced by
an explicit `llvm.cd.*` dynamic-value intrinsic. The result has the same LLVM
type as the assigned value. LLVM's overloaded intrinsic spelling may carry a
type suffix; that spelling identifies the same intrinsic ID and is not a
second ABI operation.

The operation emits:

```text
rD = assign_index rCollection, rIndex, rValue
```

It mutates the existing array or map handle in place and returns the assigned
value, matching the Rust VM. Nested CD handles retain aliasing: replacing a
slot changes the collection, while the assigned nested handle remains shared
according to the VM value contract. Array indexes must be finite,
non-negative, integer-valued numbers and in range. Map keys must satisfy the
VM's existing key capability rules; a new key consumes the VM element budget.
Range assignment and unsupported collection/value kinds remain runtime errors.
The LLVM target does not pre-evaluate these checks or convert failures to
`nil`.

The mutation result is local in this slice. It may be printed, used as an
array-constructor element, or passed to another explicit access/assertion
intrinsic. It may not cross ordinary function parameters/returns, PHI/select,
alloca, pointer operations, or external calls. Ordinary LLVM stores and
aggregate operations never imply `assign_index`.

### Map constructor ABI gate

The next collection operation is the explicit `llvm.cd.map` intrinsic:

```tablegen
def int_cd_map : DefaultAttrsIntrinsic<
    [llvm_ptr_ty], [llvm_i32_ty, llvm_vararg_ty],
    [ImmArg<ArgIndex<0>>]>;
```

The immediate count is the number of map entries, and the variadic payload is
exactly `2 * count` operands in source order: key, value, key, value. A key is
statically limited to an integer or floating scalar, `ptr null` for CD nil, or
the result of `llvm.cd.string`. A value uses the existing array-element
capability matrix: scalar, nil, string token, array token, map token, or a
later explicit CD dynamic-value token. Ordinary pointers, aggregates,
vectors, poison, and undef are rejected before emission. The result is an
opaque address-space-zero CD map token.

The wire operation is:

```text
rD = map [rKey0: rValue0, rKey1: rValue1, ...]
```

Construction allocates a fresh mutable map. Values that are CD handles retain
the VM's existing shared-handle aliasing, while the map storage itself is not
aliased with any input. Duplicate runtime-equal keys are accepted and follow
the Rust VM contract: the last value wins while the first insertion position
is retained. Invalid key kinds and resource-budget failures remain runtime
errors owned by the VM; lowering does not silently coerce a key or emit nil.

The map token is local in the first slice. It may be printed, indexed, measured,
asserted, used as an explicit `assign_index` collection, or nested as a value
in another explicit constructor. It may not cross ordinary function
parameters/returns, PHI/select, allocas, pointer operations, or external calls.
The intrinsic is the only proof that an LLVM `ptr` denotes a CD map.

## Record values: first field ABI group

### Accepted source shape

`llvm.cd.struct` uses the following textual shape:

```llvm
@person = private unnamed_addr constant [7 x i8] c"Person\00"
@field_age = private unnamed_addr constant [4 x i8] c"age\00"

declare ptr @llvm.cd.struct(ptr, i32, ...)

%person = call ptr (ptr, i32, ...) @llvm.cd.struct(
    ptr @person, i32 1, ptr @field_age, i64 42)
```

The first operand may instead be `ptr null` for an anonymous struct. The
immediate `i32` count must equal the number of following field-name/value
pairs. Every name operand must be a direct private, constant, address-space-zero
byte global containing a non-empty, valid UTF-8 C string. These globals are
interned in the artifact name table; they are metadata and are not
`llvm.cd.string` values. Every field value must be an integer or floating scalar,
the address-space-zero `ptr null` CD nil token, or a value produced by an
explicit CD intrinsic. Ordinary pointers, aggregates, poison, undef, and
pointer operations remain rejected during lowering.

Field names and their source order are preserved in the existing artifact
operations:

```text
rD = struct nType {nField0: r0, nField1: r1}
rD = struct {nField0: r0}
rD = field rObject, nField
rD = assign_field rObject, nField, rValue
```

The first form is nominal; the second is anonymous. Struct construction creates
a fresh mutable VM object. Nested CD handles retain the VM's shared-handle
semantics. `field` reads a value, while `assign_field` updates the existing
struct in place and returns the assigned value. A non-struct object and a
missing field remain Rust VM runtime errors; for example, the missing-field
diagnostic is `undefined field \`missing\``.

The record results are local CD values in this slice. They may be printed,
passed to another explicit field/collection intrinsic, or used as a supported
dynamic-value operand. They may not cross ordinary function parameters/returns,
PHI/select, allocas, pointer operations, or external calls. An arbitrary LLVM
pointer is never treated as a struct object.

## Enum variant values: explicit variant ABI

### Accepted source shape

`llvm.cd.variant` uses the following textual shape:

```llvm
@result = private unnamed_addr constant [7 x i8] c"Result\00"
@ok = private unnamed_addr constant [3 x i8] c"Ok\00"

declare ptr @llvm.cd.variant(ptr, ptr, i32, ...)

%value = call ptr (ptr, ptr, i32, ...) @llvm.cd.variant(
    ptr @result, ptr @ok, i32 1, i64 42)
```

Both name operands must be direct private, constant, address-space-zero byte
globals containing non-empty valid UTF-8 strings. The immediate `i32` count
must equal the number of payload operands. Payloads use the existing scalar,
CD nil, and explicit CD dynamic-value capability matrix. Ordinary pointers,
aggregates, vectors, poison, undef, and pointer operations remain rejected
during lowering.

The access operations use these shapes:

```llvm
declare i1 @llvm.cd.variant.tag(ptr, ptr, ptr)
declare ptr @llvm.cd.variant.field(ptr, i32)

%matches = call i1 @llvm.cd.variant.tag(ptr %value, ptr @result, ptr @ok)
%payload = call ptr @llvm.cd.variant.field(ptr %value, i32 0)
```

`variant` emits an owned fresh VM value with the enum name, variant name, and
payload values in source order. `variant_tag` compares both names and returns
`false` for a non-matching value, including a value that is not an enum
variant. `variant_field` reads a positional payload without mutating the
variant. The Rust VM remains authoritative for runtime failures: accessing a
non-variant produces `can only access fields on enum variants`, and an invalid
payload index produces `enum variant field index out of bounds`.

Variant values are local CD values in this slice. They may be printed, used as
payloads of another explicit constructor, or passed to another explicit
variant/collection/field intrinsic. They may not cross ordinary function
parameters/returns, PHI/select, allocas, pointer operations, or external calls.

## Native calls: bounded stdlib ABI

The bounded allowlist below is implemented through the shared ABI validator,
the typed artifact model, and both direct and opt-in machine emitters. The
Rust VM remains the runtime oracle; only the explicitly documented `map`,
`filter`, `flatMap`, `reduce`, `any`, `all`, `count`, `find`, and `findIndex` callback shapes are admitted in
addition to the non-callback helpers.

### Accepted source shape

`llvm.cd.native` uses a private name global followed by a variadic argument
list:

```llvm
@sqrt_name = private unnamed_addr constant [5 x i8] c"sqrt\00"

declare double @llvm.cd.native(ptr, ...)

%root = call double (ptr, ...) @llvm.cd.native(ptr @sqrt_name, double 9.0)
```

The name must be a direct private, constant, address-space-zero byte global
containing one non-empty valid UTF-8 name. The LLVM result type is overloaded
by the intrinsic declaration, but target lowering accepts only the exact type
for the selected native name. The first bounded capability matrix is:

| Native name | Arguments | Result | Runtime role |
| --- | --- | --- | --- |
| `floor`, `ceil`, `sqrt` | exactly one `double` | `double` | numeric operation |
| `str` | exactly one scalar or CD dynamic value | address-space-zero `ptr` | string conversion |
| `typeOf` | exactly one scalar or CD dynamic value | address-space-zero `ptr` | runtime type name |
| `hash` | exactly one scalar or CD dynamic value | `double` | runtime hash number |
| `range` | one to three `double` values | address-space-zero `ptr` | range producer for existing access ops |
| `substr` | one CD dynamic value, two `double` values | address-space-zero `ptr` | Unicode-scalar string slice |
| `charAt` | one CD dynamic value, one `double` value | address-space-zero `ptr` | Unicode-scalar character extraction |
| `map` | one CD dynamic-value token, one direct callback function value | address-space-zero `ptr` | fresh array of callback results |
| `flatMap` | one CD dynamic-value token, one direct callback function value | address-space-zero `ptr` | fresh array with one-level callback-array flattening |
| `reduce` | one CD dynamic-value array token, one scalar or CD dynamic-value initial value, one direct callback function value | address-space-zero `ptr` | left-to-right accumulator threading |
| `filter` | one CD dynamic-value token, one direct predicate function value | address-space-zero `ptr` | fresh shallow array of matching source values |
| `any` | one CD dynamic-value token, one direct predicate function value | `i1` | short-circuit true when any predicate result is true |
| `all` | one CD dynamic-value token, one direct predicate function value | `i1` | short-circuit false when any predicate result is false |
| `count` | one CD dynamic-value token, one direct predicate function value | `double` | number of elements whose predicate result is true |
| `find` | one CD dynamic-value token, one direct predicate function value | address-space-zero `ptr` | first matching element, or `nil` when there is no match |
| `findIndex` | one CD dynamic-value token, one direct predicate function value | `double` | zero-based index of the first match, or `-1` when there is no match |

The `str`, `typeOf`, and `hash` operands may be scalar, CD nil, or a value
produced by an explicit CD intrinsic. The `range` result is a CD dynamic-value
token and may be consumed by the existing `len`, `index`, and `assert_array`
intrinsics. `substr` and `charAt` require an explicit CD dynamic-value token;
the target cannot prove its runtime string tag, so non-string values remain a
valid compile-time token shape and receive the Rust VM's runtime type error.
Their numeric operands must be `double`; the VM owns integer-valuedness,
Unicode scalar boundaries, and range checks. Both operations return a fresh
string value and do not mutate or alias the source. Name-table metadata is not
a CD string value, and arbitrary ordinary pointers are rejected.

Unknown names and callback helpers without a documented matrix remain
compile-time target errors. This restriction prevents a
native call from becoming an unbounded external-call
escape hatch.

### `map` callback boundary

`map` is the first callback-native vertical slice. Its source shape is:

```llvm
@map_name = private unnamed_addr constant [4 x i8] c"map\00"

define ptr @identity(ptr %value) #0 {
entry:
  ret ptr %value
}

%mapped = call ptr (ptr, ...) @llvm.cd.native(
    ptr @map_name, ptr %array, ptr @identity)

attributes #0 = { "cd.value.params"="0" "cd.value.return" }
```

The array operand must be a proven address-space-zero CD value token; its
runtime tag remains owned by the VM, so a non-array token reaches the VM and
produces `map expects array as first argument`. The callback must be a direct,
defined LLVM function, not a declaration, `bitcast`, indirect function
pointer, or `@main`. It must have exactly one address-space-zero `ptr`
parameter marked by `cd.value.params="0"` and an address-space-zero `ptr`
return marked by `cd.value.return`. The callback may return any CD dynamic
value, including nil.

Both emitters materialize the direct callback with the existing
`make_function` operation before emitting `native_call`; no new intrinsic,
artifact opcode, or `.cdbc 0.1` field is introduced. The Rust VM requires
exactly two arguments and callback arity one. It snapshots the input elements,
returns a fresh array, preserves shared handles passed to the callback, checks
the instruction budget before each callback iteration, and charges the output
allocation as one array plus its elements. Cancellation is checked before
budget/resource growth, matching the existing VM native-call contract.

### `flatMap` callback boundary

`flatMap` reuses the dynamic callback transport used by `map`:

```llvm
@flat_map_name = private unnamed_addr constant [8 x i8] c"flatMap\00"

define ptr @identity(ptr %value) #0 {
entry:
  ret ptr %value
}

%flattened = call ptr (ptr, ...) @llvm.cd.native(
    ptr @flat_map_name, ptr %array, ptr @identity)

attributes #0 = { "cd.value.params"="0" "cd.value.return" }
```

The array operand and callback have the same provenance and direct-defined
function requirements as `map`. The callback must return an address-space-zero
pointer marked by `cd.value.return`, and the native result is an exact
address-space-zero `ptr`. The Rust VM snapshots the input, invokes the callback
left to right, requires each callback result to be an array, and appends that
array's elements to a fresh one-level result. It checks the native checkpoint
before each callback and before each flattened element; runtime array/result
types, budget, cancellation, and callback failures remain VM-owned. Empty input
returns a fresh empty array.

### `reduce` callback boundary

`reduce` carries both the initial accumulator and each array element through a
two-parameter dynamic callback:

```llvm
@reduce_name = private unnamed_addr constant [7 x i8] c"reduce\00"

define ptr @last(ptr %accumulator, ptr %item) #0 {
entry:
  ret ptr %item
}

%reduced = call ptr (ptr, ...) @llvm.cd.native(
    ptr @reduce_name, ptr %array, double 0.0, ptr @last)

attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }
```

The array operand must be a proven address-space-zero CD array token, and the
initial value may be a scalar or proven CD dynamic-value token. The callback
must be a direct, defined LLVM function with exactly two address-space-zero
pointer parameters marked by `cd.value.params="0,1"`, and an address-space-zero
pointer return marked by `cd.value.return`. The native result is an exact
address-space-zero `ptr`; declarations, casts, indirect callbacks, `@main`,
ordinary pointer operands, and incomplete parameter markers remain rejected.

Both emitters materialize the callback with `make_function` before emitting
`native_call`. The Rust VM snapshots the input, returns the initial value for
an empty array, and invokes the callback left to right with `(accumulator,
item)`, threading each result into the next call. Callback frames, native
checkpoints, budget, cancellation, and runtime array checks remain VM-owned.

### `filter` callback boundary

`filter` uses the same native-call transport with a boolean predicate:

```llvm
@filter_name = private unnamed_addr constant [7 x i8] c"filter\00"

define i1 @keep(ptr %value) #0 {
entry:
  ret i1 true
}

%filtered = call ptr (ptr, ...) @llvm.cd.native(
    ptr @filter_name, ptr %array, ptr @keep)

attributes #0 = { "cd.value.params"="0" }
```

The array operand must be a proven address-space-zero CD value token; its
runtime tag remains owned by the VM, so a non-array token reaches the VM and
produces `filter expects array as first argument`. The predicate must be a
direct, defined LLVM function, not a declaration, `bitcast`, indirect function
pointer, or `@main`. It must have exactly one address-space-zero `ptr`
parameter marked by `cd.value.params="0"` and return exactly `i1`; it does not
use `cd.value.return`.

Both emitters materialize the predicate with `make_function` before
`native_call`. The Rust VM requires exactly two arguments and callback arity
one, snapshots the input array, invokes the predicate from left to right, and
returns a fresh shallow array containing the original elements whose predicate
result is `true`. Predicate result typing, resource-budget, cancellation, and
callback failures remain VM-owned behavior.

### `any` and `all` predicate boundaries

`any` and `all` reuse the same direct predicate transport with an exact `i1`
result:

```llvm
@any_name = private unnamed_addr constant [4 x i8] c"any\00"

define i1 @predicate(ptr %value) #0 {
entry:
  ret i1 true
}

%matched = call i1 (ptr, ...) @llvm.cd.native(
    ptr @any_name, ptr %array, ptr @predicate)

attributes #0 = { "cd.value.params"="0" }
```

The array operand must be a proven address-space-zero CD token, and the
predicate must be a direct defined function with one marked CD parameter and
no `cd.value.return` marker. Both emitters materialize the predicate with
`make_function` before `native_call`. The Rust VM snapshots the input, invokes
the predicate left to right, checks the native checkpoint before each callback,
and short-circuits: `any` returns `true` on the first true result and `false`
for an empty array, while `all` returns `false` on the first false result and
`true` for an empty array. The result type is exactly `i1`; callback arity,
predicate result, runtime array type, budget, cancellation, and failures remain
VM-owned behavior.

### `count` predicate boundary

`count` reuses the same direct predicate transport with an exact `double`
result:

```llvm
@count_name = private unnamed_addr constant [6 x i8] c"count\00"

define i1 @predicate(ptr %value) #0 {
entry:
  ret i1 true
}

%matches = call double (ptr, ...) @llvm.cd.native(
    ptr @count_name, ptr %array, ptr @predicate)

attributes #0 = { "cd.value.params"="0" }
```

The array operand and predicate have the same provenance and direct-defined
function requirements as `any` and `all`. Both emitters materialize the
predicate with `make_function` before `native_call`. The Rust VM snapshots the
input, invokes the predicate left to right for every element, checks the native
checkpoint before each callback, and returns a numeric count; an empty array
returns `0`. The result type is exactly `double`, while callback arity,
predicate result, runtime array type, budget, cancellation, and failures remain
VM-owned behavior.

### `find` predicate boundary

`find` reuses the same direct predicate transport with an exact address-space-zero
`ptr` result:

```llvm
@find_name = private unnamed_addr constant [5 x i8] c"find\00"

define i1 @predicate(ptr %value) #0 {
entry:
  ret i1 true
}

%match = call ptr (ptr, ...) @llvm.cd.native(
    ptr @find_name, ptr %array, ptr @predicate)

attributes #0 = { "cd.value.params"="0" }
```

The array operand and predicate have the same provenance and direct-defined
function requirements as `any`, `all`, and `count`. The predicate must return
exactly `i1` and must not carry `cd.value.return`. Both emitters materialize the
predicate with `make_function` before `native_call`. The Rust VM snapshots the
input, invokes the predicate from left to right, checks the native checkpoint
before each callback, and returns the first matching element. Empty arrays and
arrays with no match return `nil`; a non-array input retains the VM diagnostic
`find expects array as first argument`. Callback arity, predicate result,
runtime type, budget, cancellation, and failures remain VM-owned behavior.

### `findIndex` predicate boundary

`findIndex` reuses the same direct predicate transport with an exact `double`
result:

```llvm
@find_index_name = private unnamed_addr constant [10 x i8] c"findIndex\00"

define i1 @predicate(ptr %value) #0 {
entry:
  ret i1 true
}

%index = call double (ptr, ...) @llvm.cd.native(
    ptr @find_index_name, ptr %array, ptr @predicate)

attributes #0 = { "cd.value.params"="0" }
```

The array operand and predicate have the same provenance and direct-defined
function requirements as `any`, `all`, `count`, and `find`. The predicate must
return exactly `i1` and must not carry `cd.value.return`. Both emitters
materialize the predicate with `make_function` before `native_call`. The Rust VM
snapshots the input, invokes the predicate from left to right, checks the native
checkpoint before each callback, and returns the zero-based index of the first
matching element. Empty arrays and arrays with no match return `-1`; a non-array
input retains the VM diagnostic `findIndex expects array as first argument`.
Callback arity, predicate result, runtime type, budget, cancellation, and
failures remain VM-owned behavior.

The wire operation is:

```text
rD = native_call nName [rArg0, rArg1, ...]
```

The Rust VM owns native arity, runtime type, resource-budget, cancellation,
and failure behavior. For example, `sqrt` preserves the runtime error
`sqrt expects non-negative number` for a negative input, while representative
string failures include `substr length out of bounds` and
`charAt index out of bounds`. Native calls do not cross ordinary LLVM function
boundaries in this slice; their values may only flow through already-supported
local explicit CD consumers.

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

The array-access group is complete only when `llvm.cd.index`, `llvm.cd.len`,
and `llvm.cd.assert.array` have exact intrinsic signatures, reject ordinary
pointer substitutes in both backends, emit the existing `index`, `len`, and
`assert_array` operations, and pass Rust `dump`/`run` plus direct/machine
behavior parity. The positive path must cover a scalar array element, a nested
array handle, length observation, and assertion; a separate runtime check must
preserve the VM's bounds/type errors.

The array-mutation group is complete only when the overloaded
`llvm.cd.assign.index` intrinsic preserves scalar and CD-token result types,
rejects ordinary pointer/value substitutes, emits `assign_index` through both
backends, and passes Rust `dump`/`run`, mutation behavior parity, and explicit
runtime bounds-error parity. The Rust VM's existing in-place aliasing and
failure behavior remains authoritative.

The map-constructor group is complete only when `llvm.cd.map` enforces the
entry-count/pair shape and key/value capability matrix, emits `map` through
both backends, preserves duplicate-key ordering and shared-value behavior,
and passes Rust `dump`/`run`, malformed-input checks, direct/machine parity,
and runtime resource/error checks.

The record-value group is complete only when `llvm.cd.struct`,
`llvm.cd.field`, and `llvm.cd.assign.field` enforce the name/count/value and
overloaded result contracts, emit `struct`, `field`, and `assign_field` through
both backends, reject ordinary pointer substitutes, pass Rust `dump`/`run`,
and cover direct/machine artifact, dynamic-value, and missing-field runtime
parity.

The enum-variant group is complete only when `llvm.cd.variant`,
`llvm.cd.variant.tag`, and `llvm.cd.variant.field` enforce the name/count,
payload capability, and overloaded result contracts, emit `variant`,
`variant_tag`, and `variant_field` through both backends, reject ordinary
pointer substitutes, pass Rust `dump`/`run`, and cover direct/machine artifact,
dynamic-value, non-variant, and out-of-bounds runtime parity.

The bounded native-call group is complete only when `llvm.cd.native` enforces
the name-specific matrix above, emits `native_call` through both backends,
rejects unknown/not-yet-selected callback names and ordinary pointer
substitutes, passes Rust `dump`/`run`, and covers direct/machine artifact parity
plus shared runtime failures for numeric, string, `map`, `filter`, `flatMap`,
`reduce`, `any`, `all`, `count`, `find`, and `findIndex` helpers.
The string-helper extension additionally requires UTF-8 scalar-boundary output
and malformed shape diagnostics for both `substr` and `charAt`.

The sibling `cd-compiler` checkout already defines the string, collection,
record, and enum-variant operations in the `cdbc 0.1` parser, formatter, and
VM. These M4 slices therefore change the LLVM artifact model and lowering only;
they do not add a new Rust opcode or alter the artifact version.

## Module artifact envelope: M6 foundation

Program mode remains the default for `llc -mtriple=cd-unknown-unknown` and emits
the existing linked-program envelope. The opt-in `-cd-artifact=module` mode
emits the Rust VM's existing `artifact: module` envelope; it never changes the
`cdbc 0.1` version or the ordinary constants, names, function, body, and debug
sections.

Module mode consumes exactly one `!cd.module` record. Its positional shape is
four operands for a non-entry module and five operands for an entry module:

```llvm
!cd.module = !{!0}
!0 = !{!"entry", !"entry.cd", !"/workspace/entry.cd", i1 true, i64 0}
```

The operands are `identity`, display `path`, `canonical_path`, the `i1` entry
flag, and the optional non-negative 64-bit `entry_order`. An entry module must
have an order, and a non-entry module must omit it. All strings are UTF-8 and
all identities and paths are non-empty.

Dependencies are an optional ordered `!cd.dependencies` named-metadata list.
Each record has four operands: dependency kind (`import` or `re_export`),
target module identity, non-negative 64-bit local main-instruction offset, and
requested source path:

```llvm
!cd.dependencies = !{!1}
!1 = !{!"import", !"/workspace/lib.cd", i64 1, !"./lib.cd"}
```

Offsets are serialized as the existing `dN target=... kind=... at=...
requested=...` records and must be nondecreasing and no greater than the
lowered main instruction count. A non-entry module is a fall-through product:
module mode omits the LLVM terminal return from its `main` body so the Rust
linker can insert that body at a dependency offset. Entry modules retain their
terminal return. If `llvm-link` combines input modules, LLVM concatenates their
named-metadata records; the resulting multiple `!cd.module` records are
rejected rather than synthesized into one module product. Module products are
linked by the Rust VM's module-aware linker. Supplying module metadata without
`-cd-artifact=module` is also an error, so the default program path cannot
silently discard it.

The opt-in `llvm/utils/cd_module_link.py` harness emits entry and dependency
products through both LLVM paths, then checks Rust `dump`, unlinked `run`
rejection, `link`, linked `dump`, and linked `run`. It also checks missing
dependencies, dependency cycles, duplicate identities, non-contiguous entry
orders, and invalid insertion offsets without changing the nested VM checkout.
Its source-backed diagnostic pair additionally compares direct and machine
`run` stderr, source caret/call-stack rendering, and the `debug` error pause's
module identity after linking.

## Debug source tables: M5A foundation

The target accepts source bytes only through an explicit `!cd.sources` named
metadata node. Ordinary `-g`/`llvm.dbg.*` metadata is not treated as a source
text provider. Each record has one of these exact shapes:

```llvm
!cd.sources = !{!0, !1}
!0 = !{!"demo.cd", !"print 1;\0A"}
!1 = !{!"/workspace/lib.cd", !"lib.cd", !"fun fail() { return 1 / 0; }\0A"}
```

A two-string record is `path,text`; a three-string record is
`module,path,text`. Paths are non-empty, module identities are non-empty when
present, all strings must be valid UTF-8, and `(module,path)` identities must
be unique. Valid records are emitted as the optional `debug_sources` section
in source order through both direct and machine artifact paths.

When an explicit source table is present, a non-zero `DILocation` is emitted as
an optional sparse `debug_locations` entry when its `DIFile` filename, or its
directory-qualified filename, matches exactly one source path. Locations with
no matching explicit source, no file scope, or a zero line/column are omitted;
ambiguous path matches are target errors. Direct and machine lowering preserve
the same source, line, and one-based column values after branch patching, while
synthetic constants and instructions without a source location remain sparse.
The Rust VM can therefore render source lines, carets, and nested call stacks
for these locations; the LLVM parity fixture covers a nested divide-by-zero
failure through both direct and machine artifacts. The optional `!cd.ranges`
named metadata supplies exact source-local byte offsets without changing the
line/column mapping:

```llvm
!cd.ranges = !{!40}
!40 = !{!10, i64 6, i64 11}
!10 = !DILocation(line: 1, column: 7, scope: !5)
```

Each record is keyed by one `DILocation` node and contains a half-open
`[start,end)` byte interval. The target rejects malformed records, negative or
reversed offsets, duplicate location records, ranges without a matching
source-backed location, and ranges beyond the UTF-8 source bytes. It never
infers a range from line/column values. Direct and machine paths emit identical
sparse `debug_ranges` entries; synthetic instructions remain range-free. The
parity fixtures also cover a failed `sqrt(-1)` native call and an out-of-range
array index with source-backed diagnostics. The outer direct/machine parity gate
also covers source-backed interactive runtime-error pauses; richer interactive
session behavior remains a later M5 boundary.
