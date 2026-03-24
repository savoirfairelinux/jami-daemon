#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/common.sh"
source "${SCRIPT_DIR}/lib/upnp.sh"
source "${SCRIPT_DIR}/topologies/single-router.sh"

usage() {
    echo "Usage: sudo $0 [--no-hold] [up|down]"
    echo "  up       Create the virtual network (default)"
    echo "  down     Tear down the virtual network"
    echo "  --no-hold  Exit after setup and leave the lab running"
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

vnet_topology_single_router_defaults

UUID="deadbeef-dead-beef-dead-beef-deadbeefdead"
LAB_NAME="fake-upnp-network"
STATE_FILE="$(vnet_state_file_path "${LAB_NAME}")"
TMPDIR=""
MINIUPNPD_CONFIG=""
MINIUPNPD_PIDFILE=""
MINIUPNPD_LOGFILE=""
UPNPC_DISCOVERY_LOG=""

cleanup() {
    if vnet_load_env_file "${STATE_FILE}"; then
        :
    fi

    if [[ -n "${MINIUPNPD_PIDFILE:-}" ]]; then
        vnet_kill_pidfile "${MINIUPNPD_PIDFILE}"
    fi

    mapfile -t topology_namespaces < <(vnet_topology_single_router_namespaces)
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

    mapfile -t topology_namespaces < <(vnet_topology_single_router_namespaces)
    if ! vnet_assert_namespaces_absent "${topology_namespaces[@]}"; then
        echo "Run: sudo $0 down" >&2
        exit 1
    fi
}

write_state() {
    mapfile -t topology_vars < <(vnet_topology_single_router_state_vars)
    vnet_write_env_file "${STATE_FILE}" \
        LAB_NAME STATE_FILE TMPDIR UUID MINIUPNPD_CONFIG MINIUPNPD_PIDFILE \
        MINIUPNPD_LOGFILE UPNPC_DISCOVERY_LOG "${topology_vars[@]}"
}

setup_upnp() {
    vnet_write_miniupnpd_config \
        "${MINIUPNPD_CONFIG}" \
        "${RTR_IFACE_WAN}" \
        "${RTR_IFACE_LAN}" \
        "${UUID}" \
        "ns-igd" \
        "${RTR_EXT_IP}"

    vnet_start_miniupnpd "${RTR_NS}" "${MINIUPNPD_CONFIG}" "${MINIUPNPD_PIDFILE}" "${MINIUPNPD_LOGFILE}"
    vnet_wait_for_upnpc "${LAN_NS}" 10 "${UPNPC_DISCOVERY_LOG}" || true
}

if [[ "${cmd}" == "down" ]]; then
    cleanup
    exit 0
fi

ensure_lab_not_running
TMPDIR="$(vnet_make_state_dir "jami-upnp-lab")"
MINIUPNPD_CONFIG="${TMPDIR}/miniupnpd.conf"
MINIUPNPD_PIDFILE="${TMPDIR}/miniupnpd.pid"
MINIUPNPD_LOGFILE="${TMPDIR}/miniupnpd.log"
UPNPC_DISCOVERY_LOG="${TMPDIR}/upnpc-discovery.txt"
write_state

trap 'echo; echo "[+] Cleaning up..."; cleanup' EXIT

vnet_topology_single_router_up
setup_upnp

echo "[+] Discovery from LAN side:"
if [[ -s "${UPNPC_DISCOVERY_LOG}" ]]; then
    cat "${UPNPC_DISCOVERY_LOG}"
else
    ip netns exec "${LAN_NS}" upnpc -s || true
fi

echo
echo "[+] Lab is up (tmpdir: ${TMPDIR})."
echo "[+] Namespaces will stay until you Ctrl-C or run: sudo $0 down"
echo "[+] Try: sudo ip netns exec ${LAN_NS} sudo -u ${SUDO_USER:-$USER} -H bash -l"
if [[ "${hold_open}" -eq 1 ]]; then
    wait
else
    trap - EXIT
    echo "[+] Leaving lab running. Tear it down with: sudo $0 down"
fi
