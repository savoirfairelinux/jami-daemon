#!/usr/bin/env bash

if [[ -n "${JAMI_VNET_COMMON_SH:-}" ]]; then
    return 0
fi
JAMI_VNET_COMMON_SH=1

VNET_COMMON_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Provides access to VNET_STATE_ROOT
# shellcheck disable=SC2034
VNET_COMMON_ROOT="$(cd "${VNET_COMMON_LIB_DIR}/.." && pwd)"
: "${VNET_STATE_ROOT:=/tmp/jami-virtual-networking}"

vnet_require_root() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "Error: must be run as root (or inside a container with CAP_NET_ADMIN + CAP_SYS_ADMIN)." >&2
        exit 1
    fi
}

vnet_require_commands() {
    local missing=()
    local cmd
    for cmd in "$@"; do
        if ! command -v "${cmd}" >/dev/null 2>&1; then
            missing+=("${cmd}")
        fi
    done

    if ((${#missing[@]} > 0)); then
        echo "Error: missing required commands: ${missing[*]}" >&2
        exit 1
    fi
}

vnet_ensure_dir() {
    mkdir -p "$1"
}

vnet_now_iso() {
    date -u +"%Y-%m-%dT%H:%M:%SZ"
}

vnet_now_ms() {
    date +%s%3N
}

vnet_sanitize_id() {
    printf '%s' "$1" | tr -cs 'A-Za-z0-9._-' '-'
}

vnet_state_file_path() {
    local lab_name
    lab_name="$(vnet_sanitize_id "$1")"
    printf '%s/%s.env\n' "${VNET_STATE_ROOT}" "${lab_name}"
}

vnet_make_state_dir() {
    local prefix="${1:-lab}"
    vnet_ensure_dir "${VNET_STATE_ROOT}"
    mktemp -d "${VNET_STATE_ROOT}/$(vnet_sanitize_id "${prefix}").XXXXXX"
}

vnet_write_env_file() {
    local destination="$1"
    shift

    vnet_ensure_dir "$(dirname "${destination}")"
    : > "${destination}"

    local var_name
    for var_name in "$@"; do
        printf '%s=%q\n' "${var_name}" "${!var_name}" >> "${destination}"
    done
}

vnet_load_env_file() {
    local source_file="$1"
    if [[ ! -f "${source_file}" ]]; then
        return 1
    fi


    source "${source_file}"
}

vnet_kill_pidfile() {
    local pidfile="$1"
    if [[ ! -f "${pidfile}" ]]; then
        return 0
    fi

    local pid
    pid="$(<"${pidfile}")"
    if [[ "${pid}" =~ ^[0-9]+$ ]]; then
        kill "${pid}" 2>/dev/null || true
    fi
}
