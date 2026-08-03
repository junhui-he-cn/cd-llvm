# CD bytecode debugger state contract

Status: contract-only M5 slice, 2026-08-03. This document freezes the
existing Rust VM debugger surface before adding query commands or transporting
dynamic values across new LLVM function boundaries. It does not add a VM
command, an artifact field, or a new `cdbc` version.

## Scope

The debugger currently pauses before the first `main` instruction, at a
matching breakpoint or step/next transition, and at a runtime failure. The
current state machine is:

```text
entry pause -> continue -> running -> exit
entry pause -> step/next -> source pause -> continue -> exit
entry pause -> continue -> runtime-error pause -> quit
```

The supported commands are:

```text
break <path>:<line>
break-range <path>:<start>-<end>
continue | c
step | s
next | n
delete <id>
help
quit | q
```

Aliases emit the canonical resume markers (`continue`, `step`, `next`) and
`q` emits the same `debug quit` marker as `quit`. Breakpoint IDs start at one
and increase for each breakpoint created in the session.

## Pause-state record

Every pause is one line with this field order:

```text
pause reason=<reason> function=<function> instruction=<index> module=<module> location=<location> stack=<stack> locals={<locals>}[ range=s<source>:<start>:<end>]
```

The fields have these meanings:

- `reason` is `entry`, `breakpoint`, `step`, `next`, or `error`.
- `function` is the current bytecode function name; the entry body is `main`.
- `instruction` is the zero-based instruction index in that body.
- `module` is the source module name, or `none` for the current path.
- `location` is `<path>:<line>:<column>`, or `<unknown>` when the current
  instruction has no debug location.
- `stack` is the active call chain in entry-to-current order, with each frame
  written as `<function>@<location>`. A frame without a location uses
  `<unknown>`.
- `locals` is a comma-separated set of `name=<quoted-value>` entries. The
  current VM orders locals deterministically and writes an empty set as `{}`.
- `range` is present only when the current location carries a source-local,
  half-open byte range. Its source index and offsets are zero-based.

Values use the debugger's existing quoted rendering, including escaped
backslashes, quotes, and control characters. The state line is deliberately
stable and does not expose register identities.

The current contract fixture covers all fields with these states:

1. an entry pause with an empty local set;
2. an `identity` breakpoint pause with `input="2"` and `range=s0:0:1`;
3. an `identity` error pause with its source-backed call stack and the same
   captured `input` local.

## Command and error output

The existing command markers are part of the contract:

```text
debug breakpoint id=<id> spec=<normalized-spec>
debug breakpoint-deleted id=<id>
debug resumed command=continue|step|next
debug help: break <path>:<line> | break-range <path>:<start>-<end> | continue | step | next | delete <id> | quit
debug quit
```

An invalid command prints

```text
debug error message=unknown command `<command>`
```

and leaves the debugger paused. The current surface has no `list`, `where`,
`locals`, or breakpoint-listing query. Those words are therefore invalid
commands with the same error behavior; they are not reserved future output.
Malformed breakpoint and delete commands use the existing
`debug error message=...` form and also leave the session paused.

EOF while paused terminates the session successfully without emitting a
`debug quit` marker. `quit` terminates successfully and emits that marker.
At an error pause, `quit` suppresses the ordinary runtime diagnostic;
`continue` resumes propagation of the same runtime error to the caller.

## Direct/machine parity

The direct `ModulePass` emitter remains the compatibility path and the
TableGen machine emitter remains opt-in. The state parity case compares the
complete pause lines and command markers. It permits exactly two synthetic
entry differences:

1. the direct entry pause may use its source location while the machine entry
   pause may use `<unknown>`, including the matching `main@...` stack frame;
2. the direct dump may contain one `main 0 = ...` debug-location record for
   the synthetic function materialization while the machine dump omits it.

The parity harness normalizes only those values. The reason, function,
instruction, module, locals, range, source pause, error pause, stack frames
after entry, breakpoint markers, resume markers, and quit marker must match
exactly. Any additional debug-section or state difference is a parity failure.

The outer manifest records the contract as:

```text
state <input> "<semicolon-separated commands>" "<runtime-error substring>"
```

The contract fixture is
`llvm/test/CodeGen/CD/cdbc-debug-contract.ll`; it drives
`break-range contract.cd:0-1;continue;continue;quit` through the three
documented pauses and
also requires both artifacts to report the same `division by zero` runtime
diagnostic when run normally.

## Deferred boundary

Adding `list`, `where`, local inspection, breakpoint listing, or other query
commands requires a follow-on public design and fixture. Dynamic-value
transport across function parameters/returns, PHI/select, local storage, or
callback native arguments is a separate ABI decision and is not implied by
this debugger contract.
