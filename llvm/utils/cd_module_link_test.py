#!/usr/bin/env python3
"""Unit tests for the opt-in CD module-link harness."""

import pathlib
import subprocess
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).parent))
import cd_module_link  # noqa: E402


class CDModuleLinkTest(unittest.TestCase):
    def test_replaces_one_module_field(self):
        artifact = """\
module:
  identity = \"entry\"
  path = \"entry.cd\"
  canonical_path = \"/workspace/entry.cd\"
  entry = true
  entry_order = 0
  dependencies:
"""

        self.assertEqual(
            cd_module_link.replace_module_field(artifact, "entry_order", "2"),
            artifact.replace("  entry_order = 0\n", "  entry_order = 2\n"),
        )

    def test_appends_dependency_before_constants(self):
        artifact = """\
  dependencies:

constants:
"""

        self.assertEqual(
            cd_module_link.add_dependency_record(
                artifact,
                'target="library" kind=import requested="./library.cd"',
            ),
            """\
  dependencies:
    d0 target=\"library\" kind=import requested=\"./library.cd\"

constants:
""",
        )

    def test_adds_module_init_marker_to_initializer(self):
        artifact = """\
cdbc 0.2

main registers=0:
block b0:
  return_nil

function f0 name="__module_init" arity=0 registers=0:
block b0:
  return_nil
"""

        self.assertIn(
            'block b0:\n  init_module m0\n  return_nil\n',
            cd_module_link.add_module_init_marker(artifact, 0),
        )

    def test_builds_vm_command_for_cargo_manifest(self):
        self.assertEqual(
            cd_module_link.vm_command(
                pathlib.Path("/workspace/cd-compiler/vm-rs/Cargo.toml"),
                ["link", "modules", "linked.cdbc"],
            ),
            [
                "cargo",
                "run",
                "--quiet",
                "--manifest-path",
                "/workspace/cd-compiler/vm-rs/Cargo.toml",
                "--",
                "link",
                "modules",
                "linked.cdbc",
            ],
        )

    def test_builds_machine_backend_arguments(self):
        self.assertEqual(
            cd_module_link.llc_arguments(
                pathlib.Path("llc"),
                pathlib.Path("input.ll"),
                pathlib.Path("output.cdbc"),
                "machine",
            ),
            [
                "llc",
                "-mtriple=cd-unknown-unknown",
                "-cd-backend=machine",
                "-cd-artifact=module",
                "input.ll",
                "-o",
                "output.cdbc",
            ],
        )

    def test_validates_expected_failure_output(self):
        result = subprocess.CompletedProcess(
            ["vm", "run", "linked.cdbc"],
            1,
            stdout="1\n",
            stderr="Runtime error at dependency.cd:1:1: division by zero",
        )

        self.assertEqual(
            cd_module_link.validate_expected_failure(
                result,
                "linked runtime error",
                "Runtime error at dependency.cd:1:1: division by zero",
                expected_stdout="1\n",
            ),
            result.stderr,
        )


if __name__ == "__main__":
    unittest.main()
