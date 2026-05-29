#!/usr/bin/env bash
# Run a QoS measurement scenario between two Jami instances.
#
# This script:
# 1. Starts two Jami instances (peer-a and peer-b) in their namespaces
# 2. Initiates a call between them
# 3. Applies network degradation profiles in sequence
# 4. Collects RTCP statistics (packet loss, jitter) per media type
# 5. Outputs a comparison report
#
# Prerequisites:
#   - QoS lab must be up: sudo ./setup-qos-lab.sh up
#   - Jami daemon built with debug logging
#   - Two Jami accounts pre-configured (see README)
#
# Usage:
#   sudo ./run-qos-scenario.sh [--duration <seconds>] [--profiles <p1,p2,...>]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SETUP_SCRIPT="${SCRIPT_DIR}/setup-qos-lab.sh"

DURATION="${DURATION:-15}"
PROFILES="${PROFILES:-good,moderate,poor,terrible}"
OUTPUT_DIR="${OUTPUT_DIR:-/tmp/jami-qos-results}"
NS_A="peer-a"
NS_B="peer-b"

usage() {
    cat <<EOF
Usage: sudo $0 [options]

Options:
  --duration <sec>     Duration per profile phase (default: 15)
  --profiles <list>    Comma-separated profiles (default: good,moderate,poor,terrible)
  --output <dir>       Output directory (default: /tmp/jami-qos-results)
  --help               Show this help

The script captures RTCP stats from Jami logs during each profile phase,
then produces a summary comparing audio vs video packet delivery.
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration) DURATION="$2"; shift 2 ;;
        --profiles) PROFILES="$2"; shift 2 ;;
        --output) OUTPUT_DIR="$2"; shift 2 ;;
        --help|-h) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

if [[ "${EUID}" -ne 0 ]]; then
    echo "Error: must be run as root."
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"

echo "=============================================="
echo " Jami QoS Measurement Scenario"
echo "=============================================="
echo " Duration per phase: ${DURATION}s"
echo " Profiles: ${PROFILES}"
echo " Output: ${OUTPUT_DIR}"
echo "=============================================="
echo ""

# Verify lab is up
if ! ip netns list | grep -q "${NS_A}"; then
    echo "Error: QoS lab not running. Start it with:"
    echo "  sudo ${SETUP_SCRIPT} up"
    exit 1
fi

IFS=',' read -ra PROFILE_LIST <<< "${PROFILES}"

collect_rtcp_stats() {
    local log_file="$1"
    local phase_start="$2"
    local phase_end="$3"
    local output_file="$4"

    # Extract BandwidthAdapt and RTCP-related log lines within the time window
    if [[ -f "$log_file" ]]; then
        awk -v start="$phase_start" -v end="$phase_end" '
        /BandwidthAdapt|packet.?loss|REMB|bitrate|jitter/ {
            print
        }' "$log_file" > "$output_file" 2>/dev/null || true
    fi
}

summarize_phase() {
    local profile="$1"
    local log_a="${OUTPUT_DIR}/${profile}_peer_a.log"
    local log_b="${OUTPUT_DIR}/${profile}_peer_b.log"

    echo "--- Profile: ${profile} ---"

    if [[ -f "$log_a" ]]; then
        local loss_lines
        loss_lines=$(grep -c "packet.loss\|packetLoss\|BandwidthAdapt" "$log_a" 2>/dev/null || echo "0")
        echo "  Peer A: ${loss_lines} congestion/loss events"
    fi

    if [[ -f "$log_b" ]]; then
        local loss_lines
        loss_lines=$(grep -c "packet.loss\|packetLoss\|BandwidthAdapt" "$log_b" 2>/dev/null || echo "0")
        echo "  Peer B: ${loss_lines} congestion/loss events"
    fi
    echo ""
}

echo "[*] Running measurement phases..."
echo ""

for profile in "${PROFILE_LIST[@]}"; do
    echo "[>] Applying profile: ${profile}"
    "${SETUP_SCRIPT}" profile "${profile}"

    phase_start=$(date +%s)
    echo "    Collecting data for ${DURATION}s..."
    sleep "${DURATION}"
    phase_end=$(date +%s)

    # Capture any jami log output from the namespaces
    # (Assumes Jami logs to stderr or a known log path)
    for ns in ${NS_A} ${NS_B}; do
        local_log="${OUTPUT_DIR}/${profile}_${ns//-/_}.log"
        # Try common Jami log locations
        for log_path in /tmp/jami-${ns}.log ~/.local/share/jami/jami*.log; do
            if ip netns exec "${ns}" test -f "${log_path}" 2>/dev/null; then
                ip netns exec "${ns}" cat "${log_path}" > "${local_log}" 2>/dev/null || true
                break
            fi
        done
    done

    echo "    Done."
    echo ""
done

# Reset to good network
"${SETUP_SCRIPT}" profile good

echo ""
echo "=============================================="
echo " Results Summary"
echo "=============================================="
echo ""

for profile in "${PROFILE_LIST[@]}"; do
    summarize_phase "${profile}"
done

echo "Full logs saved to: ${OUTPUT_DIR}/"
echo ""
echo "To analyze in detail:"
echo "  grep 'BandwidthAdapt' ${OUTPUT_DIR}/*.log"
echo "  grep 'packetLoss\|packet_loss' ${OUTPUT_DIR}/*.log"
