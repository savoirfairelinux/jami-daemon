# Virtual networking result format

The virtual-networking helpers should report results in a stable shape that is readable by humans and reusable by automation.

## Artifact layout

Each run writes to:

```text
artifacts/<run-id>/
  summary.json
  summary.txt
  events.jsonl
  captures/
```

If a run is repeated with the same `run_id`, the previous artifact directory is replaced so captures and summaries stay consistent with the latest execution.

## `summary.json`

Top-level fields:

- `run_id`
- `scenario`
- `status`
  - one of `passed`, `failed`, `error`, `skipped`
- `started_at`
- `ended_at`
  - ISO-8601 UTC timestamps; they may include millisecond precision
- `duration_s`
- `assertions`
  - ordered list of assertion records
- `metrics`
  - key/value metrics relevant to the scenario
- `captures`
  - ordered list of captured artifacts
- `notes`
  - short free-form observations

Optional top-level fields may be added when relevant:

- `topology`
- `profile_before`
- `profile_after`
- `lab`

### Assertion records

Each assertion record contains:

- `name`
- `status`
- `duration_ms`
- `details`

### Capture records

Each capture record contains:

- `label`
- `kind`
- `path`

Typical capture kinds:

- `log`
- `command-output`
- `pcap`
- `state-dump`
- `summary`
- `scenario-definition`

Common examples now include:

- managed actor output, such as `captures/actor.log`
- managed actor launch metadata, such as `captures/actor-meta.txt`

## `summary.txt`

`summary.txt` is the user-facing scan-friendly view. It must always show:

- scenario name
- run identifier
- overall status
- start time, end time, and total duration
- each assertion with status and duration
- metrics
- the capture count
- the full path to the capture directory
- notes

## `events.jsonl`

`events.jsonl` is append-only and intended for detailed timelines. Each line is a JSON object.

Current fields:

- `timestamp`
- `event`
- `status`
- `message`

This format is intentionally simple so shell helpers can emit it directly while later orchestration code can enrich it without breaking existing consumers.

## Current adoption

The reusable summary helpers live in:

- `lib/result-summary.sh`
- `lib/result_summary.py`

`probes/probe-dht-from-wan.sh` and `run.py` both use this contract today, which keeps shell probes and orchestrated scenarios comparable.
