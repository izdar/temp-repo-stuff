#!/bin/bash
export SNPSFUZZER="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export LTL_EVAL_PATH=$SNPSFUZZER/formula_parser
export LTL_SPEC_PATH=$SNPSFUZZER/monitor-bin/dnsmasq.ltl
export AFL_PATH=$SNPSFUZZER
export WORKDIR=$(pwd)

echo "=== Starting SNPSFuzzer for Dnsmasq ==="
echo ""
echo "Input:  $WORKDIR/fuzz_input"
echo "Output: $WORKDIR/fuzz_output"
echo "Target: $WORKDIR/dnsmasq/src/dnsmasq"
echo "Config: $WORKDIR/dnsmasq.conf"
echo ""
echo "Network: tcp://127.0.0.1/5353  (DNS uses TCP for AFL)"
echo "Protocol: DNS"
echo ""

# IMPORTANT: AFLNet dnsmasq tutorial uses TCP, not UDP!
# DNS can run over both TCP and UDP, but AFLNet tutorial specifies TCP

cd $WORKDIR/dnsmasq/src

$SNPSFUZZER/afl-fuzz \
    -d \
    -i $WORKDIR/fuzz_input \
    -o $WORKDIR/fuzz_output \
    -N tcp://127.0.0.1/5353 \
    -P DNS \
    -D 10000 \
    -K \
    -R \
    -a 1 \
    -m none \
    -- $WORKDIR/dnsmasq/src/dnsmasq -C $WORKDIR/dnsmasq.conf
