#!/bin/bash
#
# Take as input a list of CPE id (string), parse them and output a minimal CycloneDX SBOM file in JSON format
#
# Copyright (C) 2024 Savoir-faire Linux, Inc.

set -euo pipefail # Enable error checking


function main() {
    local list_cpe=$1
    local output="common-jami-daemon.cdx.json"

    cat <<EOF > $output
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
            echo "        }," >> $output
        fi

        cat <<EOF >> $output
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
        echo "        }" >> $output
    fi

    cat <<EOF >> $output
    ]
}
EOF

    echo "CycloneDX SBOM file generated: $output (contains $components_writed components)"
}

main "$@"
