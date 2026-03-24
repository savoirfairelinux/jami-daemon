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

vnet_topology_require_file() {
    local topology_file="$1"
    if [[ ! -f "${topology_file}" ]]; then
        echo "Error: topology file not found: ${topology_file}" >&2
        return 1
    fi
}

vnet_topology_json_cli() {
    local action="$1"
    local topology_file="$2"

    vnet_require_commands python3
    vnet_topology_require_file "${topology_file}" || return 1

    python3 - "${action}" "${topology_file}" <<'PY'
from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

ACTION = sys.argv[1]
TOPOLOGY_PATH = Path(sys.argv[2])


def fail(message: str) -> None:
    raise SystemExit(f"{TOPOLOGY_PATH}: {message}")


def require_string(value: Any, *, field_name: str) -> str:
    if not isinstance(value, str) or not value:
        fail(f"expected non-empty string for {field_name}")
    if any(char in value for char in ("\n", "\r", "\t")):
        fail(f"{field_name} must not contain tabs or newlines")
    return value


def require_string_list(value: Any, *, field_name: str) -> list[str]:
    if not isinstance(value, list):
        fail(f"expected {field_name} to be a list")
    return [require_string(item, field_name=f"{field_name}[]") for item in value]


def require_defaults(value: Any) -> dict[str, str]:
    if not isinstance(value, dict):
        fail("expected defaults to be an object")

    normalized: dict[str, str] = {}
    for key, raw_value in value.items():
        key = require_string(key, field_name="defaults key")
        if not key.replace("_", "a").isalnum() or key[0].isdigit():
            fail(f"invalid defaults key {key!r}")
        normalized[key] = require_string(raw_value, field_name=f"defaults.{key}")
    return normalized


def require_operation_keys(raw: dict[str, Any], *, allowed: set[str], field_name: str) -> None:
    unexpected = sorted(set(raw) - allowed)
    if unexpected:
        fail(f"{field_name} has unexpected keys: {', '.join(unexpected)}")


def optional_string(raw: dict[str, Any], key: str, *, field_name: str) -> str:
    value = raw.get(key)
    if value is None:
        return ""
    return require_string(value, field_name=f"{field_name}.{key}")


def normalize_operation(index: int, raw: Any) -> list[str]:
    field_name = f"operations[{index}]"
    if not isinstance(raw, dict):
        fail(f"expected {field_name} to be an object")

    op_type = require_string(raw.get("type"), field_name=f"{field_name}.type")
    if op_type in {"create-namespaces", "set-loopbacks-up"}:
        require_operation_keys(raw, allowed={"type", "namespaces"}, field_name=field_name)
        return [op_type, *require_string_list(raw.get("namespaces"), field_name=f"{field_name}.namespaces")]
    if op_type == "connect-veth":
        require_operation_keys(
            raw,
            allowed={"type", "ns_a", "iface_a", "ns_b", "iface_b"},
            field_name=field_name,
        )
        return [
            op_type,
            require_string(raw.get("ns_a"), field_name=f"{field_name}.ns_a"),
            require_string(raw.get("iface_a"), field_name=f"{field_name}.iface_a"),
            require_string(raw.get("ns_b"), field_name=f"{field_name}.ns_b"),
            require_string(raw.get("iface_b"), field_name=f"{field_name}.iface_b"),
        ]
    if op_type == "configure-ipv4-interface":
        require_operation_keys(
            raw,
            allowed={"type", "ns", "iface", "cidr"},
            field_name=field_name,
        )
        return [
            op_type,
            require_string(raw.get("ns"), field_name=f"{field_name}.ns"),
            require_string(raw.get("iface"), field_name=f"{field_name}.iface"),
            require_string(raw.get("cidr"), field_name=f"{field_name}.cidr"),
        ]
    if op_type == "add-default-route":
        require_operation_keys(
            raw,
            allowed={"type", "ns", "via", "dev", "metric"},
            field_name=field_name,
        )
        return [
            op_type,
            require_string(raw.get("ns"), field_name=f"{field_name}.ns"),
            require_string(raw.get("via"), field_name=f"{field_name}.via"),
            optional_string(raw, "dev", field_name=field_name),
            optional_string(raw, "metric", field_name=field_name),
        ]
    if op_type == "add-device-route":
        require_operation_keys(
            raw,
            allowed={"type", "ns", "destination", "dev", "metric"},
            field_name=field_name,
        )
        return [
            op_type,
            require_string(raw.get("ns"), field_name=f"{field_name}.ns"),
            require_string(raw.get("destination"), field_name=f"{field_name}.destination"),
            require_string(raw.get("dev"), field_name=f"{field_name}.dev"),
            optional_string(raw, "metric", field_name=field_name),
        ]
    if op_type == "setup-basic-nat-router":
        require_operation_keys(
            raw,
            allowed={"type", "ns", "lan_iface", "wan_iface"},
            field_name=field_name,
        )
        return [
            op_type,
            require_string(raw.get("ns"), field_name=f"{field_name}.ns"),
            require_string(raw.get("lan_iface"), field_name=f"{field_name}.lan_iface"),
            require_string(raw.get("wan_iface"), field_name=f"{field_name}.wan_iface"),
        ]

    fail(f"unsupported operation type {op_type!r}")


def load_topology() -> dict[str, Any]:
    try:
        data = json.loads(TOPOLOGY_PATH.read_text(encoding="utf-8"))
    except FileNotFoundError:
        fail("file not found")
    except json.JSONDecodeError as exc:
        fail(f"invalid JSON: {exc}")

    if not isinstance(data, dict):
        fail("topology file must contain a JSON object")

    topology = {
        "name": require_string(data.get("name"), field_name="name"),
        "description": require_string(data.get("description"), field_name="description"),
        "defaults": require_defaults(data.get("defaults")),
        "namespaces": require_string_list(data.get("namespaces"), field_name="namespaces"),
        "state_vars": require_string_list(data.get("state_vars"), field_name="state_vars"),
        "operations": data.get("operations"),
    }
    if not isinstance(topology["operations"], list):
        fail("expected operations to be a list")
    topology["operations"] = [
        normalize_operation(index, raw_operation)
        for index, raw_operation in enumerate(topology["operations"])
    ]
    return topology


def emit_lines(values: list[str]) -> None:
    for value in values:
        print(value)


topology = load_topology()

if ACTION == "defaults":
    emit_lines([f"{key}\t{value}" for key, value in topology["defaults"].items()])
elif ACTION == "namespaces":
    emit_lines(topology["namespaces"])
elif ACTION == "state-vars":
    emit_lines(topology["state_vars"])
elif ACTION == "operations":
    emit_lines(["\t".join(operation) for operation in topology["operations"]])
else:
    fail(f"unsupported action {ACTION!r}")
PY
}

vnet_topology_load_defaults() {
    local topology_file="$1"
    local output
    local key
    local value

    output="$(vnet_topology_json_cli defaults "${topology_file}")" || return 1
    while IFS=$'\t' read -r key value; do
        [[ -n "${key}" ]] || continue
        if [[ ! -v "${key}" || -z "${!key-}" ]]; then
            printf -v "${key}" '%s' "${value}"
        fi
    done <<< "${output}"
}

vnet_topology_resolve_value() {
    local template="$1"
    local resolved=""
    local remainder="${template}"

    while [[ "${remainder}" =~ ^([^{}]*)\{([A-Za-z_][A-Za-z0-9_]*)\}(.*)$ ]]; do
        local prefix="${BASH_REMATCH[1]}"
        local variable_name="${BASH_REMATCH[2]}"
        local suffix="${BASH_REMATCH[3]}"

        if [[ ! -v "${variable_name}" ]]; then
            echo "Error: topology placeholder {${variable_name}} is not defined" >&2
            return 1
        fi

        resolved+="${prefix}${!variable_name}"
        remainder="${suffix}"
    done

    if [[ "${remainder}" == *"{"* || "${remainder}" == *"}"* ]]; then
        echo "Error: invalid topology placeholder syntax in ${template}" >&2
        return 1
    fi

    printf '%s\n' "${resolved}${remainder}"
}

vnet_topology_namespaces() {
    local topology_file="$1"
    local output
    local namespace

    vnet_topology_load_defaults "${topology_file}" || return 1
    output="$(vnet_topology_json_cli namespaces "${topology_file}")" || return 1
    while IFS= read -r namespace; do
        [[ -n "${namespace}" ]] || continue
        vnet_topology_resolve_value "${namespace}" || return 1
    done <<< "${output}"
}

vnet_topology_state_vars() {
    local topology_file="$1"
    vnet_topology_json_cli state-vars "${topology_file}"
}

vnet_topology_read_namespaces() {
    local topology_file="$1"
    local -n namespaces_ref="$2"
    local output

    namespaces_ref=()
    output="$(vnet_topology_namespaces "${topology_file}")" || return 1
    if [[ -n "${output}" ]]; then
        mapfile -t namespaces_ref <<< "${output}"
    fi
}

vnet_topology_read_state_vars() {
    local topology_file="$1"
    local -n state_vars_ref="$2"
    local output

    state_vars_ref=()
    output="$(vnet_topology_state_vars "${topology_file}")" || return 1
    if [[ -n "${output}" ]]; then
        mapfile -t state_vars_ref <<< "${output}"
    fi
}

vnet_topology_apply() {
    local topology_file="$1"
    local output
    local line

    vnet_topology_load_defaults "${topology_file}" || return 1
    output="$(vnet_topology_json_cli operations "${topology_file}")" || return 1
    while IFS= read -r line; do
        local -a fields=()
        local -a resolved=()
        local index

        [[ -n "${line}" ]] || continue
        IFS=$'\t' read -r -a fields <<< "${line}"
        for ((index = 1; index < ${#fields[@]}; index++)); do
            resolved+=("$(vnet_topology_resolve_value "${fields[index]}")") || return 1
        done

        case "${fields[0]}" in
            create-namespaces)
                vnet_create_namespaces "${resolved[@]}"
                ;;
            set-loopbacks-up)
                vnet_set_loopbacks_up "${resolved[@]}"
                ;;
            connect-veth)
                vnet_connect_namespaces_with_veth "${resolved[0]}" "${resolved[1]}" "${resolved[2]}" "${resolved[3]}"
                ;;
            configure-ipv4-interface)
                vnet_configure_ipv4_interface "${resolved[0]}" "${resolved[1]}" "${resolved[2]}"
                ;;
            add-default-route)
                vnet_add_default_route "${resolved[0]}" "${resolved[1]}" "${resolved[2]:-}" "${resolved[3]:-}"
                ;;
            add-device-route)
                vnet_add_device_route "${resolved[0]}" "${resolved[1]}" "${resolved[2]}" "${resolved[3]:-}"
                ;;
            setup-basic-nat-router)
                vnet_setup_basic_nat_router "${resolved[0]}" "${resolved[1]}" "${resolved[2]}"
                ;;
            *)
                echo "Error: unsupported topology operation ${fields[0]}" >&2
                return 1
                ;;
        esac
    done <<< "${output}"
}
