"""Local conversion and verified launch; no remote service or live compiler."""
from __future__ import annotations

import json
import os
from pathlib import Path, PurePosixPath
from typing import Any

from .pipeline import PipelineError, _run, _write_json, run_build, sha256_file, verify_package


def run_convert(args: Any) -> dict[str, Any]:
    launch = args.run if args.run is not None else args.target == "desktop"
    if launch and args.target != "desktop":
        raise PipelineError("--run requires the desktop target; install the Android APK separately")
    if args.run_timeout is not None and (type(args.run_timeout) is not int or args.run_timeout < 1):
        raise PipelineError("--run-timeout must be a positive integer")
    if type(args.adapt_rounds) is not int or not 1 <= args.adapt_rounds <= 64:
        raise PipelineError("--adapt-rounds must be in 1..64")
    if type(args.probe_timeout) is not int or not 1 <= args.probe_timeout <= 3600:
        raise PipelineError("--probe-timeout must be in 1..3600")
    result = run_build(args)
    result["run_status"] = "not_started"
    if result["status"] != "complete":
        return result

    package = Path(result["artifact"]).resolve()
    workspace = Path(result["workspace"])
    receipt = {"schema_version": 1, "build_status": "complete", "package": str(package),
               "run_status": "not_requested", "menu_approved": False,
               "gameplay_approved": False, "native_execution_qualified": False}
    try:
        integrity = verify_package(package)
        receipt["integrity"] = integrity
        if integrity["status"] != "verified":
            raise PipelineError("converted package failed integrity verification; refusing to launch")
        if launch:
            iso = Path(args.iso).expanduser().resolve()
            manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
            source = manifest.get("source")
            if not isinstance(source, dict) or source.get("iso_sha256") != sha256_file(iso):
                raise PipelineError("selected ISO differs from the converted package")
            desktop = manifest.get("desktop")
            if not isinstance(desktop, dict) or not isinstance(desktop.get("runner"), str):
                raise PipelineError("converted package has no desktop runner")
            relative = PurePosixPath(desktop["runner"])
            if relative.is_absolute() or ".." in relative.parts or "\\" in desktop["runner"]:
                raise PipelineError("converted package has an unsafe runner path")
            runner = (package / relative).resolve()
            boot = package / "game/boot.elf"
            if not runner.is_relative_to(package) or not runner.is_file() or not boot.is_file():
                raise PipelineError("converted desktop runner or boot ELF is missing")
            command = [str(runner), str(boot), str(iso)]
            env = os.environ.copy()
            env.pop("PS2X_NATIVE_OVERLAY_DRIVER", None)
            receipt.update(run_status="running", command=command,
                           live_overlay_compiler_enabled=False)
            _write_json(workspace / "conversion.json", receipt)
            _run(command, workspace / "logs/converted-runner.log", package,
                 args.run_timeout, env=env)
            receipt["run_status"] = "exited"
        result["run_status"] = receipt["run_status"]
        result["conversion_receipt"] = str(workspace / "conversion.json")
        return result
    except (PipelineError, OSError, ValueError) as error:
        receipt.update(run_status="failed", error=str(error))
        raise
    finally:
        _write_json(workspace / "conversion.json", receipt)
