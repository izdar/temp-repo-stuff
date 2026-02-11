#!/bin/bash

export SNPSFUZZER="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export WORKDIR=$SNPSFUZZER/tinydtls_workspace
export AFL_PATH=$SNPSFUZZER

# LTL Monitor Configuration - USE ABSOLUTE PATHS
export LTL_EVAL_PATH=$SNPSFUZZER/formula_parser           # ← Fixed
export LTL_SPEC_PATH=$SNPSFUZZER/monitor-bin/dtls.ltl    # ← Fixed
export MONITOR_VERBOSE=1

# AFL Configuration
export AFL_SKIP_CPUFREQ=1
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
export AFL_NO_AFFINITY=1

INPUT_DIR="$WORKDIR/fuzz_input"
OUTPUT_DIR="$WORKDIR/fuzz_output"
TARGET="$WORKDIR/tinydtls/tests/dtls-server"
POLLING_TIMEOUT=30

echo "[*] Starting SNPSFuzzer for TinyDTLS..."
echo "[*] Input: $INPUT_DIR"
echo "[*] Output: $OUTPUT_DIR"
echo "[*] Target: $TARGET"
echo "[*] Monitor: $LTL_EVAL_PATH $LTL_SPEC_PATH"
echo "[*] Polling timeout: $POLLING_TIMEOUT ms"

sudo -E $SNPSFUZZER/afl-fuzz \
    -d \
    -i $INPUT_DIR \
    -o $OUTPUT_DIR \
    -N udp://127.0.0.1/20220 \
    -P DTLS12 \
    -D 10000 \
    -q 3 \
    -s 3 \
    -E \
    -K \
    -R \
    -W $POLLING_TIMEOUT \
    -a 2 \
    -b 100 \
    -m none \
    -- $TARGET
