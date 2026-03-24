#!/usr/bin/env bash

if [[ -n "${JAMI_VNET_TOPOLOGY_SINGLE_ROUTER_SH:-}" ]]; then
    return 0
fi
JAMI_VNET_TOPOLOGY_SINGLE_ROUTER_SH=1

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/topology.sh"

# Baseline lab used by the original fake-UPnP setup:
#   lan (Jami node) -> rtr (NAT + IGD) -> wan (external peer)
# The LAN namespace also keeps multicast traffic on its local interface so SSDP
# discovery can reach the router-side IGD directly.
vnet_topology_single_router_defaults() {
    # Jami-side namespace behind the router.
    : "${LAN_NS:=lan}"
    : "${LAN_IFACE:=eth0}"
    : "${LAN_IP_CIDR:=192.168.100.2/24}"
    : "${LAN_GW:=192.168.100.1}"
    : "${LAN_MULTICAST_CIDR:=224.0.0.0/4}"

    # Router namespace exposing the synthetic public address.
    : "${RTR_NS:=rtr}"
    : "${RTR_IFACE_LAN:=veth-rtr-lan}"
    : "${RTR_IFACE_WAN:=veth-rtr-wan}"
    : "${RTR_LAN_IP_CIDR:=192.168.100.1/24}"
    : "${RTR_WAN_IP_CIDR:=11.0.0.2/24}"
    : "${RTR_EXT_IP:=11.0.0.2}"

    # External peer namespace used by WAN-side probes.
    : "${WAN_NS:=wan}"
    : "${WAN_IFACE:=veth-wan}"
    : "${WAN_IP_CIDR:=11.0.0.1/24}"
    : "${WAN_GW:=11.0.0.2}"
}

vnet_topology_single_router_namespaces() {
    printf '%s\n' "${LAN_NS}" "${RTR_NS}" "${WAN_NS}"
}

vnet_topology_single_router_state_vars() {
    cat <<'EOF'
LAN_NS
LAN_IFACE
LAN_IP_CIDR
LAN_GW
LAN_MULTICAST_CIDR
RTR_NS
RTR_IFACE_LAN
RTR_IFACE_WAN
RTR_LAN_IP_CIDR
RTR_WAN_IP_CIDR
RTR_EXT_IP
WAN_NS
WAN_IFACE
WAN_IP_CIDR
WAN_GW
EOF
}

vnet_topology_single_router_up() {
    vnet_topology_single_router_defaults

    # Build the three-namespace graph and connect the point-to-point links.
    vnet_create_namespaces "${LAN_NS}" "${RTR_NS}" "${WAN_NS}"
    vnet_connect_namespaces_with_veth "${LAN_NS}" "${LAN_IFACE}" "${RTR_NS}" "${RTR_IFACE_LAN}"
    vnet_connect_namespaces_with_veth "${WAN_NS}" "${WAN_IFACE}" "${RTR_NS}" "${RTR_IFACE_WAN}"

    # Address each interface and bring all links up.
    vnet_set_loopbacks_up "${LAN_NS}" "${RTR_NS}" "${WAN_NS}"
    vnet_configure_ipv4_interface "${LAN_NS}" "${LAN_IFACE}" "${LAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${RTR_NS}" "${RTR_IFACE_LAN}" "${RTR_LAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${RTR_NS}" "${RTR_IFACE_WAN}" "${RTR_WAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${WAN_NS}" "${WAN_IFACE}" "${WAN_IP_CIDR}"

    # LAN and WAN use the router as their default gateway.
    vnet_add_default_route "${LAN_NS}" "${LAN_GW}" "${LAN_IFACE}"
    vnet_add_default_route "${WAN_NS}" "${WAN_GW}" "${WAN_IFACE}"

    # Router performs IPv4 forwarding/NAT; LAN keeps multicast local for SSDP.
    vnet_setup_basic_nat_router "${RTR_NS}" "${RTR_IFACE_LAN}" "${RTR_IFACE_WAN}"
    vnet_add_device_route "${LAN_NS}" "${LAN_MULTICAST_CIDR}" "${LAN_IFACE}"
}
