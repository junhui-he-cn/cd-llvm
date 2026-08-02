#!/usr/bin/env python3
"""Check direct and TableGen machine CD bytecode parity.

The harness compiles each LLVM IR input twice, validates both artifacts with
the Rust VM, and compares their observable output.  Artifact-mode cases also
compare a normalized artifact projection that only canonicalizes constant,
name, function, and virtual-register indices.  Machine-specific control-flow
expansions can therefore be covered by behavior-mode cases without hiding an
unexpected scalar instruction change behind a broad text normalization.
Runtime-error cases require both paths to fail with the same VM diagnostic.
Observability cases additionally compare debug sections, trace, profile, and
scripted interactive-debugger output for metadata-backed and metadata-free
artifacts. The step-next contract also checks the debugger's distinct resume
and pause reasons. The line-delete contract also checks breakpoint removal
before execution resumes.
"""

import argparse
import re
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path


_INDEX = re.compile(r"\b([cnfr])(\d+)\b")
_CONSTANT = re.compile(r"^\s{2}(c\d+) = (.+)$")
_NAME = re.compile(r"^\s{2}(n\d+) = (.+)$")
_FUNCTION = re.compile(r'^function (f\d+) name=(\S+) arity=')
_BODY = re.compile(r"^(?:main registers=|function f\d+ name=)")
_DEBUG_SECTION_HEADERS = {"debug_sources:", "debug_locations:", "debug_ranges:"}
_REGISTER_DEFINITION = re.compile(r"^\s+r(\d+) =")


def _table_mapping(lines, pattern, prefix):
    values = {}
    for line in lines:
        match = pattern.match(line)
        if match:
            values[match.group(1)[1:]] = match.group(2)

    canonical = {value: f"{prefix}{index}" for index, value in enumerate(sorted(set(values.values())))}
    return {old: canonical[value] for old, value in values.items()}


def _function_mapping(lines):
    values = {}
    for line in lines:
        match = _FUNCTION.match(line)
        if match:
            values[match.group(1)[1:]] = match.group(2)

    canonical = {
        value: f"f{index}" for index, value in enumerate(sorted(set(values.values())))
    }
    return {old: canonical[value] for old, value in values.items()}


def _body_register_mappings(lines):
    mappings = {}
    body_start = None
    body_map = None

    def finish(end):
        if body_start is not None:
            mappings[(body_start, end)] = body_map

    for index, line in enumerate(lines):
        if _BODY.match(line):
            finish(index)
            body_start = index + 1
            body_map = {}
            continue
        if body_map is None:
            continue
        match = _REGISTER_DEFINITION.match(line)
        if match:
            old = match.group(1)
            body_map.setdefault(old, f"r{len(body_map)}")

    finish(len(lines))
    return mappings


def _body_mapping_for(index, body_mappings):
    for (start, end), mapping in body_mappings.items():
        if start <= index < end:
            return mapping
    return {}


def _replace_indices(line, mappings):
    def replace(match):
        prefix, number = match.groups()
        return mappings.get(prefix, {}).get(number, match.group(0))

    return _INDEX.sub(replace, line)


def _sort_table_definitions(lines, header, pattern):
    try:
        start = lines.index(header) + 1
    except ValueError:
        return

    end = start
    while end < len(lines) and lines[end].strip():
        end += 1

    definitions = [line for line in lines[start:end] if pattern.match(line)]
    if len(definitions) < 2:
        return
    lines[start:end] = sorted(definitions, key=lambda line: pattern.match(line).group(1))


def normalize_artifact_indices(text):
    """Canonicalize only table and register indices in a cdbc artifact."""

    trailing_newline = text.endswith("\n")
    lines = text.splitlines()
    mappings = {
        "c": _table_mapping(lines, _CONSTANT, "c"),
        "n": _table_mapping(lines, _NAME, "n"),
        "f": _function_mapping(lines),
        "r": {},
    }
    body_mappings = _body_register_mappings(lines)

    normalized = []
    for index, line in enumerate(lines):
        body_mapping = _body_mapping_for(index, body_mappings)
        line_mappings = dict(mappings)
        line_mappings["r"] = body_mapping
        normalized.append(_replace_indices(line, line_mappings))

    _sort_table_definitions(normalized, "constants:", _CONSTANT)
    _sort_table_definitions(normalized, "names:", _NAME)
    result = "\n".join(normalized)
    return result + ("\n" if trailing_newline else "")


def parse_manifest(lines):
    """Return manifest entries; error entries also carry an expected diagnostic."""

    entries = []
    for line_number, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        try:
            fields = shlex.split(stripped)
        except ValueError as error:
            raise ValueError(f"manifest line {line_number}: {error}") from error
        if len(fields) == 2 and fields[0] in {"artifact", "behavior"}:
            entries.append((fields[0], fields[1]))
            continue
        if len(fields) == 3 and fields[0] == "runtime-error" and fields[2]:
            entries.append((fields[0], fields[1], fields[2]))
            continue
        if (
            len(fields) == 4
            and fields[0] == "observability"
            and fields[2]
            and fields[3] in {"ranges", "metadata-free", "step-next", "line-delete"}
        ):
            entries.append((fields[0], fields[1], fields[2], fields[3]))
            continue
        if len(fields) != 2 or fields[0] not in {"artifact", "behavior"}:
            raise ValueError(
                f"manifest line {line_number}: expected '<artifact|behavior> <input>', "
                "'runtime-error <input> \"<diagnostic>\"', or "
                "'observability <input> \"<commands>\" "
                "<ranges|metadata-free|step-next|line-delete>'"
            )
    return entries


def _run(command, description, input_text=None):
    result = subprocess.run(
        command,
        text=True,
        input=input_text,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode:
        command_text = " ".join(str(argument) for argument in command)
        details = result.stderr.strip() or result.stdout.strip() or "no diagnostic"
        raise RuntimeError(f"{description} failed ({command_text}): {details}")
    return result.stdout


def _run_expected_failure(command, description, expected):
    result = subprocess.run(
        command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    if result.returncode == 0:
        command_text = " ".join(str(argument) for argument in command)
        raise RuntimeError(f"{description} unexpectedly succeeded ({command_text})")
    diagnostic = result.stderr.strip() or result.stdout.strip()
    if expected not in diagnostic:
        raise RuntimeError(
            f"{description} diagnostic does not contain expected text:\n"
            f"expected substring: {expected!r}\nactual: {diagnostic!r}"
        )
    return diagnostic


def _debug_sections(text):
    lines = text.splitlines(keepends=True)
    for index, line in enumerate(lines):
        if line.rstrip("\n") in _DEBUG_SECTION_HEADERS:
            return "".join(lines[index:])
    return ""


def _compare_observability_output(input_path, surface, direct, machine):
    if direct != machine:
        raise RuntimeError(
            f"{surface} output mismatch for {input_path}:\n"
            f"direct: {direct!r}\nmachine: {machine!r}"
        )


def _check_observability(
    vm,
    input_path,
    direct,
    machine,
    direct_dump,
    machine_dump,
    direct_run,
    machine_run,
    debug_commands,
    contract,
):
    direct_sections = _debug_sections(direct_dump)
    machine_sections = _debug_sections(machine_dump)
    _compare_observability_output(
        input_path, "debug section", direct_sections, machine_sections
    )

    command_input = "\n".join(debug_commands.split(";")) + "\n"
    outputs = {"trace": [], "profile": [], "debug": []}
    for surface in outputs:
        direct_output = _run(
            [str(vm), surface, str(direct)],
            f"direct {surface} for {input_path}",
            command_input if surface == "debug" else None,
        )
        machine_output = _run(
            [str(vm), surface, str(machine)],
            f"machine {surface} for {input_path}",
            command_input if surface == "debug" else None,
        )
        outputs[surface] = [direct_output, machine_output]
        _compare_observability_output(
            input_path, surface, outputs[surface][0], outputs[surface][1]
        )

    trace_output = outputs["trace"][0]
    profile_output = outputs["profile"][0]
    debug_output = outputs["debug"][0]
    all_output = (
        direct_dump
        + machine_dump
        + direct_sections
        + direct_run
        + machine_run
        + trace_output
        + profile_output
        + debug_output
    )
    if contract == "ranges":
        required = {
            "debug section": (direct_sections, "debug_ranges:"),
            "debug source": (direct_sections, 's0 path="ranges.cd"'),
            "debug section range": (direct_sections, "main 2 = s0:6:11"),
            "trace location": (trace_output, "location=ranges.cd:1:7"),
            "trace range": (trace_output, "range=s0:6:11"),
            "profile range": (
                profile_output,
                'profile source_range source=s0 path="ranges.cd" start=6 end=11 hits=1',
            ),
            "debug location": (debug_output, "location=ranges.cd:1:7"),
            "debug range": (debug_output, "range=s0:6:11"),
            "debug pause": (debug_output, "pause reason=breakpoint"),
        }
        for label, (output, expected) in required.items():
            if expected not in output:
                raise RuntimeError(
                    f"{label} missing for {input_path}: expected {expected!r}"
                )
    elif contract == "metadata-free":
        if direct_sections:
            raise RuntimeError(
                f"metadata-free debug section unexpectedly present for {input_path}: "
                f"{direct_sections!r}"
            )
        if "<unknown>" not in trace_output or "<unknown>" not in debug_output:
            raise RuntimeError(
                f"metadata-free trace/debug output lacks <unknown> for {input_path}"
            )
        if "range=" in all_output or "source_range" in all_output:
            raise RuntimeError(
                f"metadata-free observability output contains source range for "
                f"{input_path}"
            )
    elif contract == "step-next":
        required = {
            "entry pause": "pause reason=entry",
            "step command": "debug resumed command=step",
            "step pause": "pause reason=step",
            "next command": "debug resumed command=next",
            "next pause": "pause reason=next",
            "quit command": "debug quit",
        }
        for label, expected in required.items():
            if expected not in debug_output:
                raise RuntimeError(
                    f"{label} missing for {input_path}: expected {expected!r}"
                )
    elif contract == "line-delete":
        required = {
            "breakpoint creation": "debug breakpoint id=1 spec=ranges.cd:1",
            "continue before hit": "debug resumed command=continue",
            "breakpoint pause": "pause reason=breakpoint",
            "breakpoint deletion": "debug breakpoint-deleted id=1",
        }
        for label, expected in required.items():
            if expected not in debug_output:
                raise RuntimeError(
                    f"{label} missing for {input_path}: expected {expected!r}"
                )
        if debug_output.count("pause reason=breakpoint") != 1:
            raise RuntimeError(
                f"line-delete contract expected exactly one breakpoint pause for "
                f"{input_path}"
            )
        if not debug_output.endswith("2\n"):
            raise RuntimeError(
                f"line-delete contract did not finish with program output for "
                f"{input_path}"
            )
    else:
        raise RuntimeError(f"unknown observability contract {contract!r}")


def _check_case(
    llc,
    vm,
    root,
    mode,
    input_path,
    expected_error=None,
    debug_commands=None,
    observability_contract=None,
):
    source = (root / input_path).resolve()
    if not source.is_file():
        raise RuntimeError(f"input does not exist: {source}")

    with tempfile.TemporaryDirectory(prefix="cd-bytecode-parity-") as temporary:
        directory = Path(temporary)
        direct = directory / "direct.cdbc"
        machine = directory / "machine.cdbc"
        _run(
            [str(llc), "-mtriple=cd-unknown-unknown", str(source), "-o", str(direct)],
            f"direct emission for {input_path}",
        )
        _run(
            [
                str(llc),
                "-mtriple=cd-unknown-unknown",
                "-cd-backend=machine",
                str(source),
                "-o",
                str(machine),
            ],
            f"machine emission for {input_path}",
        )

        direct_dump = _run([str(vm), "dump", str(direct)], f"direct dump for {input_path}")
        machine_dump = _run([str(vm), "dump", str(machine)], f"machine dump for {input_path}")
        if not direct_dump.startswith("cdbc 0.1") or not machine_dump.startswith("cdbc 0.1"):
            raise RuntimeError(f"VM dump did not produce cdbc 0.1 for {input_path}")

        if mode == "runtime-error":
            if expected_error is None:
                raise RuntimeError(
                    f"runtime-error case has no expected diagnostic: {input_path}"
                )
            direct_error = _run_expected_failure(
                [str(vm), "run", str(direct)],
                f"direct run for {input_path}",
                expected_error,
            )
            machine_error = _run_expected_failure(
                [str(vm), "run", str(machine)],
                f"machine run for {input_path}",
                expected_error,
            )
            if direct_error != machine_error:
                raise RuntimeError(
                    f"VM error mismatch for {input_path}:\n"
                    f"direct: {direct_error!r}\nmachine: {machine_error!r}"
                )
            return

        direct_output = _run([str(vm), "run", str(direct)], f"direct run for {input_path}")
        machine_output = _run([str(vm), "run", str(machine)], f"machine run for {input_path}")
        if direct_output != machine_output:
            raise RuntimeError(
                f"VM output mismatch for {input_path}:\n"
                f"direct: {direct_output!r}\n"
                f"machine: {machine_output!r}"
            )

        if mode == "observability":
            if debug_commands is None or observability_contract is None:
                raise RuntimeError(
                    f"observability case has incomplete contract: {input_path}"
                )
            _check_observability(
                vm,
                input_path,
                direct,
                machine,
                direct_dump,
                machine_dump,
                direct_output,
                machine_output,
                debug_commands,
                observability_contract,
            )
            return

        if mode == "artifact":
            direct_artifact = direct.read_text(encoding="utf-8")
            machine_artifact = machine.read_text(encoding="utf-8")
            if normalize_artifact_indices(direct_artifact) != normalize_artifact_indices(
                machine_artifact
            ):
                raise RuntimeError(
                    f"normalized artifact mismatch for {input_path}; "
                    "use behavior mode only for a documented machine-specific lowering"
                )


def _parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llc", required=True, type=Path)
    parser.add_argument("--vm", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    return parser


def main(argv=None):
    args = _parser().parse_args(argv)
    try:
        entries = parse_manifest(args.manifest.read_text(encoding="utf-8").splitlines())
        for entry in entries:
            mode, input_path = entry[:2]
            expected_error = entry[2] if mode == "runtime-error" else None
            debug_commands = entry[2] if mode == "observability" else None
            observability_contract = entry[3] if mode == "observability" else None
            _check_case(
                args.llc,
                args.vm,
                args.root,
                mode,
                input_path,
                expected_error,
                debug_commands,
                observability_contract,
            )
            print(f"{mode} parity: {input_path}")
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
