# CD Module Link Integration Plan

**Goal:** Exercise the existing Rust VM module linker with independently
emitted LLVM CD products while keeping VM integration opt-in and the nested
`cd-compiler/` checkout untouched.

**Architecture:** The LLVM module envelope remains the product boundary. A
non-entry module's `main` body is a fall-through fragment, so module mode
omits its LLVM terminal return; the entry module retains its return. The
Python harness emits entry and dependency products through both LLVM lowering
paths, then delegates dump, link, run, and rejection behavior to the Rust VM.

## Tasks

- [x] Add direct/machine regression coverage for non-entry fall-through main
  bodies.
- [x] Add `llvm/utils/cd_module_link.py` with an explicit `--llc`, `--vm`, and
  optional backend selection; keep it outside the default LLVM lit suite.
- [x] Generate valid entry/dependency products and verify canonical dump,
  unlinked run rejection, Rust linking, and linked output `1`, `2`, `3`.
- [x] Cover missing dependencies, dependency cycles, duplicate identities,
  non-contiguous entry order, and invalid insertion offsets through the Rust
  link boundary.
- [x] Document the fall-through rule and the opt-in integration command.

The remaining M6 boundary is source-backed linked runtime diagnostics; this
slice does not change the `cdbc 0.1` format or the nested VM checkout.
