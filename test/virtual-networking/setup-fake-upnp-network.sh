#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: sudo $0 [up|down]"
    echo "  up       Create the virtual network (default)"
    echo "  down     Tear down the virtual network"
    exit 1
}

if [[ "${EUID}" -ne 0 ]]; then
    echo "Error: must be run as root (or inside a container with CAP_NET_ADMIN + CAP_SYS_ADMIN)."
    exit 1
fi

cmd="up"
for arg in "$@"; do
    case "$arg" in
        up|down) cmd="$arg" ;;
        -h|--help) usage ;;
        *) echo "Unknown argument: $arg"; usage ;;
    esac
done

# LAN
LAN_NS="lan"
# some UPnP libraries have trouble binding on veth-* interfaces,
# use a more "standard" name for the LAN interface
LAN_IFACE="eth0"
LAN_IP="192.168.100.2"
LAN_MULTICAST_IP="224.0.0.0"

# Router/IGD
RTR_NS="rtr"
RTR_IFACE_LAN="veth-rtr-lan"
RTR_IFACE_WAN="veth-rtr-wan"
RTR_TO_LAN_IP="192.168.100.1"
RTR_TO_WAN_IP="11.0.0.2"

# WAN
WAN_NS="wan"
WAN_IFACE="veth-wan"
WAN_IP="11.0.0.1"

UUID="deadbeef-dead-beef-dead-beef-deadbeefdead"
TMPDIR="$(mktemp -d /tmp/jami-upnp-lab.XXXXXX)"

cleanup() {
    ip netns del ${LAN_NS} 2>/dev/null || true
    ip netns del ${RTR_NS} 2>/dev/null || true
    ip netns del ${WAN_NS} 2>/dev/null || true
    rm -rf "${TMPDIR}" 2>/dev/null || true
}

if [[ "$cmd" == "down" ]]; then
    # When tearing down, scan for any lab temp dirs since we don't know
    # which one was used during setup.
    for d in /tmp/jami-upnp-lab.*/; do
        if [[ -f "${d}miniupnpd.pid" ]]; then
            kill "$(cat "${d}miniupnpd.pid")" 2>/dev/null || true
        fi
        rm -rf "$d" 2>/dev/null || true
    done
    ip netns del ${LAN_NS} 2>/dev/null || true
    ip netns del ${RTR_NS} 2>/dev/null || true
    ip netns del ${WAN_NS} 2>/dev/null || true
    exit 0
fi

trap 'echo; echo "[+] Cleaning up..."; cleanup' EXIT

ip netns add ${LAN_NS}
ip netns add ${RTR_NS}
ip netns add ${WAN_NS}

ip link add ${LAN_IFACE} type veth peer name ${RTR_IFACE_LAN}
ip link add ${WAN_IFACE} type veth peer name ${RTR_IFACE_WAN}

ip link set ${LAN_IFACE} netns ${LAN_NS}
ip link set ${RTR_IFACE_LAN} netns ${RTR_NS}
ip link set ${WAN_IFACE} netns ${WAN_NS}
ip link set ${RTR_IFACE_WAN} netns ${RTR_NS}

ip -n ${LAN_NS} link set lo up
ip -n ${RTR_NS} link set lo up

# --- IPv4 lab (default) ---
ip -n ${LAN_NS} addr add ${LAN_IP}/24 dev ${LAN_IFACE}
ip -n ${LAN_NS} link set ${LAN_IFACE} up
ip -n ${LAN_NS} route add default via ${RTR_TO_LAN_IP}

ip -n ${RTR_NS} addr add ${RTR_TO_LAN_IP}/24 dev ${RTR_IFACE_LAN}
ip -n ${RTR_NS} link set ${RTR_IFACE_LAN} up
# Use a non-reserved "public-looking" subnet for WAN (namespaces keep it isolated)
ip -n ${RTR_NS} addr add ${RTR_TO_WAN_IP}/24 dev ${RTR_IFACE_WAN}
ip -n ${RTR_NS} link set ${RTR_IFACE_WAN} up

ip -n ${WAN_NS} addr add ${WAN_IP}/24 dev ${WAN_IFACE}
ip -n ${WAN_NS} link set ${WAN_IFACE} up
ip -n ${WAN_NS} route add default via ${RTR_TO_WAN_IP}

ip netns exec ${RTR_NS} sysctl -w net.ipv4.ip_forward=1 >/dev/null
ip netns exec ${RTR_NS} iptables -t nat -A POSTROUTING -o ${RTR_IFACE_WAN} -j MASQUERADE
ip netns exec ${RTR_NS} iptables -A FORWARD -i ${RTR_IFACE_WAN} -o ${RTR_IFACE_LAN} -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
ip netns exec ${RTR_NS} iptables -A FORWARD -i ${RTR_IFACE_LAN} -o ${RTR_IFACE_WAN} -j ACCEPT

# Add multicast route for UPnP SSDP discovery
ip -n ${LAN_NS} route add ${LAN_MULTICAST_IP}/4 dev ${LAN_IFACE}

cat > "${TMPDIR}/miniupnpd.conf" <<EOF
ext_ifname=${RTR_IFACE_WAN}
listening_ip=${RTR_IFACE_LAN}
enable_upnp=yes
secure_mode=no
system_uptime=yes
uuid=${UUID}
friendly_name=ns-igd
ext_ip=${RTR_TO_WAN_IP}
EOF

ip netns exec ${RTR_NS} miniupnpd -d -f "${TMPDIR}/miniupnpd.conf" -P "${TMPDIR}/miniupnpd.pid" \
    > "${TMPDIR}/miniupnpd.log" 2>&1 &
sleep 1

echo "[+] Discovery from LAN side:"
ip netns exec ${LAN_NS} upnpc -s || true

echo
echo "[+] Lab is up (tmpdir: ${TMPDIR})."
echo "[+] Namespaces will stay until you Ctrl-C or run: sudo $0 down"
echo "[+] Try: sudo ip netns exec ${LAN_NS} sudo -u ${SUDO_USER:-$USER} -H bash -l"
wait