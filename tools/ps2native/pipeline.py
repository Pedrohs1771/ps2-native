from __future__ import annotations

import hashlib
import json
import csv
import os
import re
import shutil
import subprocess
import sys
import uuid
import ctypes
import errno
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any, Sequence


class PipelineError(RuntimeError):
    """An actionable pipeline/tool failure."""


class AndroidPrerequisitesMissing(PipelineError):
    def __init__(self, reasons: list[str]):
        self.reasons = reasons
        super().__init__("; ".join(reasons))


@dataclass(frozen=True)
class InspectorReport:
    image_path: str
    image_sha256: str
    boot_elf_path: str
    boot_elf_sha256: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


_SOURCE_SNAPSHOT_ROOTS = (
    ".gitignore", "CMakeLists.txt", "cmake", "android", "ps2xAnalyzer", "ps2xIOP",
    "ps2xRecomp", "ps2xRuntime", "ps2xStudio", "ps2xTest", "tools",
)
_SOURCE_SNAPSHOT_EXCLUDED_DIRS = {".git", "__pycache__", "build", "out", ".gradle"}


def source_tree_snapshot(root: Path) -> dict[str, Any]:
    """Return a compact content fingerprint for build-relevant source/config files."""
    repo = root.resolve()
    files: list[Path] = []
    for relative in _SOURCE_SNAPSHOT_ROOTS:
        candidate = repo / relative
        if candidate.is_file() or candidate.is_symlink():
            files.append(candidate)
        elif candidate.is_dir():
            for path in candidate.rglob("*"):
                if any(part in _SOURCE_SNAPSHOT_EXCLUDED_DIRS for part in path.relative_to(repo).parts):
                    continue
                if path.is_file() or path.is_symlink():
                    files.append(path)

    digest = hashlib.sha256()
    total_bytes = 0
    for path in sorted(set(files), key=lambda item: item.relative_to(repo).as_posix()):
        relative = path.relative_to(repo).as_posix()
        if path.is_symlink():
            payload = os.readlink(path).encode("utf-8", errors="surrogateescape")
            file_kind = b"symlink"
        else:
            payload = None
            file_kind = b"file"
        file_digest = hashlib.sha256(payload).hexdigest() if payload is not None else sha256_file(path)
        size_bytes = len(payload) if payload is not None else path.stat().st_size
        total_bytes += size_bytes
        digest.update(file_kind + b"\0" + relative.encode("utf-8", errors="surrogateescape") + b"\0")
        digest.update(str(size_bytes).encode("ascii") + b"\0" + file_digest.encode("ascii") + b"\n")

    revision = None
    try:
        completed = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=repo, check=False,
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=5,
        )
        if completed.returncode == 0:
            revision = completed.stdout.strip() or None
    except (OSError, subprocess.TimeoutExpired):
        pass
    return {
        "schema_version": 1,
        "roots": list(_SOURCE_SNAPSHOT_ROOTS),
        "file_count": len(files),
        "total_bytes": total_bytes,
        "sha256": digest.hexdigest(),
        "git_revision": revision,
    }


def attest_package_files(package_root: Path) -> list[dict[str, Any]]:
    """Hash every regular package file except the self-referential manifest."""
    root = package_root.expanduser().resolve()
    if not root.is_dir():
        raise PipelineError(f"package directory does not exist: {root}")
    records: list[dict[str, Any]] = []
    for path in sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix()):
        relative = path.relative_to(root)
        if path.is_symlink():
            raise PipelineError(f"package contains an unsupported symlink: {relative.as_posix()}")
        if not path.is_file() or relative.as_posix() == "manifest.json":
            continue
        records.append({
            "path": relative.as_posix(),
            "size_bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        })
    return records


def verify_package(package_root: Path) -> dict[str, Any]:
    """Compare a package's current files with its stored build-time inventory."""
    root = package_root.expanduser().resolve()
    manifest_path = root / "manifest.json"
    if not manifest_path.is_file():
        raise PipelineError(f"package manifest does not exist: {manifest_path}")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise PipelineError(f"cannot read package manifest {manifest_path}: {exc}") from exc
    expected = manifest.get("artifact_files") if isinstance(manifest, dict) else None
    if not isinstance(manifest, dict) or manifest.get("artifact_files_schema_version") != 1:
        raise PipelineError("unsupported artifact_files_schema_version; expected 1")
    if not isinstance(expected, list) or not expected:
        raise PipelineError("package manifest has no artifact_files inventory; rebuild with this pipeline version")

    expected_by_path: dict[str, dict[str, Any]] = {}
    for record in expected:
        if not isinstance(record, dict) or not isinstance(record.get("path"), str):
            raise PipelineError("package manifest contains an invalid artifact_files record")
        raw_path = record["path"]
        relative = PurePosixPath(raw_path)
        if (relative.is_absolute() or ".." in relative.parts or "\\" in raw_path
                or raw_path in ("", ".", "manifest.json") or relative.as_posix() != raw_path):
            raise PipelineError(f"package manifest contains an unsafe artifact path: {raw_path!r}")
        if (not isinstance(record.get("size_bytes"), int) or record["size_bytes"] < 0
                or not _is_sha256(record.get("sha256"))):
            raise PipelineError(f"package manifest contains an invalid hash/size for {raw_path!r}")
        if raw_path in expected_by_path:
            raise PipelineError(f"package manifest repeats an artifact path: {raw_path!r}")
        expected_by_path[raw_path] = record

    mutable_outputs = manifest.get("mutable_outputs", [])
    if not isinstance(mutable_outputs, list):
        raise PipelineError("package manifest mutable_outputs must be a list")
    mutable_paths: set[str] = set()
    for raw_path in mutable_outputs:
        if not isinstance(raw_path, str):
            raise PipelineError("package manifest contains an invalid mutable output path")
        relative = PurePosixPath(raw_path)
        if (relative.is_absolute() or ".." in relative.parts or "\\" in raw_path
                or raw_path in ("", ".", "manifest.json") or relative.as_posix() != raw_path):
            raise PipelineError(f"package manifest contains an unsafe mutable output path: {raw_path!r}")
        mutable_paths.add(raw_path)

    actual_records = attest_package_files(root)
    actual_by_path = {record["path"]: record for record in actual_records}
    expected_paths, actual_paths = set(expected_by_path), set(actual_by_path)
    missing = sorted(expected_paths - actual_paths)
    unexpected = sorted(actual_paths - expected_paths - mutable_paths)
    changed = sorted(
        path for path in expected_paths & actual_paths
        if expected_by_path[path].get("size_bytes") != actual_by_path[path]["size_bytes"]
        or expected_by_path[path].get("sha256") != actual_by_path[path]["sha256"]
    )
    return {
        "status": "verified" if not (missing or unexpected or changed) else "mismatch",
        "checked_files": len(expected_paths & actual_paths),
        "missing": missing,
        "unexpected": unexpected,
        "changed": changed,
    }


def _is_sha256(value: Any) -> bool:
    return isinstance(value, str) and re.fullmatch(r"[0-9a-fA-F]{64}", value) is not None


def validate_inspector_report(raw: Any) -> InspectorReport:
    if not isinstance(raw, dict) or raw.get("schema_version") != 1:
        version = raw.get("schema_version") if isinstance(raw, dict) else None
        raise ValueError(f"unsupported ISO inspector schema_version {version!r}; expected 1")
    image = raw.get("image")
    boot = raw.get("boot")
    elf = boot.get("elf") if isinstance(boot, dict) else None
    if not isinstance(image, dict):
        raise ValueError("inspector report is missing image metadata")
    if not isinstance(elf, dict):
        raise ValueError("inspector report is missing boot.elf metadata; this ISO has no recognized boot ELF")
    if not isinstance(elf.get("machine"), str) or not isinstance(elf.get("byte_order"), str):
        raise ValueError("inspector boot.elf machine and byte_order fields must be strings")
    if elf.get("machine_id") != 8:
        raise ValueError(f"boot ELF is not identified as MIPS (machine_id=8): {elf.get('machine_id')!r}")
    image_hash = image.get("sha256")
    elf_hash = elf.get("sha256")
    elf_path = elf.get("path")
    image_path = image.get("path")
    if not _is_sha256(image_hash):
        raise ValueError("inspector image.sha256 must contain 64 hexadecimal characters")
    if not _is_sha256(elf_hash):
        raise ValueError("inspector boot.elf.sha256 must contain 64 hexadecimal characters")
    if not isinstance(elf_path, str) or not elf_path:
        raise ValueError("inspector boot.elf.path must be a non-empty ISO path")
    if not isinstance(image_path, str):
        raise ValueError("inspector image.path must be a string")
    return InspectorReport(
        image_path=image_path,
        image_sha256=image_hash.lower(),
        boot_elf_path=elf_path,
        boot_elf_sha256=elf_hash.lower(),
    )


def iso_path_to_extracted_path(raw_path: str) -> Path:
    """Map the inspector's raw ISO path to extractor output, rejecting traversal."""
    if not isinstance(raw_path, str) or not raw_path or "\x00" in raw_path:
        raise ValueError("boot ELF ISO path is empty or unsafe")
    if "\\" in raw_path:
        raise ValueError("boot ELF ISO path is unsafe: backslashes are not valid ISO separators")
    path = PurePosixPath(raw_path)
    components: list[str] = []
    for component in path.parts:
        if component in ("/", ""):
            continue
        if component in (".", "..") or ":" in component:
            raise ValueError(f"boot ELF ISO path is unsafe: {raw_path!r}")
        cleaned = re.sub(r";[0-9]+$", "", component)
        if not cleaned or cleaned in (".", "..") or "/" in cleaned:
            raise ValueError(f"boot ELF ISO path is unsafe: {raw_path!r}")
        components.append(cleaned)
    if not components:
        raise ValueError(f"boot ELF ISO path is unsafe: {raw_path!r}")
    return Path(*components)


def _slug(title: str) -> str:
    value = re.sub(r"[^A-Za-z0-9._-]+", "-", title.strip()).strip(".-_")
    return (value[:64] or "ps2-title").lower()


def create_workspace(repo_root: Path, work_root: Path, title: str, image_sha256: str) -> Path:
    repo = repo_root.resolve()
    work = work_root.expanduser().resolve()
    protected_names = {
        "ps2xruntime", "ps2xrecomp", "ps2xanalyzer", "ps2xiop", "ps2xtest", "ps2xstudio", "android"
    }
    for parent in (work, *work.parents):
        if parent == repo:
            break
        if parent.name.lower() in protected_names and (parent == repo or repo in parent.parents or parent in work.parents):
            raise ValueError(f"workspace root is inside a protected source tree: {parent}")
    try:
        relative = work.relative_to(repo)
    except ValueError:
        relative = None
    if relative and relative.parts and relative.parts[0].lower() in protected_names:
        raise ValueError("workspace root is inside a protected source tree")
    if not _is_sha256(image_sha256):
        raise ValueError("image SHA-256 must contain 64 hexadecimal characters")
    title_dir = work / f"{_slug(title)}-{image_sha256[:12].lower()}"
    work.mkdir(parents=True, exist_ok=True)
    for _ in range(8):
        candidate = title_dir / f"run-{uuid.uuid4().hex[:12]}"
        try:
            candidate.mkdir(parents=True, exist_ok=False)
            return candidate.resolve()
        except FileExistsError:
            continue
    raise PipelineError(f"could not allocate a unique workspace under {title_dir}")


def _find_tool(explicit: str | None, env_name: str, names: Sequence[str], root: Path) -> Path:
    candidates: list[str] = []
    if explicit:
        candidates.append(explicit)
    if os.environ.get(env_name):
        candidates.append(os.environ[env_name])
    for candidate in candidates:
        direct = Path(candidate).expanduser()
        if direct.is_file():
            return direct.resolve()
        located = shutil.which(candidate)
        if located:
            return Path(located).resolve()
        raise PipelineError(f"cannot find {candidate!r}; provide a valid --{env_name.lower().removeprefix('ps2native_')} path")
    for name in names:
        located = shutil.which(name)
        if located:
            return Path(located).resolve()
    search_roots = [root / "build", root / "out" / "build"]
    for search_root in search_roots:
        if not search_root.is_dir():
            continue
        for path in search_root.rglob("*"):
            if path.is_file() and path.name in names:
                return path.resolve()
    pretty = " or ".join(names)
    raise PipelineError(
        f"could not locate {pretty}; build the corresponding project or pass the explicit tool option"
    )


def _run(command: Sequence[str], log_path: Path, cwd: Path | None, timeout: int) -> str:
    try:
        completed = subprocess.run(
            list(command), cwd=cwd, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=timeout, check=False,
        )
    except FileNotFoundError as exc:
        raise PipelineError(f"executable not found: {command[0]}") from exc
    except subprocess.TimeoutExpired as exc:
        output = exc.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(output, encoding="utf-8", errors="replace")
        raise PipelineError(f"command timed out after {timeout}s: {command[0]} (log: {log_path})") from exc
    output = completed.stdout or ""
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(output, encoding="utf-8", errors="replace")
    if completed.returncode != 0:
        tail = "\n".join(output.splitlines()[-16:])
        detail = f"\nLast tool output:\n{tail}" if tail else ""
        raise PipelineError(
            f"command exited with status {completed.returncode}: {' '.join(command)}\n"
            f"See log: {log_path}{detail}"
        )
    return output


def _load_inspector_json(inspector: Path, iso: Path, log_path: Path | None, timeout: int) -> tuple[dict[str, Any], str]:
    try:
        completed = subprocess.run(
            [str(inspector), "--json", str(iso)], text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout, check=False,
        )
    except FileNotFoundError as exc:
        raise PipelineError(f"ISO inspector executable not found: {inspector}") from exc
    except subprocess.TimeoutExpired as exc:
        raise PipelineError(f"ISO inspection timed out after {timeout}s: {iso}") from exc
    combined = (completed.stdout or "")
    if completed.stderr:
        combined += "\n[stderr]\n" + completed.stderr
    if log_path:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(combined, encoding="utf-8", errors="replace")
    if completed.returncode != 0:
        explanation = (completed.stderr or completed.stdout or "").strip()
        raise PipelineError(
            f"ISO inspector failed with status {completed.returncode}: {explanation or '(no diagnostic)'}"
            + (f"\nSee log: {log_path}" if log_path else "")
        )
    try:
        raw = json.loads(completed.stdout)
    except json.JSONDecodeError as exc:
        raise PipelineError(f"ISO inspector returned invalid JSON: {exc}") from exc
    try:
        report = validate_inspector_report(raw)
    except ValueError as exc:
        raise PipelineError(str(exc)) from exc
    return raw, report


def inspect_image(iso_path: Path, inspector_option: str | None) -> tuple[dict[str, Any], InspectorReport]:
    root = repo_root()
    iso = iso_path.expanduser().resolve()
    if not iso.is_file():
        raise PipelineError(f"ISO does not exist or is not a regular file: {iso}")
    inspector = _find_tool(inspector_option, "PS2NATIVE_INSPECTOR", ("ps2iso-inspect",), root)
    raw, report = _load_inspector_json(inspector, iso, None, timeout=3600)
    actual_hash = sha256_file(iso)
    if report.image_sha256 != actual_hash:
        raise PipelineError(
            f"inspector image SHA-256 does not match the selected ISO (reported {report.image_sha256}, actual {actual_hash})"
        )
    return raw, report


def _write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


_UNQUALIFIED_ISNAN_COMPARISONS = re.compile(
    r"\bFPU_C_(?:UN|UEQ|ULT|ULE|NGLE|NGL|NGE|NGT)_S\s*\("
)
_LARGE_GENERATED_FAST_COMPILE_THRESHOLD_BYTES = 4 * 1024 * 1024


def normalize_generated_cpp_compatibility(generated: Path) -> list[str]:
    """Make generated COP1 unordered comparisons portable across libstdc++ versions.

    The runtime macros use unqualified ``isnan`` for PS2 unordered comparisons.
    Some C++ standard libraries expose it only as ``std::isnan``. Add a local
    using-declaration only to generated translation units that expand one of
    those macros, keeping unrelated object dependencies untouched.
    """
    root = generated.expanduser().resolve()
    patched: list[str] = []
    for path in sorted(root.rglob("*.cpp")):
        if not path.is_file():
            continue
        source = path.read_text(encoding="utf-8")
        if not _UNQUALIFIED_ISNAN_COMPARISONS.search(source):
            continue
        if re.search(r"^\s*using\s+std::isnan\s*;", source, re.MULTILINE):
            continue
        include_line = '#include "ps2_runtime_macros.h"'
        include_at = source.find(include_line)
        if include_at < 0:
            raise PipelineError(
                f"generated source expands COP1 unordered comparisons without including {include_line}: {path}"
            )
        insertion_at = include_at + len(include_line)
        source = source[:insertion_at] + "\nusing std::isnan;" + source[insertion_at:]
        path.write_text(source, encoding="utf-8", newline="\n")
        patched.append(path.relative_to(root).as_posix())
    return patched


def _walk_extraction(root: Path) -> None:
    base = root.resolve()
    for current, directories, files in os.walk(root, followlinks=False):
        current_path = Path(current)
        for name in [*directories, *files]:
            item = current_path / name
            if item.is_symlink():
                raise PipelineError(f"ISO extractor produced a symlink; refusing to follow it: {item}")
            try:
                item.resolve().relative_to(base)
            except ValueError as exc:
                raise PipelineError(f"ISO extraction path escapes its workspace: {item}") from exc
        for name in files:
            item = current_path / name
            if not item.is_file():
                raise PipelineError(f"ISO extractor created a non-regular file: {item}")


def _step(manifest: dict[str, Any], name: str, status: str, detail: str | None = None) -> None:
    manifest["steps"].append({
        "name": name,
        "status": status,
        "at_utc": datetime.now(timezone.utc).isoformat(),
        **({"detail": detail} if detail else {}),
    })


def _parse_recompiler_summary(output: str) -> dict[str, int]:
    patterns = {
        "functions_discovered": r"^Functions discovered: (\d+)$",
        "additional_entry_points": r"^Additional entrypoints: (\d+)$",
        "generated_functions": r"^Generated functions: (\d+)$",
        "unhandled_instructions": r"^Unhandled instructions: (\d+)$",
        "indirect_fallback_promotions": r"^Indirect fallback promotions: (\d+) \(\d+ fallback entries\)$",
        "indirect_fallback_entries": r"^Indirect fallback promotions: \d+ \((\d+) fallback entries\)$",
        "correctness_critical_guest_fallbacks": r"^Correctness-critical guest fallbacks: (\d+), failures: \d+$",
        "correctness_critical_failures": r"^Correctness-critical guest fallbacks: \d+, failures: (\d+)$",
        "warnings": r"^Warnings: (\d+), errors: \d+$",
        "errors": r"^Warnings: \d+, errors: (\d+)$",
    }
    summary: dict[str, int] = {}
    lines = output.splitlines()
    processed = re.compile(
        r"^Functions processed: (\d+), recompiled: (\d+), stubs: (\d+), skipped: (\d+), decode failures: (\d+)$"
    )
    for name, pattern in patterns.items():
        for line in lines:
            match = re.match(pattern, line.strip())
            if match:
                summary[name] = int(match.group(1))
                break
    for line in lines:
        match = processed.match(line.strip())
        if match:
            summary.update({
                "functions_processed": int(match.group(1)),
                "functions_recompiled": int(match.group(2)),
                "functions_stubbed": int(match.group(3)),
                "functions_skipped": int(match.group(4)),
                "decode_failures": int(match.group(5)),
            })
            break
    return summary


def _translation_coverage(summary: dict[str, int]) -> str:
    required = {
        "functions_discovered",
        "functions_processed",
        "functions_recompiled",
        "functions_stubbed",
        "functions_skipped",
        "decode_failures",
        "unhandled_instructions",
        "correctness_critical_failures",
        "errors",
    }
    if not required.issubset(summary):
        return "unknown"
    if (
        summary["functions_discovered"] == 0
        or summary["functions_processed"] < summary["functions_discovered"]
        or summary["functions_recompiled"] + summary["functions_stubbed"] + summary["functions_skipped"]
        != summary["functions_processed"]
        or summary["functions_skipped"] > 0
        or summary["decode_failures"] > 0
        or summary.get("unhandled_instructions", 0) > 0
        or summary.get("correctness_critical_failures", 0) > 0
        or summary.get("errors", 0) > 0
    ):
        return "partial"
    if summary["functions_stubbed"] > 0:
        return "runtime_stubs_present"
    return "no_reported_instruction_gaps"


def _elf_integer(value: Any, field: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{field} must be an integer or hexadecimal string")
    if isinstance(value, int):
        parsed = value
    elif isinstance(value, str):
        try:
            parsed = int(value, 0)
        except ValueError:
            parsed = int(value, 16)
    else:
        raise ValueError(f"{field} must be an integer or hexadecimal string")
    if parsed < 0:
        raise ValueError(f"{field} must not be negative")
    return parsed


def _read_function_ranges(report_path: Path | None) -> tuple[list[dict[str, Any]] | None, str | None]:
    if report_path is None or not report_path.is_file():
        return None, "function coverage CSV is missing"

    required = {"start_address", "end_address", "status"}
    records: list[dict[str, Any]] = []
    try:
        with report_path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            if reader.fieldnames is None or not required.issubset(reader.fieldnames):
                return None, "function coverage CSV has an unsupported header"
            for row_number, row in enumerate(reader, start=2):
                start = _elf_integer(row["start_address"], f"function CSV row {row_number} start_address")
                end = _elf_integer(row["end_address"], f"function CSV row {row_number} end_address")
                status = row["status"]
                if start >= end or end > 0x1_0000_0000:
                    raise ValueError(f"function CSV row {row_number} has an invalid address span")
                if status not in {"recompiled", "runtime_stub", "skipped", "unprocessed"}:
                    raise ValueError(f"function CSV row {row_number} has an unknown status {status!r}")
                records.append({"start": start, "end": end, "status": status})
    except (OSError, UnicodeError, csv.Error, KeyError, TypeError, ValueError) as exc:
        return None, f"function coverage CSV could not be read: {exc}"
    return records, None


def _write_address_range_ledger(
    load_segments: Any,
    function_report_path: Path | None,
    ledger_path: Path,
) -> dict[str, Any]:
    """Partition executable PT_LOAD file ranges by reported function spans."""
    functions, report_error = _read_function_ranges(function_report_path)
    try:
        if not isinstance(load_segments, list):
            raise ValueError("ELF load_segments is missing or is not an array")
        executable_segments: list[dict[str, int]] = []
        for index, segment in enumerate(load_segments):
            if not isinstance(segment, dict):
                raise ValueError(f"PT_LOAD segment {index} is not an object")
            flags = _elf_integer(segment.get("flags"), f"PT_LOAD segment {index} flags")
            if not flags & 1:  # PF_X
                continue
            start = _elf_integer(segment.get("virtual_address"), f"PT_LOAD segment {index} virtual_address")
            file_size = _elf_integer(segment.get("file_size_bytes"), f"PT_LOAD segment {index} file_size_bytes")
            memory_size = _elf_integer(segment.get("memory_size_bytes"), f"PT_LOAD segment {index} memory_size_bytes")
            file_offset = _elf_integer(segment.get("file_offset"), f"PT_LOAD segment {index} file_offset")
            if file_size > memory_size or start + memory_size > 0x1_0000_0000:
                raise ValueError(f"PT_LOAD segment {index} has invalid bounds")
            executable_segments.append({
                "index": index,
                "start": start,
                "file_end": start + file_size,
                "memory_end": start + memory_size,
                "file_offset": file_offset,
                "file_size": file_size,
                "memory_size": memory_size,
                "flags": flags,
            })
    except (TypeError, ValueError) as exc:
        ledger_path.parent.mkdir(parents=True, exist_ok=True)
        with ledger_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow([
                "segment_index", "segment_file_offset", "segment_start", "segment_file_end", "segment_memory_end",
                "range_start", "range_end", "range_file_offset_start", "range_file_offset_end",
                "range_bytes", "classification", "function_count", "function_starts",
            ])
        return {
            "status": "unknown",
            "ledger_file": str(ledger_path),
            "error": str(exc),
            "executable_segment_count": 0,
        }

    ledger_path.parent.mkdir(parents=True, exist_ok=True)
    totals = {
        "executable_file_backed_bytes": 0,
        "recompiled_function_range_bytes": 0,
        "runtime_stub_function_range_bytes": 0,
        "skipped_function_range_bytes": 0,
        "unprocessed_function_range_bytes": 0,
        "overlapping_function_range_bytes": 0,
        "unattributed_executable_file_bytes": 0,
        "unknown_function_coverage_bytes": 0,
        "executable_zero_fill_bytes": 0,
    }
    with ledger_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow([
            "segment_index", "segment_file_offset", "segment_start", "segment_file_end", "segment_memory_end",
            "range_start", "range_end", "range_file_offset_start", "range_file_offset_end",
            "range_bytes", "classification", "function_count", "function_starts",
        ])
        for segment in executable_segments:
            start = segment["start"]
            file_end = segment["file_end"]
            memory_end = segment["memory_end"]
            totals["executable_file_backed_bytes"] += segment["file_size"]
            totals["executable_zero_fill_bytes"] += segment["memory_size"] - segment["file_size"]

            if start < file_end:
                if functions is None:
                    totals["unknown_function_coverage_bytes"] += file_end - start
                    writer.writerow([
                        segment["index"], f"0x{segment['file_offset']:x}",
                        f"0x{start:x}", f"0x{file_end:x}", f"0x{memory_end:x}",
                        f"0x{start:x}", f"0x{file_end:x}", f"0x{segment['file_offset']:x}",
                        f"0x{segment['file_offset'] + file_end - start:x}", file_end - start,
                        "unknown_function_coverage", 0, "",
                    ])
                else:
                    events: dict[int, list[tuple[bool, int]]] = {}
                    for function_index, function in enumerate(functions):
                        range_start = max(start, function["start"])
                        range_end = min(file_end, function["end"])
                        if range_start >= range_end:
                            continue
                        events.setdefault(range_start, []).append((True, function_index))
                        events.setdefault(range_end, []).append((False, function_index))

                    boundaries = sorted({start, file_end, *events.keys()})
                    active: dict[int, dict[str, Any]] = {}
                    for boundary_index, range_start in enumerate(boundaries[:-1]):
                        for is_start, function_index in events.get(range_start, []):
                            if not is_start:
                                active.pop(function_index, None)
                        for is_start, function_index in events.get(range_start, []):
                            if is_start:
                                active[function_index] = functions[function_index]

                        range_end = boundaries[boundary_index + 1]
                        if range_start >= range_end:
                            continue
                        span = range_end - range_start
                        active_functions = sorted(active.values(), key=lambda function: (function["start"], function["end"]))
                        if not active_functions:
                            classification = "unattributed_executable_file_bytes"
                            totals["unattributed_executable_file_bytes"] += span
                        elif len(active_functions) > 1:
                            classification = "overlapping_function_ranges"
                            totals["overlapping_function_range_bytes"] += span
                        else:
                            status = active_functions[0]["status"]
                            classification = f"{status}_function_range_bytes"
                            total_key = f"{status}_function_range_bytes"
                            if total_key in totals:
                                totals[total_key] += span
                            else:
                                totals["unattributed_executable_file_bytes"] += span
                                classification = "unknown_function_status"
                        function_starts = ";".join(
                            f"0x{function['start']:x}" for function in active_functions[:64]
                        )
                        file_offset_start = segment["file_offset"] + range_start - start
                        file_offset_end = segment["file_offset"] + range_end - start
                        writer.writerow([
                            segment["index"], f"0x{segment['file_offset']:x}",
                            f"0x{start:x}", f"0x{file_end:x}", f"0x{memory_end:x}",
                            f"0x{range_start:x}", f"0x{range_end:x}",
                            f"0x{file_offset_start:x}", f"0x{file_offset_end:x}", span, classification,
                            len(active_functions), function_starts,
                        ])

            if file_end < memory_end:
                writer.writerow([
                    segment["index"], f"0x{segment['file_offset']:x}",
                    f"0x{start:x}", f"0x{file_end:x}", f"0x{memory_end:x}",
                    f"0x{file_end:x}", f"0x{memory_end:x}", "", "", memory_end - file_end,
                    "executable_segment_zero_fill", 0, "",
                ])

    if not executable_segments:
        status = "no_executable_segments"
    elif functions is None:
        status = "unknown"
    elif totals["executable_file_backed_bytes"] == 0:
        status = "no_file_backed_executable_bytes"
    elif any(totals[key] for key in (
        "unattributed_executable_file_bytes", "overlapping_function_range_bytes",
        "skipped_function_range_bytes", "unprocessed_function_range_bytes",
    )):
        status = "unattributed_or_ambiguous_ranges"
    else:
        status = "function_ranges_cover_file_backed_executable_bytes"
    return {
        "status": status,
        "ledger_file": str(ledger_path),
        "function_coverage_file": str(function_report_path) if function_report_path and function_report_path.is_file() else None,
        "function_ranges_are_heuristic": True,
        "zero_fill_is_reported_separately": True,
        "error": report_error,
        "executable_segment_count": len(executable_segments),
        **totals,
    }


def _compiled_module_key(raw_path: str) -> str:
    return iso_path_to_extracted_path(raw_path).as_posix().translate(
        str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz")
    )


def _boot_module_keys(boot_path: str, boot_sha256: str, inventory_items: list[Any]) -> list[str]:
    aliases = [boot_path, "boot.elf"]
    for item in inventory_items:
        if not isinstance(item, dict) or str(item.get("sha256", "")).lower() != boot_sha256.lower():
            continue
        paths = item.get("paths")
        if isinstance(paths, list):
            aliases.extend(path for path in paths if isinstance(path, str))

    keys: list[str] = []
    for alias in aliases:
        key = _compiled_module_key(alias)
        if key and key not in keys:
            keys.append(key)
    return keys


def _configure_module_recompiler(
    config_path: Path,
    module_keys: list[str],
    symbol_prefix: str,
    emit_dense_function_table: bool = False,
) -> None:
    config_text = config_path.read_text(encoding="utf-8")
    section = re.search(r"(?m)^\[general\][ \t]*$", config_text)
    if section is None:
        raise PipelineError(f"analyzer config has no [general] table: {config_path}")
    insert_at = config_text.find("\n", section.end())
    if insert_at < 0:
        insert_at = len(config_text)
    else:
        insert_at += 1
    additions = (
        f"module_keys = {json.dumps(module_keys)}\n"
        f"module_symbol_prefix = {json.dumps(symbol_prefix)}\n"
        f"module_emit_dense_function_table = {'true' if emit_dense_function_table else 'false'}\n"
    )
    config_path.write_text(config_text[:insert_at] + additions + config_text[insert_at:], encoding="utf-8")


def _compile_secondary_ee_modules(
    workspace: Path,
    logs: Path,
    disc: Path,
    generated_root: Path,
    inventory_items: list[Any],
    analyzer: Path,
    recompiler: Path,
    timeout: int,
    manifest: dict[str, Any],
    boot_registration_symbol: str,
) -> list[dict[str, Any]]:
    candidates = [
        item for item in inventory_items
        if isinstance(item, dict) and item.get("role") == "secondary_ee_exec_candidate"
    ]
    records: list[dict[str, Any]] = []
    manifest["modules"] = {"status": "in_progress", "candidate_count": len(candidates), "items": records}
    _step(manifest, "recompile_secondary_ee_modules", "started", f"{len(candidates)} MIPS III ELF candidate(s)")
    for item in candidates:
        digest = item.get("sha256")
        paths = item.get("paths")
        if not _is_sha256(digest) or not isinstance(paths, list) or not paths:
            raise PipelineError("secondary EE inventory item has invalid SHA-256 or ISO path list")

        module_keys: list[str] = []
        source_paths: list[Path] = []
        for raw_path in paths:
            if not isinstance(raw_path, str):
                raise PipelineError(f"secondary EE inventory contains a non-string ISO path: {raw_path!r}")
            key = _compiled_module_key(raw_path)
            if key and key not in module_keys:
                module_keys.append(key)
            relative = iso_path_to_extracted_path(raw_path)
            source = disc / relative
            try:
                source.resolve().relative_to(disc.resolve())
            except ValueError as exc:
                raise PipelineError(f"secondary ELF path escapes extracted disc: {raw_path!r}") from exc
            if source.is_symlink() or not source.is_file():
                raise PipelineError(f"secondary ELF was not extracted as a regular file: {source}")
            if sha256_file(source) != digest.lower():
                raise PipelineError(f"secondary ELF SHA-256 mismatch for {raw_path!r}")
            source_paths.append(source)
        if not module_keys or not source_paths:
            raise PipelineError(f"secondary EE ELF {digest} has no usable ISO path aliases")

        module_workspace = workspace / "analysis" / "modules" / digest.lower()
        safe_elf = module_workspace / "module.elf"
        config_path = module_workspace / "module.toml"
        record: dict[str, Any] = {
            "id": f"sha256:{digest.lower()}",
            "sha256": digest.lower(),
            "classification": "MIPS III ELF32 little-endian ET_EXEC heuristic",
            "iso_paths": paths,
            "module_keys": module_keys,
            "entrypoint": item.get("entrypoint"),
            "status": "pending",
        }
        records.append(record)

        module_log = logs / f"module-{digest[:16].lower()}"
        symbol_prefix = f"ps2m_{digest[:24].lower()}_"
        packaged_module_output = generated_root / "modules" / digest.lower()
        try:
            module_workspace.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source_paths[0], safe_elf)
            _run(
                [str(analyzer), str(safe_elf), str(config_path)],
                module_log.with_suffix(".analyzer.log"),
                workspace,
                timeout,
            )
            if not config_path.is_file():
                raise PipelineError(f"analyzer returned success without creating module TOML: {config_path}")

            _configure_module_recompiler(config_path, module_keys, symbol_prefix)
            recompiler_output = _run(
                [str(recompiler), str(config_path)],
                module_log.with_suffix(".recompiler.log"),
                workspace,
                timeout,
            )
            module_output = module_workspace / "output"
            if not (module_output / "register_functions.cpp").is_file():
                raise PipelineError(
                    f"secondary module recompiler output is missing its registration source: {module_output}"
                )

            packaged_module_output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(module_output, packaged_module_output)
            report = _parse_recompiler_summary(recompiler_output)
            function_coverage_path = packaged_module_output / "function_coverage.csv"
            address_ledger = _write_address_range_ledger(
                item.get("load_segments"),
                function_coverage_path if function_coverage_path.is_file() else None,
                packaged_module_output / "address_coverage.csv",
            )
            record.update({
                "status": "compiled",
                "symbol_prefix": symbol_prefix,
                "generated_directory": str(packaged_module_output),
                "function_coverage_report": str(function_coverage_path) if function_coverage_path.is_file() else None,
                "address_range_ledger": address_ledger,
                "generated_cpp_count": sum(1 for path in packaged_module_output.rglob("*.cpp") if path.is_file()),
                "recompiler_report": report,
                "static_translation_coverage": _translation_coverage(report),
            })
        except (PipelineError, OSError) as exc:
            if packaged_module_output.exists():
                shutil.rmtree(packaged_module_output, ignore_errors=True)
            record.update({
                "status": "compile_failed",
                "error": str(exc),
                "analyzer_log": str(module_log.with_suffix(".analyzer.log")),
                "recompiler_log": str(module_log.with_suffix(".recompiler.log")),
            })

    aggregator = generated_root / "register_modules.cpp"
    declarations = [
        "#include \"ps2_runtime.h\"",
        "",
        f"void {boot_registration_symbol}(PS2Runtime &runtime);",
    ]
    compiled_records = [record for record in records if record.get("status") == "compiled"]
    for record in compiled_records:
        declarations.append(f"void {record['symbol_prefix']}register_compiled_module(PS2Runtime &runtime);")
    declarations.extend(["", "void ps2xRegisterGeneratedModules(PS2Runtime &runtime)", "{"])
    declarations.append(f"    {boot_registration_symbol}(runtime);")
    for record in compiled_records:
        declarations.append(f"    {record['symbol_prefix']}register_compiled_module(runtime);")
    declarations.extend(["}", ""])
    declarations.extend([
        "namespace {",
        "struct GeneratedModuleRegistrarInstaller",
        "{",
        "    GeneratedModuleRegistrarInstaller()",
        "    {",
        "        PS2Runtime::setGeneratedModuleRegistrar(&ps2xRegisterGeneratedModules);",
        "    }",
        "};",
        "const GeneratedModuleRegistrarInstaller g_generatedModuleRegistrarInstaller{};",
        "}",
        "",
    ])
    aggregator.write_text("\n".join(declarations), encoding="utf-8")

    compiled_count = len(compiled_records)
    status = "no_ee_candidates" if not candidates else (
        "compiled" if compiled_count == len(candidates) else "partial" if compiled_count else "none_compiled"
    )
    manifest["modules"] = {
        "status": status,
        "candidate_count": len(candidates),
        "compiled_count": compiled_count,
        "failed_count": len(candidates) - compiled_count,
        "items": records,
    }
    _step(manifest, "recompile_secondary_ee_modules", "complete", f"compiled {compiled_count} of {len(candidates)} module(s)")
    return records


def _reject_existing_output(path: Path) -> None:
    if path.exists() or path.is_symlink():
        raise PipelineError(f"output path already exists; refusing to overwrite: {path}")


def _normalize_new_destination(path: Path) -> Path:
    expanded = path.expanduser()
    # abspath normalizes relative components without resolving the final link.
    absolute = Path(os.path.abspath(expanded))
    _reject_existing_output(absolute)
    if not absolute.name:
        raise PipelineError(f"output destination must name a new directory: {absolute}")
    normalized = absolute.parent.resolve() / absolute.name
    _reject_existing_output(normalized)
    return normalized


def _rename_noreplace(source: Path, destination: Path) -> None:
    """Atomically publish a sibling staging directory without replacing a path."""
    if sys.platform == "win32":
        os.rename(source, destination)  # Windows rename fails when destination exists.
        return
    if sys.platform.startswith("linux"):
        libc = ctypes.CDLL(None, use_errno=True)
        renameat2 = getattr(libc, "renameat2", None)
        if renameat2 is None:
            raise PipelineError("libc has no renameat2; cannot atomically publish without overwrite on this host")
        renameat2.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
        renameat2.restype = ctypes.c_int
        result = renameat2(-100, os.fsencode(source), -100, os.fsencode(destination), 1)  # AT_FDCWD, RENAME_NOREPLACE
        if result != 0:
            error = ctypes.get_errno()
            if error == errno.EEXIST:
                raise PipelineError(f"output path appeared during package publication; refusing to overwrite: {destination}")
            raise OSError(error, os.strerror(error), str(destination))
        return
    if sys.platform == "darwin":
        libc = ctypes.CDLL(None, use_errno=True)
        renamex_np = getattr(libc, "renamex_np", None)
        if renamex_np is None:
            raise PipelineError("libc has no renamex_np; cannot atomically publish without overwrite on this host")
        renamex_np.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
        renamex_np.restype = ctypes.c_int
        if renamex_np(os.fsencode(source), os.fsencode(destination), 0x00000004) != 0:  # RENAME_EXCL
            error = ctypes.get_errno()
            if error == errno.EEXIST:
                raise PipelineError(f"output path appeared during package publication; refusing to overwrite: {destination}")
            raise OSError(error, os.strerror(error), str(destination))
        return
    raise PipelineError(f"atomic no-overwrite package publication is unsupported on {sys.platform}")


def publish_package(package: Path, destination: Path) -> Path:
    destination = _normalize_new_destination(destination)
    parent = destination.parent
    parent.mkdir(parents=True, exist_ok=True)
    staging = parent / f".{destination.name}.ps2native-{uuid.uuid4().hex}.staging"
    try:
        shutil.copytree(package, staging)
        _rename_noreplace(staging, destination)
    except Exception:
        if staging.exists():
            shutil.rmtree(staging, ignore_errors=True)
        raise
    return destination


def _ensure_output_outside_sources(path: Path, repo: Path) -> None:
    try:
        relative = path.resolve().relative_to(repo.resolve())
    except ValueError:
        return
    protected = {"ps2xruntime", "ps2xrecomp", "ps2xanalyzer", "ps2xiop", "ps2xtest", "ps2xstudio", "android"}
    if relative.parts and relative.parts[0].lower() in protected:
        raise PipelineError(f"package output cannot be placed inside a protected source tree: {path}")


def _cmake_template() -> Path:
    return Path(__file__).resolve().parent / "templates" / "desktop" / "CMakeLists.txt"


def _configure_and_package(
    root: Path,
    workspace: Path,
    generated: Path,
    disc: Path,
    output_dir: Path,
    cmake_option: str | None,
    jobs: int | None,
    timeout: int,
    manifest: dict[str, Any],
) -> Path:
    cmake = _find_tool(cmake_option, "PS2NATIVE_CMAKE", ("cmake",), root)
    project_dir = workspace / "desktop-project"
    build_dir = project_dir / "build"
    project_dir.mkdir(parents=True, exist_ok=True)
    template = _cmake_template()
    if not template.is_file():
        raise PipelineError(f"desktop CMake template is missing: {template}")
    shutil.copyfile(template, project_dir / "CMakeLists.txt")

    configure = [
        str(cmake), "-S", str(project_dir), "-B", str(build_dir),
        f"-DPS2X_SOURCE_ROOT:PATH={root}",
        f"-DPS2X_GENERATED_CODE_DIR:PATH={generated}",
        "-DPS2X_BUILD_RECOMP:BOOL=OFF",
        "-DPS2X_BUILD_RUNTIME:BOOL=ON",
        "-DPS2X_BUILD_ANALYZER:BOOL=OFF",
        "-DPS2X_BUILD_TEST:BOOL=OFF",
        "-DPS2X_BUILD_STUDIO:BOOL=OFF",
        "-DPS2X_BUILD_ISO_INSPECTOR:BOOL=OFF",
        "-DPS2X_ENABLE_DEBUG_UI:BOOL=OFF",
        "-DPS2X_ENABLE_RUNNER_UNITY_BUILD:BOOL=OFF",
        "-DPS2X_ENABLE_RUNNER_PCH:BOOL=ON",
        "-DPS2X_FAST_ITERATION:BOOL=ON",
        "-DPS2X_ENABLE_RELEASE_IPO:BOOL=OFF",
        "-DPS2X_ENABLE_LARGE_GENERATED_FAST_COMPILE:BOOL=ON",
        f"-DPS2X_LARGE_GENERATED_FAST_COMPILE_THRESHOLD_BYTES:STRING={_LARGE_GENERATED_FAST_COMPILE_THRESHOLD_BYTES}",
        "-DCMAKE_BUILD_TYPE:STRING=Release",
    ]
    deps = root / "build" / "_deps"
    for dependency in ("elfio", "toml11", "fmt", "libdwarf", "rabbitizer", "raylib"):
        source = deps / f"{dependency}-src"
        if source.is_dir():
            configure.append(f"-DFETCHCONTENT_SOURCE_DIR_{dependency.upper()}:PATH={source}")

    _step(manifest, "desktop_configure", "started")
    _run(configure, workspace / "logs" / "cmake-configure.log", project_dir, timeout)
    _step(manifest, "desktop_configure", "complete")
    large_generated_sources = [
        {
            "path": path.relative_to(generated).as_posix(),
            "size_bytes": path.stat().st_size,
        }
        for path in sorted(generated.rglob("*.cpp"))
        if path.is_file()
        and path.name != "register_functions.cpp"
        and path.stat().st_size > _LARGE_GENERATED_FAST_COMPILE_THRESHOLD_BYTES
    ]
    manifest.setdefault("build_inputs", {}).setdefault("configuration", {})[
        "large_generated_fast_compile"
    ] = {
        "enabled": True,
        "threshold_bytes": _LARGE_GENERATED_FAST_COMPILE_THRESHOLD_BYTES,
        "compile_options": ["-O0", "-fno-lto"],
        "sources": large_generated_sources,
    }

    build = [str(cmake), "--build", str(build_dir), "--target", "ps2EntryRunner", "--config", "Release"]
    if jobs:
        if jobs < 1:
            raise PipelineError("--build-jobs must be a positive integer")
        build.extend(["--parallel", str(jobs)])
    _step(manifest, "desktop_link", "started")
    _run(build, workspace / "logs" / "cmake-build.log", project_dir, timeout)
    _step(manifest, "desktop_link", "complete")

    output_dir.mkdir(parents=True, exist_ok=True)
    install = [str(cmake), "--install", str(build_dir), "--prefix", str(output_dir), "--config", "Release"]
    _run(install, workspace / "logs" / "cmake-install.log", project_dir, timeout)
    runner_candidates = [
        output_dir / "bin" / "ps2EntryRunner",
        output_dir / "bin" / "ps2EntryRunner.exe",
        build_dir / "ps2xRuntime" / "ps2EntryRunner",
        build_dir / "ps2xRuntime" / "Release" / "ps2EntryRunner.exe",
    ]
    runner = next((candidate for candidate in runner_candidates if candidate.is_file()), None)
    if runner is None:
        raise PipelineError(
            "CMake configure/build completed, but no ps2EntryRunner artifact was found; "
            f"inspect {workspace / 'logs' / 'cmake-build.log'}"
        )

    game_dir = output_dir / "game"
    if game_dir.exists():
        shutil.rmtree(game_dir)
    shutil.copytree(disc, game_dir)
    runner_relative = Path("bin") / runner.name
    if os.name == "nt":
        launcher = output_dir / "run-ps2native.bat"
        launcher.write_text(
            "@echo off\r\nsetlocal\r\ncd /d \"%~dp0\"\r\n"
            f'"%~dp0{runner_relative.as_posix().replace("/", chr(92))}" '
            '"%~dp0game\\boot.elf" %*\r\n'
            "exit /b %ERRORLEVEL%\r\n",
            encoding="utf-8",
        )
    else:
        launcher = output_dir / "run-ps2native.sh"
        launcher.write_text(
            "#!/bin/sh\nset -eu\ncd -- \"$(dirname -- \"$0\")\"\n"
            f"exec \"$PWD/{runner_relative.as_posix()}\" \"$PWD/game/boot.elf\" \"$@\"\n",
            encoding="utf-8",
        )
        launcher.chmod(0o755)
    manifest["desktop"] = {
        "cmake_project": str(project_dir),
        "cmake_build_dir": str(build_dir),
        "runner": str(runner),
        "launcher": str(launcher),
        "game_assets": str(game_dir),
        "desktop_scope": "builds for the host machine ABI; cross-compilation is not implemented",
    }
    return output_dir


def _android_sdk_root() -> Path | None:
    candidates = [
        os.environ.get("ANDROID_SDK_ROOT"),
        os.environ.get("ANDROID_HOME"),
        str(Path.home() / "Android" / "Sdk"),
        "/opt/android-sdk",
        "/opt/android-sdk-linux",
        "/usr/lib/android-sdk",
    ]
    return next((Path(item).expanduser().resolve() for item in candidates if item and Path(item).is_dir()), None)


def _cached_gradle() -> Path | None:
    cache = Path.home() / ".gradle" / "wrapper" / "dists"
    candidates = list(cache.glob("gradle-*-bin/*/gradle-*/bin/gradle"))
    candidates += list(cache.glob("gradle-*-bin/*/gradle-*/bin/gradle.bat"))
    def version_key(path: Path) -> tuple[int, ...]:
        match = re.search(r"gradle-([0-9]+(?:\.[0-9]+)*)", path.parent.parent.name)
        return tuple(int(part) for part in match.group(1).split(".")) if match else (0,)
    candidates.sort(key=version_key, reverse=True)
    return next((candidate.resolve() for candidate in candidates if candidate.is_file()), None)


def _java_major_version() -> tuple[int | None, str | None]:
    java = None
    java_home = os.environ.get("JAVA_HOME")
    if java_home:
        candidate = Path(java_home) / "bin" / ("java.exe" if os.name == "nt" else "java")
        if candidate.is_file():
            java = candidate
    if java is None:
        located = shutil.which("java")
        if located:
            java = Path(located)
    if java is None:
        return None, None
    try:
        result = subprocess.run([str(java), "-version"], capture_output=True, text=True, timeout=10, check=False)
    except (OSError, subprocess.TimeoutExpired):
        return None, str(java)
    output = (result.stderr or "") + (result.stdout or "")
    match = re.search(r'version\s+"(\d+)(?:\.(\d+))?', output)
    if not match:
        return None, str(java)
    major = int(match.group(1))
    if major == 1 and match.group(2):
        major = int(match.group(2))
    return major, str(java)


def _android_prerequisite_errors(android_project: Path, gradle_option: str | None) -> tuple[Path | None, Path | None, list[str]]:
    reasons: list[str] = []
    try:
        gradle = _find_tool(gradle_option, "PS2NATIVE_GRADLE", ("gradle",), repo_root())
    except PipelineError as exc:
        wrapper = android_project / ("gradlew.bat" if os.name == "nt" else "gradlew")
        if wrapper.is_file() and (os.name == "nt" or os.access(wrapper, os.X_OK)):
            gradle = wrapper.resolve()
        else:
            gradle = _cached_gradle()
        if gradle is None:
            reasons.append(f"{exc}; this checkout has no runnable Gradle wrapper")

    if gradle is not None:
        try:
            version_result = subprocess.run(
                [str(gradle), "--version"], capture_output=True, text=True,
                timeout=30, check=False,
            )
            version_output = (version_result.stdout or "") + (version_result.stderr or "")
            version_match = re.search(r"Gradle\s+(\d+)\.(\d+)(?:\.(\d+))?", version_output)
            if version_result.returncode != 0 or not version_match:
                reasons.append(f"Gradle could not report its version: {gradle}")
            elif (int(version_match.group(1)), int(version_match.group(2))) < (8, 7):
                reasons.append(f"Gradle 8.7 or newer is required; found {version_match.group(0)} at {gradle}")
        except (OSError, subprocess.TimeoutExpired):
            reasons.append(f"Gradle could not start: {gradle}")

    java_major, java_path = _java_major_version()
    if java_major is None:
        reasons.append("JDK 17 not found; set JAVA_HOME to a JDK 17 installation")
    elif java_major != 17:
        reasons.append(f"JDK 17 is required; active Java is version {java_major} at {java_path}")
    if java_path:
        javac_name = "javac.exe" if os.name == "nt" else "javac"
        if not (Path(java_path).with_name(javac_name).is_file()):
            reasons.append(f"JDK compiler javac is missing beside {java_path}; install a full JDK 17")

    sdk = _android_sdk_root()
    ndk_version_match = re.search(
        r"ndkVersion\s+['\"]([^'\"]+)['\"]",
        (android_project / "app" / "build.gradle").read_text(encoding="utf-8"),
    )
    ndk_version = ndk_version_match.group(1) if ndk_version_match else "28.2.13676358"
    ndk_env = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT")
    ndk = Path(ndk_env).expanduser().resolve() if ndk_env and Path(ndk_env).is_dir() else None
    if ndk is None and sdk:
        ndk_candidate = sdk / "ndk" / ndk_version
        if ndk_candidate.is_dir():
            ndk = ndk_candidate
    if sdk is None:
        reasons.append("Android SDK not found; set ANDROID_SDK_ROOT or ANDROID_HOME")
    elif not (sdk / "platforms" / "android-34" / "android.jar").is_file():
        reasons.append(f"Android SDK platform 34 is missing under {sdk}; install platforms;android-34")
    if sdk and not (sdk / "build-tools" / "34.0.0").is_dir():
        reasons.append(f"Android build-tools 34.0.0 are missing under {sdk}; install build-tools;34.0.0")
    if ndk is None or not (ndk / "source.properties").is_file():
        reasons.append(f"Android NDK {ndk_version} not found; install ndk;{ndk_version} or set ANDROID_NDK_HOME")
    if sdk and not (sdk / "cmake" / "3.22.1").is_dir():
        reasons.append(f"Android CMake 3.22.1 is missing under {sdk}/cmake; install cmake;3.22.1")

    activity = android_project / "app" / "src" / "main" / "java" / "com" / "ps2x" / "runner" / "Ps2PackageActivity.java"
    if not activity.is_file():
        reasons.append(f"NativeActivity asset bridge is missing: {activity}")
    return gradle, sdk, reasons


def _stage_android_project(root: Path, workspace: Path, disc: Path) -> Path:
    source = root / "android"
    if not source.is_dir():
        raise PipelineError(f"Android project is missing from the checkout: {source}")
    destination = workspace / "android-project"
    shutil.copytree(
        source,
        destination,
        ignore=shutil.ignore_patterns(".gradle", "build", "local.properties", "*.apk"),
    )
    assets_game = destination / "app" / "src" / "main" / "assets" / "game"
    assets_game.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(disc, assets_game)
    return destination


def _build_android_package(
    root: Path,
    workspace: Path,
    generated: Path,
    disc: Path,
    output_dir: Path,
    gradle_option: str | None,
    timeout: int,
    manifest: dict[str, Any],
) -> Path:
    _step(manifest, "stage_android_project", "started")
    android_project = _stage_android_project(root, workspace, disc)
    _step(manifest, "stage_android_project", "complete")
    assets_game = android_project / "app" / "src" / "main" / "assets" / "game"
    manifest["android"] = {
        "status": "staged",
        "project": str(android_project),
        "assets_game": str(assets_game),
        "expected_private_boot_path": "<ANativeActivity internalDataPath>/game/boot.elf",
        "abi": "arm64-v8a",
        "native_activity": "com.ps2x.runner.Ps2PackageActivity",
        "application_id": f"com.ps2x.native.g{manifest['source']['iso_sha256'][:12]}",
    }
    marker = android_project / "app" / "src" / "main" / "assets" / "ps2-package.marker"
    marker.write_text(
        json.dumps({
            "schema_version": 1,
            "iso_sha256": manifest["source"]["iso_sha256"],
            "boot_elf_sha256": manifest["boot_elf"]["sha256"],
        }, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    gradle, sdk, problems = _android_prerequisite_errors(android_project, gradle_option)
    if problems:
        raise AndroidPrerequisitesMissing(problems)
    assert gradle is not None and sdk is not None

    local_properties = android_project / "local.properties"
    escaped_sdk = str(sdk).replace("\\", "\\\\").replace(":", "\\:")
    local_properties.write_text(f"sdk.dir={escaped_sdk}\n", encoding="utf-8")
    manifest["android"].update({
        "sdk": str(sdk),
        "gradle": str(gradle),
        "generated_cpp": str(generated),
    })
    command = [
        str(gradle), "--no-daemon", "--console=plain",
        f"-Pps2xSourceRoot={root}",
        f"-Pps2xGeneratedDir={generated}",
        "-Pps2xAndroidAbis=arm64-v8a",
        f"-Pps2xApplicationId={manifest['android']['application_id']}",
        "assembleRelease",
    ]
    _step(manifest, "android_gradle_build", "started")
    _run(command, workspace / "logs" / "android-gradle.log", android_project, timeout)
    apk = android_project / "app" / "build" / "outputs" / "apk" / "release" / "app-release.apk"
    if not apk.is_file():
        raise PipelineError(f"Gradle completed without the expected APK artifact: {apk}")
    output_dir.mkdir(parents=True, exist_ok=True)
    package_apk = output_dir / f"{_slug(Path(manifest['source']['iso_path']).stem)}-{manifest['source']['iso_sha256'][:12]}-arm64.apk"
    shutil.copy2(apk, package_apk)
    manifest["android"].update({
        "status": "apk_built",
        "application_id": manifest["android"]["application_id"],
        "apk": str(package_apk),
        "packaged_asset_root": "assets/game/",
        "runtime_boot_path": "ANativeActivity::internalDataPath/game/boot.elf",
        "abi": "arm64-v8a",
        "distribution_note": "Disc assets are embedded in the APK; split/OBB delivery is not implemented.",
    })
    _step(manifest, "android_gradle_build", "complete")
    return output_dir


def run_build(args: Any) -> dict[str, Any]:
    root = repo_root()
    iso = Path(args.iso).expanduser().resolve()
    if not iso.is_file():
        raise PipelineError(f"ISO does not exist or is not a regular file: {iso}")
    if args.timeout < 1:
        raise PipelineError("--timeout must be a positive integer")
    image_hash = sha256_file(iso)
    source_snapshot = source_tree_snapshot(root)
    work_root = Path(args.work_root).expanduser() if args.work_root else root / "build" / "ps2native"
    workspace = create_workspace(root, work_root, iso.stem, image_hash)
    logs = workspace / "logs"
    manifest_path = workspace / "manifest.json"
    manifest: dict[str, Any] = {
        "schema_version": 1,
        "tool": {"name": "ps2native", "version": "0.1.0"},
        "status": "in_progress",
        "target": args.target,
        "support_tier": "experimental",
        "gameplay_compatibility": "unverified",
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "source": {"iso_path": str(iso), "iso_size_bytes": iso.stat().st_size, "iso_sha256": image_hash},
        "build_inputs": {
            "source_tree": source_snapshot,
            "configuration": {
                "build_type": "Release",
                "unity_build": False,
                "precompiled_headers": False,
                "build_jobs": args.build_jobs,
            },
        },
        "workspace": str(workspace),
        "steps": [],
        "limitations": [
            "Static recompilation coverage is experimental and does not imply every PS2 title will run correctly.",
            "Disc runtime compatibility depends on the current EE/IOP/GS implementations and title-specific behavior.",
        ],
    }
    _write_json(manifest_path, manifest)
    try:
        requested_output = _normalize_new_destination(Path(args.out)) if args.out else None
        if requested_output is not None:
            _ensure_output_outside_sources(requested_output, root)
        inspector = _find_tool(args.inspector, "PS2NATIVE_INSPECTOR", ("ps2iso-inspect",), root)
        analyzer = _find_tool(args.analyzer, "PS2NATIVE_ANALYZER", ("ps2_analyzer",), root)
        recompiler = _find_tool(args.recompiler, "PS2NATIVE_RECOMPILER", ("ps2_recomp",), root)
        manifest["tools"] = {
            "inspector": str(inspector), "analyzer": str(analyzer), "recompiler": str(recompiler)
        }
        manifest["build_inputs"]["tools"] = {
            name: {"path": str(path), "size_bytes": path.stat().st_size, "sha256": sha256_file(path)}
            for name, path in (("inspector", inspector), ("analyzer", analyzer), ("recompiler", recompiler))
        }

        _step(manifest, "inspect_iso", "started")
        raw, report = _load_inspector_json(inspector, iso, logs / "inspect.log", args.timeout)
        if report.image_sha256 != image_hash:
            raise PipelineError(
                f"inspector image SHA-256 mismatch: reported {report.image_sha256}, actual {image_hash}"
            )
        manifest["iso_inspection"] = raw
        inventory_items: list[Any] = []
        inventory = raw.get("elf_inventory")
        if isinstance(inventory, dict) and inventory.get("schema_version") == 1 and isinstance(inventory.get("items"), list):
            inventory_items = inventory["items"]
            manifest["executable_inventory"] = {
                "status": "available",
                "schema_version": 1,
                "unique_elf_contents": len(inventory_items),
                "secondary_mips_exec_candidates": sum(
                    1 for item in inventory_items
                    if isinstance(item, dict) and item.get("role") in {
                        "secondary_mips_exec_candidate", "secondary_ee_exec_candidate"
                    }
                ),
                "secondary_ee_recomp_candidates": sum(
                    1 for item in inventory_items
                    if isinstance(item, dict) and item.get("role") == "secondary_ee_exec_candidate"
                ),
                "items": inventory_items,
            }
        else:
            manifest["executable_inventory"] = {
                "status": "unavailable",
                "reason": "The configured ISO inspector did not provide ELF inventory schema version 1.",
                "items": [],
            }
        manifest["boot_elf"] = {
            "iso_path": report.boot_elf_path,
            "sha256": report.boot_elf_sha256,
        }
        _step(manifest, "inspect_iso", "complete")
        _write_json(manifest_path, manifest)

        disc = workspace / "disc"
        _step(manifest, "extract_iso", "started")
        _run([str(inspector), "extract", str(iso), str(disc)], logs / "extract.log", workspace, args.timeout)
        if not disc.is_dir():
            raise PipelineError(f"ISO extractor returned success but did not create the expected directory: {disc}")
        _walk_extraction(disc)
        boot_relative = iso_path_to_extracted_path(report.boot_elf_path)
        extracted_elf = disc / boot_relative
        try:
            extracted_elf.resolve().relative_to(disc.resolve())
        except ValueError as exc:
            raise PipelineError(f"boot ELF path escapes the extracted disc tree: {report.boot_elf_path}") from exc
        if extracted_elf.is_symlink() or not extracted_elf.is_file():
            raise PipelineError(
                f"boot ELF was not extracted as a regular file: {extracted_elf} "
                f"(inspector path {report.boot_elf_path!r})"
            )
        extracted_hash = sha256_file(extracted_elf)
        if extracted_hash != report.boot_elf_sha256:
            raise PipelineError(
                f"extracted boot ELF SHA-256 mismatch: expected {report.boot_elf_sha256}, got {extracted_hash}"
            )
        staged_elf = disc / "boot.elf"
        if extracted_elf.resolve() != staged_elf.resolve():
            if staged_elf.exists():
                if not staged_elf.is_file() or sha256_file(staged_elf) != report.boot_elf_sha256:
                    raise PipelineError("ISO already contains a different root-level boot.elf; cannot stage the Android boot alias")
            else:
                shutil.copyfile(extracted_elf, staged_elf)
        manifest["boot_elf"].update({
            "extracted_path": str(extracted_elf),
            "analyzer_input": str(staged_elf),
            "app_private_asset_path": "game/boot.elf",
            "sha256_verified": True,
        })
        _step(manifest, "extract_iso", "complete")

        analysis_dir = workspace / "analysis"
        analysis_dir.mkdir(parents=True, exist_ok=True)
        config = analysis_dir / "ps2native.toml"
        _step(manifest, "analyze_boot_elf", "started")
        _run(
            [str(analyzer), str(staged_elf), str(config)],
            logs / "analyzer.log", workspace, args.timeout,
        )
        if not config.is_file():
            raise PipelineError(f"ELF analyzer returned success without creating TOML config: {config}")
        _step(manifest, "analyze_boot_elf", "complete")
        manifest["build_inputs"]["analysis_config"] = {
            "path": str(config.relative_to(workspace)),
            "size_bytes": config.stat().st_size,
            "sha256": sha256_file(config),
        }

        boot_module_keys = _boot_module_keys(
            report.boot_elf_path,
            report.boot_elf_sha256,
            inventory_items if isinstance(inventory_items, list) else [],
        )
        boot_symbol_prefix = f"ps2boot_{report.boot_elf_sha256[:24].lower()}_"
        _configure_module_recompiler(config, boot_module_keys, boot_symbol_prefix, emit_dense_function_table=True)
        manifest["boot_elf"].update({
            "module_keys": boot_module_keys,
            "module_registration_symbol": f"{boot_symbol_prefix}register_compiled_module",
        })

        _step(manifest, "recompile_boot_elf", "started")
        boot_recompiler_output = _run(
            [str(recompiler), str(config)], logs / "recompiler.log", workspace, args.timeout
        )
        boot_report = _parse_recompiler_summary(boot_recompiler_output)
        generated = analysis_dir / "output"
        if not generated.is_dir():
            raise PipelineError(f"recompiler returned success without generated output directory: {generated}")
        function_coverage_report = generated / "function_coverage.csv"
        manifest["boot_elf"]["recompiler_report"] = boot_report
        manifest["boot_elf"]["static_translation_coverage"] = _translation_coverage(boot_report)
        manifest["boot_elf"]["function_coverage_report"] = (
            str(function_coverage_report) if function_coverage_report.is_file() else None
        )
        boot_metadata = raw.get("boot", {}).get("elf", {}) if isinstance(raw.get("boot"), dict) else {}
        boot_load_segments = boot_metadata.get("load_segments") if isinstance(boot_metadata, dict) else None
        boot_address_ledger = _write_address_range_ledger(
            boot_load_segments,
            function_coverage_report if function_coverage_report.is_file() else None,
            generated / "address_coverage.csv",
        )
        manifest["boot_elf"]["address_range_ledger"] = boot_address_ledger
        registration = generated / "register_functions.cpp"
        if not registration.is_file():
            raise PipelineError(
                "recompiler output is missing register_functions.cpp; the generated runner cannot replace its placeholder"
            )

        modules = _compile_secondary_ee_modules(
            workspace,
            logs,
            disc,
            generated,
            inventory_items if isinstance(inventory_items, list) else [],
            analyzer,
            recompiler,
            args.timeout,
            manifest,
            f"{boot_symbol_prefix}register_compiled_module",
        )
        compiled_modules = [module for module in modules if module.get("status") == "compiled"]
        module_coverages = [
            {
                "id": module["id"],
                "status": module.get("static_translation_coverage", "unknown"),
                "report": module.get("recompiler_report", {}),
                "function_coverage_report": module.get("function_coverage_report"),
                "address_range_ledger": module.get("address_range_ledger"),
            }
            for module in compiled_modules
        ]
        failed_modules = [
            {"id": module.get("id"), "iso_paths": module.get("iso_paths", []), "error": module.get("error")}
            for module in modules
            if module.get("status") != "compiled"
        ]
        address_ledgers = [boot_address_ledger]
        address_ledgers.extend(
            module.get("address_range_ledger", {})
            for module in compiled_modules
        )
        ledger_statuses = {ledger.get("status") for ledger in address_ledgers}
        if failed_modules or ledger_statuses & {
            "unknown", "no_executable_segments", "no_file_backed_executable_bytes"
        }:
            address_ledger_status = "unknown"
        elif "unattributed_or_ambiguous_ranges" in ledger_statuses:
            address_ledger_status = "unattributed_or_ambiguous_ranges"
        else:
            address_ledger_status = "function_ranges_cover_file_backed_executable_bytes"
        unit_statuses = [manifest["boot_elf"]["static_translation_coverage"]]
        unit_statuses.extend(module["status"] for module in module_coverages)
        if failed_modules or "partial" in unit_statuses:
            translation_status = "known_gaps"
        elif "unknown" in unit_statuses:
            translation_status = "unknown"
        elif "runtime_stubs_present" in unit_statuses:
            translation_status = "runtime_stubs_present"
        else:
            translation_status = "no_reported_instruction_gaps"
        manifest["native_translation_assessment"] = {
            "status": translation_status,
            "scope": "boot ELF and heuristic MIPS III ET_EXEC candidates; analyzer-discovered functions and recompiler-reported instruction gaps",
            "boot_elf": {
                "status": manifest["boot_elf"]["static_translation_coverage"],
                "report": boot_report,
                "function_coverage_report": manifest["boot_elf"]["function_coverage_report"],
                "address_range_ledger": boot_address_ledger,
            },
            "secondary_ee_modules": module_coverages,
            "failed_secondary_ee_modules": failed_modules,
            "address_range_ledger": {
                "status": address_ledger_status,
                "boot_elf": boot_address_ledger,
                "secondary_ee_modules": [
                    module.get("address_range_ledger") for module in compiled_modules
                ],
                "failed_secondary_module_count": len(failed_modules),
                "interpretation": "Function-range attribution only; it does not distinguish code from embedded data or prove translated semantics.",
            },
            "gameplay_compatibility": "unverified",
        }
        normalized_cpp = normalize_generated_cpp_compatibility(generated)
        generated_cpp = sorted(path for path in generated.rglob("*.cpp") if path.is_file())
        generated_headers = sorted(
            path for path in generated.rglob("*")
            if path.is_file() and path.suffix.lower() in (".h", ".hpp")
        )
        source_manifest = {
            "schema_version": 1,
            "generated_directory": str(generated),
            "boot_module_keys": boot_module_keys,
            "boot_registration_symbol": f"{boot_symbol_prefix}register_compiled_module",
            "modules": [
                {
                    "id": module["id"],
                    "sha256": module["sha256"],
                    "module_keys": module["module_keys"],
                    "directory": str(Path(module["generated_directory"]).relative_to(generated)),
                    "registration_symbol": f"{module['symbol_prefix']}register_compiled_module",
                }
                for module in compiled_modules
            ],
            "sources": [str(path.relative_to(generated)) for path in generated_cpp],
            "headers": [str(path.relative_to(generated)) for path in generated_headers],
            "register_functions_replaces": "ps2xRuntime/src/runner/register_functions.cpp",
            "module_registration_source": "register_modules.cpp",
        }
        source_manifest_path = workspace / "source_manifest.json"
        _write_json(source_manifest_path, source_manifest)
        manifest["build_inputs"]["generated_sources"] = {
            "source_manifest_sha256": sha256_file(source_manifest_path),
            "source_count": len(generated_cpp),
            "header_count": len(generated_headers),
        }
        manifest["generated"] = {
            "directory": str(generated),
            "cpp_count": len(generated_cpp),
            "header_count": len(generated_headers),
            "secondary_ee_module_count": len(compiled_modules),
            "source_manifest": str(source_manifest_path),
            "compatibility_normalization": {
                "status": "applied" if normalized_cpp else "not_needed",
                "rule": "add a local std::isnan using-declaration where generated code expands COP1 unordered-comparison macros",
                "files": normalized_cpp,
            },
        }
        _step(manifest, "recompile_boot_elf", "complete")
        _write_json(manifest_path, manifest)

        output = workspace / "package"
        if args.target == "android":
            try:
                _build_android_package(
                    root, workspace, generated, disc, output, args.gradle, args.timeout, manifest
                )
            except AndroidPrerequisitesMissing as exc:
                manifest["status"] = "blocked"
                manifest["android"].update({"status": "blocked", "blockers": exc.reasons, "apk": None})
                manifest["message"] = "Android project and game assets are staged, but APK build prerequisites are missing."
                _step(manifest, "android_prerequisites", "blocked", str(exc))
                _write_json(manifest_path, manifest)
                return {
                    "status": "blocked", "workspace": str(workspace), "message": manifest["message"],
                    "gameplay_compatibility": "unverified", "support_tier": "experimental",
                    "native_translation_status": translation_status,
                    "address_coverage_status": address_ledger_status,
                }
        else:
            _configure_and_package(root, workspace, generated, disc, output, args.cmake, args.build_jobs, args.timeout, manifest)
        manifest["artifact_files_schema_version"] = 1
        manifest["mutable_outputs"] = (
            ["ps2_log.txt", "game/ps2_log.txt"] if args.target == "desktop" else []
        )
        manifest["artifact_files"] = attest_package_files(output)
        manifest["status"] = "complete"
        manifest["artifact"] = str(requested_output or output)
        target_message = (
            "Android APK built and package assembled."
            if args.target == "android"
            else "Host desktop package built."
        )
        if translation_status == "known_gaps":
            manifest["message"] = target_message + " Static translation gaps are recorded in the manifest; gameplay compatibility is unverified."
        elif translation_status == "unknown":
            manifest["message"] = target_message + " The static translation report is incomplete; gameplay compatibility is unverified."
        elif translation_status == "runtime_stubs_present":
            manifest["message"] = target_message + " Runtime stubs are present; gameplay compatibility is unverified."
        else:
            manifest["message"] = target_message + " No reported instruction gaps; gameplay compatibility remains unverified."
        if address_ledger_status == "unknown":
            manifest["message"] += " Executable address attribution is incomplete; see address_range_ledger in the manifest."
        elif address_ledger_status == "unattributed_or_ambiguous_ranges":
            manifest["message"] += " Some executable file ranges lack unique function attribution; they may be data or padding."
        _write_json(manifest_path, manifest)
        published = publish_package(output, requested_output) if requested_output else output
        package_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if "desktop" in package_manifest:
            package_launcher = Path(package_manifest["desktop"]["launcher"]).name
            package_manifest["desktop"].update({
                "runner": f"bin/{Path(package_manifest['desktop']['runner']).name}",
                "launcher": package_launcher,
                "game_assets": "game",
                "cmake_project": "(build workspace; see workspace manifest)",
                "cmake_build_dir": "(build workspace; see workspace manifest)",
            })
        if "android" in package_manifest:
            package_manifest["android"].update({
                "project": "(build workspace; see workspace manifest)",
                "assets_game": "assets/game/",
                "apk": Path(package_manifest["android"]["apk"]).name,
                "generated_cpp": "(build workspace; see workspace manifest)",
            })
        _write_json(published / "manifest.json", package_manifest)
        return {
            "status": "complete", "workspace": str(workspace), "artifact": str(published),
            "message": manifest["message"], "gameplay_compatibility": "unverified",
            "support_tier": "experimental", "native_translation_status": translation_status,
            "address_coverage_status": address_ledger_status,
        }
    except Exception as exc:
        if isinstance(exc, (KeyboardInterrupt, SystemExit)):
            raise
        manifest["status"] = "failed"
        manifest["error"] = str(exc)
        _write_json(manifest_path, manifest)
        if isinstance(exc, PipelineError):
            raise PipelineError(f"{exc}\nManifest: {manifest_path}") from exc
        if isinstance(exc, (ValueError, OSError)):
            raise type(exc)(f"{exc}\nManifest: {manifest_path}") from exc
        raise PipelineError(f"unexpected pipeline error: {exc}\nManifest: {manifest_path}") from exc
