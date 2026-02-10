#!/bin/bash

export WORKDIR=$(pwd)
export SNPSFUZZER=/home/pirwani/SNPSFuzzer/

echo "=== Testing DNS Monitor Integration ==="
echo ""

# Check if monitor files exist
echo "[*] Checking monitor files..."

FILES_OK=1

if [ ! -f "$SNPSFUZZER/monitor-src/dnsmasq_predicate_adapter.c" ]; then
    echo "[!] MISSING: dnsmasq_predicate_adapter.c"
    FILES_OK=0
fi

if [ ! -f "$SNPSFUZZER/monitor-src/dnsmasq_predicate_adapter.h" ]; then
    echo "[!] MISSING: dnsmasq_predicate_adapter.h"
    FILES_OK=0
fi

if [ ! -f "$SNPSFUZZER/monitor-bin/dnsmasq.ltl" ]; then
    echo "[!] MISSING: dnsmasq.ltl"
    FILES_OK=0
fi

if [ ! -f "$SNPSFUZZER/monitor-bin/ltl_evaluator" ]; then
    echo "[!] MISSING: ltl_evaluator binary"
    FILES_OK=0
fi

if [ $FILES_OK -eq 0 ]; then
    echo ""
    echo "[!] Monitor files are missing. Please:"
    echo "    1. Copy dnsmasq_predicate_adapter.{h,c} to $SNPSFUZZER/monitor-src/"
    echo "    2. Copy dnsmasq.ltl to $SNPSFUZZER/monitor-bin/"
    echo "    3. Rebuild SNPSFuzzer"
    exit 1
fi

echo "[+] All monitor files found!"
echo ""

# Test the adapter standalone
echo "[*] Testing DNS predicate adapter..."
if [ -f "$SNPSFUZZER/monitor-src/test_dnsmasq_adapter" ]; then
    $SNPSFUZZER/monitor-src/test_dnsmasq_adapter
else
    echo "[!] Test program not found. You may need to compile it:"
    echo "    cd $SNPSFUZZER/monitor-src"
    echo "    gcc -o test_dnsmasq_adapter test_dnsmasq_adapter.c dnsmasq_predicate_adapter.c"
fi

echo ""
echo "[+] Monitor integration test completed!"
