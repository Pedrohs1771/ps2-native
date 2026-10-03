"""Relink a laboratory runner from a receipt and a small object delta."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time
from typing import Sequence


class RelinkError(RuntimeError):
    """An invalid baseline, delta, or link command prevented an offline relink."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def rewrite_link_argv(
    link_argv: Sequence[str],
    base_archive: Path,
    output_archive: Path,
    output_runner: Path,
    dependency_file: Path,
) -> list[str]:
    """Copy a receipt's link command while redirecting only its output paths."""
    argv = list(link_argv)
    archive_indexes = [
        index for index, value in enumerate(argv)
        if value == str(base_archive) or value == base_archive.name
    ]
    if len(archive_indexes) != 1:
        raise RelinkError("link command must contain exactly one base runtime archive")

    output_indexes = [index for index, value in enumerate(argv[:-1]) if value == "-o"]
    if len(output_indexes) != 1:
        raise RelinkError("link command must contain exactly one -o output")

    dependency_indexes = [
        index for index, value in enumerate(argv)
        if value.startswith("-Wl,--dependency-file=")
    ]
    if len(dependency_indexes) > 1:
        raise RelinkError("link command has ambiguous dependency-file outputs")

    argv[archive_indexes[0]] = str(output_archive)
    argv[output_indexes[0] + 1] = str(output_runner)
    if dependency_indexes:
        argv[dependency_indexes[0]] = f"-Wl,--dependency-file={dependency_file}"
    return argv


def _write_receipt(path: Path, receipt: dict) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def _read_receipt(path: Path) -> tuple[dict, Path]:
    resolved = path.expanduser().resolve(strict=True)
    if not resolved.is_file() or resolved.stat().st_size > 8 * 1024 * 1024:
        raise RelinkError("base receipt is unavailable or exceeds 8 MiB")
    try:
        value = json.loads(resolved.read_text(encoding="utf-8"))
    except (UnicodeError, json.JSONDecodeError) as error:
        raise RelinkError(f"base receipt is not valid JSON: {error}") from error
    if not isinstance(value, dict):
        raise RelinkError("base receipt must be a JSON object")
    return value, resolved


def _verified_input(receipt: dict, field: str, hash_field: str) -> tuple[Path, str]:
    raw_path = receipt.get(field)
    expected = receipt.get(hash_field)
    if not isinstance(raw_path, str) or not isinstance(expected, str) or len(expected) != 64:
        raise RelinkError(f"base receipt lacks {field} and {hash_field}")
    path = Path(raw_path).expanduser().resolve(strict=True)
    if not path.is_file() or sha256_file(path) != expected:
        raise RelinkError(f"base receipt hash mismatch for {path}")
    return path, expected


def _parse_replacements(values: Sequence[str]) -> dict[str, Path]:
    replacements: dict[str, Path] = {}
    for value in values:
        name, separator, raw_path = value.partition("=")
        if not separator or not name.endswith(".o") or Path(name).name != name:
            raise RelinkError("each replacement must be OBJECT.o=/path/to/object.o")
        if name in replacements:
            raise RelinkError(f"replacement member is repeated: {name}")
        path = Path(raw_path).expanduser().resolve(strict=True)
        if not path.is_file() or path.name != name:
            raise RelinkError(f"replacement path must be an existing {name}: {path}")
        replacements[name] = path
    if not replacements:
        raise RelinkError("at least one replacement object is required")
    return replacements


def relink(
    base_receipt_path: Path,
    link_cwd: Path,
    output_dir: Path,
    replacement_specs: Sequence[str],
    timeout: float = 180.0,
) -> dict:
    if timeout <= 0 or timeout > 3600:
        raise RelinkError("timeout must be in 0..3600 seconds")
    receipt, base_receipt = _read_receipt(base_receipt_path)
    base_archive, archive_hash = _verified_input(
        receipt, "runtime_archive", "runtime_archive_sha256"
    )
    base_runner, runner_hash = _verified_input(receipt, "runner", "runner_sha256")
    replacements = _parse_replacements(replacement_specs)
    cwd = link_cwd.expanduser().resolve(strict=True)
    if not cwd.is_dir():
        raise RelinkError(f"link working directory is unavailable: {cwd}")
    argv = receipt.get("link_argv")
    if not isinstance(argv, list) or not argv or any(not isinstance(item, str) for item in argv):
        raise RelinkError("base receipt has no string-array link_argv")

    output = output_dir.expanduser().absolute()
    output.mkdir(parents=True, exist_ok=False)
    output = output.resolve()
    output_archive = output / "libps2_runtime.a"
    output_runner = output / "ps2EntryRunner"
    dependency_file = output / "link.d"
    receipt_path = output / "relink-receipt.json"
    started = time.monotonic()
    result = {
        "status": "LAB_RELINKING",
        "base_receipt": str(base_receipt),
        "base_receipt_sha256": sha256_file(base_receipt),
        "base_runner": str(base_runner),
        "base_runner_sha256": runner_hash,
        "base_runtime_archive": str(base_archive),
        "base_runtime_archive_sha256": archive_hash,
        "link_cwd": str(cwd),
        "replacement_objects": {
            name: {"path": str(path), "sha256": sha256_file(path)}
            for name, path in replacements.items()
        },
        "strict_approval": False,
        "closure_proved": False,
        "menu_approved": False,
        "gameplay_approved": False,
    }
    _write_receipt(receipt_path, result)

    try:
        ar = shutil.which("ar")
        ranlib = shutil.which("ranlib")
        if not ar or not ranlib:
            raise RelinkError("GNU ar and ranlib are required")
        members_result = subprocess.run(
            [ar, "t", str(base_archive)], check=True, capture_output=True, text=True, timeout=30
        )
        members = members_result.stdout.splitlines()
        for name in replacements:
            if members.count(name) != 1:
                raise RelinkError(f"base runtime archive must contain exactly one {name}")

        shutil.copy2(base_archive, output_archive)
        subprocess.run(
            [ar, "r", str(output_archive), *(str(path) for path in replacements.values())],
            check=True, capture_output=True, text=True, timeout=60, cwd=output,
        )
        subprocess.run(
            [ranlib, str(output_archive)], check=True, capture_output=True, text=True,
            timeout=30, cwd=output,
        )
        member_hashes: dict[str, str] = {}
        for name in replacements:
            member = subprocess.run(
                [ar, "p", str(output_archive), name], check=True,
                capture_output=True, timeout=30,
            ).stdout
            member_hashes[name] = hashlib.sha256(member).hexdigest()
            if member_hashes[name] != sha256_file(replacements[name]):
                raise RelinkError(f"runtime archive did not take replacement member {name}")

        link_argv = rewrite_link_argv(
            argv, base_archive, output_archive, output_runner, dependency_file
        )
        for argument in link_argv:
            if argument.startswith("@"):
                response = Path(argument[1:])
                if not response.is_absolute():
                    response = cwd / response
                if not response.is_file():
                    raise RelinkError(f"link response file is missing: {response}")
        with (output / "link.log").open("wb") as log:
            linked = subprocess.run(
                link_argv, cwd=cwd, stdout=log, stderr=subprocess.STDOUT,
                check=False, timeout=timeout,
            )
        if linked.returncode != 0 or not output_runner.is_file():
            raise RelinkError(
                f"runner link failed with status {linked.returncode}; inspect {output / 'link.log'}"
            )

        result.update(
            status="LAB_BUILT_UNVERIFIED",
            runtime_archive=str(output_archive),
            runtime_archive_sha256=sha256_file(output_archive),
            archive_member_sha256s=member_hashes,
            runner=str(output_runner),
            runner_sha256=sha256_file(output_runner),
            link_argv=link_argv,
            seconds=time.monotonic() - started,
        )
    except (OSError, subprocess.SubprocessError, RelinkError) as error:
        result.update(
            status="LAB_RELINK_FAILED",
            error_type=type(error).__name__,
            error=str(error),
            seconds=time.monotonic() - started,
        )
        _write_receipt(receipt_path, result)
        raise

    _write_receipt(receipt_path, result)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-receipt", required=True, type=Path)
    parser.add_argument("--link-cwd", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--replace", action="append", required=True,
                        help="archive member mapping in the form NAME.o=/path/NAME.o")
    parser.add_argument("--timeout", type=float, default=180.0)
    args = parser.parse_args()
    try:
        result = relink(args.base_receipt, args.link_cwd, args.output_dir,
                        args.replace, args.timeout)
    except (OSError, subprocess.SubprocessError, RelinkError) as error:
        print(f"[offline-relink:error] {error}", file=sys.stderr)
        raise SystemExit(1) from error
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
