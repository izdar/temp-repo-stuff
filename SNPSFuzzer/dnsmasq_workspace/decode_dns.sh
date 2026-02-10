#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <dns_packet_file>"
    echo "Example: $0 fuzz_input/query_a_test.bin"
    exit 1
fi

echo "=== DNS Packet Analysis ==="
echo "File: $1"
echo ""

# Use xxd to show hex dump
echo "Hex dump (first 128 bytes):"
xxd -l 128 "$1"
echo ""

# Try to decode with dig if available
if command -v dig &> /dev/null; then
    echo "=== Testing query against dnsmasq ==="
    # Send to dnsmasq on port 5353
    nc -u 127.0.0.1 5353 < "$1" | xxd -l 128
fi
