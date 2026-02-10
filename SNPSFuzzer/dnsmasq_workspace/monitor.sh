#!/bin/bash

export SNPSFUZZER=/home/pirwani/SNPSFuzzer/

while true; do
    clear
    echo "=== Dnsmasq Fuzzing Status ==="
    echo ""
    $SNPSFUZZER/afl-whatsup fuzz_output 2>/dev/null || echo "Waiting for fuzzer..."
    echo ""
    echo "Crashes:"
    ls -lh fuzz_output/replayable-crashes/ 2>/dev/null | tail -5 || echo "No crashes yet"
    echo ""
    echo "Press Ctrl+C to exit"
    sleep 5
done
