# CD Bytecode Verification

This document records the reproducible verification boundary for the
experimental LLVM CD target. The LLVM target emits text cdbc 0.1 artifacts;
the Rust VM remains an explicitly selected integration dependency.

## Recorded Environment

Recorded on 2026-08-10 from the current checkout:

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

The current local LLVM-only run is `122 passed / 1 unsupported` across 123
fixtures. The direct/machine parity manifest has 92 passing entries, including
the `contains`, `slice`, `copy`, `concat`, `push`, `remove`, `clear`, `merge`, `keys`, and `values` behavior, selected `map`, `filter`, `flatMap`, `reduce`, `any`,
`all`, `count`, `find`, and `findIndex` callback behavior and runtime type-error
cases, plus dynamic CD `select`, PHI, and one-slot storage behavior.

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

## CI Jobs

`.github/workflows/cd-bytecode.yml` keeps the two dependency boundaries
explicit:

- `llvm-only` builds `llc`, `FileCheck`, `count`, `not`, `opt`, `llvm-config`,
  `llvm-readobj`, and `split-file`, then runs the CD lit directory with
  `CD_COMPILER_ROOT` unset. It does not check out the Rust VM.
- `vm-integration` checks out `junhui-he-cn/cd-compiler` at the recorded
  commit `0295380ce3e29763949c09a815bda96cbed28ee2`, installs Rust 1.94.1,
  runs the VM Cargo tests, builds the VM binary for direct/machine parity,
  runs the parity manifest, and runs the linked module integration harness.

The workflow is triggered for CD target, fixture, utility, documentation, or
workflow changes on pull requests and pushes to `main`. The VM job is the only
job that sets `CD_COMPILER_ROOT`.

Run `31245584718` for committed baseline `770542a7e` completed with both jobs
failing during test setup: the clean runner had built `llc`, `FileCheck`,
`count`, and `not`, but `llvm-lit` could not invoke missing `llvm-config` and
reported missing `llvm-readobj`. The workflow now builds those tools plus
`split-file`. Hosted run `31312424006` for `4704d3668` then built that closure
successfully and passed the VM integration job, but the LLVM-only job failed
only `cdbc-optimization.ll` because its `default<O2>` RUN line also requires
`opt`. The workflow now builds `opt` as well; a hosted rerun remains pending.

## LLVM Build And Checks

Build the target tools before running the gates:

~~~sh
ninja -C build-cd llc FileCheck count not opt llvm-config llvm-readobj split-file
build-cd/bin/llvm-lit -q llvm/test/CodeGen/CD
git diff --check
~~~

Focused module and diagnostic tests can be run directly:

~~~sh
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-modules.ll llvm/test/CodeGen/CD/cdbc-module-errors.ll llvm/test/CodeGen/CD/cdbc-module-fallthrough.ll llvm/test/CodeGen/CD/cdbc-module-link-diagnostic-entry.ll llvm/test/CodeGen/CD/cdbc-module-link-diagnostic-dependency.ll
~~~

The driver and output-mode boundary can be checked directly:

~~~sh
build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD/cdbc-driver-options.ll
~~~

The wider release matrix remains separate from this opt-in gate. In
particular, the supported O0/O2, metadata-free, object-output rejection,
invalid IR, and invalid CD ABI operation checks are covered by the existing
CD lit fixtures and the direct/machine parity manifest. The
`cdbc-driver-options.ll` fixture also records the remaining `-g` subcase as an
upstream tool boundary: LLVM 24 `llc` rejects `-g` as an unknown command-line
argument before CD target selection, so CD debug coverage uses explicit `!dbg`
and `!cd.sources` metadata instead.

## Coverage Matrix

| Area | Evidence | Current result |
| --- | --- | --- |
| O0 and O2 | cdbc-optimization.ll | Passed in the CD lit suite |
| Source-backed debug locations and ranges | cdbc-debug-*.ll and the parity manifest | Direct/machine parity passed |
| Metadata-free output | The metadata-free observability entry for cdbc-machine.ll | No debug sections and unknown runtime locations passed |
| Debugger command aliases | The aliases observability entry for cdbc-debug-ranges.ll | Direct/machine canonical step/next/quit parity passed |
| Debugger help output | The help observability entry for cdbc-debug-ranges.ll | Direct/machine command-reference parity passed |
| Source-backed debugger error pause | The debug-error parity entry for cdbc-debug-runtime.ll | Direct/machine error pause and call-stack parity passed |
| Debugger pause-state contract | The state parity entry for cdbc-debug-contract.ll and `docs/cd-bytecode-debugger-contract.md` | Entry, source-breakpoint, and error pause fields match; synthetic entry exception is constrained |
| Driver options and output modes | cdbc-driver-options.ll and cdbc-optimization.ll | `-mtriple`, O0/O2, object rejection, and the upstream `-g` boundary passed |
| Object output rejection | cdbc-basic.ll and cdbc-machine-control-flow.ll | Stable rejection passed |
| Invalid IR shape and CD ABI operations | cdbc-invalid-shape.ll, cdbc-array-errors.ll, cdbc-map-errors.ll, and related error fixtures | Direct/machine diagnostics passed |
| Dynamic CD `select` propagation | cdbc-dynamic-select.ll and cdbc-dynamic-select-errors.ll | Proven address-space-zero arms pass direct/machine artifact and behavior parity; ordinary, foreign, and mixed pointer arms remain rejected |
| Dynamic CD PHI propagation | cdbc-dynamic-phi.ll and cdbc-dynamic-phi-errors.ll | Proven address-space-zero incoming edges, including a loop-carried PHI, pass direct/machine artifact and behavior parity; ordinary, foreign, mixed, undef, and poison incoming values remain rejected |
| Dynamic CD one-slot storage | cdbc-dynamic-storage.ll and cdbc-dynamic-storage-errors.ll | Direct/machine load/store parity for straight-line replacement and branch-complete initialization; uninitialized, partially initialized, escaped, self-address, and volatile storage remain rejected |
| Native `contains` | cdbc-native-contains.ll, cdbc-native-contains-runtime.ll, and contains cases in cdbc-native-errors.ll | Array and map membership, scalar/CD needle capability, runtime non-collection error, and ordinary-pointer diagnostics pass direct/machine parity |
| Native `slice` | cdbc-native-slice.ll, cdbc-native-slice-runtime.ll, and slice cases in cdbc-native-errors.ll | Empty/middle/tail slices, runtime non-array error, scalar/type/arity/result/pointer diagnostics, and direct/machine parity pass |
| Native `copy` | cdbc-native-copy.ll, cdbc-native-copy-runtime.ll, and copy cases in cdbc-native-errors.ll | Empty/non-empty fresh shallow copies, runtime non-array error, arity/result/pointer diagnostics, and direct/machine parity pass |
| Native `concat` | cdbc-native-concat.ll, cdbc-native-concat-runtime.ll, and concat cases in cdbc-native-errors.ll | Empty/non-empty ordered concatenation, runtime non-array error, arity/result/pointer diagnostics, and direct/machine parity pass |
| Native `push` | cdbc-native-push.ll, cdbc-native-push-runtime.ll, and push cases in cdbc-native-errors.ll | Scalar/CD value append, shared-array mutation, nil result, runtime non-array error, arity/result/pointer diagnostics, and direct/machine parity pass |
| Native `remove` | cdbc-native-remove.ll, cdbc-native-remove-runtime.ll, and remove cases in cdbc-native-errors.ll | Scalar/CD key removal, shared-map mutation, removed-value return, runtime non-map error, arity/result/pointer diagnostics, and direct/machine parity pass |
| Native `clear` | cdbc-native-clear.ll, cdbc-native-clear-runtime.ll, and clear cases in cdbc-native-errors.ll | In-place map clearing, nil result, runtime non-map error, arity/result/pointer diagnostics, and direct/machine parity pass |
| Native `merge` | cdbc-native-merge.ll, cdbc-native-merge-left-runtime.ll, cdbc-native-merge-right-runtime.ll, and merge cases in cdbc-native-errors.ll | Fresh ordered map, right-side duplicate replacement, both runtime map errors, arity/result/pointer diagnostics, and direct/machine parity pass |
| Native `keys` | cdbc-native-keys.ll, cdbc-native-keys-runtime.ll, and keys cases in cdbc-native-errors.ll | Empty/non-empty insertion-ordered key arrays, runtime non-map error, arity/result/pointer diagnostics, and direct/machine parity pass |
| Native `values` | cdbc-native-map-values.ll, cdbc-native-values-runtime.ll, and values cases in cdbc-native-errors.ll | Empty/non-empty insertion-ordered value arrays, runtime non-map error, arity/result/pointer diagnostics, and direct/machine parity pass |
| Callback native `map`, `filter`, `flatMap`, `reduce`, `any`, `all`, `count`, `find`, and `findIndex` | cdbc-native-map.ll, cdbc-native-map-runtime.ll, cdbc-native-flat-map.ll, cdbc-native-flat-map-runtime.ll, cdbc-native-flat-map-errors.ll, cdbc-native-reduce.ll, cdbc-native-reduce-runtime.ll, cdbc-native-reduce-errors.ll, cdbc-native-filter.ll, cdbc-native-filter-runtime.ll, cdbc-native-any-all.ll, cdbc-native-any-runtime.ll, cdbc-native-all-runtime.ll, cdbc-native-count.ll, cdbc-native-count-runtime.ll, cdbc-native-find.ll, cdbc-native-find-runtime.ll, cdbc-native-find-index.ll, cdbc-native-find-index-runtime.ll, cdbc-native-predicate-errors.ll, and callback cases in cdbc-native-errors.ll | Direct/machine output, runtime type errors, empty-array identities, one-level callback-array flattening, left-to-right accumulator threading, predicate ABI, full count traversal, first-match/no-match `find` behavior, zero-based first-match/no-match `findIndex` behavior, and callback-shape diagnostics passed |
| llc -g | cdbc-driver-options.ll | Explicitly rejected by the upstream llc driver; target-side `-g` semantics remain open |

The parity evidence can be rerun with an explicitly built VM binary:

~~~sh
PYTHONDONTWRITEBYTECODE=1 python3 llvm/utils/cd_bytecode_parity.py --llc build-cd/bin/llc --vm cd-compiler/vm-rs/target/debug/compiler-design-vm --manifest llvm/test/CodeGen/CD/cdbc-machine-parity.list --root .
~~~

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
