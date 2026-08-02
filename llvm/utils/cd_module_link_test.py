#!/usr/bin/env python3
"""Unit tests for the opt-in CD module-link harness."""

import pathlib
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
                'target="library" kind=import at=0 requested="./library.cd"',
            ),
            """\
  dependencies:
    d0 target=\"library\" kind=import at=0 requested=\"./library.cd\"

constants:
""",
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


if __name__ == "__main__":
    unittest.main()
