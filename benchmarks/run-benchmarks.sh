#!/usr/bin/env bash
#
# LightDM Performance Benchmarks Runner Script
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BENCH_BIN="${SCRIPT_DIR}/lightdm-benchmark"

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  -q, --quick                Run quick smoke benchmarks (warmup: 2, iterations: 5)
  -f, --full                 Run full benchmark suite (warmup: 5, iterations: 30) [Default]
  -s, --suite SUITE          Run specific suite (config, user_list, xauthority, xdmcp)
  -p, --pattern PATTERN      Filter benchmarks by name or suite substring
  -b, --save-baseline [FILE] Run benchmarks and save baseline JSON (default: benchmarks/baseline.json)
  -c, --compare [FILE]       Run benchmarks and compare against baseline JSON
  -j, --json                 Output machine-readable JSON to stdout
  -l, --list                 List all available benchmarks
  -h, --help                 Show this help message

Examples:
  $(basename "$0") --quick
  $(basename "$0") --suite config
  $(basename "$0") --save-baseline
  $(basename "$0") --compare benchmarks/baseline.json
EOF
    exit 0
}

# Ensure benchmark binary is built
build_benchmarks() {
    if [[ ! -x "${BENCH_BIN}" ]]; then
        echo "Building lightdm-benchmark..."
        make -C "${SCRIPT_DIR}"
    fi
}

MODE="full"
SUITE=""
PATTERN=""
BASELINE_FILE="${SCRIPT_DIR}/baseline.json"
COMPARE_FILE=""
OUTPUT_FILE=""
JSON_STDOUT=0
LIST_ONLY=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        -q|--quick)
            MODE="quick"
            shift
            ;;
        -f|--full)
            MODE="full"
            shift
            ;;
        -s|--suite)
            SUITE="$2"
            shift 2
            ;;
        -p|--pattern)
            PATTERN="$2"
            shift 2
            ;;
        -b|--save-baseline)
            if [[ -n "$2" && "$2" != -* ]]; then
                OUTPUT_FILE="$2"
                shift 2
            else
                OUTPUT_FILE="${BASELINE_FILE}"
                shift
            fi
            ;;
        -c|--compare)
            if [[ -n "$2" && "$2" != -* ]]; then
                COMPARE_FILE="$2"
                shift 2
            else
                COMPARE_FILE="${BASELINE_FILE}"
                shift
            fi
            ;;
        -j|--json)
            JSON_STDOUT=1
            shift
            ;;
        -l|--list)
            LIST_ONLY=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            ;;
    esac
done

build_benchmarks

ARGS=()

if [[ ${LIST_ONLY} -eq 1 ]]; then
    "${BENCH_BIN}" --list
    exit 0
fi

if [[ "${MODE}" == "quick" ]]; then
    ARGS+=(--warmup=2 --iterations=5)
elif [[ "${MODE}" == "full" ]]; then
    ARGS+=(--warmup=5 --iterations=30)
fi

if [[ -n "${SUITE}" ]]; then
    ARGS+=(--suite="${SUITE}")
fi

if [[ -n "${PATTERN}" ]]; then
    ARGS+=(--filter="${PATTERN}")
fi

if [[ -n "${OUTPUT_FILE}" ]]; then
    ARGS+=(--output="${OUTPUT_FILE}")
fi

if [[ -n "${COMPARE_FILE}" ]]; then
    ARGS+=(--compare="${COMPARE_FILE}")
fi

if [[ ${JSON_STDOUT} -eq 1 ]]; then
    ARGS+=(--json)
fi

"${BENCH_BIN}" "${ARGS[@]}"
