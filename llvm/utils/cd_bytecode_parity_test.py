#!/usr/bin/env python3
"""Unit tests for the CD direct/machine parity harness."""

import pathlib
import sys
import unittest


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
"""

        self.assertEqual(
            cd_bytecode_parity.parse_manifest(manifest.splitlines()),
            [
                ("artifact", "llvm/test/CodeGen/CD/cdbc-machine.ll"),
                ("behavior", "llvm/test/CodeGen/CD/cdbc-machine-control-flow.ll"),
            ],
        )


if __name__ == "__main__":
    unittest.main()
