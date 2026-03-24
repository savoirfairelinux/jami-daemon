#!/usr/bin/env bash

if [[ -n "${JAMI_VNET_RESULT_SUMMARY_SH:-}" ]]; then
    return 0
fi
JAMI_VNET_RESULT_SUMMARY_SH=1

VNET_RESULT_SUMMARY_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VNET_RESULT_SUMMARY_ROOT="$(cd "${VNET_RESULT_SUMMARY_LIB_DIR}/.." && pwd)"
VNET_RESULT_SUMMARY_PY="${VNET_RESULT_SUMMARY_LIB_DIR}/result_summary.py"
: "${VNET_ARTIFACT_ROOT:=${VNET_RESULT_SUMMARY_ROOT}/artifacts}"

vnet_results_now_iso() {
    date -u +"%Y-%m-%dT%H:%M:%S.%3NZ"
}

vnet_results_default_run_id() {
    local scenario="${1:-scenario}"
    local slug
    slug="$(printf '%s' "${scenario}" | tr -cs 'A-Za-z0-9._-' '-')"
    printf '%s_%s\n' "$(date -u +"%Y-%m-%dT%H-%M-%SZ")" "${slug}"
}

vnet_results_cli() {
    python3 "${VNET_RESULT_SUMMARY_PY}" "$@"
}

vnet_results_init() {
    local scenario="$1"
    local run_id="${2:-$(vnet_results_default_run_id "${scenario}")}"

    VNET_RESULT_SCENARIO="${scenario}"
    VNET_RESULT_RUN_ID="${run_id}"
    VNET_RESULT_DIR="${VNET_ARTIFACT_ROOT}/${VNET_RESULT_RUN_ID}"
    VNET_RESULT_META_DIR="${VNET_RESULT_DIR}/.meta"
    VNET_RESULT_CAPTURES_DIR="${VNET_RESULT_DIR}/captures"
    VNET_RESULT_EVENTS_FILE="${VNET_RESULT_DIR}/events.jsonl"
    VNET_RESULT_ASSERTIONS_FILE="${VNET_RESULT_META_DIR}/assertions.jsonl"
    VNET_RESULT_CAPTURES_FILE="${VNET_RESULT_META_DIR}/captures.jsonl"
    VNET_RESULT_METRICS_FILE="${VNET_RESULT_META_DIR}/metrics.jsonl"
    VNET_RESULT_NOTES_FILE="${VNET_RESULT_META_DIR}/notes.jsonl"
    VNET_RESULT_FIELDS_FILE="${VNET_RESULT_META_DIR}/fields.jsonl"
    VNET_RESULT_STARTED_AT="$(vnet_results_now_iso)"

    rm -rf "${VNET_RESULT_DIR}"
    mkdir -p "${VNET_RESULT_CAPTURES_DIR}" "${VNET_RESULT_META_DIR}"
    : > "${VNET_RESULT_EVENTS_FILE}"
    : > "${VNET_RESULT_ASSERTIONS_FILE}"
    : > "${VNET_RESULT_CAPTURES_FILE}"
    : > "${VNET_RESULT_METRICS_FILE}"
    : > "${VNET_RESULT_NOTES_FILE}"
    : > "${VNET_RESULT_FIELDS_FILE}"

    export VNET_RESULT_SCENARIO
    export VNET_RESULT_RUN_ID
    export VNET_RESULT_DIR
    export VNET_RESULT_META_DIR
    export VNET_RESULT_CAPTURES_DIR
    export VNET_RESULT_EVENTS_FILE
    export VNET_RESULT_ASSERTIONS_FILE
    export VNET_RESULT_CAPTURES_FILE
    export VNET_RESULT_METRICS_FILE
    export VNET_RESULT_NOTES_FILE
    export VNET_RESULT_FIELDS_FILE
    export VNET_RESULT_STARTED_AT

    vnet_results_event "run_started" "info" "Scenario '${VNET_RESULT_SCENARIO}' started"
}

vnet_results_event() {
    local event="$1"
    local status="${2:-info}"
    local message="${3:-}"

    vnet_results_cli append-event \
        --output "${VNET_RESULT_EVENTS_FILE}" \
        --event "${event}" \
        --status "${status}" \
        --message "${message}" \
        --timestamp "$(vnet_results_now_iso)"
}

vnet_results_field() {
    local key="$1"
    local value="$2"

    vnet_results_cli append-field \
        --output "${VNET_RESULT_FIELDS_FILE}" \
        --key "${key}" \
        --value "${value}"
}

vnet_results_assert() {
    local name="$1"
    local status="$2"
    local duration_ms="$3"
    local details="${4:-}"

    vnet_results_cli append-assertion \
        --output "${VNET_RESULT_ASSERTIONS_FILE}" \
        --name "${name}" \
        --status "${status}" \
        --duration-ms "${duration_ms}" \
        --details "${details}"
}

vnet_results_metric() {
    local key="$1"
    local value="$2"

    vnet_results_cli append-metric \
        --output "${VNET_RESULT_METRICS_FILE}" \
        --key "${key}" \
        --value "${value}"
}

vnet_results_note() {
    local note="$1"

    vnet_results_cli append-note \
        --output "${VNET_RESULT_NOTES_FILE}" \
        --note "${note}"
}

vnet_results_capture() {
    local label="$1"
    local kind="$2"
    local path="$3"

    vnet_results_cli append-capture \
        --output "${VNET_RESULT_CAPTURES_FILE}" \
        --label "${label}" \
        --kind "${kind}" \
        --path "${path}"
}

vnet_results_capture_command() {
    local label="$1"
    local kind="$2"
    local filename="$3"
    shift 3

    local destination="${VNET_RESULT_CAPTURES_DIR}/${filename}"
    local rc=0
    if "$@" > "${destination}" 2>&1; then
        rc=0
    else
        rc=$?
    fi

    vnet_results_capture "${label}" "${kind}" "captures/${filename}"
    return "${rc}"
}

vnet_results_finalize() {
    local status="$1"
    local ended_at
    ended_at="$(vnet_results_now_iso)"

    vnet_results_cli build \
        --output-dir "${VNET_RESULT_DIR}" \
        --run-id "${VNET_RESULT_RUN_ID}" \
        --scenario "${VNET_RESULT_SCENARIO}" \
        --status "${status}" \
        --started-at "${VNET_RESULT_STARTED_AT}" \
        --ended-at "${ended_at}" \
        --assertions "${VNET_RESULT_ASSERTIONS_FILE}" \
        --captures "${VNET_RESULT_CAPTURES_FILE}" \
        --metrics "${VNET_RESULT_METRICS_FILE}" \
        --notes "${VNET_RESULT_NOTES_FILE}" \
        --fields "${VNET_RESULT_FIELDS_FILE}"
}
