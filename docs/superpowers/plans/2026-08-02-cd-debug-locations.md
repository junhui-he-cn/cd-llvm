# CD Debug Location Slice Implementation Plan

**Goal:** Lower representable LLVM `DILocation` metadata into the optional
`debug_locations` section of `cdbc 0.1` when explicit `!cd.sources` records
provide the source bytes.

**Boundary:** Source paths are resolved from `DIFile.filename` or its
directory-qualified filename. A location without an exact source match, file
scope, or positive line/column is omitted from the sparse table. Ambiguous
source-path matches are target errors. This slice does not emit
`debug_ranges`, infer source text, or change the nested `cd-compiler` checkout.

## Task 1: Add the contract fixture and record the red baseline

- [x] Add a direct/machine fixture with main and function `DILocation` records.
- [x] Check the old emitter fails because `debug_locations` is absent.

## Task 2: Extend the typed artifact and shared resolver

- [x] Add optional per-instruction locations to `CDBody`.
- [x] Validate source, line, column, and instruction-table cardinality before
  serialization.
- [x] Serialize sparse `main` and `function fN` location entries after
  `debug_sources`.
- [x] Resolve `DILocation` through the explicit source table without inferring
  source bytes from ordinary LLVM debug metadata.

## Task 3: Wire direct and machine emission

- [x] Preserve direct source locations on emitted instructions while leaving
  synthetic constants and location-free instructions sparse.
- [x] Propagate source locations to generated machine pseudos and resolve them
  in the shared artifact bridge after branch lowering.
- [x] Extend the direct/machine parity manifest with the location fixture.

## Task 4: Verification and delivery

- [x] Run focused/full CD lit, Rust dump/run, direct/machine parity, Cargo
  tests, and hygiene checks.
- [x] Update ABI, machine-backend, target README, and roadmap documentation.
- [x] Commit as `feat(cd): add debug locations`; do not push or modify the
  nested checkout.

Verification on 2026-08-02: focused CD lit `1/1`, complete CD lit `58/58`,
Rust VM cargo tests `73 + 3 + 8`, direct/machine parity `35/35`, and explicit
Rust VM `dump`/`run` checks for both location artifacts all passed.
