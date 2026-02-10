#!/bin/bash
# run_trace.sh - Replay an FTP trace through the predicate adapter
#
# Usage:
#   ./run_trace.sh <trace_file>                          # just show predicates
#   ./run_trace.sh <trace_file> <monitor> <spec.ltl>     # pipe to monitor
#
# Examples:
#   ./run_trace.sh test_trace.txt                        # decode + show predicates
#   ./run_trace.sh test_trace.txt ./monitor spec.ltl     # run through LTL monitor

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPLAY="${SCRIPT_DIR}/ftp_trace_replay"

# Build if needed
if [ ! -x "$REPLAY" ]; then
    echo "[*] Building ftp_trace_replay..."
    make -C "$SCRIPT_DIR" >/dev/null 2>&1
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 <trace_file> [monitor_binary spec.ltl]"
    echo ""
    echo "Mode 1 - Decode & show predicates (no monitor):"
    echo "  $0 trace.txt"
    echo ""
    echo "Mode 2 - Pipe to LTL monitor:"
    echo "  $0 trace.txt ./monitor spec.ltl"
    exit 1
fi

TRACE_FILE="$1"

if [ ! -f "$TRACE_FILE" ]; then
    echo "ERROR: Trace file not found: $TRACE_FILE"
    exit 1
fi

if [ $# -ge 3 ]; then
    # Mode 2: pipe to monitor
    MONITOR="$2"
    SPEC="$3"
    echo "[*] Replaying $TRACE_FILE through monitor..."
    echo "[*] Monitor: $MONITOR"
    echo "[*] Spec:    $SPEC"
    echo "[*] Protocol: ftp"
    echo ""
    "$REPLAY" "$TRACE_FILE" 2>"${TRACE_FILE%.txt}_adapter.log" | "$MONITOR" "$SPEC" ftp
    echo ""
    echo "[*] Adapter diagnostics saved to: ${TRACE_FILE%.txt}_adapter.log"
else
    # Mode 1: just show decoded predicates
    echo "[*] Decoding trace and showing predicate lines..."
    echo "[*] (Pipe to your monitor with: $0 $TRACE_FILE ./monitor spec.ltl)"
    echo ""
    "$REPLAY" --decode "$TRACE_FILE"
fi
