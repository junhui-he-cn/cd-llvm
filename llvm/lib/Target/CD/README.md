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
`cd_print` or `print` with one argument.  Unsupported LLVM instructions fail
with a CD-target diagnostic rather than producing an invalid artifact.

The emitter builds a typed `llvm::cd::CDArtifact` before writing anything. Its
`CDBytecodeFormat` validator checks table references, register operands, branch
targets, instruction shapes, function parameter records, and finite constants;
the canonical serializer is the only component that spells the `cdbc 0.1`
wire format. This boundary is also the input contract for the later TableGen-
backed machine path.

Integer constants are accepted only when their signed value is exactly
representable as an IEEE-754 double; otherwise the target reports a diagnostic
instead of silently changing the value. Number constants are serialized with a
canonical `%.17g` spelling after finite-value validation.

This target deliberately has no object-file, assembler, JIT, or native-call
output.  `-filetype=obj` is rejected.  Arrays, maps, structs, variants,
general globals, native calls, and debug source sections remain deferred until
an explicit LLVM-to-CD representation is defined.
