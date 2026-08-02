# CD Bytecode Verification

This document records the reproducible verification boundary for the
experimental LLVM CD target. The LLVM target emits text cdbc 0.1 artifacts;
the Rust VM remains an explicitly selected integration dependency.

## Recorded Environment

Recorded on 2026-08-02 from the current checkout:

| Component | Observed version |
| --- | --- |
| LLVM llc | LLVM 24.0.0git |
| LLVM llvm-lit | lit 24.0.0dev |
| Rust | rustc 1.94.1 (e408947bf 2026-03-25) |
| Cargo | cargo 1.94.1 (29ea6fb6a 2026-03-24) |
| VM checkout | 0295380ce3e29763949c09a815bda96cbed28ee2 |
| Artifact contract | cdbc 0.1 |

The VM checkout is not part of the LLVM source tree. The commands below use
CD_COMPILER_ROOT explicitly; no command searches for a sibling checkout.

## LLVM-Only Gate

This command must work without a VM checkout or a CD_COMPILER_ROOT variable:

~~~sh
env -u CD_COMPILER_ROOT build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD
~~~

The CD directory includes the VM integration file, but it is reported
unsupported when the cd-vm feature is unavailable. All LLVM-only tests remain
executable in that mode.

## Opt-In VM Gate

Set the root explicitly to a checkout containing vm-rs/Cargo.toml:

~~~sh
CD_COMPILER_ROOT="$PWD/cd-compiler" build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-vm-integration.py
~~~

The integration test delegates to the standalone module-link harness. It
emits direct and machine module products, checks canonical dump, rejects
unlinked run, links valid products, checks linked output and graph failures,
and compares linked source-backed runtime diagnostics.

The harness can also be run without lit:

~~~sh
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_module_link_test.py
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_module_link.py --llc build-cd/bin/llc --vm "$PWD/cd-compiler/vm-rs"
~~~

## LLVM Build And Checks

Build the target tools before running the gates:

~~~sh
ninja -C build-cd llc
build-cd/bin/llvm-lit -q llvm/test/CodeGen/CD
git diff --check
~~~

Focused module and diagnostic tests can be run directly:

~~~sh
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-modules.ll llvm/test/CodeGen/CD/cdbc-module-errors.ll llvm/test/CodeGen/CD/cdbc-module-fallthrough.ll llvm/test/CodeGen/CD/cdbc-module-link-diagnostic-entry.ll llvm/test/CodeGen/CD/cdbc-module-link-diagnostic-dependency.ll
~~~

The wider release matrix remains separate from this opt-in gate. In
particular, -O0, -O2, -g, metadata-free output, object-output rejection,
invalid IR, and invalid CD ABI operation checks must be included before M7 is
called complete.

## Boundary Rules

- Program mode remains the default; module mode is selected with
  -cd-artifact=module.
- The artifact format stays cdbc 0.1; this verification slice adds no wire
  fields and no new version.
- Missing CD_COMPILER_ROOT never activates VM integration and never causes
  the test suite to inspect cd-compiler.
- An explicit VM path is passed to Cargo through the existing harness; the
  harness uses temporary artifact directories and does not alter LLVM source
  files.
