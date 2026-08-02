#!/usr/bin/env python3
"""Unit tests for the CD direct/machine parity harness."""

import pathlib
import sys
import unittest
from unittest import mock


sys.path.insert(0, str(pathlib.Path(__file__).parent))
import cd_bytecode_parity  # noqa: E402


class CDBytecodeParityTest(unittest.TestCase):
    def test_normalizes_permitted_table_and_register_indices(self):
        direct = """\
cdbc 0.1

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
cdbc 0.1

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
cdbc 0.1

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
runtime-error llvm/test/CodeGen/CD/cdbc-array-access-runtime.ll "for-in expects array, range, or map"
"""

        self.assertEqual(
            cd_bytecode_parity.parse_manifest(manifest.splitlines()),
            [
                ("artifact", "llvm/test/CodeGen/CD/cdbc-machine.ll"),
                ("behavior", "llvm/test/CodeGen/CD/cdbc-machine-control-flow.ll"),
                (
                    "runtime-error",
                    "llvm/test/CodeGen/CD/cdbc-array-access-runtime.ll",
                    "for-in expects array, range, or map",
                ),
            ],
        )

    def test_reads_observability_manifest_contracts(self):
        manifest = """\
observability llvm/test/CodeGen/CD/cdbc-debug-ranges.ll "break-range ranges.cd:6-11;continue;quit" ranges
observability llvm/test/CodeGen/CD/cdbc-machine.ll "continue" metadata-free
observability llvm/test/CodeGen/CD/cdbc-debug-ranges.ll "break-range ranges.cd:6-11;step;next;quit" step-next
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
            ],
        )

    def test_rejects_invalid_observability_manifest_contracts(self):
        invalid_lines = (
            'observability input.ll "continue"',
            'observability input.ll "continue" invalid',
            'observability input.ll "continue" ranges extra',
        )

        for line in invalid_lines:
            with self.subTest(line=line):
                with self.assertRaisesRegex(ValueError, "manifest line 1: expected"):
                    cd_bytecode_parity.parse_manifest([line])

    def test_ranges_requires_dump_source_evidence(self):
        dump = """\
debug_ranges:
  main 2 = s0:6:11
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
                    "cdbc 0.1\nrange=forbidden\n",
                    "cdbc 0.1\nsource_range forbidden\n",
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
                    "cdbc 0.1\n",
                    "cdbc 0.1\n",
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
                "cdbc 0.1\n",
                "cdbc 0.1\n",
                "run\n",
                "run\n",
                "break-range ranges.cd:6-11;step;next;quit",
                "step-next",
            )


if __name__ == "__main__":
    unittest.main()
