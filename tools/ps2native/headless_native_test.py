#!/usr/bin/env python3
"""Run and inspect the native desktop runner on an isolated Xvfb display.

Input commands connect only to this tool's recorded virtual display. They do
not use the desktop display or change the user's default audio output.
"""
from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time


KEYS = {"x", "space", "Return", "Up", "Down", "Left", "Right", "w", "a", "s", "d",
        "q", "e", "r", "f", "z", "c", "v", "Escape"}


def environment(state_path: Path) -> tuple[dict, dict]:
    state = json.loads(state_path.read_text())
    if not isinstance(state, dict):
        raise RuntimeError("invalid headless session record")
    display = state.get("display")
    if state.get("version") != 1 or not isinstance(display, str) or \
       not display.startswith(":") or int(display[1:]) < 90:
        raise RuntimeError("session is not an isolated test display")
    pid = state.get("runner_pid")
    if type(pid) is not int or pid < 1 or not isinstance(state.get("xauthority"), str):
        raise RuntimeError("invalid headless session owner or authorization")
    process_env = Path(f"/proc/{pid}/environ").read_bytes().split(b"\0")
    if f"DISPLAY={display}".encode() not in process_env or \
       f"PS2X_HEADLESS_STATE={state_path.resolve()}".encode() not in process_env:
        raise RuntimeError("recorded runner no longer owns this test session")
    if not Path(state["xauthority"]).is_file():
        raise RuntimeError("virtual display authorization has expired")
    if "xvfb_pid" in state:
        server_pid = state["xvfb_pid"]
        if type(server_pid) is not int or server_pid < 1 or \
           os.getpgid(server_pid) != os.getpgid(pid) or \
           int(Path(f"/tmp/.X{display[1:]}-lock").read_text()) != server_pid:
            raise RuntimeError("recorded virtual server is not owned by this session")
    env = os.environ.copy()
    env.update(DISPLAY=display, XAUTHORITY=state["xauthority"])
    return state, env


def window(env: dict) -> str:
    found = subprocess.check_output(
        ["xdotool", "search", "--name", "^PS2-Recomp"], env=env, text=True).split()
    if len(found) != 1:
        raise RuntimeError(f"expected one game window on the virtual display, found {len(found)}")
    return found[0]


def child(args: argparse.Namespace) -> None:
    display = os.environ["DISPLAY"]
    if not display.startswith(":") or int(display[1:]) < 90:
        raise RuntimeError("Xvfb did not provide an isolated display")
    # Some xvfb-run versions still execute the client after Xvfb failed. A
    # display string or a new xauth file alone does not prove server ownership.
    server_pid = int(Path(f"/tmp/.X{display[1:]}-lock").read_text())
    if os.getpgid(server_pid) != os.getpgrp() or \
       Path(f"/proc/{server_pid}/comm").read_text().strip() != "Xvfb":
        raise RuntimeError("Xvfb failed or selected another session's virtual server")
    state = args.state.resolve()
    package, iso = args.package.resolve(), args.iso.resolve()
    runner = args.runner.resolve() if args.runner else package / "bin/ps2EntryRunner"
    record = {"version": 1, "display": display, "xauthority": os.environ["XAUTHORITY"],
              "runner_pid": os.getpid(), "package": str(package),
              "iso": str(iso), "runner": str(runner), "xvfb_pid": server_pid,
              "created_unix": time.time()}
    os.environ["PS2X_HEADLESS_STATE"] = str(state)
    os.environ["PS2X_FUNCTION_TRACE"] = "0"
    os.environ["PS2X_TRACE_SIF_DMA"] = "0"
    if args.disable_overlay_driver:
        os.environ.pop("PS2X_NATIVE_OVERLAY_DRIVER", None)
    else:
        os.environ["PS2X_NATIVE_OVERLAY_DRIVER"] = str(Path(__file__).with_name("native_overlay_driver.py"))
    record["overlay_driver_disabled"] = args.disable_overlay_driver
    # Captures are requested explicitly in separate correctness runs. Reusing a
    # desktop diagnostic directory adds unnecessary work to timing runs.
    if args.capture_scene:
        os.environ["PS2X_CAPTURE_SCENE"] = str(args.capture_scene.resolve())
        record["capture_scene"] = os.environ["PS2X_CAPTURE_SCENE"]
    else:
        os.environ.pop("PS2X_CAPTURE_SCENE", None)
    temporary = state.with_suffix(".tmp")
    temporary.write_text(json.dumps(record, indent=2) + "\n")
    temporary.chmod(0o600)
    temporary.replace(state)
    os.chdir(package.parent)
    os.execv(str(runner), [str(runner), str(package / "game/boot.elf"), str(iso)])


def launch(args: argparse.Namespace) -> None:
    for executable in ("xvfb-run", "Xvfb", "xdotool", "import"):
        if not shutil.which(executable):
            raise RuntimeError(f"required executable is unavailable: {executable}")
    package, iso, state, log = (p.resolve() for p in (args.package, args.iso, args.state, args.log))
    runner = args.runner.resolve() if args.runner else package / "bin/ps2EntryRunner"
    if not runner.is_file() or not (package / "game/boot.elf").is_file():
        raise RuntimeError("native package is incomplete")
    if args.capture_scene:
        if not args.runner:
            raise RuntimeError("scene capture requires an explicitly selected laboratory runner")
        args.capture_scene.resolve().mkdir(parents=True, exist_ok=True)
    if not iso.is_file():
        raise RuntimeError("ISO is unavailable")
    if state.exists():
        previous = json.loads(state.read_text())
        previous_pid = previous.get("runner_pid", 0)
        if previous_pid and Path(f"/proc/{previous_pid}").exists():
            raise RuntimeError("a runner already owns this session; use a new state path")
    state.parent.mkdir(parents=True, exist_ok=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    module = None
    process = None
    stopped = False
    sink = f"ps2native_headless_{os.getpid()}"
    if shutil.which("pactl"):
        result = subprocess.run(["pactl", "load-module", "module-null-sink", f"sink_name={sink}"],
                                text=True, capture_output=True)
        if result.returncode == 0:
            module = result.stdout.strip()
            env["PULSE_SINK"] = sink
    if module is None:
        raise RuntimeError("cannot create the isolated silent audio sink")
    # The installed T2/Debian wrapper computes a free number when it parses -a;
    # -n must precede it. Save server errors and verify ownership in the child.
    command = ["xvfb-run", "-n", "90", "-a", "-e", str(log.with_suffix(".xvfb.log")), "-s",
               "-screen 0 1024x768x24 -nolisten tcp +extension GLX",
               sys.executable, str(Path(__file__).resolve()), "_child", "--package", str(package),
               "--iso", str(iso), "--state", str(state)]
    if args.runner:
        command += ["--runner", str(runner)]
    if args.disable_overlay_driver:
        command += ["--disable-overlay-driver"]
    if args.capture_scene:
        command += ["--capture-scene", str(args.capture_scene.resolve())]

    def stop(_signum, _frame):
        nonlocal stopped
        stopped = True
        if process is not None and process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass

    try:
        signal.signal(signal.SIGTERM, stop)
        signal.signal(signal.SIGINT, stop)
        with log.open("w") as output:
            process = subprocess.Popen(command, env=env, stdout=output, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            print(json.dumps({"status": "starting", "state": str(state), "log": str(log),
                              "launcher_pid": os.getpid()}), flush=True)
            deadline = time.monotonic() + 30
            while not stopped and process.poll() is None:
                try:
                    record, test_env = environment(state)
                    game = window(test_env)
                    print(json.dumps({"status": "ready", "display": record["display"],
                                      "runner_pid": record["runner_pid"], "window": game}), flush=True)
                    break
                except (OSError, ValueError, RuntimeError, subprocess.SubprocessError):
                    if time.monotonic() >= deadline:
                        raise RuntimeError(f"virtual game window was not ready after 30 seconds; see {log}")
                    time.sleep(0.1)
            code = process.wait()
        if code and not stopped:
            raise RuntimeError(f"headless runner exited with status {code}; see {log}")
    finally:
        try:
            if process is not None and process.poll() is None:
                stop(0, None)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    process.wait(timeout=10)
        finally:
            subprocess.run(["pactl", "unload-module", module], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("launch", "_child"):
        command = commands.add_parser(name)
        command.add_argument("--package", required=True, type=Path)
        command.add_argument("--iso", required=True, type=Path)
        command.add_argument("--state", required=True, type=Path)
        command.add_argument("--runner", type=Path,
                             help="Explicit laboratory runner; package game data remains local")
        command.add_argument("--capture-scene", type=Path,
                             help="Opt-in scene request directory for a laboratory runner")
        command.add_argument("--disable-overlay-driver", action="store_true",
                             help="Remove the diagnostic EE runtime compiler driver for AOT runs")
        if name == "launch":
            command.add_argument("--log", required=True, type=Path)
    screenshot = commands.add_parser("screenshot")
    screenshot.add_argument("--state", required=True, type=Path)
    screenshot.add_argument("--output", required=True, type=Path)
    press = commands.add_parser("press")
    press.add_argument("--state", required=True, type=Path)
    press.add_argument("key", choices=sorted(KEYS))
    press.add_argument("--hold", type=float, default=1.0)
    args = parser.parse_args()
    if args.command == "launch":
        launch(args)
    elif args.command == "_child":
        child(args)
    else:
        state, env = environment(args.state)
        game = window(env)
        if args.command == "screenshot":
            output = args.output.resolve()
            output.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["import", "-window", game, str(output)], env=env, check=True)
            print(json.dumps({"display": state["display"], "screenshot": str(output)}))
        else:
            if not math.isfinite(args.hold) or not 0 < args.hold <= 60:
                raise RuntimeError("key hold must be finite and within 0..60 seconds")
            subprocess.run(["xdotool", "windowfocus", "--sync", game], env=env, check=True)
            try:
                subprocess.run(["xdotool", "keydown", args.key], env=env, check=True)
                time.sleep(args.hold)
            finally:
                subprocess.run(["xdotool", "keyup", args.key], env=env, check=True)
            print(json.dumps({"display": state["display"], "key": args.key, "hold_seconds": args.hold}))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"[headless:error] {error}", file=sys.stderr)
        sys.exit(1)
