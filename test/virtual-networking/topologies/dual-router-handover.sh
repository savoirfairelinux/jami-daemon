#!/usr/bin/env bash

if [[ -n "${JAMI_VNET_TOPOLOGY_DUAL_ROUTER_HANDOVER_SH:-}" ]]; then
    return 0
fi
JAMI_VNET_TOPOLOGY_DUAL_ROUTER_HANDOVER_SH=1

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/topology.sh"

# Dual-router lab used for Wi-Fi-to-Wi-Fi style handover work:
#   node -> router A -> wan
#        \-> router B -> wan
# Route metrics prefer access A while keeping access B installed as a standby
# path so later scenarios can swap between them without rebuilding the topology.
vnet_topology_dual_router_handover_defaults() {
    # Single node with two uplinks.
    : "${NODE_NS:=node}"
    : "${NODE_IFACE_A:=eth0}"
    : "${NODE_IFACE_B:=eth1}"
    : "${NODE_A_IP_CIDR:=192.168.10.2/24}"
    : "${NODE_A_GW:=192.168.10.1}"
    : "${NODE_B_IP_CIDR:=192.168.20.2/24}"
    : "${NODE_B_GW:=192.168.20.1}"
    : "${NODE_MULTICAST_CIDR:=224.0.0.0/4}"
    : "${NODE_ROUTE_A_METRIC:=100}"
    : "${NODE_ROUTE_B_METRIC:=200}"

    # First routed access network and its public side.
    : "${RTR_A_NS:=rtr-a}"
    : "${RTR_A_IFACE_LAN:=veth-rtr-a-lan}"
    : "${RTR_A_IFACE_WAN:=veth-rtr-a-wan}"
    : "${RTR_A_LAN_IP_CIDR:=192.168.10.1/24}"
    : "${RTR_A_WAN_IP_CIDR:=11.0.1.2/24}"
    : "${RTR_A_EXT_IP:=11.0.1.2}"

    # Second routed access network and its public side.
    : "${RTR_B_NS:=rtr-b}"
    : "${RTR_B_IFACE_LAN:=veth-rtr-b-lan}"
    : "${RTR_B_IFACE_WAN:=veth-rtr-b-wan}"
    : "${RTR_B_LAN_IP_CIDR:=192.168.20.1/24}"
    : "${RTR_B_WAN_IP_CIDR:=11.0.2.2/24}"
    : "${RTR_B_EXT_IP:=11.0.2.2}"

    # Shared external namespace with one subnet per router.
    : "${WAN_NS:=wan}"
    : "${WAN_IFACE_A:=veth-wan-a}"
    : "${WAN_IFACE_B:=veth-wan-b}"
    : "${WAN_A_IP_CIDR:=11.0.1.1/24}"
    : "${WAN_B_IP_CIDR:=11.0.2.1/24}"
}

vnet_topology_dual_router_handover_namespaces() {
    printf '%s\n' "${NODE_NS}" "${RTR_A_NS}" "${RTR_B_NS}" "${WAN_NS}"
}

vnet_topology_dual_router_handover_state_vars() {
    cat <<'EOF'
NODE_NS
NODE_IFACE_A
NODE_IFACE_B
NODE_A_IP_CIDR
NODE_A_GW
NODE_B_IP_CIDR
NODE_B_GW
NODE_MULTICAST_CIDR
NODE_ROUTE_A_METRIC
NODE_ROUTE_B_METRIC
RTR_A_NS
RTR_A_IFACE_LAN
RTR_A_IFACE_WAN
RTR_A_LAN_IP_CIDR
RTR_A_WAN_IP_CIDR
RTR_A_EXT_IP
RTR_B_NS
RTR_B_IFACE_LAN
RTR_B_IFACE_WAN
RTR_B_LAN_IP_CIDR
RTR_B_WAN_IP_CIDR
RTR_B_EXT_IP
WAN_NS
WAN_IFACE_A
WAN_IFACE_B
WAN_A_IP_CIDR
WAN_B_IP_CIDR
EOF
}

vnet_topology_dual_router_handover_up() {
    vnet_topology_dual_router_handover_defaults

    # Build the four-namespace graph with one node and two independent routers.
    vnet_create_namespaces "${NODE_NS}" "${RTR_A_NS}" "${RTR_B_NS}" "${WAN_NS}"
    vnet_connect_namespaces_with_veth "${NODE_NS}" "${NODE_IFACE_A}" "${RTR_A_NS}" "${RTR_A_IFACE_LAN}"
    vnet_connect_namespaces_with_veth "${NODE_NS}" "${NODE_IFACE_B}" "${RTR_B_NS}" "${RTR_B_IFACE_LAN}"
    vnet_connect_namespaces_with_veth "${WAN_NS}" "${WAN_IFACE_A}" "${RTR_A_NS}" "${RTR_A_IFACE_WAN}"
    vnet_connect_namespaces_with_veth "${WAN_NS}" "${WAN_IFACE_B}" "${RTR_B_NS}" "${RTR_B_IFACE_WAN}"

    # Address each access segment and the two public subnets.
    vnet_set_loopbacks_up "${NODE_NS}" "${RTR_A_NS}" "${RTR_B_NS}" "${WAN_NS}"
    vnet_configure_ipv4_interface "${NODE_NS}" "${NODE_IFACE_A}" "${NODE_A_IP_CIDR}"
    vnet_configure_ipv4_interface "${NODE_NS}" "${NODE_IFACE_B}" "${NODE_B_IP_CIDR}"
    vnet_configure_ipv4_interface "${RTR_A_NS}" "${RTR_A_IFACE_LAN}" "${RTR_A_LAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${RTR_A_NS}" "${RTR_A_IFACE_WAN}" "${RTR_A_WAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${RTR_B_NS}" "${RTR_B_IFACE_LAN}" "${RTR_B_LAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${RTR_B_NS}" "${RTR_B_IFACE_WAN}" "${RTR_B_WAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${WAN_NS}" "${WAN_IFACE_A}" "${WAN_A_IP_CIDR}"
    vnet_configure_ipv4_interface "${WAN_NS}" "${WAN_IFACE_B}" "${WAN_B_IP_CIDR}"

    # Prefer router A while keeping router B reachable as a standby route.
    vnet_add_default_route "${NODE_NS}" "${NODE_A_GW}" "${NODE_IFACE_A}" "${NODE_ROUTE_A_METRIC}"
    vnet_add_default_route "${NODE_NS}" "${NODE_B_GW}" "${NODE_IFACE_B}" "${NODE_ROUTE_B_METRIC}"
    vnet_add_device_route "${NODE_NS}" "${NODE_MULTICAST_CIDR}" "${NODE_IFACE_A}" "${NODE_ROUTE_A_METRIC}"
    vnet_add_device_route "${NODE_NS}" "${NODE_MULTICAST_CIDR}" "${NODE_IFACE_B}" "${NODE_ROUTE_B_METRIC}"

    # Each router forwards/NATs traffic towards its own public subnet.
    vnet_setup_basic_nat_router "${RTR_A_NS}" "${RTR_A_IFACE_LAN}" "${RTR_A_IFACE_WAN}"
    vnet_setup_basic_nat_router "${RTR_B_NS}" "${RTR_B_IFACE_LAN}" "${RTR_B_IFACE_WAN}"
}
