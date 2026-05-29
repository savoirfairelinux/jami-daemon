#!/usr/bin/env bash
# QoS Integration Test Lab — Network degradation with tc/netem
#
# Creates a virtual network between two Jami peers and applies configurable
# network impairments to test audio-over-video QoS strategies.
#
# Architecture:
#   ┌──────────────┐       ┌──────────────────┐       ┌──────────────┐
#   │  peer-a      │───────│  bridge (netem)   │───────│  peer-b      │
#   │ 10.99.0.1    │       │  10.99.0.254      │       │ 10.99.0.2    │
#   └──────────────┘       └──────────────────┘       └──────────────┘
#
# Netem rules are applied on the bridge interfaces to simulate degradation
# in both directions independently.
#
# Usage:
#   sudo ./setup-qos-lab.sh [up|down|status]
#   sudo ./setup-qos-lab.sh profile <name>
#
# Profiles: good, moderate, poor, terrible, custom
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    cat <<EOF
Usage: sudo $0 <command>

Commands:
  up                 Create the virtual network lab
  down               Tear down the lab
  status             Show current netem settings and interface stats
  profile <name>     Apply a network profile (see below)
  custom <opts>      Apply custom netem options (passed directly to tc)

Network profiles:
  good               No impairment (clear all netem rules)
  moderate           50ms delay, 1% loss, 1Mbit rate limit
  poor               100ms delay ±30ms, 5% loss, 300Kbit rate limit
  terrible           200ms delay ±80ms, 15% loss, 150Kbit rate limit
  audio-only         50Kbit rate limit (barely enough for audio)

Examples:
  sudo $0 up
  sudo $0 profile poor
  sudo $0 custom "delay 150ms 40ms loss 8% rate 200kbit"
  sudo $0 down
EOF
    exit 1
}

if [[ "${EUID}" -ne 0 ]]; then
    echo "Error: must be run as root."
    exit 1
fi

# Namespace and interface names
NS_A="peer-a"
NS_B="peer-b"
VETH_A="veth-a"
VETH_B="veth-b"
BR_A="br-a"
BR_B="br-b"
# Use distinct subnets so host can route between them (L3 forwarding)
IP_A="10.99.1.1"
IP_B="10.99.2.1"
GW_A="10.99.1.254"
GW_B="10.99.2.254"

cleanup() {
    ip netns del ${NS_A} 2>/dev/null || true
    ip netns del ${NS_B} 2>/dev/null || true
    ip link del ${BR_A} 2>/dev/null || true
    ip link del ${BR_B} 2>/dev/null || true
}

do_up() {
    echo "[+] Creating QoS test lab..."
    cleanup

    # Trap to roll back on failure
    trap 'echo "[!] Setup failed, rolling back..."; do_down; exit 1' ERR

    # Save current ip_forward state in a private directory
    STATE_DIR="/run/jami-qos-lab"
    rm -rf "$STATE_DIR"
    mkdir -m 700 "$STATE_DIR"
    cat /proc/sys/net/ipv4/ip_forward > "$STATE_DIR/ipfwd"

    # Create namespaces
    ip netns add ${NS_A}
    ip netns add ${NS_B}

    # Create veth pairs: peer-a <-> host, peer-b <-> host
    ip link add ${VETH_A} type veth peer name ${BR_A}
    ip link add ${VETH_B} type veth peer name ${BR_B}

    # Move peer ends into namespaces
    ip link set ${VETH_A} netns ${NS_A}
    ip link set ${VETH_B} netns ${NS_B}

    # Configure peer-a namespace (subnet 10.99.1.0/24)
    ip -n ${NS_A} link set lo up
    ip -n ${NS_A} addr add ${IP_A}/24 dev ${VETH_A}
    ip -n ${NS_A} link set ${VETH_A} up
    ip -n ${NS_A} route add default via ${GW_A}

    # Configure peer-b namespace (subnet 10.99.2.0/24)
    ip -n ${NS_B} link set lo up
    ip -n ${NS_B} addr add ${IP_B}/24 dev ${VETH_B}
    ip -n ${NS_B} link set ${VETH_B} up
    ip -n ${NS_B} route add default via ${GW_B}

    # Configure bridge-side interfaces on host (act as gateway for each peer)
    ip addr add ${GW_A}/24 dev ${BR_A}
    ip link set ${BR_A} up
    ip addr add ${GW_B}/24 dev ${BR_B}
    ip link set ${BR_B} up

    # Enable forwarding between the two bridge interfaces
    sysctl -w net.ipv4.ip_forward=1 >/dev/null

    # Add forwarding rules (check first to avoid duplicates)
    iptables -C FORWARD -i ${BR_A} -o ${BR_B} -j ACCEPT 2>/dev/null \
        || iptables -A FORWARD -i ${BR_A} -o ${BR_B} -j ACCEPT
    iptables -C FORWARD -i ${BR_B} -o ${BR_A} -j ACCEPT 2>/dev/null \
        || iptables -A FORWARD -i ${BR_B} -o ${BR_A} -j ACCEPT

    echo "[+] Lab is up."
    echo "    Peer A (${IP_A}): sudo ip netns exec ${NS_A} bash"
    echo "    Peer B (${IP_B}): sudo ip netns exec ${NS_B} bash"
    echo "    Apply degradation: sudo $0 profile poor"
    echo ""
    echo "[+] Connectivity check:"
    ip netns exec ${NS_A} ping -c 1 -W 2 ${IP_B} && echo "    OK: peer-a -> peer-b" || echo "    FAIL"

    # Clear ERR trap on success
    trap - ERR
}

do_down() {
    echo "[+] Tearing down QoS test lab..."
    # Remove forwarding rules (loop to handle duplicates)
    while iptables -D FORWARD -i ${BR_A} -o ${BR_B} -j ACCEPT 2>/dev/null; do :; done
    while iptables -D FORWARD -i ${BR_B} -o ${BR_A} -j ACCEPT 2>/dev/null; do :; done
    cleanup
    # Restore ip_forward state only if it's still what we set (1)
    local state_dir="/run/jami-qos-lab"
    if [[ -f "$state_dir/ipfwd" ]]; then
        local saved_val
        saved_val="$(cat "$state_dir/ipfwd")"
        local current_val
        current_val="$(sysctl -n net.ipv4.ip_forward)"
        if [[ "$current_val" == "1" && "$saved_val" != "1" ]]; then
            sysctl -w net.ipv4.ip_forward="$saved_val" >/dev/null
        elif [[ "$current_val" != "$saved_val" ]]; then
            echo "    Warning: ip_forward changed externally ($current_val), not restoring"
        fi
        rm -rf "$state_dir"
    fi
    echo "[+] Done."
}

do_status() {
    echo "=== Netem on ${BR_A} (toward peer-a) ==="
    tc qdisc show dev ${BR_A} 2>/dev/null || echo "  (no qdisc / interface not found)"
    echo ""
    echo "=== Netem on ${BR_B} (toward peer-b) ==="
    tc qdisc show dev ${BR_B} 2>/dev/null || echo "  (no qdisc / interface not found)"
    echo ""
    echo "=== Interface stats ==="
    ip -s link show ${BR_A} 2>/dev/null | grep -A2 "TX\|RX" || true
    ip -s link show ${BR_B} 2>/dev/null | grep -A2 "TX\|RX" || true
}

apply_netem() {
    local opts="$1"
    if [[ -z "$opts" ]]; then
        # Clear netem
        tc qdisc del dev ${BR_A} root 2>/dev/null || true
        tc qdisc del dev ${BR_B} root 2>/dev/null || true
        echo "[+] Cleared all netem rules (good network)."
    else
        # Apply symmetric degradation on both directions
        tc qdisc replace dev ${BR_A} root netem ${opts}
        tc qdisc replace dev ${BR_B} root netem ${opts}
        echo "[+] Applied netem: ${opts}"
    fi
}

apply_profile() {
    local profile="$1"
    case "$profile" in
        good)
            apply_netem ""
            ;;
        moderate)
            apply_netem "delay 50ms 10ms loss 1% rate 1mbit"
            ;;
        poor)
            apply_netem "delay 100ms 30ms loss 5% rate 300kbit"
            ;;
        terrible)
            apply_netem "delay 200ms 80ms loss 15% rate 150kbit"
            ;;
        audio-only)
            apply_netem "delay 50ms 10ms loss 2% rate 50kbit"
            ;;
        *)
            echo "Unknown profile: $profile"
            echo "Available: good, moderate, poor, terrible, audio-only"
            exit 1
            ;;
    esac
}

# Main command dispatch
cmd="${1:-}"
shift || true

case "$cmd" in
    up)       do_up ;;
    down)     do_down ;;
    status)   do_status ;;
    profile)  apply_profile "${1:-}" ;;
    custom)   apply_netem "$*" ;;
    -h|--help|"") usage ;;
    *)        echo "Unknown command: $cmd"; usage ;;
esac
