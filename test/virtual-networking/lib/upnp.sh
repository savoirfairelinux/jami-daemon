#!/usr/bin/env bash

if [[ -n "${JAMI_VNET_UPNP_SH:-}" ]]; then
    return 0
fi
JAMI_VNET_UPNP_SH=1

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

vnet_write_miniupnpd_config() {
    local destination="$1"
    local ext_ifname="$2"
    local listening_ip="$3"
    local uuid="$4"
    local friendly_name="$5"
    local ext_ip="$6"

    cat > "${destination}" <<CFG
ext_ifname=${ext_ifname}
listening_ip=${listening_ip}
enable_upnp=yes
secure_mode=no
system_uptime=yes
uuid=${uuid}
friendly_name=${friendly_name}
ext_ip=${ext_ip}
CFG
}

vnet_start_miniupnpd() {
    local namespace="$1"
    local config_file="$2"
    local pidfile="$3"
    local logfile="$4"

    ip netns exec "${namespace}" miniupnpd -d -f "${config_file}" -P "${pidfile}" \
        > "${logfile}" 2>&1 &
}

vnet_wait_for_upnpc() {
    local namespace="$1"
    local timeout_s="${2:-10}"
    local output_file="${3:-}"
    local probe_output
    local cleanup_output=0
    local attempt

    if [[ -n "${output_file}" ]]; then
        probe_output="${output_file}"
    else
        probe_output="$(mktemp "${VNET_STATE_ROOT}/upnpc.XXXXXX")"
        cleanup_output=1
    fi

    for ((attempt = 1; attempt <= timeout_s; attempt++)); do
        if ip netns exec "${namespace}" upnpc -s > "${probe_output}" 2>&1; then
            if ((cleanup_output)); then
                rm -f "${probe_output}"
            fi
            return 0
        fi
        sleep 1
    done

    if ((cleanup_output)); then
        rm -f "${probe_output}"
    fi
    return 1
}
