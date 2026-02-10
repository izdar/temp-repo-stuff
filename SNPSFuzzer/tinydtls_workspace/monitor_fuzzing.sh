#!/bin/bash

export WORKDIR=$(pwd)
export SNPSFUZZER=/home/pirwani/SNPSFuzzer

echo "=== SNPSFuzzer TinyDTLS Monitoring ==="
echo ""
echo "Press Ctrl+C to exit"
echo ""

while true; do
    clear
    echo "=== Fuzzing Status ==="
    $SNPSFUZZER/afl-whatsup $WORKDIR/fuzz_output 2>/dev/null || echo "No fuzzing data yet..."
    echo ""
    echo "=== Recent Crashes ==="
    if [ -d "$WORKDIR/fuzz_output/replayable-crashes" ]; then
        ls -lht $WORKDIR/fuzz_output/replayable-crashes/ 2>/dev/null | head -10
    else
        echo "No crashes directory yet"
    fi
    echo ""
    echo "=== System Info ==="
    echo "CPU: $(top -bn1 | grep "Cpu(s)" | awk '{print $2}')% usage"
    echo "Memory: $(free -h | awk '/^Mem:/ {print $3 "/" $2}')"
    sleep 5
done
