# Virtual networking test lab

This directory contains privileged integration helpers for exercising the daemon under controlled networking conditions.

The current entry points are:

- `setup-fake-upnp-network.sh`
  - single LAN behind a NAT/IGD router plus a WAN peer namespace
- `setup-dual-router-handover-network.sh`
  - one node with two simultaneous Wi-Fi-like uplinks, each behind its own NAT/IGD and public IP
- `setup-dual-access-network.sh`
  - one node with a preferred Wi-Fi-like uplink plus a standby mobile-like uplink; only the Wi-Fi router exposes UPnP
- `run.py`
  - scenario runner that reuses the same topology scripts and result-summary contract

The internal shell plumbing is now split into reusable libraries under `lib/`, and reusable topology definitions live under `topologies/`.

## Current scripts

- `setup-fake-upnp-network.sh`
  - creates or tears down the static UPnP lab
- `setup-dual-router-handover-network.sh`
  - creates a dual-router topology suitable for Wi-Fi to Wi-Fi handover tests
- `setup-dual-access-network.sh`
  - creates a dual-access topology suitable for Wi-Fi to mobile-style path changes
- `probes/probe-dht-from-wan.sh`
  - checks whether a UPnP-mapped UDP port is actually reachable from the WAN side
- `probes/dht-node-pinger.py`
  - minimal OpenDHT-based reachability probe used by the WAN probe
- `run.py`
  - lists, describes, and runs scenario definitions from `scenarios/*.json`

## Shell libraries

- `lib/common.sh`
  - shared helpers for dependency checks, state files, timestamps, and process cleanup
- `lib/netns.sh`
  - namespace existence and lifecycle helpers
- `lib/topology.sh`
  - reusable veth, route, and NAT primitives used by multiple network layouts
- `lib/upnp.sh`
  - `miniupnpd` configuration and readiness helpers
- `lib/result-summary.sh`
  - reusable result-summary helpers that emit:
    - `summary.json`
    - `summary.txt`
    - `events.jsonl`
    - `captures/`
- `lib/result_summary.py`
  - canonical summary builder used by both `lib/result-summary.sh` and `run.py`

## Topology definitions

- `topologies/single-router.sh`
  - reusable definition for the current baseline LAN/router/WAN layout
- `topologies/dual-router-handover.sh`
  - reusable definition for a node with two routed uplinks and two distinct IGDs
- `topologies/dual-access.sh`
  - reusable definition for a preferred Wi-Fi-like uplink plus a standby mobile-like uplink

## Scenario definitions

- `scenarios/upnp-static.json`
  - baseline orchestrated scenario: setup, managed daemon launch, IGD discovery, WAN DHT probe, teardown
- `scenarios/dual-router-handover-smoke.json`
  - topology smoke scenario for the dual-router handover lab
- `scenarios/dual-access-smoke.json`
  - topology smoke scenario for the dual-access lab

The setup wrappers now also support `--no-hold`, which creates the topology and exits without waiting. This is what the orchestrator uses so it can continue with probes and captures before calling the matching `down` action.

The orchestrator can also manage a process inside the scenario's actor namespace by using `--launch-command '...'`.

While a scenario runs, `run.py` prints concise major-step progress updates so you can see where execution is currently blocked without waiting for the final summary.

When `--launch-command` is provided:

- the command is started after topology setup and state discovery
- it runs inside the scenario actor namespace
- it runs as the original invoking user (`SUDO_USER` when present), not as root's effective home/session
- the runner forces `HOME`, `USER`, and `LOGNAME` to the target user's values and captures startup/output in:
  - `captures/actor.log`
  - `captures/actor-meta.txt`
- the command should keep the daemon in the foreground; if it exits during the launch wait window, the run is marked failed

`scenarios/upnp-static.json` now requires `--launch-command` because a fresh topology without a daemon cannot satisfy the WAN DHT probe.

Before each scenario setup, the orchestrator also issues the scenario's `down` action as a best-effort pre-cleanup step so stale namespaces or state files do not cause immediate setup failures on repeated runs.

When the setup wrapper records topology-side paths such as `MINIUPNPD_LOGFILE`, `UPNPC_DISCOVERY_LOG`, or generated config files in the lab state file, the orchestrator copies those artifacts into the run captures before teardown so router-side evidence survives temp-directory cleanup.

## Result artifacts

Probe-oriented scripts can now emit reusable artifacts under:

```text
test/virtual-networking/artifacts/<run-id>/
```

These artifacts are ignored by git and are intended to be inspected locally or harvested by future orchestration code.

The canonical summary contract is documented in `docs/result-format.md`.

The orchestrator writes the same artifact shape as the shell probe scripts, so runs remain comparable across manual scripts and future automated scenarios.

## Lab state

The setup script now persists exact lab state in:

```text
/tmp/jami-virtual-networking/
```

This lets follow-up tools discover the active lab without hardcoding the router public IP or relying on broad temp-directory scans.

## Notes

- These helpers require root privileges or equivalent capabilities.
- They are not adapted for CI integration as of yet.
- The next planned work slice after this is dynamic scenario work on top of the orchestrator, starting with route changes and handover events.
