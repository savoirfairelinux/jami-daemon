#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import pwd
import shlex
import signal
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import IO, Any

ROOT = Path(__file__).resolve().parent
LIB_DIR = ROOT / "lib"
SCENARIO_DIR = ROOT / "scenarios"
DEFAULT_ARTIFACT_ROOT = ROOT / "artifacts"
DEFAULT_STATE_ROOT = Path(os.environ.get("VNET_STATE_ROOT", "/tmp/jami-virtual-networking"))

if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
if str(LIB_DIR) not in sys.path:
    sys.path.insert(0, str(LIB_DIR))

from result_summary import append_jsonl_record, build_summary_files


class ScenarioError(RuntimeError):
    pass


@dataclass(frozen=True)
class CopyOutputSpec:
    source: str
    destination: str
    label: str
    kind: str = "captured-file"


@dataclass(frozen=True)
class StepSpec:
    name: str
    step_type: str
    argv: list[str]
    namespace: str | None = None
    capture: str | None = None
    label: str | None = None
    kind: str = "command-output"
    allow_failure: bool = False
    copy_outputs: list[CopyOutputSpec] = field(default_factory=list)


@dataclass(frozen=True)
class ScenarioSpec:
    name: str
    description: str
    topology: str
    lab: str
    setup: list[str]
    teardown: list[str]
    steps: list[StepSpec]
    notes: list[str]
    fields: dict[str, Any]
    path: Path
    state_file: str | None = None
    actor_namespace: str | None = None
    requires_actor: bool = False
    actor_wait_s: float = 0.0


@dataclass(frozen=True)
class LaunchUser:
    username: str
    uid: int
    gid: int
    home: str
    shell: str
    env: dict[str, str]


@dataclass
class ManagedActor:
    namespace: str
    launch_command: str
    user: LaunchUser
    process: subprocess.Popen[str]
    log_handle: IO[str]
    log_path: Path
    meta_path: Path


class ResultRecorder:
    def __init__(self, *, run_id: str, scenario: str, artifact_root: Path) -> None:
        self.run_id = run_id
        self.scenario = scenario
        self.artifact_root = artifact_root
        self.run_dir = artifact_root / run_id
        self.meta_dir = self.run_dir / ".meta"
        self.captures_dir = self.run_dir / "captures"
        self.events_file = self.run_dir / "events.jsonl"
        self.assertions_file = self.meta_dir / "assertions.jsonl"
        self.captures_file = self.meta_dir / "captures.jsonl"
        self.metrics_file = self.meta_dir / "metrics.jsonl"
        self.notes_file = self.meta_dir / "notes.jsonl"
        self.fields_file = self.meta_dir / "fields.jsonl"
        self.started_at = now_iso()

        if self.run_dir.exists():
            shutil.rmtree(self.run_dir)
        self.captures_dir.mkdir(parents=True, exist_ok=True)
        self.meta_dir.mkdir(parents=True, exist_ok=True)
        for path in (
            self.events_file,
            self.assertions_file,
            self.captures_file,
            self.metrics_file,
            self.notes_file,
            self.fields_file,
        ):
            path.write_text("", encoding="utf-8")

    def event(self, event: str, status: str, message: str) -> None:
        append_jsonl_record(
            self.events_file,
            {
                "timestamp": now_iso(),
                "event": event,
                "status": status,
                "message": message,
            },
        )

    def assertion(self, name: str, status: str, duration_ms: int, details: str) -> None:
        append_jsonl_record(
            self.assertions_file,
            {
                "name": name,
                "status": status,
                "duration_ms": duration_ms,
                "details": details,
            },
        )

    def metric(self, key: str, value: Any) -> None:
        append_jsonl_record(self.metrics_file, {"key": key, "value": value})

    def note(self, note: str) -> None:
        append_jsonl_record(self.notes_file, {"note": note})

    def field(self, key: str, value: Any) -> None:
        append_jsonl_record(self.fields_file, {"key": key, "value": value})

    def record_capture(self, label: str, kind: str, relative_path: str) -> None:
        append_jsonl_record(
            self.captures_file,
            {"label": label, "kind": kind, "path": relative_path},
        )

    def copy_capture(self, *, source: Path, destination: str, label: str, kind: str) -> Path:
        if not source.exists():
            raise ScenarioError(f"Capture source does not exist: {source}")
        destination_path = self.captures_dir / destination
        destination_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination_path)
        relative = destination_path.relative_to(self.captures_dir).as_posix()
        self.record_capture(label, kind, f"captures/{relative}")
        return destination_path

    def command_capture_path(self, filename: str) -> Path:
        path = self.captures_dir / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        return path

    def finalize(self, status: str) -> None:
        build_summary_files(
            output_dir=self.run_dir,
            scenario=self.scenario,
            status=status,
            started_at=self.started_at,
            ended_at=now_iso(),
            run_id=self.run_id,
            assertions=self.assertions_file,
            captures=self.captures_file,
            metrics=self.metrics_file,
            notes=self.notes_file,
            fields=self.fields_file,
        )


def format_scenario_rows(scenarios: list[ScenarioSpec]) -> str:
    headers = ("SCENARIO", "TOPOLOGY", "DESCRIPTION")
    rows = [(scenario.name, scenario.topology, scenario.description) for scenario in scenarios]
    widths = [
        max(len(headers[index]), *(len(row[index]) for row in rows)) if rows else len(headers[index])
        for index in range(len(headers))
    ]

    lines = [
        f"{headers[0]:<{widths[0]}}  {headers[1]:<{widths[1]}}  {headers[2]}",
        f"{'-' * widths[0]}  {'-' * widths[1]}  {'-' * widths[2]}",
    ]
    for row in rows:
        lines.append(f"{row[0]:<{widths[0]}}  {row[1]:<{widths[1]}}  {row[2]}")
    return "\n".join(lines)


def print_progress(message: str) -> None:
    print(f"[RUN] {message}", flush=True)


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def now_ms() -> int:
    return int(datetime.now(timezone.utc).timestamp() * 1000)


def slugify(value: str) -> str:
    cleaned = [char if char.isalnum() or char in "._-" else "-" for char in value]
    slug = "".join(cleaned).strip("-")
    return slug or "scenario"


def default_run_id(scenario: str) -> str:
    return f"{datetime.now(timezone.utc).strftime('%Y-%m-%dT%H-%M-%SZ')}_{slugify(scenario)}"


def default_state_file(lab: str) -> Path:
    return DEFAULT_STATE_ROOT / f"{slugify(lab)}.env"


def resolve_launch_user() -> LaunchUser:
    if os.environ.get("SUDO_USER"):
        passwd_entry = pwd.getpwnam(os.environ["SUDO_USER"])
    else:
        passwd_entry = pwd.getpwuid(os.getuid())

    env: dict[str, str] = {}
    for key in (
        "LANG",
        "LC_ALL",
        "LC_CTYPE",
        "DISPLAY",
        "WAYLAND_DISPLAY",
        "DBUS_SESSION_BUS_ADDRESS",
        "XDG_CONFIG_HOME",
        "XDG_CACHE_HOME",
        "XDG_DATA_HOME",
    ):
        value = os.environ.get(key)
        if value:
            env[key] = value

    xdg_runtime_dir = os.environ.get("XDG_RUNTIME_DIR")
    if not xdg_runtime_dir:
        candidate = Path(f"/run/user/{passwd_entry.pw_uid}")
        if candidate.is_dir():
            xdg_runtime_dir = str(candidate)
    if xdg_runtime_dir:
        env["XDG_RUNTIME_DIR"] = xdg_runtime_dir

    env["HOME"] = passwd_entry.pw_dir
    env["USER"] = passwd_entry.pw_name
    env["LOGNAME"] = passwd_entry.pw_name
    env["PWD"] = passwd_entry.pw_dir
    env["SHELL"] = passwd_entry.pw_shell or "/bin/sh"

    return LaunchUser(
        username=passwd_entry.pw_name,
        uid=passwd_entry.pw_uid,
        gid=passwd_entry.pw_gid,
        home=passwd_entry.pw_dir,
        shell=passwd_entry.pw_shell or "/bin/sh",
        env=env,
    )


def build_actor_argv(namespace: str, launch_command: str, launch_user: LaunchUser) -> list[str]:
    if shutil.which("sudo") is None:
        raise ScenarioError("Managed actor launch requires 'sudo' to be available in PATH")
    if shutil.which("bash") is None:
        raise ScenarioError("Managed actor launch requires 'bash' to be available in PATH")

    env_assignments = [f"{key}={value}" for key, value in launch_user.env.items()]
    return [
        "ip",
        "netns",
        "exec",
        namespace,
        "sudo",
        "-u",
        launch_user.username,
        "-H",
        "env",
        *env_assignments,
        "bash",
        "-lc",
        f"exec {launch_command}",
    ]


def launch_actor(
    recorder: ResultRecorder,
    *,
    namespace: str,
    launch_command: str,
    launch_wait_s: float,
) -> ManagedActor:
    launch_user = resolve_launch_user()
    argv = build_actor_argv(namespace, launch_command, launch_user)
    log_path = recorder.command_capture_path("actor.log")
    meta_path = recorder.command_capture_path("actor-meta.txt")

    meta_lines = [
        f"namespace={namespace}",
        f"username={launch_user.username}",
        f"uid={launch_user.uid}",
        f"gid={launch_user.gid}",
        f"home={launch_user.home}",
        f"shell={launch_user.shell}",
        f"launch_wait_s={launch_wait_s}",
        f"launch_command={launch_command}",
        f"argv={shlex.join(argv)}",
    ]
    meta_path.write_text("\n".join(meta_lines) + "\n", encoding="utf-8")
    recorder.record_capture("Managed actor metadata", "state-dump", "captures/actor-meta.txt")

    log_handle = log_path.open("w", encoding="utf-8")
    log_handle.write(f"$ {shlex.join(argv)}\n\n")
    log_handle.flush()
    recorder.record_capture("Managed actor log", "log", "captures/actor.log")
    try:
        process = subprocess.Popen(
            argv,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            text=True,
            start_new_session=True,
        )
    except OSError as exc:
        log_handle.write(f"ERROR: {exc}\n")
        log_handle.close()
        raise ScenarioError(f"Failed to launch actor command: {exc}") from exc
    actor = ManagedActor(
        namespace=namespace,
        launch_command=launch_command,
        user=launch_user,
        process=process,
        log_handle=log_handle,
        log_path=log_path,
        meta_path=meta_path,
    )

    if launch_wait_s > 0:
        time.sleep(launch_wait_s)
    return actor


def actor_status(actor: ManagedActor) -> tuple[bool, str]:
    exit_code = actor.process.poll()
    if exit_code is None:
        return True, (
            f"Actor is now running in namespace {actor.namespace}. "
            f"Log: captures/{actor.log_path.name}"
        )
    return False, (
        f"Actor exited with code {exit_code} in namespace {actor.namespace}. "
        f"Log: captures/{actor.log_path.name}"
    )


def stop_actor(actor: ManagedActor, *, timeout_s: float) -> tuple[str, str]:
    exit_code = actor.process.poll()
    if exit_code is not None:
        actor.log_handle.close()
        return "passed", (
            f"Actor already exited with code {exit_code}. "
            f"Log: captures/{actor.log_path.name}"
        )

    try:
        os.killpg(actor.process.pid, signal.SIGTERM)
        actor.process.wait(timeout=timeout_s)
        return "passed", (
            f"Actor stopped with SIGTERM. Log: captures/{actor.log_path.name}"
        )
    except ProcessLookupError:
        return "passed", (
            f"Actor process group already disappeared. Log: captures/{actor.log_path.name}"
        )
    except subprocess.TimeoutExpired:
        os.killpg(actor.process.pid, signal.SIGKILL)
        actor.process.wait(timeout=5)
        return "failed", (
            f"Actor required SIGKILL after SIGTERM timeout. "
            f"Log: captures/{actor.log_path.name}"
        )
    finally:
        actor.log_handle.close()


def parse_shell_env_file(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.lstrip().startswith("#"):
            continue
        if "=" not in line:
            raise ScenarioError(f"Invalid env line in {path}: {line!r}")
        key, raw_value = line.split("=", 1)
        parsed = shlex.split(raw_value, posix=True)
        if len(parsed) > 1:
            raise ScenarioError(f"Could not parse env value for {key!r} in {path}")
        values[key] = parsed[0] if parsed else ""
    return values


def require_string(value: Any, *, field_name: str, scenario_path: Path) -> str:
    if not isinstance(value, str) or not value:
        raise ScenarioError(f"{scenario_path}: expected non-empty string for {field_name}")
    return value


def require_string_list(value: Any, *, field_name: str, scenario_path: Path) -> list[str]:
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise ScenarioError(f"{scenario_path}: expected a string list for {field_name}")
    return list(value)


def require_nonnegative_float(value: Any, *, field_name: str, scenario_path: Path) -> float:
    if not isinstance(value, (int, float)):
        raise ScenarioError(f"{scenario_path}: expected a number for {field_name}")
    if value < 0:
        raise ScenarioError(f"{scenario_path}: expected a non-negative number for {field_name}")
    return float(value)


def parse_copy_output(raw: Any, *, scenario_path: Path, step_name: str) -> CopyOutputSpec:
    if not isinstance(raw, dict):
        raise ScenarioError(f"{scenario_path}: step {step_name!r} copy_outputs entries must be objects")
    return CopyOutputSpec(
        source=require_string(raw.get("source"), field_name="copy_outputs.source", scenario_path=scenario_path),
        destination=require_string(raw.get("destination"), field_name="copy_outputs.destination", scenario_path=scenario_path),
        label=require_string(raw.get("label"), field_name="copy_outputs.label", scenario_path=scenario_path),
        kind=str(raw.get("kind", "captured-file")),
    )


def parse_step(raw: Any, *, scenario_path: Path) -> StepSpec:
    if not isinstance(raw, dict):
        raise ScenarioError(f"{scenario_path}: steps must contain objects")
    name = require_string(raw.get("name"), field_name="steps[].name", scenario_path=scenario_path)
    step_type = require_string(raw.get("type"), field_name="steps[].type", scenario_path=scenario_path)
    if step_type not in {"command", "namespace-command"}:
        raise ScenarioError(f"{scenario_path}: unsupported step type {step_type!r}")
    namespace = raw.get("namespace")
    if step_type == "namespace-command":
        namespace = require_string(namespace, field_name="steps[].namespace", scenario_path=scenario_path)
    elif namespace is not None and not isinstance(namespace, str):
        raise ScenarioError(f"{scenario_path}: steps[].namespace must be a string when present")
    copy_outputs = [parse_copy_output(item, scenario_path=scenario_path, step_name=name) for item in raw.get("copy_outputs", [])]
    return StepSpec(
        name=name,
        step_type=step_type,
        argv=require_string_list(raw.get("argv"), field_name="steps[].argv", scenario_path=scenario_path),
        namespace=namespace,
        capture=raw.get("capture"),
        label=raw.get("label"),
        kind=str(raw.get("kind", "command-output")),
        allow_failure=bool(raw.get("allow_failure", False)),
        copy_outputs=copy_outputs,
    )


def load_scenario(path: Path) -> ScenarioSpec:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ScenarioError(f"{path}: scenario file must contain a JSON object")
    fields = data.get("fields", {})
    if not isinstance(fields, dict):
        raise ScenarioError(f"{path}: fields must be an object")
    notes = data.get("notes", [])
    if not isinstance(notes, list) or not all(isinstance(item, str) for item in notes):
        raise ScenarioError(f"{path}: notes must be a string list")
    steps = data.get("steps", [])
    if not isinstance(steps, list):
        raise ScenarioError(f"{path}: steps must be a list")
    state_file = data.get("state_file")
    if state_file is not None and not isinstance(state_file, str):
        raise ScenarioError(f"{path}: state_file must be a string when present")
    actor_namespace = data.get("actor_namespace")
    if actor_namespace is not None and not isinstance(actor_namespace, str):
        raise ScenarioError(f"{path}: actor_namespace must be a string when present")
    requires_actor = data.get("requires_actor", False)
    if not isinstance(requires_actor, bool):
        raise ScenarioError(f"{path}: requires_actor must be a boolean when present")
    actor_wait_s = data.get("actor_wait_s", 0.0)
    return ScenarioSpec(
        name=require_string(data.get("name"), field_name="name", scenario_path=path),
        description=require_string(data.get("description"), field_name="description", scenario_path=path),
        topology=require_string(data.get("topology"), field_name="topology", scenario_path=path),
        lab=require_string(data.get("lab"), field_name="lab", scenario_path=path),
        setup=require_string_list(data.get("setup"), field_name="setup", scenario_path=path),
        teardown=require_string_list(data.get("teardown"), field_name="teardown", scenario_path=path),
        steps=[parse_step(item, scenario_path=path) for item in steps],
        notes=list(notes),
        fields=dict(fields),
        path=path,
        state_file=state_file,
        actor_namespace=actor_namespace,
        requires_actor=requires_actor,
        actor_wait_s=require_nonnegative_float(
            actor_wait_s,
            field_name="actor_wait_s",
            scenario_path=path,
        ),
    )


def load_scenarios(scenario_dir: Path) -> dict[str, ScenarioSpec]:
    scenarios: dict[str, ScenarioSpec] = {}
    for path in sorted(scenario_dir.glob("*.json")):
        scenario = load_scenario(path)
        if scenario.name in scenarios:
            raise ScenarioError(f"Duplicate scenario name {scenario.name!r} in {path}")
        scenarios[scenario.name] = scenario
    return scenarios


def resolve_text(template: str, context: dict[str, str], *, scenario_name: str) -> str:
    try:
        return template.format_map(context)
    except KeyError as exc:
        missing = exc.args[0]
        raise ScenarioError(
            f"Scenario {scenario_name!r} references unknown placeholder {missing!r} in {template!r}"
        ) from exc


def resolve_argv(argv: list[str], context: dict[str, str], *, scenario_name: str) -> list[str]:
    return [resolve_text(item, context, scenario_name=scenario_name) for item in argv]


def run_command(argv: list[str], capture_path: Path) -> int:
    with capture_path.open("w", encoding="utf-8") as handle:
        handle.write(f"$ {shlex.join(argv)}\n\n")
        try:
            completed = subprocess.run(argv, stdout=handle, stderr=subprocess.STDOUT, text=True, check=False)
        except OSError as exc:
            handle.write(f"ERROR: {exc}\n")
            return 127
    return completed.returncode


def capture_best_effort(recorder: ResultRecorder, label: str, kind: str, filename: str, argv: list[str]) -> None:
    capture_path = recorder.command_capture_path(filename)
    rc = run_command(argv, capture_path)
    recorder.record_capture(label, kind, f"captures/{filename}")
    if rc != 0:
        recorder.note(f"capture_failed:{filename}:exit_code={rc}")


def copy_state_artifacts(recorder: ResultRecorder, context: dict[str, str]) -> None:
    for key, value in sorted(context.items()):
        if not isinstance(value, str) or not value.startswith("/"):
            continue
        if "LOGFILE" not in key and "DISCOVERY_LOG" not in key and "CONFIG" not in key:
            continue

        source = Path(value)
        if not source.exists():
            recorder.note(f"state_artifact_missing:{key}={value}")
            continue

        suffix = source.suffix or ".txt"
        destination = f"state-{slugify(key.lower())}{suffix}"
        try:
            recorder.copy_capture(
                source=source,
                destination=destination,
                label=f"State artifact {key}",
                kind="state-dump",
            )
        except ScenarioError as exc:
            recorder.note(f"state_artifact_copy_failed:{key}={exc}")


def collect_namespace_snapshots(recorder: ResultRecorder, context: dict[str, str], phase: str) -> None:
    capture_best_effort(recorder, f"{phase} namespace list", "state-dump", f"{phase}-netns-list.txt", ["ip", "netns", "list"])

    namespaces: dict[str, str] = {}
    for key, value in context.items():
        if key.endswith("_NS") and value:
            namespaces.setdefault(value, key)

    recorder.metric("namespace_count", len(namespaces))
    for namespace in sorted(namespaces):
        ns_slug = slugify(namespace)
        capture_best_effort(
            recorder,
            f"{phase} {namespace} addresses",
            "state-dump",
            f"{phase}-{ns_slug}-addr.txt",
            ["ip", "-n", namespace, "addr", "show"],
        )
        capture_best_effort(
            recorder,
            f"{phase} {namespace} routes",
            "state-dump",
            f"{phase}-{ns_slug}-route.txt",
            ["ip", "-n", namespace, "route", "show"],
        )



def execute_lifecycle_command(
    recorder: ResultRecorder,
    *,
    assertion_name: str,
    event_name: str,
    argv: list[str],
    capture_name: str,
) -> bool:
    started_ms = now_ms()
    capture_path = recorder.command_capture_path(capture_name)
    recorder.event(event_name, "info", shlex.join(argv))
    rc = run_command(argv, capture_path)
    recorder.record_capture(assertion_name, "command-output", f"captures/{capture_name}")
    status = "passed" if rc == 0 else "failed"
    recorder.assertion(
        assertion_name,
        status,
        now_ms() - started_ms,
        f"Command exited {rc}. Capture: captures/{capture_name}",
    )
    recorder.event(f"{event_name}_finished", status, f"exit_code={rc}")
    return rc == 0


def execute_nonfatal_cleanup(
    recorder: ResultRecorder,
    *,
    argv: list[str],
    capture_name: str,
) -> None:
    capture_path = recorder.command_capture_path(capture_name)
    recorder.event("pre_cleanup_started", "info", shlex.join(argv))
    rc = run_command(argv, capture_path)
    recorder.record_capture("pre_cleanup_topology", "command-output", f"captures/{capture_name}")
    if rc == 0:
        recorder.event("pre_cleanup_finished", "passed", "Pre-cleanup completed")
    else:
        recorder.note(f"pre_cleanup_exit_code={rc}")
        recorder.event("pre_cleanup_finished", "warning", f"Pre-cleanup exited {rc}")


def execute_step(recorder: ResultRecorder, step: StepSpec, context: dict[str, str], *, scenario_name: str) -> bool:
    capture_name = step.capture or f"{slugify(step.name)}.txt"
    capture_path = recorder.command_capture_path(capture_name)
    argv = resolve_argv(step.argv, context, scenario_name=scenario_name)
    if step.step_type == "namespace-command":
        assert step.namespace is not None
        namespace = resolve_text(step.namespace, context, scenario_name=scenario_name)
        command = ["ip", "netns", "exec", namespace, *argv]
    else:
        command = argv

    started_ms = now_ms()
    recorder.event("step_started", "info", f"{step.name}: {shlex.join(command)}")
    rc = run_command(command, capture_path)
    recorder.record_capture(step.label or step.name, step.kind, f"captures/{capture_name}")

    details = [f"Command exited {rc}.", f"Capture: captures/{capture_name}"]
    copy_failed = False
    for copy_output in step.copy_outputs:
        source = Path(resolve_text(copy_output.source, context, scenario_name=scenario_name))
        destination = resolve_text(copy_output.destination, context, scenario_name=scenario_name)
        try:
            recorder.copy_capture(
                source=source,
                destination=destination,
                label=copy_output.label,
                kind=copy_output.kind,
            )
        except ScenarioError as exc:
            copy_failed = True
            details.append(str(exc))

    if rc != 0 or copy_failed:
        status = "failed"
        success = step.allow_failure
    else:
        status = "passed"
        success = True

    recorder.assertion(step.name, status, now_ms() - started_ms, " ".join(details))
    recorder.event("step_finished", status, f"{step.name}: exit_code={rc}")
    return success



def print_dry_run(scenario: ScenarioSpec) -> None:
    print(f"Scenario: {scenario.name}")
    print(f"Description: {scenario.description}")
    print(f"Topology: {scenario.topology}")
    print(f"Lab: {scenario.lab}")
    print(f"Scenario file: {scenario.path}")
    if scenario.actor_namespace:
        print(f"Actor namespace: {scenario.actor_namespace}")
        print(f"Requires actor: {'yes' if scenario.requires_actor else 'no'}")
        print(f"Actor wait: {scenario.actor_wait_s:.1f}s")
    print("Setup:")
    print(f"  {shlex.join(scenario.setup)}")
    print("Steps:")
    for step in scenario.steps:
        if step.step_type == "namespace-command":
            namespace = step.namespace or "<namespace>"
            print(f"  - {step.name}: ip netns exec {namespace} {shlex.join(step.argv)}")
        else:
            print(f"  - {step.name}: {shlex.join(step.argv)}")
    print("Teardown:")
    print(f"  {shlex.join(scenario.teardown)}")



def run_scenario(
    scenario: ScenarioSpec,
    *,
    artifact_root: Path,
    run_id: str,
    keep_topology: bool,
    launch_command: str | None,
    launch_namespace_override: str | None,
    launch_wait_s: float | None,
    actor_stop_timeout_s: float,
) -> int:
    if os.geteuid() != 0:
        raise ScenarioError("run requires root privileges")
    if launch_wait_s is not None and launch_wait_s < 0:
        raise ScenarioError("--launch-wait-s must be non-negative")
    if actor_stop_timeout_s <= 0:
        raise ScenarioError("--actor-stop-timeout-s must be greater than zero")

    artifact_root.mkdir(parents=True, exist_ok=True)
    recorder = ResultRecorder(run_id=run_id, scenario=scenario.name, artifact_root=artifact_root)
    context: dict[str, str] = {
        "root": str(ROOT),
        "artifact_root": str(artifact_root),
        "run_id": run_id,
        "run_dir": str(recorder.run_dir),
        "scenario": scenario.name,
        "lab": scenario.lab,
        "state_root": str(DEFAULT_STATE_ROOT),
    }
    status = "passed"
    setup_ok = False
    should_exit = False
    actor: ManagedActor | None = None

    recorder.field("lab", scenario.lab)
    recorder.field("topology", scenario.topology)
    recorder.field("scenario_file", str(scenario.path.relative_to(ROOT)))
    for key, value in scenario.fields.items():
        recorder.field(key, value)
    for note in scenario.notes:
        recorder.note(note)

    recorder.copy_capture(
        source=scenario.path,
        destination="scenario.json",
        label="Scenario definition",
        kind="scenario-definition",
    )
    recorder.event("run_started", "info", f"Scenario {scenario.name!r} started")
    print_progress(f"Scenario {scenario.name} (run-id: {run_id})")

    try:
        teardown_argv = resolve_argv(scenario.teardown, context, scenario_name=scenario.name)
        print_progress("Pre-cleaning topology")
        execute_nonfatal_cleanup(
            recorder,
            argv=teardown_argv,
            capture_name="pre-cleanup.txt",
        )

        setup_argv = resolve_argv(scenario.setup, context, scenario_name=scenario.name)
        print_progress("Setting up topology")
        setup_ok = execute_lifecycle_command(
            recorder,
            assertion_name="setup_topology",
            event_name="setup_started",
            argv=setup_argv,
            capture_name="setup.txt",
        )
        if not setup_ok:
            status = "failed"
            should_exit = True

        if not should_exit:
            print_progress("Loading lab state")
            state_path = Path(
                resolve_text(
                    scenario.state_file or str(default_state_file(scenario.lab)),
                    context,
                    scenario_name=scenario.name,
                )
            )
            context["state_file"] = str(state_path)
            if not state_path.exists():
                recorder.assertion(
                    "state_file_present",
                    "failed",
                    0,
                    f"Expected state file {state_path} after setup.",
                )
                status = "failed"
                should_exit = True
            else:
                recorder.copy_capture(
                    source=state_path,
                    destination="lab-state.env",
                    label="Lab state file",
                    kind="state-dump",
                )
                state_values = parse_shell_env_file(state_path)
                context.update(state_values)
                recorder.assertion(
                    "state_file_present",
                    "passed",
                    0,
                    f"Loaded state file {state_path}.",
                )
                collect_namespace_snapshots(recorder, context, "setup")

        if not should_exit:
            needs_actor = scenario.requires_actor or bool(launch_command)
            if needs_actor:
                if not launch_command:
                    recorder.assertion(
                        "launch_actor",
                        "failed",
                        0,
                        "Scenario requires --launch-command so Jami can run inside the topology.",
                    )
                    status = "failed"
                    should_exit = True
                else:
                    actor_namespace_template = launch_namespace_override or scenario.actor_namespace
                    if not actor_namespace_template:
                        recorder.assertion(
                            "launch_actor",
                            "failed",
                            0,
                            "No actor namespace was provided. Use --launch-namespace or set actor_namespace in the scenario.",
                        )
                        status = "failed"
                        should_exit = True
                    else:
                        actor_namespace = resolve_text(
                            actor_namespace_template,
                            context,
                            scenario_name=scenario.name,
                        )
                        effective_launch_wait_s = (
                            launch_wait_s if launch_wait_s is not None else scenario.actor_wait_s
                        )
                        started_ms = now_ms()
                        print_progress(f"Launching actor in namespace {actor_namespace}")
                        actor = launch_actor(
                            recorder,
                            namespace=actor_namespace,
                            launch_command=launch_command,
                            launch_wait_s=effective_launch_wait_s,
                        )
                        recorder.field("actor_namespace", actor_namespace)
                        recorder.note(f"launch_user={actor.user.username}")
                        recorder.note(f"launch_home={actor.user.home}")
                        alive, details = actor_status(actor)
                        recorder.assertion(
                            "launch_actor",
                            "passed" if alive else "failed",
                            now_ms() - started_ms,
                            details,
                        )
                        if not alive:
                            status = "failed"
                            should_exit = True

        if not should_exit:
            for index, step in enumerate(scenario.steps, start=1):
                print_progress(f"Step {index}/{len(scenario.steps)}: {step.name}")
                if not execute_step(recorder, step, context, scenario_name=scenario.name):
                    status = "failed"
                    break
                if actor is not None:
                    alive, details = actor_status(actor)
                    if not alive:
                        recorder.assertion(
                            f"actor_alive_after_{slugify(step.name)}",
                            "failed",
                            0,
                            details,
                        )
                        status = "failed"
                        break

            print_progress("Collecting final snapshots")
            collect_namespace_snapshots(recorder, context, "final")
            if actor is not None:
                alive, details = actor_status(actor)
                if not alive:
                    recorder.assertion(
                        "actor_alive_before_cleanup",
                        "failed",
                        0,
                        details,
                    )
                    status = "failed"
    except ScenarioError as exc:
        recorder.event("orchestrator_error", "error", str(exc))
        recorder.note(f"orchestrator_error={exc}")
        if status == "passed":
            status = "error"
    finally:
        try:
            if setup_ok:
                copy_state_artifacts(recorder, context)

            if actor is not None:
                print_progress("Stopping actor")
                started_ms = now_ms()
                actor_stop_status, actor_stop_details = stop_actor(
                    actor,
                    timeout_s=actor_stop_timeout_s,
                )
                recorder.assertion(
                    "stop_actor",
                    actor_stop_status,
                    now_ms() - started_ms,
                    actor_stop_details,
                )
                if actor_stop_status != "passed" and status == "passed":
                    status = "error"

            if setup_ok:
                if keep_topology:
                    print_progress("Leaving topology running")
                    recorder.note("topology_left_running=1")
                    recorder.event("teardown_skipped", "info", "Topology left running by request")
                else:
                    teardown_argv = resolve_argv(scenario.teardown, context, scenario_name=scenario.name)
                    print_progress("Tearing down topology")
                    teardown_ok = execute_lifecycle_command(
                        recorder,
                        assertion_name="teardown_topology",
                        event_name="teardown_started",
                        argv=teardown_argv,
                        capture_name="teardown.txt",
                    )
                    if not teardown_ok and status == "passed":
                        status = "error"
        except ScenarioError as exc:
            recorder.event("teardown_error", "error", str(exc))
            recorder.note(f"teardown_error={exc}")
            if status == "passed":
                status = "error"

        recorder.metric("step_count", len(scenario.steps))
        recorder.event("run_finished", status, f"Scenario finished with status {status}")
        recorder.finalize(status)

    print_progress("Run complete")
    print(f"[SUMMARY] {recorder.run_dir / 'summary.txt'}")
    print((recorder.run_dir / "summary.txt").read_text(encoding="utf-8"), end="")
    return 0 if status == "passed" else 1



def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run virtual-networking scenarios")
    parser.add_argument(
        "--scenario-dir",
        default=str(SCENARIO_DIR),
        help="Directory containing scenario JSON files",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("list", help="List available scenarios")

    describe = subparsers.add_parser("describe", help="Describe a scenario")
    describe.add_argument("scenario")

    run = subparsers.add_parser("run", help="Run a scenario")
    run.add_argument("scenario")
    run.add_argument("--artifact-root", default=str(DEFAULT_ARTIFACT_ROOT))
    run.add_argument("--run-id")
    run.add_argument("--keep-topology", action="store_true")
    run.add_argument("--launch-command")
    run.add_argument("--launch-namespace")
    run.add_argument("--launch-wait-s", type=float)
    run.add_argument("--actor-stop-timeout-s", type=float, default=10.0)
    run.add_argument("--dry-run", action="store_true")

    return parser



def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    scenarios = load_scenarios(Path(args.scenario_dir))

    if args.command == "list":
        print(format_scenario_rows(list(scenarios.values())))
        return 0

    scenario = scenarios.get(args.scenario)
    if scenario is None:
        parser.error(f"Unknown scenario: {args.scenario}")

    if args.command == "describe":
        print_dry_run(scenario)
        return 0

    if args.command == "run":
        if args.dry_run:
            print_dry_run(scenario)
            return 0
        run_id = args.run_id or default_run_id(scenario.name)
        return run_scenario(
            scenario,
            artifact_root=Path(args.artifact_root),
            run_id=run_id,
            keep_topology=args.keep_topology,
            launch_command=args.launch_command,
            launch_namespace_override=args.launch_namespace,
            launch_wait_s=args.launch_wait_s,
            actor_stop_timeout_s=args.actor_stop_timeout_s,
        )

    parser.error(f"Unsupported command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
