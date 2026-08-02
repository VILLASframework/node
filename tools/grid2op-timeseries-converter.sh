#!/usr/bin/env bash
#
# Create chronics files for OpenDSS.
# This script reads a chronics config, grid mapping, and per-element CSV series,
# then writes the OpenDSS-ready load and generator chronics files.
# Reference: https://grid2op.readthedocs.io/en/latest/user/chronics.html
#
# SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

die() {
    echo "[error] $1" >&2
    exit 1
}

cleanup_tmp_dir() {
    [[ -n "${tmp_dir:-}" && -d "$tmp_dir" ]] && rm -rf "$tmp_dir"
}

require_cmd() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        die "$cmd is not available. Please install it first."
    fi
}

round_dec() {
    local value="$1"
    local decimals="$2"

    awk -v value="$value" -v decimals="$decimals" 'BEGIN {
        scale = 10^decimals
        if (value >= 0) {
            rounded = int(value * scale + 0.5) / scale
        } else {
            rounded = -int(-value * scale + 0.5) / scale
        }
        printf "%.15g\n", rounded
    }'
}

extract_file_number() {
    local file="$1"
    local stem

    stem=$(basename -- "$file")
    stem=${stem%.*}

    if [[ "$stem" =~ ([0-9]+) ]]; then
        printf '%s\n' "${BASH_REMATCH[1]}"
    else
        die "No numeric index in filename: $file"
    fi
}

sorted_files() {
    local dir="$1"
    local prefix="$2"

    [[ -d "$dir" ]] || die "Directory missing: $dir"

    local -a files=()
    local file
    shopt -s nullglob
    for file in "$dir"/"$prefix"*.csv; do
        files+=("$file")
    done
    shopt -u nullglob

    if (( ${#files[@]} == 0 )); then
        return 0
    fi

    for file in "${files[@]}"; do
        printf '%s\t%s\n' "$(extract_file_number "$file")" "$file"
    done | sort -n -k1,1 | cut -f2-
}

load_grid_mapping() {
    local grid_file="$1"
    local table="$2"

    jq -r --arg table "$table" '
        .["_object"][$table]["_object"] | fromjson as $df |
        if ($df.columns | index("bus")) == null then
            error($table + " dataframe does not contain a bus column")
        else
            $df
        end |
        if (.index | length) != (.data | length) then
            error($table + " dataframe index and data length mismatch")
        else
            .
        end |
        (.columns | index("bus")) as $bus_idx |
        range(0; (.index | length)) as $i |
        "\(.index[$i])\t\(.data[$i][$bus_idx])"
    ' "$grid_file"
}

parse_series_csv() {
    local file="$1"
    local p_out="$2"
    local q_out="$3"
    local decimals="$4"

    : > "$p_out"
    : > "$q_out"

    local first_line=1
    local line
    local -a columns=()
    local -a cells=()
    local p_idx=-1
    local q_idx=-1
    local p_value
    local q_value

    while IFS= read -r line || [[ -n "$line" ]]; do
        if (( first_line )); then
            first_line=0
            [[ -n "$line" ]] || die "Empty CSV: $file"

            IFS=, read -r -a columns <<< "$line"
            for i in "${!columns[@]}"; do
                case "${columns[$i]}" in
                    P_norm) p_idx=$i ;;
                    Q_norm) q_idx=$i ;;
                esac
            done

            (( p_idx >= 0 )) || die "Column P_norm missing in $file"
            (( q_idx >= 0 )) || die "Column Q_norm missing in $file"
            continue
        fi

        [[ -z "$line" ]] && continue

        IFS=, read -r -a cells <<< "$line"
        p_value="${cells[$p_idx]:-0}"
        q_value="${cells[$q_idx]:-0}"

        round_dec "$p_value" "$decimals" >> "$p_out"
        round_dec "$q_value" "$decimals" >> "$q_out"
    done < "$file"

    (( first_line == 0 )) || die "Empty CSV: $file"
}

write_output() {
    local compress="$1"
    local dest="$2"
    local header="$3"
    shift 3

    local -a columns=("$@")

    if [[ "$compress" == true ]]; then
        {
            printf '%s\n' "$header"
            if (( ${#columns[@]} > 0 )); then
                paste -d ';' "${columns[@]}"
            fi
        } | bzip2 -c > "$dest"
    else
        {
            printf '%s\n' "$header"
            if (( ${#columns[@]} > 0 )); then
                paste -d ';' "${columns[@]}"
            fi
        } > "$dest"
    fi
}

usage() {
    cat >&2 <<EOF
Usage: $(basename -- "$0") [--compress] [--help] CONFIG.json

Options:
  --compress  write .bz2 output files even if the config disables compression
  --help      show this help and exit
EOF
}

parse_args() {
    config_path=""
    force_compress=false

    while (($#)); do
        case "$1" in
            --compress)
                force_compress=true
                ;;
            --help|-h)
                usage
                exit 0
                ;;
            -*)
                die "Unknown option: $1"
                ;;
            *)
                if [[ -z "$config_path" ]]; then
                    config_path="$1"
                else
                    die "Unexpected argument: $1"
                fi
                ;;
        esac
        shift
    done

    if [[ -z "$config_path" ]]; then
        usage
        exit 1
    fi
}

main() {
    parse_args "$@"
    [[ -f "$config_path" ]] || die "Cannot open config file: $config_path"

    require_cmd jq
    require_cmd awk

    local loads_dir sgens_dir grid_path output_dir
    local round_decimals compress voltage

    loads_dir=$(jq -r '.loads_dir // empty' "$config_path")
    sgens_dir=$(jq -r '.sgens_dir // empty' "$config_path")
    grid_path=$(jq -r '.grid // empty' "$config_path")
    output_dir=$(jq -r '.output // empty' "$config_path")
    round_decimals=$(jq -r '.round_decimals // 3' "$config_path")
    compress=$(jq -r 'if has("compress") then .compress else true end' "$config_path")
    voltage=$(jq -r '.voltage // 20.0' "$config_path")

    if [[ "$force_compress" == true ]]; then
        compress=true
    fi

    [[ -n "$loads_dir" && -n "$sgens_dir" && -n "$grid_path" && -n "$output_dir" ]] || \
        die "chronics: loads_dir, sgens_dir, grid, output are required"

    [[ "$compress" == true || "$compress" == false ]] || die "Invalid compress value in $config_path"

    local -A load_bus_map=()
    local -A sgen_bus_map=()

    while IFS=$'\t' read -r idx bus; do
        [[ -n "$idx" ]] || continue
        load_bus_map["$idx"]="$bus"
    done < <(load_grid_mapping "$grid_path" load)

    while IFS=$'\t' read -r idx bus; do
        [[ -n "$idx" ]] || continue
        sgen_bus_map["$idx"]="$bus"
    done < <(load_grid_mapping "$grid_path" sgen)

    local -a load_files=()
    local -a sgen_files=()
    mapfile -t load_files < <(sorted_files "$loads_dir" "Load")
    mapfile -t sgen_files < <(sorted_files "$sgens_dir" "SGen")

    if (( ${#load_files[@]} == 0 && ${#sgen_files[@]} == 0 )); then
        die "No CSV files found"
    fi

    tmp_dir=$(mktemp -d)
    trap cleanup_tmp_dir EXIT

    local -a load_p_columns=()
    local -a load_q_columns=()
    local -a prod_p_columns=()
    local -a prod_q_columns=()
    local -a prod_v_columns=()

    local load_col_names=""
    local sgen_col_names=""
    local sgen_idx=0

    local file element_index bus_id p_out q_out v_out

    for file in "${load_files[@]}"; do
        element_index=$(extract_file_number "$file")
        [[ -n "${load_bus_map[$element_index]:-}" ]] || die "Load index missing in grid mapping: $element_index"

        bus_id="${load_bus_map[$element_index]}"
        local load_col_idx=${#load_p_columns[@]}
        p_out="$tmp_dir/load_p_${#load_p_columns[@]}"
        q_out="$tmp_dir/load_q_${#load_q_columns[@]}"
        parse_series_csv "$file" "$p_out" "$q_out" "$round_decimals"

        load_p_columns+=("$p_out")
        load_q_columns+=("$q_out")

        if [[ -n "$load_col_names" ]]; then
            load_col_names+=';'
        fi
        load_col_names+="load_${bus_id}_${load_col_idx}"
    done

    for file in "${sgen_files[@]}"; do
        element_index=$(extract_file_number "$file")
        [[ -n "${sgen_bus_map[$element_index]:-}" ]] || die "SGen index missing in grid mapping: $element_index"

        bus_id="${sgen_bus_map[$element_index]}"
        p_out="$tmp_dir/prod_p_${#prod_p_columns[@]}"
        q_out="$tmp_dir/prod_q_${#prod_q_columns[@]}"
        v_out="$tmp_dir/prod_v_${#prod_v_columns[@]}"
        parse_series_csv "$file" "$p_out" "$q_out" "$round_decimals"

        : > "$v_out"
        while IFS= read -r _; do
            printf '%s\n' "$voltage" >> "$v_out"
        done < "$p_out"

        prod_p_columns+=("$p_out")
        prod_q_columns+=("$q_out")
        prod_v_columns+=("$v_out")

        if [[ -n "$sgen_col_names" ]]; then
            sgen_col_names+=';'
        fi
        sgen_col_names+="sgen_${bus_id}_${sgen_idx}"
        sgen_idx=$((sgen_idx + 1))
    done

    mkdir -p "$output_dir"

    local output_suffix=""
    if [[ "$compress" == true ]]; then
        require_cmd bzip2
        output_suffix=".bz2"
    fi

    local -a output_names=(load_p load_q prod_p prod_q prod_v)
    local -a output_headers=(load_col_names load_col_names sgen_col_names sgen_col_names sgen_col_names)
    local -a output_columns=(load_p_columns load_q_columns prod_p_columns prod_q_columns prod_v_columns)

    local i output_name header_name columns_name
    for i in "${!output_names[@]}"; do
        output_name="${output_names[$i]}"
        header_name="${output_headers[$i]}"
        columns_name="${output_columns[$i]}"

        local -n output_columns_ref="$columns_name"
        local output_path="$output_dir/${output_name}.csv${output_suffix}"
        write_output "$compress" "$output_path" "${!header_name}" "${output_columns_ref[@]}"
    done
}

main "$@"
