import unittest
from pathlib import Path

from tools.ps2native.offline_relink import RelinkError, rewrite_link_argv


class LinkCommandRewriteTests(unittest.TestCase):
    def test_rewrites_only_runtime_archive_runner_and_dependency_file(self):
        base_archive = Path("/lab/base/libps2_runtime.a")
        output_archive = Path("/lab/job/libps2_runtime.a")
        output_runner = Path("/lab/job/ps2EntryRunner")
        dependency_file = Path("/lab/job/link.d")
        command = [
            "/usr/bin/c++",
            "-Wl,--dependency-file=/lab/base/link.d",
            "@CMakeFiles/runner-objects.rsp",
            "-o",
            str(Path("/lab/base/ps2EntryRunner")),
            str(base_archive),
            "-ldl",
        ]

        rewritten = rewrite_link_argv(
            command, base_archive, output_archive, output_runner, dependency_file
        )

        self.assertEqual(
            rewritten,
            [
                "/usr/bin/c++",
                "-Wl,--dependency-file=/lab/job/link.d",
                "@CMakeFiles/runner-objects.rsp",
                "-o",
                str(output_runner),
                str(output_archive),
                "-ldl",
            ],
        )

    def test_rejects_ambiguous_runtime_archive_argument(self):
        archive = Path("/lab/base/libps2_runtime.a")
        with self.assertRaisesRegex(RelinkError, "exactly one"):
            rewrite_link_argv(
                ["c++", "-o", "runner", str(archive), str(archive)],
                archive,
                Path("/lab/job/libps2_runtime.a"),
                Path("/lab/job/ps2EntryRunner"),
                Path("/lab/job/link.d"),
            )


if __name__ == "__main__":
    unittest.main()
