#!/usr/bin/env bash

if [[ -n "${JAMI_VNET_NETNS_SH:-}" ]]; then
    return 0
fi
JAMI_VNET_NETNS_SH=1

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

vnet_namespace_exists() {
    ip netns list | awk '{print $1}' | grep -Fxq "$1"
}

vnet_create_namespaces() {
    local ns
    for ns in "$@"; do
        ip netns add "${ns}"
    done
}

vnet_delete_namespace() {
    ip netns del "$1" 2>/dev/null || true
}

vnet_delete_namespaces() {
    local ns
    for ns in "$@"; do
        vnet_delete_namespace "${ns}"
    done
}

vnet_assert_namespaces_exist() {
    local missing=()
    local ns
    for ns in "$@"; do
        if ! vnet_namespace_exists "${ns}"; then
            missing+=("${ns}")
        fi
    done

    if ((${#missing[@]} > 0)); then
        echo "Error: required namespace(s) missing: ${missing[*]}" >&2
        return 1
    fi
}

vnet_assert_namespaces_absent() {
    local existing=()
    local ns
    for ns in "$@"; do
        if vnet_namespace_exists "${ns}"; then
            existing+=("${ns}")
        fi
    done

    if ((${#existing[@]} > 0)); then
        echo "Error: namespace(s) already exist: ${existing[*]}" >&2
        return 1
    fi
}
