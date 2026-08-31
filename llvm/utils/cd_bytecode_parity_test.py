#!/usr/bin/env python3
"""Unit tests for the CD direct/machine parity harness."""

import pathlib
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(pathlib.Path(__file__).parent))
import cd_bytecode_parity  # noqa: E402


class CDBytecodeParityTest(unittest.TestCase):
    def test_normalizes_cdbc_02_tables_storage_blocks_and_variants(self):
        direct = """\
cdbc 0.2

constants:
  c9 = number 2
  c4 = number 1

names:
  n7 = "slot"
  n3 = "Option"
  n8 = "Some"
  n1 = "None"

globals:
  g4 = n7

types:
  t8 = enum "Option" v6="Some" payload=1 v4="None" payload=0
  t3 = struct "unused" field0="x"

native_imports:
  i9 = "print" abi=1
  i4 = "sqrt" abi=1

modules:
  m7 = f4

main registers=6:
block b4:
  r9 = constant c4
  r3 = init_global g4, r9
  r7 = make_variant t8, v6 [r9]
  r4 = is_variant r7, t8, v6
  r2 = variant_get r7, t8, v6, 0
  r1 = call_native i4 [r2]
  br_if r4, b9, b3
block b9:
  return_nil
block b3:
  return_nil

function f4 name="worker" arity=1 registers=5:
  param 0 = "value"
block b7:
  r6 = load_local l8
  r5 = load_upvalue u2
  r4 = set_local l8, r5
  r3 = load_global g4
  r2 = call_native i9 [r3]
  return r2
"""
        machine = """\
cdbc 0.2

constants:
  c1 = number 1
  c8 = number 2

names:
  n0 = "None"
  n4 = "Some"
  n6 = "Option"
  n2 = "slot"

globals:
  g2 = n2

types:
  t0 = struct "unused" field0="x"
  t1 = enum "Option" v9="Some" payload=1 v2="None" payload=0

native_imports:
  i1 = "sqrt" abi=1
  i5 = "print" abi=1

modules:
  m3 = f0

main registers=6:
block b0:
  r5 = constant c1
  r6 = init_global g2, r5
  r7 = make_variant t1, v9 [r5]
  r8 = is_variant r7, t1, v9
  r9 = variant_get r7, t1, v9, 0
  r10 = call_native i1 [r9]
  br_if r3, b1, b2
block b1:
  return_nil
block b2:
  return_nil

function f0 name="worker" arity=1 registers=5:
  param 0 = "value"
block b1:
  r0 = load_local l0
  r1 = load_upvalue u0
  r2 = set_local l0, r1
  r3 = load_global g2
  r4 = call_native i5 [r3]
  return r4
"""

        self.assertEqual(
            cd_bytecode_parity.normalize_artifact_indices(direct),
            cd_bytecode_parity.normalize_artifact_indices(machine),
        )

    def test_normalization_preserves_quoted_indices(self):
        artifact = """\
cdbc 0.2

constants:

names:
  n4 = "r9 v7 g3 l2"

main registers=0:
block b8:
  return_nil

debug_sources:
  s0 path="source-r9-v7.cd" text="let r9 = v7;\\n"
"""

        normalized = cd_bytecode_parity.normalize_artifact_indices(artifact)
        self.assertIn('n0 = "r9 v7 g3 l2"', normalized)
        self.assertIn('path="source-r9-v7.cd"', normalized)
        self.assertIn('text="let r9 = v7;\\n"', normalized)

    def test_normalizes_permitted_table_and_register_indices(self):
        direct = """\
cdbc 0.2

constants:
  c0 = number 2
  c1 = number 40

names:

main registers=3:
  r1 = constant c0
  r0 = move r1
  return r0
"""
        machine = """\
cdbc 0.2

constants:
  c0 = number 40
  c1 = number 2

names:

main registers=3:
  r0 = constant c1
  r1 = move r0
  return r1
"""

        self.assertEqual(
            cd_bytecode_parity.normalize_artifact_indices(direct),
            cd_bytecode_parity.normalize_artifact_indices(machine),
        )

    def test_normalization_does_not_hide_opcode_changes(self):
        add = """\
cdbc 0.2

constants:
  c0 = number 1
  c1 = number 2

names:

main registers=3:
  r0 = constant c0
  r1 = constant c1
  r2 = add r0, r1
  return r2
"""
        subtract = add.replace("r2 = add r0, r1", "r2 = subtract r0, r1")

        self.assertNotEqual(
            cd_bytecode_parity.normalize_artifact_indices(add),
            cd_bytecode_parity.normalize_artifact_indices(subtract),
        )

    def test_reads_modeled_parity_manifest(self):
        manifest = """\
# mode and input path
artifact llvm/test/CodeGen/CD/cdbc-machine.ll
behavior llvm/test/CodeGen/CD/cdbc-machine-control-flow.ll
runtime-error llvm/test/CodeGen/CD/cdbc-array-assign-runtime.ll "array index out of range"
"""

        self.assertEqual(
            cd_bytecode_parity.parse_manifest(manifest.splitlines()),
            [
                ("artifact", "llvm/test/CodeGen/CD/cdbc-machine.ll"),
                ("behavior", "llvm/test/CodeGen/CD/cdbc-machine-control-flow.ll"),
                (
                    "runtime-error",
                    "llvm/test/CodeGen/CD/cdbc-array-assign-runtime.ll",
                    "array index out of range",
                ),
            ],
        )

    def test_reads_observability_manifest_contracts(self):
        manifest = """\
observability llvm/test/CodeGen/CD/cdbc-debug-ranges.ll "break-range ranges.cd:6-11;continue;quit" ranges
observability llvm/test/CodeGen/CD/cdbc-machine.ll "continue" metadata-free
observability llvm/test/CodeGen/CD/cdbc-debug-ranges.ll "break-range ranges.cd:6-11;step;next;quit" step-next
observability llvm/test/CodeGen/CD/cdbc-debug-ranges.ll "break ranges.cd:1;continue;delete 1;continue" line-delete
observability llvm/test/CodeGen/CD/cdbc-debug-ranges.ll "break-range ranges.cd:6-11;s;n;q" aliases
observability llvm/test/CodeGen/CD/cdbc-debug-ranges.ll "help;quit" help
debug-error llvm/test/CodeGen/CD/cdbc-debug-runtime.ll "continue;quit" "pause reason=error function=fail instruction=3 module=none location=runtime.cd:1:22 stack=main@runtime.cd:2:1>fail@runtime.cd:1:22"
state llvm/test/CodeGen/CD/cdbc-debug-contract.ll "break-range contract.cd:0-1;continue;continue;quit" "division by zero"
"""

        self.assertEqual(
            cd_bytecode_parity.parse_manifest(manifest.splitlines()),
            [
                (
                    "observability",
                    "llvm/test/CodeGen/CD/cdbc-debug-ranges.ll",
                    "break-range ranges.cd:6-11;continue;quit",
                    "ranges",
                ),
                (
                    "observability",
                    "llvm/test/CodeGen/CD/cdbc-machine.ll",
                    "continue",
                    "metadata-free",
                ),
                (
                    "observability",
                    "llvm/test/CodeGen/CD/cdbc-debug-ranges.ll",
                    "break-range ranges.cd:6-11;step;next;quit",
                    "step-next",
                ),
                (
                    "observability",
                    "llvm/test/CodeGen/CD/cdbc-debug-ranges.ll",
                    "break ranges.cd:1;continue;delete 1;continue",
                    "line-delete",
                ),
                (
                    "observability",
                    "llvm/test/CodeGen/CD/cdbc-debug-ranges.ll",
                    "break-range ranges.cd:6-11;s;n;q",
                    "aliases",
                ),
                (
                    "observability",
                    "llvm/test/CodeGen/CD/cdbc-debug-ranges.ll",
                    "help;quit",
                    "help",
                ),
                (
                    "debug-error",
                    "llvm/test/CodeGen/CD/cdbc-debug-runtime.ll",
                    "continue;quit",
                    "pause reason=error function=fail instruction=3 module=none location=runtime.cd:1:22 stack=main@runtime.cd:2:1>fail@runtime.cd:1:22",
                ),
                (
                    "state",
                    "llvm/test/CodeGen/CD/cdbc-debug-contract.ll",
                    "break-range contract.cd:0-1;continue;continue;quit",
                    "division by zero",
                ),
            ],
        )

    def test_rejects_invalid_observability_manifest_contracts(self):
        invalid_lines = (
            'observability input.ll "continue"',
            'observability input.ll "continue" invalid',
            'observability input.ll "continue" ranges extra',
            'debug-error input.ll "continue"',
            'debug-error input.ll "continue" ""',
            'debug-error input.ll "continue" "expected" extra',
            'state input.ll "continue"',
            'state input.ll "continue" ""',
            'state input.ll "" "division by zero"',
            'state input.ll "continue" "division by zero" extra',
        )

        for line in invalid_lines:
            with self.subTest(line=line):
                with self.assertRaisesRegex(ValueError, "manifest line 1: expected"):
                    cd_bytecode_parity.parse_manifest([line])

    def test_ranges_requires_dump_source_evidence(self):
        dump = """\
debug_ranges:
  main 3 = s0:6:11
"""

        def run_surface(command, description, input_text=None):
            return {
                "trace": "location=ranges.cd:1:7 range=s0:6:11\n",
                "profile": (
                    'profile source_range source=s0 path="ranges.cd" '
                    "start=6 end=11 hits=1\n"
                ),
                "debug": (
                    "pause reason=breakpoint location=ranges.cd:1:7 "
                    "range=s0:6:11\n"
                ),
            }[command[1]]

        with mock.patch.object(cd_bytecode_parity, "_run", side_effect=run_surface):
            with self.assertRaisesRegex(RuntimeError, "debug source"):
                cd_bytecode_parity._check_observability(
                    pathlib.Path("vm"),
                    "fixture.ll",
                    pathlib.Path("direct.cdbc"),
                    pathlib.Path("machine.cdbc"),
                    dump,
                    dump,
                    "run\n",
                    "run\n",
                    "continue",
                    "ranges",
                )

    def test_metadata_free_rejects_dump_source_ranges(self):
        def run_surface(command, description, input_text=None):
            return {
                "trace": "location=<unknown>\n",
                "profile": "profile status=ok\n",
                "debug": "location=<unknown>\n",
            }[command[1]]

        with mock.patch.object(cd_bytecode_parity, "_run", side_effect=run_surface):
            with self.assertRaisesRegex(RuntimeError, "source range"):
                cd_bytecode_parity._check_observability(
                    pathlib.Path("vm"),
                    "fixture.ll",
                    pathlib.Path("direct.cdbc"),
                    pathlib.Path("machine.cdbc"),
                    "cdbc 0.2\nrange=forbidden\n",
                    "cdbc 0.2\nsource_range forbidden\n",
                    "run\n",
                    "run\n",
                    "continue",
                    "metadata-free",
                )

    def test_metadata_free_rejects_run_source_ranges(self):
        def run_surface(command, description, input_text=None):
            return {
                "trace": "location=<unknown>\n",
                "profile": "profile status=ok\n",
                "debug": "location=<unknown>\n",
            }[command[1]]

        with mock.patch.object(cd_bytecode_parity, "_run", side_effect=run_surface):
            with self.assertRaisesRegex(RuntimeError, "source range"):
                cd_bytecode_parity._check_observability(
                    pathlib.Path("vm"),
                    "fixture.ll",
                    pathlib.Path("direct.cdbc"),
                    pathlib.Path("machine.cdbc"),
                    "cdbc 0.2\n",
                    "cdbc 0.2\n",
                    "run range=forbidden\n",
                    "run source_range forbidden\n",
                    "continue",
                    "metadata-free",
                )

    def test_step_next_requires_distinct_debug_pause_reasons(self):
        def run_surface(command, description, input_text=None):
            return {
                "trace": "trace status=ok\n",
                "profile": "profile status=ok\n",
                "debug": (
                    "pause reason=entry\n"
                    "debug resumed command=step\n"
                    "pause reason=step\n"
                    "debug resumed command=next\n"
                    "pause reason=next\n"
                    "debug quit\n"
                ),
            }[command[1]]

        with mock.patch.object(cd_bytecode_parity, "_run", side_effect=run_surface):
            cd_bytecode_parity._check_observability(
                pathlib.Path("vm"),
                "fixture.ll",
                pathlib.Path("direct.cdbc"),
                pathlib.Path("machine.cdbc"),
                "cdbc 0.2\n",
                "cdbc 0.2\n",
                "run\n",
                "run\n",
                "break-range ranges.cd:6-11;step;next;quit",
                "step-next",
            )

    def test_line_delete_requires_one_breakpoint_pause_and_program_output(self):
        def run_surface(command, description, input_text=None):
            return {
                "trace": "trace status=ok\n",
                "profile": "profile status=ok\n",
                "debug": (
                    "pause reason=entry\n"
                    "debug breakpoint id=1 spec=ranges.cd:1\n"
                    "debug resumed command=continue\n"
                    "pause reason=breakpoint\n"
                    "debug breakpoint-deleted id=1\n"
                    "debug resumed command=continue\n"
                    "2\n"
                ),
            }[command[1]]

        with mock.patch.object(cd_bytecode_parity, "_run", side_effect=run_surface):
            cd_bytecode_parity._check_observability(
                pathlib.Path("vm"),
                "fixture.ll",
                pathlib.Path("direct.cdbc"),
                pathlib.Path("machine.cdbc"),
                "cdbc 0.2\n",
                "cdbc 0.2\n",
                "run\n",
                "run\n",
                "break ranges.cd:1;continue;delete 1;continue",
                "line-delete",
            )

    def test_aliases_require_canonical_debugger_actions(self):
        def run_surface(command, description, input_text=None):
            return {
                "trace": "trace status=ok\n",
                "profile": "profile status=ok\n",
                "debug": (
                    "pause reason=entry\n"
                    "debug resumed command=step\n"
                    "pause reason=step\n"
                    "debug resumed command=next\n"
                    "pause reason=next\n"
                    "debug quit\n"
                ),
            }[command[1]]

        with mock.patch.object(cd_bytecode_parity, "_run", side_effect=run_surface):
            cd_bytecode_parity._check_observability(
                pathlib.Path("vm"),
                "fixture.ll",
                pathlib.Path("direct.cdbc"),
                pathlib.Path("machine.cdbc"),
                "cdbc 0.2\n",
                "cdbc 0.2\n",
                "run\n",
                "run\n",
                "break-range ranges.cd:6-11;s;n;q",
                "aliases",
            )

    def test_help_requires_debugger_command_reference_and_quit(self):
        def run_surface(command, description, input_text=None):
            return {
                "trace": "trace status=ok\n",
                "profile": "profile status=ok\n",
                "debug": (
                    "pause reason=entry\n"
                    "debug help: break <path>:<line> | break-range "
                    "<path>:<start>-<end> | continue | step | next | "
                    "delete <id> | quit\n"
                    "debug quit\n"
                ),
            }[command[1]]

        with mock.patch.object(cd_bytecode_parity, "_run", side_effect=run_surface):
            cd_bytecode_parity._check_observability(
                pathlib.Path("vm"),
                "fixture.ll",
                pathlib.Path("direct.cdbc"),
                pathlib.Path("machine.cdbc"),
                "cdbc 0.2\n",
                "cdbc 0.2\n",
                "run\n",
                "run\n",
                "help;quit",
                "help",
            )

    def test_debug_error_requires_matching_source_backed_error_pause(self):
        def run_surface(command, description, input_text=None):
            if command[1] == "dump":
                return "cdbc 0.2\n"
            if command[1] == "debug":
                return (
                    "pause reason=entry function=main instruction=0 "
                    "location=runtime.cd:2:1\n"
                    "debug resumed command=continue\n"
                    "pause reason=error function=fail instruction=3 module=none "
                    "location=runtime.cd:1:22 "
                    "stack=main@runtime.cd:2:1>fail@runtime.cd:1:22\n"
                    "debug quit\n"
                )
            return ""

        with mock.patch.object(cd_bytecode_parity, "_run", side_effect=run_surface):
            cd_bytecode_parity._check_case(
                pathlib.Path("llc"),
                pathlib.Path("vm"),
                pathlib.Path("."),
                "debug-error",
                "llvm/utils/cd_bytecode_parity.py",
                debug_commands="continue;quit",
                debug_error_expected=(
                    "pause reason=error function=fail instruction=3 module=none "
                    "location=runtime.cd:1:22 "
                    "stack=main@runtime.cd:2:1>fail@runtime.cd:1:22"
                ),
            )

    def test_state_contract_allows_only_synthetic_entry_difference(self):
        direct_debug = (
            "pause reason=entry function=main instruction=0 module=none "
            "location=contract.cd:2:1 stack=main@contract.cd:2:1 locals={}\n"
            "debug breakpoint id=1 spec=contract.cd:0-1\n"
            "debug resumed command=continue\n"
            "pause reason=breakpoint function=identity instruction=3 module=none "
            "location=contract.cd:1:29 "
            "stack=main@contract.cd:2:1>identity@contract.cd:1:29 "
            "locals={input=\"2\"} range=s0:0:1\n"
            "debug resumed command=continue\n"
            "pause reason=error function=identity instruction=5 module=none "
            "location=contract.cd:1:42 "
            "stack=main@contract.cd:2:1>identity@contract.cd:1:42 "
            "locals={input=\"2\"}\n"
            "debug quit\n"
        )
        machine_debug = direct_debug.replace(
            "location=contract.cd:2:1 stack=main@contract.cd:2:1",
            "location=<unknown> stack=main@<unknown>",
            1,
        )
        direct_dump = "debug_locations:\n  main 0 = s0:2:1\n  main 2 = s0:2:1\n"
        machine_dump = "debug_locations:\n  main 2 = s0:2:1\n"

        def run_surface(command, description, input_text=None):
            return direct_debug if "direct.cdbc" in command[2] else machine_debug

        with mock.patch.object(cd_bytecode_parity, "_run", side_effect=run_surface):
            cd_bytecode_parity._check_state(
                pathlib.Path("vm"),
                "llvm/test/CodeGen/CD/cdbc-debug-contract.ll",
                pathlib.Path("/tmp/direct.cdbc"),
                pathlib.Path("/tmp/machine.cdbc"),
                direct_dump,
                machine_dump,
                "break-range contract.cd:0-1;continue;continue;quit",
            )


if __name__ == "__main__":
    unittest.main()
