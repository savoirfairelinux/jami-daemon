#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: sudo $0 [--ipv6] [up|down]"
    echo "  --ipv6   Configure the lab with IPv6 addressing (default: IPv4 only)"
    echo "  up       Create the virtual network (default)"
    echo "  down     Tear down the virtual network"
    exit 1
}

if [[ "${EUID}" -ne 0 ]]; then
    echo "Error: must be run as root (or inside a container with CAP_NET_ADMIN + CAP_SYS_ADMIN)."
    exit 1
fi

use_ipv6=false
cmd="up"
for arg in "$@"; do
    case "$arg" in
        --ipv6) use_ipv6=true ;;
        up|down) cmd="$arg" ;;
        -h|--help) usage ;;
        *) echo "Unknown argument: $arg"; usage ;;
    esac
done

TMPDIR="$(mktemp -d /tmp/jami-upnp-lab.XXXXXX)"

cleanup() {
    # Kill miniupnpd if running
    if [[ -f "${TMPDIR}/miniupnpd.pid" ]]; then
        kill "$(cat "${TMPDIR}/miniupnpd.pid")" 2>/dev/null || true
    fi
    ip netns del lan 2>/dev/null || true
    ip netns del rtr 2>/dev/null || true
    ip netns del wan 2>/dev/null || true
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
    ip netns del lan 2>/dev/null || true
    ip netns del rtr 2>/dev/null || true
    ip netns del wan 2>/dev/null || true
    exit 0
fi

cleanup
trap 'echo; echo "[+] Cleaning up..."; cleanup' INT TERM EXIT

ip netns add lan
ip netns add rtr
ip netns add wan

ip link add veth-lan type veth peer name veth-rtr-lan
ip link add veth-wan type veth peer name veth-rtr-wan

ip link set veth-lan netns lan
ip link set veth-rtr-lan netns rtr
ip link set veth-wan netns wan
ip link set veth-rtr-wan netns rtr

# Rename veth-lan to eth0 in the lan namespace (some UPnP libraries
# have trouble binding on veth-* interfaces)
ip -n lan link set veth-lan name eth0

ip -n lan link set lo up
ip -n rtr link set lo up

if [[ "$use_ipv6" == true ]]; then
    # --- IPv6 lab ---
    ip netns exec lan sysctl -w net.ipv6.conf.eth0.disable_ipv6=0 >/dev/null
    ip netns exec rtr sysctl -w net.ipv6.conf.veth-rtr-lan.disable_ipv6=0 >/dev/null
    ip netns exec rtr sysctl -w net.ipv6.conf.veth-rtr-wan.disable_ipv6=0 >/dev/null
    ip netns exec wan sysctl -w net.ipv6.conf.veth-wan.disable_ipv6=0 >/dev/null

    ip -n lan addr add fd00:lan::2/64 dev eth0
    ip -n lan link set eth0 up
    ip -n lan route add default via fd00:lan::1

    ip -n rtr addr add fd00:lan::1/64 dev veth-rtr-lan
    ip -n rtr link set veth-rtr-lan up
    ip -n rtr addr add fd00:wan::2/64 dev veth-rtr-wan
    ip -n rtr link set veth-rtr-wan up

    ip -n wan addr add fd00:wan::1/64 dev veth-wan
    ip -n wan link set veth-wan up
    ip -n wan route add default via fd00:wan::2

    ip netns exec rtr sysctl -w net.ipv6.conf.all.forwarding=1 >/dev/null
    ip netns exec rtr ip6tables -A FORWARD -i veth-rtr-wan -o veth-rtr-lan -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
    ip netns exec rtr ip6tables -A FORWARD -i veth-rtr-lan -o veth-rtr-wan -j ACCEPT
    ip netns exec rtr ip6tables -t nat -A POSTROUTING -o veth-rtr-wan -j MASQUERADE

    RTR_EXT_IP="fd00:wan::2"
    LAN_IFACE="eth0"
else
    # --- IPv4 lab (default) ---
    ip -n lan addr add 192.168.100.2/24 dev eth0
    ip -n lan link set eth0 up
    ip -n lan route add default via 192.168.100.1

    ip -n rtr addr add 192.168.100.1/24 dev veth-rtr-lan
    ip -n rtr link set veth-rtr-lan up
    # Use a non-reserved "public-looking" subnet for WAN (namespaces keep it isolated)
    ip -n rtr addr add 11.0.0.2/24 dev veth-rtr-wan
    ip -n rtr link set veth-rtr-wan up

    ip -n wan addr add 11.0.0.1/24 dev veth-wan
    ip -n wan link set veth-wan up
    ip -n wan route add default via 11.0.0.2

    ip netns exec rtr sysctl -w net.ipv4.ip_forward=1 >/dev/null
    ip netns exec rtr iptables -t nat -A POSTROUTING -o veth-rtr-wan -j MASQUERADE
    ip netns exec rtr iptables -A FORWARD -i veth-rtr-wan -o veth-rtr-lan -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
    ip netns exec rtr iptables -A FORWARD -i veth-rtr-lan -o veth-rtr-wan -j ACCEPT

    RTR_EXT_IP="11.0.0.2"
    LAN_IFACE="eth0"
fi

# Add multicast route for UPnP SSDP discovery
ip -n lan route add 224.0.0.0/4 dev "${LAN_IFACE}"

cat > "${TMPDIR}/miniupnpd.conf" <<EOF
ext_ifname=veth-rtr-wan
listening_ip=veth-rtr-lan
enable_upnp=yes
secure_mode=no
system_uptime=yes
uuid=deadbeef-dead-beef-dead-beefdeadbeef
friendly_name=ns-igd
ext_ip=${RTR_EXT_IP}
EOF

ip netns exec rtr miniupnpd -d -f "${TMPDIR}/miniupnpd.conf" -P "${TMPDIR}/miniupnpd.pid" \
    > "${TMPDIR}/miniupnpd.log" 2>&1 &
sleep 1

echo "[+] Discovery from LAN side:"
ip netns exec lan upnpc -s || true

echo
echo "[+] Lab is up (tmpdir: ${TMPDIR})."
echo "[+] Namespaces will stay until you Ctrl-C or run: sudo $0 down"
echo "[+] Try: sudo ip netns exec lan sudo -u ${SUDO_USER:-$USER} -H bash -l"
wait