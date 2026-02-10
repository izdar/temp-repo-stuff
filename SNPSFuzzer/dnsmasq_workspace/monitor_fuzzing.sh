#!/bin/bash

export WORKDIR=$(pwd)
export SNPSFUZZER=/home/pirwani/SNPSFuzzer/

echo "=== SNPSFuzzer Dnsmasq Monitoring ==="
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
    
    echo "=== LTL Monitor Violations ==="
    if [ -f "/tmp/monitor_violations.log" ]; then
        echo "Total violations: $(grep -c "VIOLATION" /tmp/monitor_violations.log 2>/dev/null || echo 0)"
        echo "Last 3 violations:"
        tail -3 /tmp/monitor_violations.log 2>/dev/null | grep "VIOLATION" || echo "No recent violations"
    else
        echo "Monitor log not found (may not be enabled)"
    fi
    echo ""
    
    echo "=== System Info ==="
    echo "CPU: $(top -bn1 | grep "Cpu(s)" | awk '{print $2}' 2>/dev/null || echo "N/A")% usage"
    echo "Memory: $(free -h | awk '/^Mem:/ {print $3 "/" $2}' 2>/dev/null || echo "N/A")"
    
    sleep 5
done
