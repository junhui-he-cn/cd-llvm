#!/usr/bin/env python3
"""Check direct and TableGen machine CD bytecode parity.

The harness compiles each LLVM IR input twice, validates both artifacts with
the Rust VM, and compares their observable output.  Artifact-mode cases also
compare a normalized artifact projection that only canonicalizes constant,
name, function, and virtual-register indices.  Machine-specific control-flow
expansions can therefore be covered by behavior-mode cases without hiding an
unexpected scalar instruction change behind a broad text normalization.
Error-mode cases require both paths to fail with the same exact VM diagnostic.
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
        if len(fields) == 3 and fields[0] == "error" and fields[2]:
            entries.append((fields[0], fields[1], fields[2]))
            continue
        if len(fields) != 2 or fields[0] not in {"artifact", "behavior"}:
            raise ValueError(
                f"manifest line {line_number}: expected '<artifact|behavior> <input>' "
                "or 'error <input> \"<diagnostic>\"'"
            )
    return entries


def _run(command, description):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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
    if diagnostic != expected:
        raise RuntimeError(
            f"{description} diagnostic mismatch:\n"
            f"expected: {expected!r}\nactual: {diagnostic!r}"
        )
    return diagnostic


def _check_case(llc, vm, root, mode, input_path, expected_error=None):
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

        if mode == "error":
            if expected_error is None:
                raise RuntimeError(f"error case has no expected diagnostic: {input_path}")
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
            expected_error = entry[2] if len(entry) == 3 else None
            _check_case(args.llc, args.vm, args.root, mode, input_path, expected_error)
            print(f"{mode} parity: {input_path}")
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
