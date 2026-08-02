#!/usr/bin/env python3
"""Opt-in integration checks for LLVM-produced CD module products.

The harness deliberately lives outside LLVM lit.  It compiles a small module
product set through both CD lowering paths, then delegates artifact parsing,
linking, and execution to the sibling Rust VM.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


DEFAULT_INPUT_ROOT = (
    Path(__file__).resolve().parent / "testdata" / "cd-module-link"
)
EXPECTED_OUTPUT = "1\n2\n3\n"


def llc_arguments(llc, source, output, backend):
    arguments = [
        str(llc),
        "-mtriple=cd-unknown-unknown",
    ]
    if backend == "machine":
        arguments.append("-cd-backend=machine")
    arguments.extend(["-cd-artifact=module", str(source), "-o", str(output)])
    return arguments


def vm_command(vm, arguments):
    vm = Path(vm)
    if vm.name == "Cargo.toml":
        manifest = vm
    elif vm.is_dir() and (vm / "Cargo.toml").is_file():
        manifest = vm / "Cargo.toml"
    else:
        return [str(vm), *map(str, arguments)]
    return [
        "cargo",
        "run",
        "--quiet",
        "--manifest-path",
        str(manifest),
        "--",
        *map(str, arguments),
    ]


def replace_module_field(artifact, field, value):
    prefix = f"  {field} = "
    matches = [line for line in artifact.splitlines(keepends=True) if line.startswith(prefix)]
    if len(matches) != 1:
        raise ValueError(f"expected exactly one module field {field!r}")
    old = matches[0]
    return artifact.replace(old, f"{prefix}{value}\n", 1)


def add_dependency_record(artifact, record):
    marker = "  dependencies:\n"
    if artifact.count(marker) != 1:
        raise ValueError("expected exactly one dependencies section")
    dependency_lines = [
        line
        for line in artifact.splitlines()
        if line.startswith("    d") and " target=" in line
    ]
    line = f"    d{len(dependency_lines)} {record}\n"
    return artifact.replace(marker, marker + line, 1)


def run(command, description, input_text=None):
    result = subprocess.run(
        command,
        text=True,
        input=input_text,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        details = result.stderr.strip() or result.stdout.strip() or "no diagnostic"
        raise RuntimeError(f"{description} failed: {details}")
    return result


def expect_failure(command, description, expected):
    result = subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode == 0:
        raise RuntimeError(f"{description} unexpectedly succeeded")
    diagnostic = result.stderr.strip() or result.stdout.strip()
    if expected not in diagnostic:
        raise RuntimeError(
            f"{description} diagnostic does not contain {expected!r}: {diagnostic!r}"
        )
    return result


def compile_module(llc, source, output, backend):
    run(
        llc_arguments(llc, source, output, backend),
        f"{backend} module emission for {source}",
    )


def vm_dump(vm, artifact, expected=None):
    result = run(vm_command(vm, ["dump", artifact]), f"dump for {artifact}")
    if expected is None:
        expected = artifact.read_text(encoding="utf-8")
    if result.stdout != expected or result.stderr:
        raise RuntimeError(
            f"canonical dump mismatch for {artifact}: "
            f"stdout={result.stdout!r} stderr={result.stderr!r}"
        )
    return result.stdout


def validate_unlinked_module(vm, artifact):
    expect_failure(
        vm_command(vm, ["run", artifact]),
        f"unlinked module execution for {artifact}",
        "error: cannot run an unlinked module artifact",
    )


def link_modules(vm, directory, output):
    run(
        vm_command(vm, ["link", directory, output]),
        f"module link for {directory}",
    )
    if not output.is_file():
        raise RuntimeError(f"module linker did not create {output}")


def expect_link_failure(vm, directory, expected):
    expect_failure(
        vm_command(vm, ["link", directory, directory.parent / "rejected.cdbc"]),
        f"module link rejection for {directory}",
        expected,
    )


def artifact_text(path):
    return path.read_text(encoding="utf-8")


def write_products(directory, entry_text, dependency_text):
    directory.mkdir(exist_ok=True)
    (directory / "module-entry.cdbc").write_text(entry_text, encoding="utf-8")
    (directory / "module-dependency.cdbc").write_text(
        dependency_text, encoding="utf-8"
    )


def validate_valid_products(vm, directory):
    products = sorted(directory.glob("module-*.cdbc"))
    for product in products:
        text = artifact_text(product)
        if not text.startswith("cdbc 0.1\n\nartifact: module\n"):
            raise RuntimeError(f"{product} is not a module artifact")
        vm_dump(vm, product, text)
        validate_unlinked_module(vm, product)

    linked = directory.parent / "linked.cdbc"
    link_modules(vm, directory, linked)
    linked_text = artifact_text(linked)
    vm_dump(vm, linked, linked_text)
    result = run(vm_command(vm, ["run", linked]), f"linked execution for {directory}")
    if result.stdout != EXPECTED_OUTPUT or result.stderr:
        raise RuntimeError(
            f"linked execution mismatch for {directory}: "
            f"stdout={result.stdout!r} stderr={result.stderr!r}"
        )


def validate_link_failures(vm, directory, entry_text, dependency_text):
    missing = directory / "missing"
    missing.mkdir()
    (missing / "module-entry.cdbc").write_text(entry_text, encoding="utf-8")
    expect_link_failure(vm, missing, "targets missing module")

    cycle = directory / "cycle"
    cycle.mkdir()
    cycle_dependency = add_dependency_record(
        dependency_text,
        'target="/workspace/cd-llvm-entry.cd" kind=import at=0 requested="./entry.cd"',
    )
    write_products(cycle, entry_text, cycle_dependency)
    expect_link_failure(vm, cycle, "module dependency cycle")

    duplicate = directory / "duplicate"
    duplicate.mkdir()
    (duplicate / "module-entry-a.cdbc").write_text(entry_text, encoding="utf-8")
    (duplicate / "module-entry-b.cdbc").write_text(entry_text, encoding="utf-8")
    expect_link_failure(vm, duplicate, "duplicate module identity")

    non_contiguous = directory / "non-contiguous-entry-order"
    non_contiguous.mkdir()
    second_entry = replace_module_field(dependency_text, "entry", "true")
    second_entry = second_entry.replace(
        '  entry = true\n', '  entry = true\n  entry_order = 2\n', 1
    )
    write_products(non_contiguous, entry_text, second_entry)
    expect_link_failure(vm, non_contiguous, "entry module orders must be contiguous")

    invalid_offset = directory / "invalid-offset"
    invalid_offset.mkdir()
    invalid_entry = entry_text.replace(" at=2 ", " at=999 ", 1)
    write_products(invalid_offset, invalid_entry, dependency_text)
    expect_link_failure(vm, invalid_offset, "instruction offset out of range")


def run_backend(llc, vm, entry_source, dependency_source, backend, temporary):
    backend_directory = temporary / backend
    backend_directory.mkdir()
    entry_artifact = backend_directory / "module-entry.cdbc"
    dependency_artifact = backend_directory / "module-dependency.cdbc"
    compile_module(llc, entry_source, entry_artifact, backend)
    compile_module(llc, dependency_source, dependency_artifact, backend)
    validate_valid_products(vm, backend_directory)
    validate_link_failures(
        vm,
        backend_directory,
        artifact_text(entry_artifact),
        artifact_text(dependency_artifact),
    )


def parser():
    argument_parser = argparse.ArgumentParser(description=__doc__)
    argument_parser.add_argument("--llc", required=True, type=Path)
    argument_parser.add_argument("--vm", required=True, type=Path)
    argument_parser.add_argument("--entry", type=Path)
    argument_parser.add_argument("--dependency", type=Path)
    argument_parser.add_argument(
        "--backend",
        choices=("direct", "machine", "both"),
        default="both",
    )
    return argument_parser


def main(argv=None):
    args = parser().parse_args(argv)
    entry_source = (args.entry or DEFAULT_INPUT_ROOT / "entry.ll").resolve()
    dependency_source = (
        args.dependency or DEFAULT_INPUT_ROOT / "dependency.ll"
    ).resolve()
    if not entry_source.is_file() or not dependency_source.is_file():
        print("error: module-link input fixture is missing", file=sys.stderr)
        return 1

    backends = ("direct", "machine") if args.backend == "both" else (args.backend,)
    try:
        with tempfile.TemporaryDirectory(prefix="cd-module-link-") as temporary:
            temporary_path = Path(temporary)
            for backend in backends:
                run_backend(
                    args.llc,
                    args.vm,
                    entry_source,
                    dependency_source,
                    backend,
                    temporary_path,
                )
                print(f"{backend} module-link integration: valid and rejected graphs")
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
