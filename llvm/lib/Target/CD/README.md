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

`cdbc-optimization.ll` checks direct `llc` emission at `-O0` and `-O2`, and
also runs LLVM's explicit `default<O2>` middle-end pipeline before emission.
The optimized artifact exercises alloca promotion, constant folding, dead-code
elimination, and scalar-select lowering; its Rust VM `dump` and `run` results
must remain canonical and observable-equivalent to the `-O0` artifact.

This target deliberately has no object-file, assembler, JIT, or native-call
output.  `-filetype=obj` is rejected.  Arrays, maps, structs, variants,
non-string globals, native calls, and debug source sections remain deferred
until an explicit LLVM-to-CD representation is defined.
