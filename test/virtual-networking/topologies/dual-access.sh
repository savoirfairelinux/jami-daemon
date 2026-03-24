#!/usr/bin/env bash

if [[ -n "${JAMI_VNET_TOPOLOGY_DUAL_ACCESS_SH:-}" ]]; then
    return 0
fi
JAMI_VNET_TOPOLOGY_DUAL_ACCESS_SH=1

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/topology.sh"

# Dual-access lab used for Wi-Fi-to-mobile style scenarios:
#   node -> wifi-rtr   -> wan
#       \-> mobile-rtr -> wan
# Metrics prefer the Wi-Fi path while keeping the mobile-like path installed for
# later route flips and degraded-access scenarios.
vnet_topology_dual_access_defaults() {
    # Single node with a preferred Wi-Fi-like uplink and a standby mobile uplink.
    : "${NODE_NS:=node}"
    : "${NODE_WIFI_IFACE:=eth0}"
    : "${NODE_MOBILE_IFACE:=eth1}"
    : "${NODE_WIFI_IP_CIDR:=192.168.30.2/24}"
    : "${NODE_WIFI_GW:=192.168.30.1}"
    : "${NODE_MOBILE_IP_CIDR:=100.64.10.2/24}"
    : "${NODE_MOBILE_GW:=100.64.10.1}"
    : "${NODE_MULTICAST_CIDR:=224.0.0.0/4}"
    : "${NODE_WIFI_METRIC:=100}"
    : "${NODE_MOBILE_METRIC:=200}"

    # Wi-Fi-like access network with its public side.
    : "${WIFI_RTR_NS:=wifi-rtr}"
    : "${WIFI_RTR_IFACE_LAN:=veth-wifi-lan}"
    : "${WIFI_RTR_IFACE_WAN:=veth-wifi-wan}"
    : "${WIFI_RTR_LAN_IP_CIDR:=192.168.30.1/24}"
    : "${WIFI_RTR_WAN_IP_CIDR:=11.0.10.2/24}"
    : "${WIFI_RTR_EXT_IP:=11.0.10.2}"

    # Mobile-like access network with a distinct public side.
    : "${MOBILE_RTR_NS:=mobile-rtr}"
    : "${MOBILE_RTR_IFACE_LAN:=veth-mobile-lan}"
    : "${MOBILE_RTR_IFACE_WAN:=veth-mobile-wan}"
    : "${MOBILE_RTR_LAN_IP_CIDR:=100.64.10.1/24}"
    : "${MOBILE_RTR_WAN_IP_CIDR:=12.0.10.2/24}"
    : "${MOBILE_RTR_EXT_IP:=12.0.10.2}"

    # External namespace with one subnet per access type.
    : "${WAN_NS:=wan}"
    : "${WAN_IFACE_WIFI:=veth-wan-wifi}"
    : "${WAN_IFACE_MOBILE:=veth-wan-mobile}"
    : "${WAN_WIFI_IP_CIDR:=11.0.10.1/24}"
    : "${WAN_MOBILE_IP_CIDR:=12.0.10.1/24}"
}

vnet_topology_dual_access_namespaces() {
    printf '%s\n' "${NODE_NS}" "${WIFI_RTR_NS}" "${MOBILE_RTR_NS}" "${WAN_NS}"
}

vnet_topology_dual_access_state_vars() {
    cat <<'EOF'
NODE_NS
NODE_WIFI_IFACE
NODE_MOBILE_IFACE
NODE_WIFI_IP_CIDR
NODE_WIFI_GW
NODE_MOBILE_IP_CIDR
NODE_MOBILE_GW
NODE_MULTICAST_CIDR
NODE_WIFI_METRIC
NODE_MOBILE_METRIC
WIFI_RTR_NS
WIFI_RTR_IFACE_LAN
WIFI_RTR_IFACE_WAN
WIFI_RTR_LAN_IP_CIDR
WIFI_RTR_WAN_IP_CIDR
WIFI_RTR_EXT_IP
MOBILE_RTR_NS
MOBILE_RTR_IFACE_LAN
MOBILE_RTR_IFACE_WAN
MOBILE_RTR_LAN_IP_CIDR
MOBILE_RTR_WAN_IP_CIDR
MOBILE_RTR_EXT_IP
WAN_NS
WAN_IFACE_WIFI
WAN_IFACE_MOBILE
WAN_WIFI_IP_CIDR
WAN_MOBILE_IP_CIDR
EOF
}

vnet_topology_dual_access_up() {
    vnet_topology_dual_access_defaults

    # Build the four-namespace graph with two routed access networks.
    vnet_create_namespaces "${NODE_NS}" "${WIFI_RTR_NS}" "${MOBILE_RTR_NS}" "${WAN_NS}"
    vnet_connect_namespaces_with_veth "${NODE_NS}" "${NODE_WIFI_IFACE}" "${WIFI_RTR_NS}" "${WIFI_RTR_IFACE_LAN}"
    vnet_connect_namespaces_with_veth "${NODE_NS}" "${NODE_MOBILE_IFACE}" "${MOBILE_RTR_NS}" "${MOBILE_RTR_IFACE_LAN}"
    vnet_connect_namespaces_with_veth "${WAN_NS}" "${WAN_IFACE_WIFI}" "${WIFI_RTR_NS}" "${WIFI_RTR_IFACE_WAN}"
    vnet_connect_namespaces_with_veth "${WAN_NS}" "${WAN_IFACE_MOBILE}" "${MOBILE_RTR_NS}" "${MOBILE_RTR_IFACE_WAN}"

    # Address each access link and both public subnets.
    vnet_set_loopbacks_up "${NODE_NS}" "${WIFI_RTR_NS}" "${MOBILE_RTR_NS}" "${WAN_NS}"
    vnet_configure_ipv4_interface "${NODE_NS}" "${NODE_WIFI_IFACE}" "${NODE_WIFI_IP_CIDR}"
    vnet_configure_ipv4_interface "${NODE_NS}" "${NODE_MOBILE_IFACE}" "${NODE_MOBILE_IP_CIDR}"
    vnet_configure_ipv4_interface "${WIFI_RTR_NS}" "${WIFI_RTR_IFACE_LAN}" "${WIFI_RTR_LAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${WIFI_RTR_NS}" "${WIFI_RTR_IFACE_WAN}" "${WIFI_RTR_WAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${MOBILE_RTR_NS}" "${MOBILE_RTR_IFACE_LAN}" "${MOBILE_RTR_LAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${MOBILE_RTR_NS}" "${MOBILE_RTR_IFACE_WAN}" "${MOBILE_RTR_WAN_IP_CIDR}"
    vnet_configure_ipv4_interface "${WAN_NS}" "${WAN_IFACE_WIFI}" "${WAN_WIFI_IP_CIDR}"
    vnet_configure_ipv4_interface "${WAN_NS}" "${WAN_IFACE_MOBILE}" "${WAN_MOBILE_IP_CIDR}"

    # Prefer Wi-Fi while keeping the mobile route installed for later handovers.
    vnet_add_default_route "${NODE_NS}" "${NODE_WIFI_GW}" "${NODE_WIFI_IFACE}" "${NODE_WIFI_METRIC}"
    vnet_add_default_route "${NODE_NS}" "${NODE_MOBILE_GW}" "${NODE_MOBILE_IFACE}" "${NODE_MOBILE_METRIC}"
    vnet_add_device_route "${NODE_NS}" "${NODE_MULTICAST_CIDR}" "${NODE_WIFI_IFACE}" "${NODE_WIFI_METRIC}"
    vnet_add_device_route "${NODE_NS}" "${NODE_MULTICAST_CIDR}" "${NODE_MOBILE_IFACE}" "${NODE_MOBILE_METRIC}"

    # Each router forwards/NATs traffic towards its own public subnet.
    vnet_setup_basic_nat_router "${WIFI_RTR_NS}" "${WIFI_RTR_IFACE_LAN}" "${WIFI_RTR_IFACE_WAN}"
    vnet_setup_basic_nat_router "${MOBILE_RTR_NS}" "${MOBILE_RTR_IFACE_LAN}" "${MOBILE_RTR_IFACE_WAN}"
}
