#!/usr/bin/env bash
# Verify that a UPnP-mapped port on the virtual router is reachable from the WAN side.
# Must be run as root from the host (not from inside a namespace).
#
# Usage:  sudo bash probe-dht-from-wan.sh [port]
#
# Without a port argument, the script:
#   1. Lists all UPnP-mapped UDP ports (miniupnpd nft table)
#   2. Lists all UDP ports bound by Jami in the lan namespace
#   3. Reports any mismatch (mapped but not bound, or bound but not mapped)
#   4. For ports in both sets, sends a DHT ping from the wan namespace
#
# With an explicit port argument, that port is checked directly.
#
# The router WAN IP (11.0.0.2) matches setup-fake-upnp-network.sh.
#
# miniupnpd on Debian 12 uses native nftables (libmnl) rather than iptables.
# Forwarding rules appear in 'table ip miniupnpd' — not in the iptables MINIUPNPD chain.
set -euo pipefail

RTR_EXT_IP="11.0.0.2"
RTR_NS="rtr"
WAN_NS="wan"
LAN_NS="lan"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "${EUID}" -ne 0 ]]; then
    echo "Error: must be run as root (needed to access rtr, lan, and wan namespaces)."
    exit 1
fi

# Pre-flight: verify namespaces exist
for ns in "${RTR_NS}" "${WAN_NS}" "${LAN_NS}"; do
    if ! ip netns list | grep -qw "${ns}"; then
        echo "Error: namespace '${ns}' does not exist. Run setup-fake-upnp-network.sh first."
        exit 1
    fi
done

# Collect the set of UDP ports with DNAT rules in miniupnpd's nft table.
mapped_ports() {
    ip netns exec "${RTR_NS}" nft list chain ip miniupnpd prerouting 2>/dev/null \
        | grep -oP 'udp dport \K[0-9]+' | sort -u
}

# Collect the set of non-loopback, non-multicast IPv4 UDP ports bound in the lan namespace.
bound_ports() {
    ip netns exec "${LAN_NS}" ss -ulnp 2>/dev/null \
        | awk 'NR > 1 && $1 == "UNCONN" {
            split($4, a, ":");
            port = a[length(a)];
            if ($4 !~ /^127\./ && $4 !~ /^\[::1\]/ && port != "1900")
                print port
        }' | sort -u
}

if [[ $# -ge 1 ]]; then
    # Explicit port: check DNAT + DHT ping directly.
    PORT="$1"

    echo "[CHECK] DNAT for UDP port ${PORT} in ${RTR_NS} namespace..."
    if ip netns exec "${RTR_NS}" nft list chain ip miniupnpd prerouting 2>/dev/null \
            | grep -q "dport ${PORT} "; then
        echo "[OK]    DNAT rule found in miniupnpd nftables table."
    else
        echo "[FAIL]  Port ${PORT} has no DNAT rule in miniupnpd's nftables table."
        echo ""
        echo "  Currently mapped UDP ports:"
        mapped_ports | sed 's/^/    /'
        exit 1
    fi

    echo "[PROBE] Sending DHT ping to ${RTR_EXT_IP}:${PORT} from ${WAN_NS} namespace..."
    if ip netns exec "${WAN_NS}" python3 "${SCRIPT_DIR}/dht-node-pinger.py" "${RTR_EXT_IP}" "${PORT}"; then
        echo "PASS: Jami DHT node at ${RTR_EXT_IP}:${PORT} is reachable through UPnP."
    else
        echo "FAIL: DNAT rule present but DHT ping timed out."
        echo "      Check: sudo ip netns exec lan ss -ulnp | grep ${PORT}"
        exit 1
    fi
    exit 0
fi

# --- Auto mode: compare mapped vs bound ports and report ---
echo "=== UPnP Port Analysis ==="
echo ""

m=$(mapped_ports)
b=$(bound_ports)

echo "UPnP-mapped UDP ports (miniupnpd nft prerouting):"
if [[ -n "${m}" ]]; then
    echo "${m}" | sed 's/^/    /'
else
    echo "    (none — Jami has not registered any UPnP mappings)"
    exit 1
fi

echo ""
echo "Bound UDP ports in lan namespace (ss -ulnp, excl. loopback/SSDP):"
if [[ -n "${b}" ]]; then
    echo "${b}" | sed 's/^/    /'
else
    echo "    (none — Jami is not listening on any UDP ports)"
    exit 1
fi

# Find intersection
both=$(comm -12 <(echo "${m}") <(echo "${b}"))

if [[ -z "${both}" ]]; then
    echo ""
    echo "[FAIL]  No port is both UPnP-mapped and actively bound."
    echo "        The UPnP external port and the DHT bound port are out of sync."
    exit 1
fi

echo ""
echo "Ports both mapped and bound (candidates for DHT ping):"
echo "${both}" | sed 's/^/    /'

# Probe each port in the intersection.
echo ""
exit_code=0
while IFS= read -r port; do
    echo "[PROBE] DHT ping to ${RTR_EXT_IP}:${port} from ${WAN_NS}..."
    if ip netns exec "${WAN_NS}" python3 "${SCRIPT_DIR}/dht-node-pinger.py" "${RTR_EXT_IP}" "${port}"; then
        echo "[PASS]  Port ${port}: DHT node reachable through UPnP mapping."
    else
        echo "[FAIL]  Port ${port}: DNAT present and port bound, but DHT ping timed out."
        exit_code=1
    fi
done <<< "${both}"

exit "${exit_code}"
