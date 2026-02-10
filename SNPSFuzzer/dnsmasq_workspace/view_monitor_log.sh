#!/bin/bash

echo "=== LTL Monitor Logs ==="
echo ""

if [ ! -f "/tmp/monitor.log" ]; then
    echo "Monitor log not found at /tmp/monitor.log"
    echo "Monitor may not be running or not enabled."
    exit 1
fi

echo "Showing last 50 lines of monitor log:"
echo "========================================"
tail -50 /tmp/monitor.log

echo ""
echo "========================================"
echo "To follow in real-time: tail -f /tmp/monitor.log"
echo ""

if [ -f "/tmp/monitor_violations.log" ]; then
    VIOLATIONS=$(grep -c "VIOLATION" /tmp/monitor_violations.log 2>/dev/null || echo 0)
    echo "Total violations detected: $VIOLATIONS"
    
    if [ $VIOLATIONS -gt 0 ]; then
        echo ""
        echo "Latest violations:"
        grep "VIOLATION" /tmp/monitor_violations.log | tail -5
    fi
fi
