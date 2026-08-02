# CD bytecode target

The experimental `CD` target is a software target for the Compiler Design
bytecode VM.  It is selected with:

```text
llc -mtriple=cd-unknown-unknown input.ll -o output.cdbc
```

The target emits the text `cdbc 0.1` artifact format consumed by the Rust VM in
the sibling `cd-compiler` project.  The output contains deterministic constant
and name tables, a `main` body, and function bodies.  LLVM function arguments
are represented by `param` metadata and an initial `load_var`; direct calls
materialize a function value with `make_function` before using `call`.

The initial lowering supports scalar integer and floating-point constants,
arithmetic, comparisons, scalar casts as `move`, direct single-slot `alloca`
storage with `load`/`store`, direct calls to defined functions, conditional and
unconditional branches, PHI edge stores, returns, and declarations named
`cd_print` or `print` with one argument.  `fneg` lowers to `negate`.  A scalar
`select` lowers to a small conditional control-flow sequence using
`jump_if_false`, `move`, and `jump`.  The only supported `xor` form is boolean
inversion: an `i1` value XORed with the literal `true` (in either operand order)
lowers to `not`; other XOR operations remain unsupported.  Unsupported LLVM
instructions fail with a CD-target diagnostic rather than producing an invalid
artifact. A `ptr null` return is accepted as the CD `nil` value; non-nil pointer
returns remain unsupported.

The emitter builds a typed `llvm::cd::CDArtifact` before writing anything. Its
`CDBytecodeFormat` validator checks table references, register operands, branch
targets, instruction shapes, function parameter records, and finite constants;
the canonical serializer is the only component that spells the `cdbc 0.1`
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
one-argument `cd_print` or `print` declaration. Ordinary pointer operations,
comparisons, storage, function parameters/returns, and non-string globals
remain rejected.

The first M4 collection operation is `llvm.cd.array(i32 count, ...)`. The
immediate count must equal the number of following scalar, address-space-zero
`nil`, string-token, or array-token operands. The variadic textual IR call type
must be explicit (`call ptr (i32, ...) @llvm.cd.array(...)`). The target lowers
the payload to the existing `array` instruction; array results may be printed
or nested in another constructor, while ordinary pointer, aggregate, storage,
function-parameter/return, and `select` uses remain rejected. The direct and
machine paths validate the same capability matrix and share the same artifact
bridge.

The first M4 array-access operations use `llvm.cd.index(ptr, double)`,
`llvm.cd.len(ptr)`, and `llvm.cd.assert.array(ptr)`. Their collection operands
must be explicit CD dynamic-value tokens (or the CD nil token), not arbitrary
LLVM pointers. They lower to the existing `index`, `len`, and `assert_array`
instructions. Index results remain dynamic-value tokens so scalar and nested
array elements can share one ABI; `len` returns a CD number as `double`.
`assert.array` preserves the VM's runtime type/conversion behavior. The direct
and machine paths share validation, artifact serialization, and Rust VM output
parity. These results remain local and cannot yet cross ordinary function,
pointer, or PHI/select boundaries.

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
lowers to `variant nEnum.nVariant [rPayload0, ...]`; `variant_tag` compares
the two names and returns a boolean, while `variant_field` reads an immediate
positional payload index. These map to the existing Rust VM operations and
preserve its non-variant and out-of-bounds runtime diagnostics. Names remain
name-table metadata, ordinary pointers and aggregates remain unsupported, and
variant values stay local to explicit CD intrinsic consumers in this slice.

The bounded native-call slice implements `llvm.cd.native(ptr name, ...)` for
the allowlisted non-callback names `floor`, `ceil`, `sqrt`, `str`, `typeOf`,
`hash`, and `range`. The name must be a private constant UTF-8 global; each
name has an exact scalar/CD-value argument and result signature recorded in
`docs/cd-bytecode-llvm-abi.md`. Direct and opt-in machine lowering share the
`native_call` artifact bridge and parity coverage. Callback helpers, unsupported
names, and ordinary pointer arguments remain rejected.

The M5A debug-source-table foundation accepts an explicit `!cd.sources` named
metadata node with `path,text` or `module,path,text` string records. It validates
UTF-8, non-empty identities, and duplicate `(module,path)` entries, then emits
the optional `debug_sources` section in both artifact paths. When a source
table is present, matching non-zero `DILocation` metadata is emitted as sparse
`debug_locations` entries through both paths; `-g` and ordinary `llvm.dbg.*`
metadata still do not supply source bytes. Unmatched locations are omitted,
ambiguous source-path matches are rejected, and `debug_ranges` remains deferred.

`cdbc-optimization.ll` checks direct `llc` emission at `-O0` and `-O2`, and
also runs LLVM's explicit `default<O2>` middle-end pipeline before emission.
The optimized artifact exercises alloca promotion, constant folding, dead-code
elimination, and scalar-select lowering; its Rust VM `dump` and `run` results
must remain canonical and observable-equivalent to the `-O0` artifact.

This target deliberately has no object-file, assembler, JIT, or arbitrary
native-call output. `-filetype=obj` is rejected. Native calls beyond the
bounded group, non-string globals, and debug ranges remain deferred until
separate LLVM-to-CD representations and parity gates are defined.
