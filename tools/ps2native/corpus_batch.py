"""Inventory ISO candidates, build a corpus, or merge captured EE misses offline.

These are experimental conversion stages, not a gameplay approval or a complete
headless/relink/replay loop. Run from the repository root with
``python3 -m tools.ps2native.corpus_batch --help``.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
import time
from typing import Any

from .native_recovery import ordinary
from .pipeline import (PipelineError, _ensure_output_outside_sources, _run,
                       _write_json, repo_root, run_build, sha256_file,
                       source_tree_snapshot, verify_package)


def stable_digest(path: Path) -> tuple[str, int]:
    before = path.stat()
    if not path.is_file() or before.st_size < 1:
        raise PipelineError(f"ISO candidate must be a nonempty regular file: {path}")
    digest = sha256_file(path)
    after = path.stat()
    identity = lambda stat: (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns)
    if identity(before) != identity(after):
        raise PipelineError(f"ISO changed while calculating its identity: {path}")
    return digest, after.st_size


def inventory(directory: Path) -> dict[str, Any]:
    directory = ordinary(directory)
    if not directory.is_dir():
        raise PipelineError(f"ISO directory is unavailable: {directory}")
    rows = []
    by_hash = {}
    entries = 0
    duplicates = 0
    for path in sorted(directory.iterdir(), key=lambda item: item.name):
        entries += 1
        if entries > 4096:
            raise PipelineError("ISO directory inventory exceeds 4096 entries")
        if path.suffix.lower() != ".iso":
            continue
        path = ordinary(path)
        digest, size = stable_digest(path)
        if digest in by_hash:
            by_hash[digest]["duplicate_paths"].append(str(path))
            duplicates += 1
            continue
        if len(rows) >= 128:
            raise PipelineError("one corpus inventory supports at most 128 unique ISOs")
        row = {"iso_path": str(path), "iso_sha256": digest, "iso_bytes": size,
               "duplicate_paths": [], "image_kind": "ISO_CANDIDATE"}
        by_hash[digest] = row
        rows.append(row)
    if not rows:
        raise PipelineError(f"directory contains no ISO candidates: {directory}")
    return {"schema_version": 1, "status": "INVENTORIED", "iso_directory": str(directory),
            "unique_images": len(rows), "duplicate_images": duplicates, "images": rows,
            "strict_approval": False, "closure_proved": False, "gameplay_approved": False,
            "scope": "content inventory only; PS2 format is validated by the build inspector"}


def fresh_output(path: Path) -> Path:
    out = ordinary(path)
    _ensure_output_outside_sources(out, repo_root())
    if out.exists():
        raise PipelineError(f"output must be a new directory: {out}")
    return out


def validate_budget(workers: int, timeout: int) -> None:
    if type(workers) is not int or not 1 <= workers <= 16:
        raise PipelineError("worker budget must be within 1..16")
    if type(timeout) is not int or not 1 <= timeout <= 86400:
        raise PipelineError("per-tool timeout must be within 1..86400 seconds")


def build_corpus(args: argparse.Namespace, *, builder=None) -> dict[str, Any]:
    validate_budget(args.build_jobs, args.timeout)
    out = fresh_output(args.out)
    report = inventory(args.iso_dir)
    source = source_tree_snapshot(repo_root())
    report.update(status="BUILDING", source_snapshot=source,
                  scope="sequential ISO builds with integrity receipts; gameplay and AOT unqualified")
    for row in report["images"]:
        row["status"] = "QUEUED"
    out.mkdir(parents=True)
    receipt = out / "corpus.json"
    _write_json(receipt, report)
    selected_builder = builder if builder is not None else run_build
    started = time.monotonic()
    try:
        for row in report["images"]:
            # Do not mix packages made from different live source revisions.
            if source_tree_snapshot(repo_root())["sha256"] != source["sha256"]:
                raise PipelineError("source tree changed during the corpus build; create a new job")
            job = out / "images" / row["iso_sha256"]
            row.update(status="BUILDING", job=str(job))
            _write_json(receipt, report)
            item_started = time.monotonic()
            try:
                iso = ordinary(row["iso_path"])
                if stable_digest(iso) != (row["iso_sha256"], row["iso_bytes"]):
                    raise PipelineError("ISO changed since the corpus inventory")
                job_args = argparse.Namespace(
                    iso=iso, target="desktop", out=job / "package", work_root=job / "work",
                    inspector=args.inspector, analyzer=args.analyzer,
                    recompiler=args.recompiler, cmake=args.cmake, gradle=None,
                    build_jobs=args.build_jobs, timeout=args.timeout)
                result = selected_builder(job_args)
                if result.get("status") != "complete" or not result.get("artifact"):
                    raise PipelineError(result.get("message", "build did not produce a package"))
                package = ordinary(result["artifact"])
                if package != job_args.out:
                    raise PipelineError("build returned a package outside its assigned job")
                integrity = verify_package(package)
                if integrity["status"] != "verified":
                    raise PipelineError("built package failed its file-integrity check")
                if stable_digest(iso) != (row["iso_sha256"], row["iso_bytes"]):
                    raise PipelineError("ISO changed during the build")
                if source_tree_snapshot(repo_root())["sha256"] != source["sha256"]:
                    raise PipelineError("source tree changed during this build")
                row.update(status="BUILT_UNVERIFIED", artifact=str(package),
                           package_manifest_sha256=sha256_file(package / "manifest.json"),
                           integrity=integrity, gameplay_approved=False,
                           installed_in_aot_runner=False)
            except (PipelineError, OSError, ValueError) as error:
                row.update(status="FAILED", error_type=type(error).__name__, error=str(error))
            finally:
                row["seconds"] = time.monotonic() - item_started
                _write_json(receipt, report)
        report["status"] = ("BUILT_UNVERIFIED" if all(
            row["status"] == "BUILT_UNVERIFIED" for row in report["images"]) else "PARTIAL_FAILURE")
    except BaseException:
        report["status"] = "INTERRUPTED_OR_FAILED"
        raise
    finally:
        report["seconds"] = time.monotonic() - started
        _write_json(receipt, report)
    return report


def capture_selection(roots: list[Path]) -> list[Path]:
    """Select the existing CLI's --capture inputs without copying guest RAM."""
    selected = []
    visited = set()
    for root in roots:
        root = ordinary(root)
        if not root.is_dir():
            raise PipelineError(f"capture root is unavailable: {root}")
        if root in visited:
            continue
        visited.add(root)
        entries = 0
        found = False
        for path in sorted(root.iterdir(), key=lambda item: item.name):
            entries += 1
            if entries > 1024:
                raise PipelineError("capture directory inventory exceeds 1024 entries")
            if not path.name.startswith("ee-miss-"):
                continue
            if not re.fullmatch(r"ee-miss-[0-9]{6}", path.name):
                raise PipelineError(f"invalid EE capture name: {path.name}")
            path = ordinary(path)
            if not path.is_dir() or any(not ordinary(path / name).is_file()
                                      for name in ("request.json", "snapshot.bin", "ee-ram.bin")):
                raise PipelineError(f"incomplete EE capture: {path}")
            found = True
            selected.append(path)
            if len(selected) > 16:
                raise PipelineError("at most 16 new captures per merge; merge bounded groups sequentially")
        if not found:
            raise PipelineError(f"capture root contains no EE misses: {root}")
    if not selected:
        raise PipelineError("at least one EE capture root is required")
    return selected


def merge_captures(args: argparse.Namespace) -> dict[str, Any]:
    validate_budget(args.workers, args.timeout)
    out = fresh_output(args.out)
    captures = capture_selection(args.capture_root)
    input_roots = [ordinary(root) for root in args.capture_root]
    for path in (args.previous_batch, args.previous_family_catalog):
        if path is not None:
            input_roots.append(ordinary(path))
    if any(out.is_relative_to(path) or path.is_relative_to(out) for path in input_roots):
        raise PipelineError("merge output overlaps an input")
    generator = ordinary(args.family_generator)
    overlay = ordinary(args.overlay_generator) if args.overlay_generator else None
    if not generator.is_file() or (overlay is not None and not overlay.is_file()):
        raise PipelineError("offline generator is unavailable")
    command = [sys.executable, str(repo_root() / "lab/prepare_ee_family_batch.py"),
               "--family-generator", str(generator), "--output", str(out / "batch"),
               "--workers", str(args.workers)]
    for capture in captures:
        command += ["--capture", str(capture)]
    if args.previous_batch:
        command += ["--previous-batch", str(ordinary(args.previous_batch))]
    if args.previous_family_catalog:
        command += ["--previous-family-catalog", str(ordinary(args.previous_family_catalog))]
    if overlay:
        command += ["--overlay-generator", str(overlay)]
    report = {"schema_version": 1, "status": "GENERATING", "command": command,
              "capture_paths": [str(path) for path in captures], "strict_approval": False,
              "closure_proved": False, "gameplay_approved": False, "compiled": False,
              "installed_in_runner": False,
              "scope": "offline EE corpus union; no compilation, runner installation or replay"}
    out.mkdir(parents=True)
    started = time.monotonic()
    try:
        _write_json(out / "merge.json", report)
        _run(command, out / "prepare.log", repo_root(), args.timeout)
        batch_report = out / "batch/report.json"
        manifest = out / "batch/catalog/catalog.json"
        if not batch_report.is_file() or batch_report.stat().st_size > 8 * 1024 * 1024:
            raise PipelineError("merge did not publish a bounded batch receipt")
        publication = json.loads(batch_report.read_text())
        if not isinstance(publication, dict) or publication.get("status") != "PUBLISHED_LABORATORY" or \
                publication.get("strict_approval") is not False or \
                publication.get("closure_proved") is not False or \
                publication.get("manifest_published") is not True or not manifest.is_file() or \
                publication.get("family_catalog_sha256") != sha256_file(manifest) or \
                any(type(publication.get(key)) is not int or publication[key] < 1
                    for key in ("family_count", "owned_cases")):
            raise PipelineError("merge did not publish an identified laboratory catalog")
        report.update(status="GENERATED_LABORATORY", family_manifest=str(manifest),
                      family_manifest_sha256=sha256_file(manifest))
        for key in ("family_count", "owned_cases", "duplicate_cases", "candidate_count",
                    "declined_structures", "reused_body_sources"):
            report[key] = publication.get(key)
        return report
    except BaseException as error:
        report.update(status="FAILED", error_type=type(error).__name__, error=str(error))
        raise
    finally:
        report["seconds"] = time.monotonic() - started
        _write_json(out / "merge.json", report)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    plan = commands.add_parser("plan", help="hash/deduplicate .iso candidates without building")
    plan.add_argument("--iso-dir", type=Path, required=True)
    plan.add_argument("--out", type=Path, help="new JSON receipt file (optional; default stdout)")
    build = commands.add_parser("build", help="build ISO packages sequentially; gameplay remains unverified")
    build.add_argument("--iso-dir", type=Path, required=True)
    build.add_argument("--out", type=Path, required=True, help="new corpus job directory")
    build.add_argument("--build-jobs", type=int, default=4)
    for name in ("inspector", "analyzer", "recompiler", "cmake"):
        build.add_argument("--" + name)
    merge = commands.add_parser("merge", help="merge up to 16 new EE captures into a generated lab catalog")
    merge.add_argument("--capture-root", type=Path, action="append", required=True)
    merge.add_argument("--previous-batch", type=Path)
    merge.add_argument("--previous-family-catalog", type=Path)
    merge.add_argument("--family-generator", type=Path, required=True)
    merge.add_argument("--overlay-generator", type=Path)
    merge.add_argument("--out", type=Path, required=True)
    merge.add_argument("--workers", type=int, default=4)
    for command in (build, merge):
        command.add_argument("--timeout", type=int, default=3600)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        if args.command == "plan":
            report = inventory(args.iso_dir)
            if args.out:
                out = ordinary(args.out)
                _ensure_output_outside_sources(out, repo_root())
                out.parent.mkdir(parents=True, exist_ok=True)
                with out.open("x", encoding="utf-8") as stream:
                    stream.write(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
        elif args.command == "build":
            report = build_corpus(args)
        else:
            report = merge_captures(args)
        print(json.dumps(report, indent=2, ensure_ascii=False))
        return 2 if report["status"] == "PARTIAL_FAILURE" else 0
    except (PipelineError, OSError, ValueError) as error:
        print(f"corpus-batch: error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
