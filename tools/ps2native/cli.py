from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import __version__
from .pipeline import PipelineError, inspect_image, run_build, verify_package
from .native_recovery import run_recovery
from .conversion import run_convert


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ps2native",
        description="Inspect PS2 ISO images and build experimental static-recompiler packages.",
    )
    parser.add_argument("--version", action="version", version=f"ps2native {__version__}")
    subparsers = parser.add_subparsers(dest="command", required=True)

    inspect_parser = subparsers.add_parser("inspect", help="inspect an ISO and report its boot ELF")
    inspect_parser.add_argument("--iso", required=True, type=Path, help="PS2 ISO image")
    inspect_parser.add_argument("--inspector", help="path to ps2iso-inspect")
    inspect_parser.add_argument("--json-output", action="store_true", help="print the raw schema-v1 JSON report")

    build_parser = subparsers.add_parser("build", help="extract, analyze, recompile, and package an ISO")
    build_parser.add_argument("--iso", required=True, type=Path, help="PS2 ISO image")
    convert_parser = subparsers.add_parser("convert", help="build, verify, and open an experimental desktop package")
    convert_parser.add_argument("iso", type=Path, help="PS2 ISO image")
    launch_options = convert_parser.add_mutually_exclusive_group()
    launch_options.add_argument("--run", dest="run", action="store_true",
                                help="open the desktop package after conversion (desktop default)")
    launch_options.add_argument("--no-run", dest="run", action="store_false",
                                help="convert and verify without opening a window")
    convert_parser.set_defaults(run=None)
    convert_parser.add_argument("--run-timeout", type=int,
                                help="optional execution budget in seconds; expiration is a failure")
    convert_parser.add_argument("--no-adapt", dest="adapt", action="store_false",
                                help="skip offline recovery probing and produce an unverified build")
    convert_parser.set_defaults(adapt=True, build_jobs=4)
    convert_parser.add_argument("--adapt-rounds", type=int, default=8,
                                help="maximum offline EE recovery rounds (1..64, default: 8)")
    convert_parser.add_argument("--probe-timeout", type=int, default=30,
                                help="seconds per automatic execution probe (1..3600, default: 30)")
    convert_parser.add_argument("--memory-root", type=Path,
                                help="private local adaptation memory; defaults to build/ps2native-memory")
    convert_parser.add_argument("--overlay-generator", help="path to the offline ps2_native_overlay generator")
    for command_parser in (build_parser, convert_parser):
        command_parser.add_argument("--target", choices=("desktop", "android"), default="desktop")
        command_parser.add_argument("--out", type=Path, help="new package destination (must not already exist)")
        command_parser.add_argument("--work-root", type=Path, help="isolated workspace root; defaults to <repo>/build/ps2native")
        command_parser.add_argument("--inspector", help="path to ps2iso-inspect")
        command_parser.add_argument("--analyzer", help="path to ps2_analyzer")
        command_parser.add_argument("--recompiler", help="path to ps2_recomp")
        command_parser.add_argument("--cmake", help="path to cmake")
        command_parser.add_argument("--gradle", help="path to Gradle (Android target)")
        command_parser.add_argument("--build-jobs", type=int, help="parallel desktop build jobs")
        command_parser.add_argument("--timeout", type=int, default=3600, help="per-tool timeout in seconds (default: 3600)")

    verify_parser = subparsers.add_parser("verify", help="verify a built package against its file hashes")
    verify_parser.add_argument("--package", required=True, type=Path, help="package directory to verify")

    recovery = subparsers.add_parser("recover-ee", help="recover captured EE code and build an incremental catalog")
    recovery.add_argument("--capture-root", type=Path, required=True)
    recovery.add_argument("--previous-batch", type=Path, required=True)
    recovery.add_argument("--previous-family-catalog", type=Path)
    recovery.add_argument("--family-generator")
    recovery.add_argument("--overlay-generator")
    recovery.add_argument("--out", type=Path, required=True)
    recovery.add_argument("--native-build", type=Path, help="existing CMake build for the native family catalog")
    recovery.add_argument("--cmake")
    recovery.add_argument("--workers", type=int, default=4)
    recovery.add_argument("--timeout", type=int, default=3600)

    return parser


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    return create_parser().parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.command == "recover-ee":
            result = run_recovery(args)
            print(f"EE catalog: {result['status']}")
            print(f"Families:   {result['family_count']}")
            print(f"Manifest:   {result['family_manifest']}")
            return 0
        if args.command == "inspect":
            raw, report = inspect_image(args.iso, args.inspector)
            if args.json_output:
                print(json.dumps(raw, indent=2, sort_keys=True))
            else:
                print(f"ISO:       {report.image_path}")
                print(f"Volume ID: {raw['image'].get('volume_id') or '(unknown)'}")
                print(f"Filesystem:{' ' if raw['image'].get('filesystem') else ''}{raw['image'].get('filesystem') or '(unknown)'}")
                print(f"Boot ELF:  {report.boot_elf_path}")
                print(f"ELF SHA256:{' '}{report.boot_elf_sha256}")
                print(f"Image SHA256: {report.image_sha256}")
            return 0

        if args.command == "verify":
            report = verify_package(args.package)
            print(f"Package:   {args.package.expanduser().resolve()}")
            print(f"Integrity: {report['status']} ({report['checked_files']} files checked)")
            for category in ("changed", "missing", "unexpected"):
                for path in report[category]:
                    print(f"{category}: {path}")
            return 0 if report["status"] == "verified" else 2

        result = run_convert(args) if args.command == "convert" else run_build(args)
        print(f"Status:    {result['status']}")
        print(f"Workspace: {result['workspace']}")
        if result.get("artifact"):
            print(f"Package:   {result['artifact']}")
        if result.get("run_status"):
            print(f"Run:       {result['run_status']}")
        if result.get("adaptation_status"):
            print(f"Adaptation:{' '}{result['adaptation_status']} (menu unverified)")
        if result.get("gameplay_compatibility"):
            print(f"Gameplay:  {result['gameplay_compatibility']} ({result.get('support_tier', 'experimental')})")
        if result.get("native_translation_status"):
            print(f"Code:      {result['native_translation_status']}")
        if result.get("address_coverage_status"):
            print(f"Ranges:    {result['address_coverage_status']}")
        if result.get("message"):
            print(result["message"], file=sys.stderr if result["status"] != "complete" else sys.stdout)
        return 0 if result["status"] == "complete" else 2
    except (PipelineError, OSError, ValueError) as exc:
        print(f"ps2native: error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
