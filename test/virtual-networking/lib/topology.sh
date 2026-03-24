#!/usr/bin/env bash

if [[ -n "${JAMI_VNET_TOPOLOGY_SH:-}" ]]; then
    return 0
fi
JAMI_VNET_TOPOLOGY_SH=1

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/netns.sh"

vnet_connect_namespaces_with_veth() {
    local ns_a="$1"
    local iface_a="$2"
    local ns_b="$3"
    local iface_b="$4"

    ip link add "${iface_a}" type veth peer name "${iface_b}"
    ip link set "${iface_a}" netns "${ns_a}"
    ip link set "${iface_b}" netns "${ns_b}"
}

vnet_set_loopbacks_up() {
    local ns
    for ns in "$@"; do
        ip -n "${ns}" link set lo up
    done
}

vnet_configure_ipv4_interface() {
    local ns="$1"
    local iface="$2"
    local cidr="$3"

    ip -n "${ns}" addr add "${cidr}" dev "${iface}"
    ip -n "${ns}" link set "${iface}" up
}

vnet_add_default_route() {
    local ns="$1"
    local via="$2"
    local dev="${3:-}"
    local metric="${4:-}"

    local cmd=(ip -n "${ns}" route add default via "${via}")
    if [[ -n "${dev}" ]]; then
        cmd+=(dev "${dev}")
    fi
    if [[ -n "${metric}" ]]; then
        cmd+=(metric "${metric}")
    fi
    "${cmd[@]}"
}

vnet_replace_default_route() {
    local ns="$1"
    local via="$2"
    local dev="${3:-}"
    local metric="${4:-}"

    local cmd=(ip -n "${ns}" route replace default via "${via}")
    if [[ -n "${dev}" ]]; then
        cmd+=(dev "${dev}")
    fi
    if [[ -n "${metric}" ]]; then
        cmd+=(metric "${metric}")
    fi
    "${cmd[@]}"
}

vnet_add_device_route() {
    local ns="$1"
    local destination="$2"
    local dev="$3"
    local metric="${4:-}"

    local cmd=(ip -n "${ns}" route add "${destination}" dev "${dev}")
    if [[ -n "${metric}" ]]; then
        cmd+=(metric "${metric}")
    fi
    "${cmd[@]}"
}

vnet_enable_ipv4_forwarding() {
    local ns="$1"
    ip netns exec "${ns}" sysctl -w net.ipv4.ip_forward=1 >/dev/null
}

vnet_setup_basic_nat_router() {
    local ns="$1"
    local lan_iface="$2"
    local wan_iface="$3"

    vnet_enable_ipv4_forwarding "${ns}"
    ip netns exec "${ns}" iptables -t nat -A POSTROUTING -o "${wan_iface}" -j MASQUERADE
    ip netns exec "${ns}" iptables -A FORWARD -i "${wan_iface}" -o "${lan_iface}" \
        -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
    ip netns exec "${ns}" iptables -A FORWARD -i "${lan_iface}" -o "${wan_iface}" -j ACCEPT
}
