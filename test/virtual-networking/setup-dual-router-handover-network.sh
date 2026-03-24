#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/common.sh"
source "${SCRIPT_DIR}/lib/upnp.sh"
source "${SCRIPT_DIR}/topologies/dual-router-handover.sh"

usage() {
    echo "Usage: sudo $0 [--no-hold] [up|down]"
    echo "  up       Create the dual-router handover topology (default)"
    echo "  down     Tear down the topology"
    echo "  --no-hold  Exit after setup and leave the topology running"
    exit 1
}

cmd="up"
hold_open=1
for arg in "$@"; do
    case "$arg" in
        up|down) cmd="$arg" ;;
        --no-hold) hold_open=0 ;;
        -h|--help) usage ;;
        *) echo "Unknown argument: $arg"; usage ;;
    esac
done

vnet_require_root
vnet_require_commands ip iptables miniupnpd upnpc mktemp sysctl
vnet_topology_dual_router_handover_defaults

UUID_A="deadbeef-dead-beef-dead-0000000000a1"
UUID_B="deadbeef-dead-beef-dead-0000000000a2"
LAB_NAME="dual-router-handover"
STATE_FILE="$(vnet_state_file_path "${LAB_NAME}")"
TMPDIR=""
MINIUPNPD_CONFIG_A=""
MINIUPNPD_PIDFILE_A=""
MINIUPNPD_LOGFILE_A=""
MINIUPNPD_CONFIG_B=""
MINIUPNPD_PIDFILE_B=""
MINIUPNPD_LOGFILE_B=""
UPNPC_DISCOVERY_LOG=""

cleanup() {
    if vnet_load_env_file "${STATE_FILE}"; then
        :
    fi

    vnet_kill_pidfile "${MINIUPNPD_PIDFILE_A:-}"
    vnet_kill_pidfile "${MINIUPNPD_PIDFILE_B:-}"

    mapfile -t topology_namespaces < <(vnet_topology_dual_router_handover_namespaces)
    vnet_delete_namespaces "${topology_namespaces[@]}"
    if [[ -n "${TMPDIR:-}" ]]; then
        rm -rf "${TMPDIR}" 2>/dev/null || true
    fi
    rm -f "${STATE_FILE}"
}

ensure_lab_not_running() {
    if [[ -f "${STATE_FILE}" ]]; then
        echo "Error: lab state already exists at ${STATE_FILE}. Run: sudo $0 down" >&2
        exit 1
    fi

    mapfile -t topology_namespaces < <(vnet_topology_dual_router_handover_namespaces)
    if ! vnet_assert_namespaces_absent "${topology_namespaces[@]}"; then
        echo "Run: sudo $0 down" >&2
        exit 1
    fi
}

write_state() {
    mapfile -t topology_vars < <(vnet_topology_dual_router_handover_state_vars)
    vnet_write_env_file "${STATE_FILE}" \
        LAB_NAME STATE_FILE TMPDIR UUID_A UUID_B MINIUPNPD_CONFIG_A MINIUPNPD_PIDFILE_A \
        MINIUPNPD_LOGFILE_A MINIUPNPD_CONFIG_B MINIUPNPD_PIDFILE_B MINIUPNPD_LOGFILE_B \
        UPNPC_DISCOVERY_LOG "${topology_vars[@]}"
}

setup_upnp() {
    vnet_write_miniupnpd_config \
        "${MINIUPNPD_CONFIG_A}" \
        "${RTR_A_IFACE_WAN}" \
        "${RTR_A_IFACE_LAN}" \
        "${UUID_A}" \
        "ns-igd-a" \
        "${RTR_A_EXT_IP}"
    vnet_start_miniupnpd "${RTR_A_NS}" "${MINIUPNPD_CONFIG_A}" "${MINIUPNPD_PIDFILE_A}" "${MINIUPNPD_LOGFILE_A}"

    vnet_write_miniupnpd_config \
        "${MINIUPNPD_CONFIG_B}" \
        "${RTR_B_IFACE_WAN}" \
        "${RTR_B_IFACE_LAN}" \
        "${UUID_B}" \
        "ns-igd-b" \
        "${RTR_B_EXT_IP}"
    vnet_start_miniupnpd "${RTR_B_NS}" "${MINIUPNPD_CONFIG_B}" "${MINIUPNPD_PIDFILE_B}" "${MINIUPNPD_LOGFILE_B}"

    vnet_wait_for_upnpc "${NODE_NS}" 10 "${UPNPC_DISCOVERY_LOG}" || true
}

if [[ "${cmd}" == "down" ]]; then
    cleanup
    exit 0
fi

ensure_lab_not_running
TMPDIR="$(vnet_make_state_dir "jami-dual-router-handover")"
MINIUPNPD_CONFIG_A="${TMPDIR}/miniupnpd-a.conf"
MINIUPNPD_PIDFILE_A="${TMPDIR}/miniupnpd-a.pid"
MINIUPNPD_LOGFILE_A="${TMPDIR}/miniupnpd-a.log"
MINIUPNPD_CONFIG_B="${TMPDIR}/miniupnpd-b.conf"
MINIUPNPD_PIDFILE_B="${TMPDIR}/miniupnpd-b.pid"
MINIUPNPD_LOGFILE_B="${TMPDIR}/miniupnpd-b.log"
UPNPC_DISCOVERY_LOG="${TMPDIR}/upnpc-discovery.txt"
write_state

trap 'echo; echo "[+] Cleaning up..."; cleanup' EXIT

vnet_topology_dual_router_handover_up
setup_upnp

echo "[+] Discovery from node side:"
if [[ -s "${UPNPC_DISCOVERY_LOG}" ]]; then
    cat "${UPNPC_DISCOVERY_LOG}"
else
    ip netns exec "${NODE_NS}" upnpc -s || true
fi

echo
echo "[+] Topology is up (tmpdir: ${TMPDIR})."
echo "[+] ${NODE_NS} currently prefers ${NODE_IFACE_A} via ${NODE_A_GW} (metric ${NODE_ROUTE_A_METRIC})."
echo "[+] ${NODE_IFACE_B} via ${NODE_B_GW} is available as a standby route (metric ${NODE_ROUTE_B_METRIC})."
echo "[+] Inspect routes with: sudo ip -n ${NODE_NS} route"
echo "[+] Try: sudo ip netns exec ${NODE_NS} sudo -u ${SUDO_USER:-$USER} -H bash -l"
if [[ "${hold_open}" -eq 1 ]]; then
    wait
else
    trap - EXIT
    echo "[+] Leaving topology running. Tear it down with: sudo $0 down"
fi
