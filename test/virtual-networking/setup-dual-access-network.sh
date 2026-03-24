#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/common.sh"
source "${SCRIPT_DIR}/lib/upnp.sh"
source "${SCRIPT_DIR}/topologies/dual-access.sh"

usage() {
    echo "Usage: sudo $0 [--no-hold] [up|down]"
    echo "  up       Create the dual-access topology (default)"
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
vnet_topology_dual_access_defaults

UUID_WIFI="deadbeef-dead-beef-dead-0000000000b1"
LAB_NAME="dual-access"
STATE_FILE="$(vnet_state_file_path "${LAB_NAME}")"
TMPDIR=""
MINIUPNPD_CONFIG_WIFI=""
MINIUPNPD_PIDFILE_WIFI=""
MINIUPNPD_LOGFILE_WIFI=""
UPNPC_DISCOVERY_LOG=""

cleanup() {
    if vnet_load_env_file "${STATE_FILE}"; then
        :
    fi

    vnet_kill_pidfile "${MINIUPNPD_PIDFILE_WIFI:-}"

    mapfile -t topology_namespaces < <(vnet_topology_dual_access_namespaces)
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

    mapfile -t topology_namespaces < <(vnet_topology_dual_access_namespaces)
    if ! vnet_assert_namespaces_absent "${topology_namespaces[@]}"; then
        echo "Run: sudo $0 down" >&2
        exit 1
    fi
}

write_state() {
    mapfile -t topology_vars < <(vnet_topology_dual_access_state_vars)
    vnet_write_env_file "${STATE_FILE}" \
        LAB_NAME STATE_FILE TMPDIR UUID_WIFI MINIUPNPD_CONFIG_WIFI MINIUPNPD_PIDFILE_WIFI \
        MINIUPNPD_LOGFILE_WIFI UPNPC_DISCOVERY_LOG "${topology_vars[@]}"
}

setup_upnp() {
    vnet_write_miniupnpd_config \
        "${MINIUPNPD_CONFIG_WIFI}" \
        "${WIFI_RTR_IFACE_WAN}" \
        "${WIFI_RTR_IFACE_LAN}" \
        "${UUID_WIFI}" \
        "ns-igd-wifi" \
        "${WIFI_RTR_EXT_IP}"
    vnet_start_miniupnpd "${WIFI_RTR_NS}" "${MINIUPNPD_CONFIG_WIFI}" "${MINIUPNPD_PIDFILE_WIFI}" "${MINIUPNPD_LOGFILE_WIFI}"
    vnet_wait_for_upnpc "${NODE_NS}" 10 "${UPNPC_DISCOVERY_LOG}" || true
}

if [[ "${cmd}" == "down" ]]; then
    cleanup
    exit 0
fi

ensure_lab_not_running
TMPDIR="$(vnet_make_state_dir "jami-dual-access")"
MINIUPNPD_CONFIG_WIFI="${TMPDIR}/miniupnpd-wifi.conf"
MINIUPNPD_PIDFILE_WIFI="${TMPDIR}/miniupnpd-wifi.pid"
MINIUPNPD_LOGFILE_WIFI="${TMPDIR}/miniupnpd-wifi.log"
UPNPC_DISCOVERY_LOG="${TMPDIR}/upnpc-discovery.txt"
write_state

trap 'echo; echo "[+] Cleaning up..."; cleanup' EXIT

vnet_topology_dual_access_up
setup_upnp

echo "[+] Discovery from node side:"
if [[ -s "${UPNPC_DISCOVERY_LOG}" ]]; then
    cat "${UPNPC_DISCOVERY_LOG}"
else
    ip netns exec "${NODE_NS}" upnpc -s || true
fi

echo
echo "[+] Topology is up (tmpdir: ${TMPDIR})."
echo "[+] ${NODE_NS} currently prefers wifi access on ${NODE_WIFI_IFACE} via ${NODE_WIFI_GW} (metric ${NODE_WIFI_METRIC})."
echo "[+] Mobile-like standby access on ${NODE_MOBILE_IFACE} via ${NODE_MOBILE_GW} is present with metric ${NODE_MOBILE_METRIC}."
echo "[+] Only the wifi router exposes a UPnP IGD in this topology."
echo "[+] Inspect routes with: sudo ip -n ${NODE_NS} route"
echo "[+] Try: sudo ip netns exec ${NODE_NS} sudo -u ${SUDO_USER:-$USER} -H bash -l"
if [[ "${hold_open}" -eq 1 ]]; then
    wait
else
    trap - EXIT
    echo "[+] Leaving topology running. Tear it down with: sudo $0 down"
fi
