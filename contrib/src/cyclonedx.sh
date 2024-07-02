#!/bin/bash
#
# Take as input a list of CPE id (string), parse them and output a minimal CycloneDX SBOM file in JSON format
#
# Copyright (C) 2024 Savoir-faire Linux, Inc.

set -euo pipefail # Enable error checking


function read_package_jsons() {
    local SRC="${1:-}"
    local win_cpe_list=()

    for folder in "${SRC}"/*; do
        if [[ -d "${folder}" ]]; then
            local package_json="${folder}/package.json"
            if [[ -f "${package_json}" ]]; then
                local cpe=""
                cpe=$(jq -r '.cpe' "${package_json}")

                # if cpe string start with "cpe:2.3:" then it's a valid CPE
                if [[ "${cpe}" == cpe:2.3:* ]]; then
                    win_cpe_list+=("${cpe}")
                fi
            fi
        fi
    done
    echo "${win_cpe_list[@]}"
}


function main() {
    local list_cpe=$1
    local filename="${2:-sbom.cdx.json}"

    cat <<EOF > "$filename"
{
    "bomFormat": "CycloneDX",
    "specVersion": "1.5",
    "serialNumber": "urn:uuid:$(uuidgen)",
    "version": 1,
    "metadata": {
        "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
    },
    "components": [
EOF

    local already_done=()
    local components_writed=0

    for cpe in $list_cpe; do
        # Skip duplicates
        # shellcheck disable=SC2076 # CPE is not a regex
        if [[ " ${already_done[*]} " =~ " ${cpe} " ]]; then
            continue
        fi

        # Split CPE v2.3 string to extract vendor, product, and version
        IFS=':' read -r -a cpe_parts <<< "$cpe"

        if (( ${#cpe_parts[@]} < 6 )); then
            continue
        fi
        # Assuming standard CPE v2.3 format: cpe:2.3:a:vendor:product:version:...
        vendor="${cpe_parts[3]}"
        product="${cpe_parts[4]}"
        version="${cpe_parts[5]}"

        case "${cpe_parts[2]}" in
            o)
                kind="operating-system"
                ;;
            h)
                kind="device"
                ;;
            *)
                kind="library"
                ;;
        esac

        if (( components_writed >= 1 )); then
            echo "        }," >> "$filename"
        fi

        cat <<EOF >> "$filename"
        {
            "type": "$kind",
            "bom-ref": "$cpe",
            "publisher": "$vendor",
            "name": "$product",
            "version": "$version",
            "cpe": "$cpe"
EOF

        already_done+=("$cpe")
        components_writed=$((components_writed + 1))
    done

    if (( components_writed >= 1 )); then
        echo "        }" >> "$filename"
    fi

    cat <<EOF >> "$filename"
    ]
}
EOF

    echo "CycloneDX SBOM file generated: $filename (contains $components_writed components)"
}

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <list of CPE id> <SRC folder>"
    exit 1
fi

main "$1" "common-jami-daemon.cdx.json"

if ! command -v jq &> /dev/null; then
    echo "jq is not installed, please install it"
    exit 1
fi

cpe_windows=$(read_package_jsons "$2")
main "$cpe_windows" "windows-jami-daemon.cdx.json"
