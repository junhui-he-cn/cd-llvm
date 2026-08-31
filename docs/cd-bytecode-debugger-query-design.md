# CD bytecode debugger query design

Status: design-only M5 follow-on, 2026-08-09. The existing pause-state and
resume contract in `docs/cd-bytecode-debugger-contract.md` remains authoritative.
This document fixes the next read-only query surface without implementing a VM
command, changing the artifact format, or changing the `cdbc 0.2` version.

## Goals and boundary

The queries are available only while the interactive debugger is paused. They
inspect the current pause and never resume, mutate program state, change
breakpoints, or affect step/next suppression. A query can therefore be issued
before a later `continue`, `step`, `next`, or `quit` command at the same pause.

The first query slice is deliberately limited to:

```text
where | w
locals | l
breakpoints | bl
list [<before>] [<after>]
```

The aliases are part of the command contract. `where`, `locals`, and
`breakpoints` take no arguments. `list` takes zero, one, or two non-negative
decimal counts. With no counts it uses `2` lines before and after the current
line; with one count it uses that count on both sides. Each count is bounded by
`32`. The current line is always included when source-backed context exists.

The design does not include expression evaluation, local assignment, arbitrary
frame selection, conditional breakpoints, watchpoints, or command scripting.
Those features would need separate state, resource, and security contracts.

## Output records

Every successful query emits one or more machine-readable lines. Values and
source text use the existing debugger quoting rules: backslashes, quotes, and
control characters are escaped and the payload is enclosed in double quotes.
The output never exposes LLVM or VM register identities.

### `where`

`where` reports the active call stack in entry-to-current order. Depth zero is
the entry frame and the greatest depth is the current frame:

```text
debug where depth=0 function=main module=none location=contract.cd:1:1
debug where depth=1 function=identity module=none location=contract.cd:1:1
```

The `module` and `location` fields use the same source-table lookup and
`<unknown>` rendering as the frozen pause record. A frame without a source
location uses `module=none location=<unknown>`. Stack depth is included so a
consumer does not need to infer frame identity from names.

### `locals`

`locals` reports the locals visible in the current frame only:

```text
debug locals function=identity values={input="2"}
```

The values are ordered and named exactly as in the pause line, including the
existing source-local name disambiguation rule. An empty set is represented by
`values={}`. Closure and global visibility therefore follows the VM's current
pause snapshot; this query does not introduce a second visibility model.

The first slice has no `locals <depth>` form. Reporting another frame's
environment would require extending the pause snapshot and must be reviewed as
a separate VM API decision.

### `breakpoints`

`breakpoints` lists active breakpoints in creation order. Deleted breakpoints
are absent and IDs are never reused:

```text
debug breakpoint-list id=1 spec=contract.cd:1
debug breakpoint-list id=2 spec=contract.cd:0-1
```

An empty set has one stable record:

```text
debug breakpoint-list empty
```

The `spec` field is the same normalized representation emitted when the
breakpoint is created. The query does not report transient suppression state;
that state remains an implementation detail of `step` and `next`.

### `list`

`list` reports source lines around the current source-backed line. Line numbers
are one-based, matching line breakpoints. The source path is the path from the
explicit `!cd.sources` table, not a path inferred from ordinary LLVM debug
metadata:

```text
debug list source="contract.cd" line=1 current=true text="input"
debug list source="contract.cd" line=2 current=false text="return input"
```

Records are emitted in ascending source-line order. `current=true` appears on
exactly one record. Context is clipped at the start and end of the source; no
blank padding records are emitted. A source line is emitted without its line
terminator, while an embedded carriage return or other control character is
quoted like every other source payload.

If the current pause has no source location, `list` leaves the debugger paused
and reports:

```text
debug error message=list unavailable at <unknown>
```

If its counts are malformed or exceed the bound, it reports a command-specific
`debug error message=...` and leaves the pause unchanged. The existing unknown
command behavior remains unchanged for all other commands.

## Help and errors

After implementation, the canonical help marker will extend the current command
reference with the query commands:

```text
debug help: break <path>:<line> | break-range <path>:<start>-<end> | continue | step | next | delete <id> | where | locals | breakpoints | list [<before>] [<after>] | quit
```

The short aliases are not expanded in the help line, matching the current
contract's treatment of `c`, `s`, `n`, and `q`. Query errors are non-terminal and
leave the debugger at the same pause. EOF and `quit` retain their existing
termination behavior.

Queries are valid at entry, breakpoint, step, next, and error pauses. At an
error pause, issuing a query must not suppress or duplicate the runtime error;
the following `continue` and `quit` behavior remains the frozen contract.

## Direct and machine parity

The query fixture will use explicit `!cd.sources` and a source-backed range so
both emitters have the same source bytes and current line. The future parity
case will drive a sequence equivalent to:

```text
break contract.cd:1
where
locals
breakpoints
list 1 1
continue
quit
```

Direct and machine output must match for every query line, source line, local,
breakpoint ID/spec, and command marker. The only allowed difference remains
the existing synthetic entry location and matching `main@...` stack frame
described by the pause-state contract. A new difference is a parity failure.

The future parity manifest may add a `queries` observability contract beside
the existing `ranges`, `metadata-free`, `step-next`, `aliases`, `help`, and
`line-delete` cases. The fixture must also cover an empty breakpoint set,
unknown-location `list`, malformed counts, and a query followed by an error
pause so read-only behavior is verified at both normal and failing states.

## Implementation gate

This design is intentionally separate from the current contract-only commit.
The implementation slice may update the sibling VM CLI/debugger, the parity
harness, one source-backed fixture, and the verification matrix, but it must
not add an artifact opcode, artifact field, source bytes inferred from DWARF,
or a new `.cdbc` version. It must run the existing direct/machine parity and
Rust VM gates before the commands are removed from the current unknown-command
set.
