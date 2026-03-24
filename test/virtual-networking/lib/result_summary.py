#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def iso_to_datetime(value: str) -> datetime:
    if value.endswith("Z"):
        value = value[:-1] + "+00:00"
    return datetime.fromisoformat(value).astimezone(timezone.utc)


def read_jsonl(path: Path | None) -> list[dict[str, Any]]:
    if path is None or not path.exists():
        return []
    records: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            records.append(json.loads(line))
    return records


def read_metrics(path: Path | None) -> dict[str, Any]:
    metrics: dict[str, Any] = {}
    for record in read_jsonl(path):
        key = record["key"]
        metrics[key] = record["value"]
    return metrics


def read_fields(path: Path | None) -> dict[str, Any]:
    fields: dict[str, Any] = {}
    for record in read_jsonl(path):
        fields[record["key"]] = record["value"]
    return fields


def append_jsonl_record(path: Path, record: dict[str, Any]) -> int:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(record, ensure_ascii=True) + "\n")
    return 0


def render_text(summary: dict[str, Any], *, captures_dir: Path | None = None) -> str:
    lines = [
        f"Scenario: {summary['scenario']}",
        f"Run ID: {summary['run_id']}",
        f"Status: {summary['status'].upper()}",
        f"Started: {summary['started_at']}",
        f"Ended: {summary['ended_at']}",
        f"Duration: {summary['duration_s']:.3f}s",
    ]

    for optional_key in ("topology", "profile_before", "profile_after", "lab"):
        if optional_key in summary:
            label = optional_key.replace("_", " ").title()
            lines.append(f"{label}: {summary[optional_key]}")

    lines.append("")
    lines.append("Assertions:")
    if summary["assertions"]:
        for assertion in summary["assertions"]:
            duration = assertion.get("duration_ms")
            duration_label = f"{duration}ms" if duration is not None else "n/a"
            lines.append(
                f"  - [{assertion['status'].upper()}] {assertion['name']} ({duration_label})"
            )
            details = assertion.get("details")
            if details:
                lines.append(f"      {details}")
    else:
        lines.append("  - (none)")

    lines.append("")
    lines.append("Metrics:")
    if summary["metrics"]:
        for key, value in summary["metrics"].items():
            lines.append(f"  - {key}: {value}")
    else:
        lines.append("  - (none)")

    lines.append("")
    lines.append("Captures:")
    lines.append(f"  - Count: {len(summary['captures'])}")
    if captures_dir is not None:
        lines.append(f"  - Directory: {captures_dir.resolve()}")

    lines.append("")
    lines.append("Notes:")
    if summary["notes"]:
        for note in summary["notes"]:
            lines.append(f"  - {note}")
    else:
        lines.append("  - (none)")

    return "\n".join(lines) + "\n"


def build_summary_files(
    *,
    output_dir: Path,
    scenario: str,
    status: str,
    started_at: str,
    ended_at: str,
    run_id: str | None = None,
    assertions: Path | None = None,
    captures: Path | None = None,
    metrics: Path | None = None,
    notes: Path | None = None,
    fields: Path | None = None,
) -> int:
    output_dir.mkdir(parents=True, exist_ok=True)

    started_at_dt = iso_to_datetime(started_at)
    ended_at_dt = iso_to_datetime(ended_at)
    duration_s = (ended_at_dt - started_at_dt).total_seconds()

    field_values = read_fields(fields)
    summary: dict[str, Any] = {
        "run_id": run_id or output_dir.name,
        "scenario": scenario,
        "status": status,
        "started_at": started_at,
        "ended_at": ended_at,
        "duration_s": duration_s,
        "assertions": read_jsonl(assertions),
        "metrics": read_metrics(metrics),
        "captures": read_jsonl(captures),
        "notes": [record["note"] for record in read_jsonl(notes)],
    }
    summary.update(field_values)

    summary_json = output_dir / "summary.json"
    summary_txt = output_dir / "summary.txt"

    with summary_json.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2, sort_keys=False)
        handle.write("\n")

    with summary_txt.open("w", encoding="utf-8") as handle:
        handle.write(render_text(summary, captures_dir=output_dir / "captures"))

    return 0


def build_summary(args: argparse.Namespace) -> int:
    return build_summary_files(
        output_dir=Path(args.output_dir),
        scenario=args.scenario,
        status=args.status,
        started_at=args.started_at,
        ended_at=args.ended_at,
        run_id=args.run_id,
        assertions=Path(args.assertions) if args.assertions else None,
        captures=Path(args.captures) if args.captures else None,
        metrics=Path(args.metrics) if args.metrics else None,
        notes=Path(args.notes) if args.notes else None,
        fields=Path(args.fields) if args.fields else None,
    )


def append_event(args: argparse.Namespace) -> int:
    return append_jsonl_record(
        Path(args.output),
        {
            "timestamp": args.timestamp,
            "event": args.event,
            "status": args.status,
            "message": args.message,
        },
    )


def append_field(args: argparse.Namespace) -> int:
    return append_jsonl_record(
        Path(args.output),
        {
            "key": args.key,
            "value": args.value,
        },
    )


def append_assertion(args: argparse.Namespace) -> int:
    return append_jsonl_record(
        Path(args.output),
        {
            "name": args.name,
            "status": args.status,
            "duration_ms": int(args.duration_ms) if args.duration_ms else None,
            "details": args.details,
        },
    )


def append_metric(args: argparse.Namespace) -> int:
    return append_jsonl_record(
        Path(args.output),
        {
            "key": args.key,
            "value": args.value,
        },
    )


def append_note(args: argparse.Namespace) -> int:
    return append_jsonl_record(
        Path(args.output),
        {
            "note": args.note,
        },
    )


def append_capture(args: argparse.Namespace) -> int:
    return append_jsonl_record(
        Path(args.output),
        {
            "label": args.label,
            "kind": args.kind,
            "path": args.path,
        },
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build virtual-networking result summaries")
    subparsers = parser.add_subparsers(dest="command", required=True)

    build = subparsers.add_parser("build", help="Render summary.json and summary.txt")
    build.add_argument("--output-dir", required=True)
    build.add_argument("--scenario", required=True)
    build.add_argument("--status", required=True)
    build.add_argument("--started-at", required=True)
    build.add_argument("--ended-at", required=True)
    build.add_argument("--run-id")
    build.add_argument("--assertions")
    build.add_argument("--captures")
    build.add_argument("--metrics")
    build.add_argument("--notes")
    build.add_argument("--fields")

    append_event_parser = subparsers.add_parser("append-event", help="Append an event record")
    append_event_parser.add_argument("--output", required=True)
    append_event_parser.add_argument("--event", required=True)
    append_event_parser.add_argument("--status", required=True)
    append_event_parser.add_argument("--message", required=True)
    append_event_parser.add_argument("--timestamp", required=True)

    append_field_parser = subparsers.add_parser("append-field", help="Append a field record")
    append_field_parser.add_argument("--output", required=True)
    append_field_parser.add_argument("--key", required=True)
    append_field_parser.add_argument("--value", required=True)

    append_assertion_parser = subparsers.add_parser("append-assertion", help="Append an assertion record")
    append_assertion_parser.add_argument("--output", required=True)
    append_assertion_parser.add_argument("--name", required=True)
    append_assertion_parser.add_argument("--status", required=True)
    append_assertion_parser.add_argument("--duration-ms", default="")
    append_assertion_parser.add_argument("--details", default="")

    append_metric_parser = subparsers.add_parser("append-metric", help="Append a metric record")
    append_metric_parser.add_argument("--output", required=True)
    append_metric_parser.add_argument("--key", required=True)
    append_metric_parser.add_argument("--value", required=True)

    append_note_parser = subparsers.add_parser("append-note", help="Append a note record")
    append_note_parser.add_argument("--output", required=True)
    append_note_parser.add_argument("--note", required=True)

    append_capture_parser = subparsers.add_parser("append-capture", help="Append a capture record")
    append_capture_parser.add_argument("--output", required=True)
    append_capture_parser.add_argument("--label", required=True)
    append_capture_parser.add_argument("--kind", required=True)
    append_capture_parser.add_argument("--path", required=True)

    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.command == "build":
        return build_summary(args)
    if args.command == "append-event":
        return append_event(args)
    if args.command == "append-field":
        return append_field(args)
    if args.command == "append-assertion":
        return append_assertion(args)
    if args.command == "append-metric":
        return append_metric(args)
    if args.command == "append-note":
        return append_note(args)
    if args.command == "append-capture":
        return append_capture(args)
    raise ValueError(f"Unsupported command: {args.command}")


if __name__ == "__main__":
    raise SystemExit(main())
