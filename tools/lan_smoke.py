#!/usr/bin/env -S uv run
"""Run a host and guest native game process through a real LAN campaign."""
# /// script
# dependencies = []
# ///

from __future__ import annotations

import argparse
import json
import os
import pathlib
import socket
import subprocess
import sys
import tempfile
import time
from collections.abc import Mapping


def existing_config_path() -> pathlib.Path | None:
    configured = os.environ.get("REBELLION_CONFIG")
    if configured:
        return pathlib.Path(configured).expanduser()
    home = pathlib.Path.home()
    if sys.platform == "darwin":
        return home / "Library" / "Application Support" / "Rebellion" / "config.ini"
    if os.name == "nt":
        appdata = os.environ.get("APPDATA")
        return pathlib.Path(appdata) / "Rebellion" / "config.ini" if appdata else pathlib.Path("config.ini")
    xdg = os.environ.get("XDG_CONFIG_HOME")
    return (pathlib.Path(xdg) if xdg else home / ".config") / "rebellion" / "config.ini"


def write_isolated_config(destination: pathlib.Path, source: pathlib.Path | None, checkpoint_dir: pathlib.Path) -> None:
    lines = source.read_text(encoding="utf-8").splitlines() if source and source.is_file() else []
    kept: list[str] = []
    inserted = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("[") and not inserted:
            kept.append(f"multiplayer_save_dir={checkpoint_dir}")
            kept.append("multiplayer_autosave_seconds=1")
            inserted = True
        if "=" in line and line.split("=", 1)[0].strip().lower() in {
            "multiplayer_save_dir",
            "multiplayer_autosave_seconds",
        }:
            continue
        kept.append(line)
    if not inserted:
        kept.append(f"multiplayer_save_dir={checkpoint_dir}")
        kept.append("multiplayer_autosave_seconds=1")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(kept) + "\n", encoding="utf-8")


def choose_port(requested: int) -> int:
    attempts = 1 if requested else 16
    for _ in range(attempts):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as tcp_probe, \
                    socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_probe:
                tcp_probe.bind(("127.0.0.1", requested))
                candidate = int(tcp_probe.getsockname()[1])
                udp_probe.bind(("127.0.0.1", candidate))
                return candidate
        except OSError:
            if requested:
                raise
    raise RuntimeError("could not find a port available for both TCP and UDP")


def process_environment(role: str, root: pathlib.Path, config_source: pathlib.Path | None, port: int,
                        resume: pathlib.Path | None, host_side: int, battle: bool,
                        endgame: bool, timeout: float) -> dict[str, str]:
    environment = os.environ.copy()
    role_dir = root / role
    checkpoint_dir = role_dir / "checkpoints"
    write_isolated_config(role_dir / "config.ini", config_source, checkpoint_dir)
    environment.update(
        {
            "REBELLION_CONFIG": str(role_dir / "config.ini"),
            "REB_LAN_SMOKE": role,
            "REB_LAN_SMOKE_PORT": str(port),
            "REB_LAN_SMOKE_OUTPUT": str(root / "reports"),
            "REB_LAN_SMOKE_TIMEOUT": str(max(1, int(timeout))),
            "SDL_VIDEODRIVER": "dummy",
            "SDL_AUDIODRIVER": "dummy",
        }
    )
    if role == "host" and resume:
        environment["REB_LAN_SMOKE_RESUME"] = str(resume)
    else:
        environment.pop("REB_LAN_SMOKE_RESUME", None)
    if role == "host":
        environment["REB_LAN_SMOKE_HOST_SIDE"] = str(host_side)
    if battle:
        environment["REB_LAN_SMOKE_BATTLE"] = "1"
    else:
        environment.pop("REB_LAN_SMOKE_BATTLE", None)
    if endgame:
        environment["REB_LAN_SMOKE_ENDGAME"] = "1"
    else:
        environment.pop("REB_LAN_SMOKE_ENDGAME", None)
    return environment


def stop_processes(processes: Mapping[str, subprocess.Popen[str]]) -> None:
    for process in processes.values():
        if process.poll() is None:
            process.terminate()
    deadline = time.monotonic() + 5
    for process in processes.values():
        if process.poll() is None:
            try:
                process.wait(timeout=max(0.1, deadline - time.monotonic()))
            except subprocess.TimeoutExpired:
                process.kill()


def main() -> int:
    parser = argparse.ArgumentParser(description="Run two native processes through a LAN multiplayer smoke test.")
    parser.add_argument("binary", type=pathlib.Path, help="native Rebellion executable to run twice")
    parser.add_argument("--output", type=pathlib.Path, help="report/checkpoint directory (defaults to a new temp dir)")
    parser.add_argument("--port", type=int, default=0, help="TCP/UDP port; zero chooses an available local port")
    parser.add_argument("--timeout", type=float, default=210, help="per-process timeout in seconds")
    parser.add_argument("--resume", type=pathlib.Path, help="host checkpoint to resume before exercising the link")
    parser.add_argument("--host-side", type=int, choices=(1, 2), default=1, help="host faction for a fresh campaign")
    parser.add_argument("--battle", action="store_true", help="exercise the strategic-to-tactical battle vote")
    parser.add_argument("--endgame", action="store_true", help="verify the multiplayer victory/defeat movie transition")
    args = parser.parse_args()

    binary = args.binary.expanduser().resolve()
    if not binary.is_file():
        parser.error(f"binary does not exist: {binary}")
    if not 0 <= args.port <= 65535:
        parser.error("--port must be zero or between 1 and 65535")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    resume = args.resume.expanduser().resolve() if args.resume else None
    if resume and not resume.is_file():
        parser.error(f"resume checkpoint does not exist: {resume}")

    if args.output:
        output = args.output.expanduser().resolve()
        output.mkdir(parents=True, exist_ok=True)
    else:
        output = pathlib.Path(tempfile.mkdtemp(prefix="reb-lan-smoke-"))
    root = output / "runtime"
    root.mkdir(parents=True, exist_ok=True)
    reports = root / "reports"
    reports.mkdir(parents=True, exist_ok=True)
    port = choose_port(args.port)
    config_source = existing_config_path()
    processes: dict[str, subprocess.Popen[str]] = {}
    logs: dict[str, str] = {}
    log_paths = {role: reports / f"{role}.log" for role in ("host", "guest")}
    log_handles: dict[str, object] = {}
    try:
        for role in ("host", "guest"):
            log_handle = log_paths[role].open("w", encoding="utf-8")
            log_handles[role] = log_handle
            processes[role] = subprocess.Popen(
                [str(binary)],
                env=process_environment(role, root, config_source, port, resume, args.host_side, args.battle,
                                        args.endgame, args.timeout),
                stdout=log_handle,
                stderr=subprocess.STDOUT,
                text=True,
            )
        try:
            deadline = time.monotonic() + args.timeout
            for process in processes.values():
                process.wait(timeout=max(0.1, deadline - time.monotonic()))
        except subprocess.TimeoutExpired:
            stop_processes(processes)
            for handle in log_handles.values():
                handle.close()
            logs = {role: path.read_text(encoding="utf-8", errors="replace") for role, path in log_paths.items()}
            print(f"LAN smoke timed out after {args.timeout:g}s", file=sys.stderr)
            print_logs(logs)
            return 1

        for handle in log_handles.values():
            handle.close()
        logs = {role: path.read_text(encoding="utf-8", errors="replace") for role, path in log_paths.items()}
        print_logs(logs)
        if any(process.returncode != 0 for process in processes.values()):
            print("LAN smoke process exited unsuccessfully", file=sys.stderr)
            return 1
        host_report = read_report(reports / "host.json")
        guest_report = read_report(reports / "guest.json")
        validate_reports(host_report, guest_report, resume, args.battle, args.endgame, args.host_side)
        print(f"LAN smoke passed. Reports and checkpoints: {output}")
        return 0
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as error:
        stop_processes(processes)
        for handle in log_handles.values():
            if not handle.closed:
                handle.close()
        logs = {role: path.read_text(encoding="utf-8", errors="replace") for role, path in log_paths.items() if path.exists()}
        print(f"LAN smoke failed: {error}", file=sys.stderr)
        print_logs(logs)
        return 1
    except KeyboardInterrupt:
        stop_processes(processes)
        for handle in log_handles.values():
            if not handle.closed:
                handle.close()
        print("LAN smoke interrupted", file=sys.stderr)
        return 130


def print_logs(logs: Mapping[str, str]) -> None:
    for role, log in logs.items():
        print(f"--- {role} ---")
        print(log, end="" if log.endswith("\n") else "\n")


def read_report(path: pathlib.Path) -> dict[str, object]:
    if not path.is_file():
        raise RuntimeError(f"missing process report: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def validate_reports(host: dict[str, object], guest: dict[str, object], resume: pathlib.Path | None,
                    battle: bool, endgame: bool, host_side: int) -> None:
    if not host.get("success") or not guest.get("success"):
        raise RuntimeError(f"smoke reports indicate failure: host={host!r}, guest={guest!r}")
    if int(host.get("ticks_advanced", 0)) < 200 or int(guest.get("ticks_advanced", 0)) < 200:
        raise RuntimeError("both game processes must observe at least 200 advanced campaign ticks")
    if int(host.get("accepted_orders", 0)) < 1 or not guest.get("guest_order_sent"):
        raise RuntimeError("the host did not accept the guest's test order")
    if not host.get("rename_applied") or not guest.get("rename_applied"):
        raise RuntimeError("the rename order did not reach both players' game views")
    if not guest.get("reconnect_verified"):
        raise RuntimeError("the guest did not reconnect and receive a fresh authoritative snapshot")
    if int(host.get("automatic_checkpoints", 0)) < 1:
        raise RuntimeError("the host did not create an automatic campaign checkpoint")
    if not host.get("manual_checkpoint") or not guest.get("manual_checkpoint"):
        raise RuntimeError("the manual checkpoint was not saved and mirrored to the guest")
    if int(host.get("final_tick", -1)) != int(guest.get("final_tick", -2)):
        raise RuntimeError("host and guest ended on different replicated ticks")
    checkpoint = pathlib.Path(str(host.get("checkpoint", "")))
    if not checkpoint.is_file():
        raise RuntimeError(f"host did not leave a readable checkpoint: {checkpoint}")
    if resume and not host.get("resumed"):
        raise RuntimeError("the host did not resume the requested checkpoint")
    if battle and (not host.get("battle_completed") or not guest.get("battle_vote_submitted")):
        raise RuntimeError("the opposing-fleet battle did not complete with a guest vote")
    if endgame:
        victory_state = 17 if host_side == 1 else 16
        defeat_state = 18 if host_side == 1 else 19
        if not host.get("endgame_completed") or not guest.get("endgame_completed"):
            raise RuntimeError("host victory and guest defeat movie transitions did not both complete")
        if int(host.get("movie_state", -1)) != victory_state or int(guest.get("movie_state", -1)) != defeat_state:
            raise RuntimeError("the host and guest reached unexpected movie states")
        if not host.get("lan_closed") or not guest.get("lan_closed"):
            raise RuntimeError("the LAN session remained active after the end-game transition")


if __name__ == "__main__":
    raise SystemExit(main())
