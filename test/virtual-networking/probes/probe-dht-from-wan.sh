#!/usr/bin/env bash
# Verify that a UPnP-mapped port on the virtual router is reachable from the WAN side.
# Must be run as root from the host (not from inside a namespace).
#
# Usage:  sudo bash probes/probe-dht-from-wan.sh [--run-id ID] [--artifact-root DIR] [port]
#
# Without a port argument, the script:
#   1. Lists all UPnP-mapped UDP ports (miniupnpd nft table)
#   2. Lists all UDP ports bound by Jami in the lan namespace
#   3. Reports any mismatch (mapped but not bound, or bound but not mapped)
#   4. For ports in both sets, sends a DHT ping from the wan namespace
#
# With an explicit port argument, that port is checked directly.
#
# miniupnpd on Debian 12 uses native nftables (libmnl) rather than iptables.
# Forwarding rules appear in 'table ip miniupnpd' — not in the iptables MINIUPNPD chain.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VNET_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
source "${VNET_ROOT}/lib/common.sh"
source "${VNET_ROOT}/lib/netns.sh"
source "${VNET_ROOT}/lib/result-summary.sh"

usage() {
    echo "Usage: sudo bash $0 [--run-id ID] [--artifact-root DIR] [port]"
    echo "  --run-id ID         Override the generated result run identifier"
    echo "  --artifact-root DIR Override the artifact root (default: test/virtual-networking/artifacts)"
    echo "  port                Probe a single explicit UDP port"
    exit 1
}

LAB_NAME="fake-upnp-network"
STATE_FILE="$(vnet_state_file_path "${LAB_NAME}")"
RTR_EXT_IP="11.0.0.2"
RTR_NS="rtr"
WAN_NS="wan"
LAN_NS="lan"
RUN_ID=""
PORT=""
PROBE_EXIT_CODE=0
PROBE_OVERALL_STATUS="passed"
PROBE_SUMMARY_FINALIZED=0

while (($# > 0)); do
    case "$1" in
        --artifact-root)
            shift
            [[ $# -gt 0 ]] || usage
            VNET_ARTIFACT_ROOT="$1"
            ;;
        --run-id)
            shift
            [[ $# -gt 0 ]] || usage
            RUN_ID="$1"
            ;;
        -h|--help)
            usage
            ;;
        *)
            if [[ -n "${PORT}" ]]; then
                echo "Error: multiple ports provided." >&2
                usage
            fi
            PORT="$1"
            ;;
    esac
    shift
done

vnet_require_root
vnet_require_commands ip nft ss python3 awk grep sort comm

load_lab_state() {
    if vnet_load_env_file "${STATE_FILE}"; then
        RTR_EXT_IP="${RTR_TO_WAN_IP:-${RTR_EXT_IP}}"
    fi
}

append_text_capture() {
    local label="$1"
    local kind="$2"
    local filename="$3"
    local content="$4"

    printf '%s\n' "${content}" > "${VNET_RESULT_CAPTURES_DIR}/${filename}"
    vnet_results_capture "${label}" "${kind}" "captures/${filename}"
}

mark_failed() {
    PROBE_OVERALL_STATUS="failed"
    PROBE_EXIT_CODE=1
}

record_assertion() {
    local name="$1"
    local status="$2"
    local started_ms="$3"
    local details="$4"
    local ended_ms
    ended_ms="$(vnet_now_ms)"
    vnet_results_assert "${name}" "${status}" "$((ended_ms - started_ms))" "${details}"
}

finalize_summary() {
    local trap_rc=$?

    if [[ "${PROBE_SUMMARY_FINALIZED}" -eq 1 ]] || [[ -z "${VNET_RESULT_DIR:-}" ]]; then
        return
    fi

    if [[ "${PROBE_OVERALL_STATUS}" == "passed" && "${trap_rc}" -ne 0 ]]; then
        PROBE_OVERALL_STATUS="error"
        PROBE_EXIT_CODE="${trap_rc}"
    fi

    vnet_results_note "script_exit_code=${PROBE_EXIT_CODE:-${trap_rc}}"
    vnet_results_event "run_finished" "${PROBE_OVERALL_STATUS}" \
        "Probe finished with status ${PROBE_OVERALL_STATUS}"
    vnet_results_finalize "${PROBE_OVERALL_STATUS}"
    PROBE_SUMMARY_FINALIZED=1

    echo
    echo "[SUMMARY] ${VNET_RESULT_DIR}/summary.txt"
    cat "${VNET_RESULT_DIR}/summary.txt"
}

trap finalize_summary EXIT

load_lab_state
vnet_results_init "probe-dht-from-wan" "${RUN_ID}"
vnet_results_field "lab" "${LAB_NAME}"
vnet_results_field "topology" "single-router"
vnet_results_note "router_external_ip=${RTR_EXT_IP}"

vnet_results_capture_command "namespace list" "state-dump" "netns-list.txt" ip netns list || true

start_ms="$(vnet_now_ms)"
if vnet_assert_namespaces_exist "${RTR_NS}" "${WAN_NS}" "${LAN_NS}"; then
    record_assertion "namespaces_exist" "passed" "${start_ms}" \
        "Namespaces ${RTR_NS}, ${WAN_NS}, and ${LAN_NS} are present."
else
    record_assertion "namespaces_exist" "failed" "${start_ms}" \
        "Run setup-fake-upnp-network.sh first."
    mark_failed
    exit "${PROBE_EXIT_CODE}"
fi

mapped_ports() {
    ip netns exec "${RTR_NS}" nft list chain ip miniupnpd prerouting 2>/dev/null \
        | grep -oP 'udp dport \K[0-9]+' | sort -u
}

bound_ports() {
    ip netns exec "${LAN_NS}" ss -ulnp 2>/dev/null \
        | awk 'NR > 1 && $1 == "UNCONN" {
            split($4, a, ":");
            port = a[length(a)];
            if ($4 !~ /^127\./ && $4 !~ /^\[::1\]/ && port != "1900")
                print port
        }' | sort -u
}

vnet_results_capture_command "router miniupnpd prerouting" "state-dump" \
    "router-nft-prerouting.txt" ip netns exec "${RTR_NS}" nft list chain ip miniupnpd prerouting || true
vnet_results_capture_command "lan udp sockets" "state-dump" \
    "lan-udp-sockets.txt" ip netns exec "${LAN_NS}" ss -ulnp || true

run_explicit_probe() {
    local probe_port="$1"
    local started_ms
    local current_ports=""

    echo "[CHECK] DNAT for UDP port ${probe_port} in ${RTR_NS} namespace..."
    started_ms="$(vnet_now_ms)"
    if ip netns exec "${RTR_NS}" nft list chain ip miniupnpd prerouting 2>/dev/null \
        | grep -q "dport ${probe_port} "; then
        echo "[OK]    DNAT rule found in miniupnpd nftables table."
        record_assertion "dnat_rule_present_${probe_port}" "passed" "${started_ms}" \
            "DNAT rule exists for UDP port ${probe_port}."
    else
        echo "[FAIL]  Port ${probe_port} has no DNAT rule in miniupnpd's nftables table."
        echo
        echo "  Currently mapped UDP ports:"
        if current_ports="$(mapped_ports)"; then
            echo "${current_ports}" | sed 's/^/    /'
            append_text_capture "current mapped ports" "command-output" \
                "current-mapped-ports.txt" "${current_ports}"
        fi
        record_assertion "dnat_rule_present_${probe_port}" "failed" "${started_ms}" \
            "DNAT rule is missing for UDP port ${probe_port}."
        mark_failed
        return 1
    fi

    echo "[PROBE] Sending DHT ping to ${RTR_EXT_IP}:${probe_port} from ${WAN_NS} namespace..."
    started_ms="$(vnet_now_ms)"
    if vnet_results_capture_command "wan DHT probe port ${probe_port}" "command-output" \
        "dht-probe-port-${probe_port}.txt" \
        ip netns exec "${WAN_NS}" python3 "${SCRIPT_DIR}/dht-node-pinger.py" "${RTR_EXT_IP}" "${probe_port}"; then
        echo "PASS: Jami DHT node at ${RTR_EXT_IP}:${probe_port} is reachable through UPnP."
        record_assertion "dht_reachable_${probe_port}" "passed" "${started_ms}" \
            "DHT ping succeeded through the UPnP mapping."
    else
        echo "FAIL: DNAT rule present but DHT ping timed out."
        echo "      Check: sudo ip netns exec lan ss -ulnp | grep ${probe_port}"
        record_assertion "dht_reachable_${probe_port}" "failed" "${started_ms}" \
            "DNAT rule existed but the DHT ping timed out."
        mark_failed
        return 1
    fi
}

if [[ -n "${PORT}" ]]; then
    run_explicit_probe "${PORT}" || true
    exit "${PROBE_EXIT_CODE}"
fi

echo "=== UPnP Port Analysis ==="
echo

m=""
if m="$(mapped_ports)"; then
    :
fi
b=""
if b="$(bound_ports)"; then
    :
fi

append_text_capture "mapped UDP ports" "command-output" "mapped-ports.txt" "${m:-}"
append_text_capture "bound UDP ports" "command-output" "bound-ports.txt" "${b:-}"

echo "UPnP-mapped UDP ports (miniupnpd nft prerouting):"
start_ms="$(vnet_now_ms)"
if [[ -n "${m}" ]]; then
    echo "${m}" | sed 's/^/    /'
    record_assertion "upnp_mapped_ports_present" "passed" "${start_ms}" \
        "At least one UPnP-mapped UDP port exists."
else
    echo "    (none — Jami has not registered any UPnP mappings)"
    record_assertion "upnp_mapped_ports_present" "failed" "${start_ms}" \
        "No UPnP-mapped UDP ports were found."
    mark_failed
    exit "${PROBE_EXIT_CODE}"
fi

echo
echo "Bound UDP ports in lan namespace (ss -ulnp, excl. loopback/SSDP):"
start_ms="$(vnet_now_ms)"
if [[ -n "${b}" ]]; then
    echo "${b}" | sed 's/^/    /'
    record_assertion "bound_udp_ports_present" "passed" "${start_ms}" \
        "At least one bound UDP port exists in the LAN namespace."
else
    echo "    (none — Jami is not listening on any UDP ports)"
    record_assertion "bound_udp_ports_present" "failed" "${start_ms}" \
        "No bound UDP ports were found in the LAN namespace."
    mark_failed
    exit "${PROBE_EXIT_CODE}"
fi

both="$(comm -12 <(printf '%s\n' "${m}") <(printf '%s\n' "${b}"))"
append_text_capture "overlapping UDP ports" "command-output" "overlapping-ports.txt" "${both:-}"

echo
start_ms="$(vnet_now_ms)"
if [[ -z "${both}" ]]; then
    echo "[FAIL]  No port is both UPnP-mapped and actively bound."
    echo "        The UPnP external port and the DHT bound port are out of sync."
    record_assertion "mapped_and_bound_overlap" "failed" "${start_ms}" \
        "No UPnP-mapped UDP port is actively bound in the LAN namespace."
    mark_failed
    exit "${PROBE_EXIT_CODE}"
fi

record_assertion "mapped_and_bound_overlap" "passed" "${start_ms}" \
    "At least one UDP port is both mapped and bound."
echo "Ports both mapped and bound (candidates for DHT ping):"
echo "${both}" | sed 's/^/    /'

reachable_count=0
failed_count=0
echo
while IFS= read -r port; do
    [[ -n "${port}" ]] || continue
    echo "[PROBE] DHT ping to ${RTR_EXT_IP}:${port} from ${WAN_NS}..."
    started_ms="$(vnet_now_ms)"
    if vnet_results_capture_command "wan DHT probe port ${port}" "command-output" \
        "dht-probe-port-${port}.txt" \
        ip netns exec "${WAN_NS}" python3 "${SCRIPT_DIR}/dht-node-pinger.py" "${RTR_EXT_IP}" "${port}"; then
        echo "[PASS]  Port ${port}: DHT node reachable through UPnP mapping."
        record_assertion "dht_reachable_${port}" "passed" "${started_ms}" \
            "Port ${port} is reachable through the UPnP mapping."
        reachable_count=$((reachable_count + 1))
    else
        echo "[FAIL]  Port ${port}: DNAT present and port bound, but DHT ping timed out."
        record_assertion "dht_reachable_${port}" "failed" "${started_ms}" \
            "Port ${port} is mapped and bound but the DHT ping timed out."
        failed_count=$((failed_count + 1))
        mark_failed
    fi
done <<< "${both}"

vnet_results_metric "reachable_ports" "${reachable_count}"
vnet_results_metric "failed_ports" "${failed_count}"

exit "${PROBE_EXIT_CODE}"
