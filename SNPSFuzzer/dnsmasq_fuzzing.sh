#!/bin/bash

# SNPSFuzzer Dnsmasq Fuzzing Script (AFLNet-compatible)
# Based on the official AFLNet dnsmasq tutorial

set -e

echo "[*] SNPSFuzzer Dnsmasq Fuzzing Setup"
echo ""

# =============================================================================
# Check Prerequisites
# =============================================================================

if [ -z "$SNPSFUZZER" ]; then
    echo "[!] ERROR: SNPSFUZZER environment variable not set"
    echo ""
    echo "Please run:"
    echo "  export SNPSFUZZER=/path/to/SNPSFuzzer"
    echo ""
    exit 1
fi

if [ ! -f "$SNPSFUZZER/afl-clang-fast" ]; then
    echo "[!] ERROR: afl-clang-fast not found at $SNPSFUZZER"
    exit 1
fi

echo "[+] Using SNPSFuzzer: $SNPSFUZZER"

# =============================================================================
# Setup Directories
# =============================================================================

export WORKDIR=$(pwd)/dnsmasq_workspace
export AFL_PATH=$SNPSFUZZER

mkdir -p $WORKDIR
cd $WORKDIR

echo "[+] Working directory: $WORKDIR"

# =============================================================================
# Build Dnsmasq v2.73rc6
# =============================================================================

echo ""
echo "[*] Building dnsmasq v2.73rc6..."

if [ ! -d "dnsmasq" ]; then
    git clone git://thekelleys.org.uk/dnsmasq.git
fi

cd dnsmasq
git checkout v2.73rc6

echo "[*] Compiling with afl-clang-fast..."
CC=$SNPSFUZZER/afl-clang-fast make clean
CC=$SNPSFUZZER/afl-clang-fast make

cd $WORKDIR

echo "[+] Dnsmasq built successfully"

# =============================================================================
# Create Configuration (AFLNet-compatible)
# =============================================================================

echo ""
echo "[*] Creating dnsmasq.conf..."

cat > dnsmasq.conf << 'EOF'
# AFLNet-compatible dnsmasq configuration
port=5353
bind-interfaces
listen-address=127.0.0.1
no-resolv
no-hosts
no-poll
address=/test.com/5.5.5.5
address=/example.com/1.2.3.4
no-dnssec
log-queries
EOF

echo "[+] Configuration created"

# =============================================================================
# Get Seed Inputs
# =============================================================================

echo ""
echo "[*] Setting up seed inputs..."

mkdir -p fuzz_input

if [ -d "$SNPSFUZZER/tutorials/dnsmasq/in-dns" ]; then
    echo "[*] Copying seeds from SNPSFuzzer..."
    cp $SNPSFUZZER/tutorials/dnsmasq/in-dns/* fuzz_input/
    echo "[+] Copied $(ls fuzz_input/ | wc -l) seed files"
else
    echo "[!] Seeds not found in SNPSFuzzer"
    echo "[*] Please add DNS query seeds to fuzz_input/"
    echo ""
    echo "You can:"
    echo "  1. Copy from AFLNet: cp \$AFLNET/tutorials/dnsmasq/in-dns/* fuzz_input/"
    echo "  2. Create with: dig @127.0.0.1 -p 5353 test.com > query.txt"
    echo "  3. Use tcpdump: tcpdump -w dns.pcap port 53"
    echo ""
    read -p "Continue without seeds? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

mkdir -p fuzz_output

# =============================================================================
# Test Dnsmasq
# =============================================================================

echo ""
echo "[*] Testing dnsmasq..."

# Start server briefly to test
./dnsmasq/src/dnsmasq -C dnsmasq.conf &
SERVER_PID=$!
sleep 2

if command -v dig &> /dev/null; then
    echo "[*] Testing with dig..."
    dig @127.0.0.1 -p 5353 test.com +short || echo "Query failed (may be normal)"
fi

kill $SERVER_PID 2>/dev/null || true
sleep 1

echo "[+] Test complete"

# =============================================================================
# Create Fuzzing Scripts
# =============================================================================

echo ""
echo "[*] Creating fuzzing scripts..."

# Main fuzzing script - EXACTLY like AFLNet tutorial
cat > run_fuzzing.sh << 'FUZZSCRIPT'
#!/bin/bash

SNPSFUZZER_PLACEHOLDER
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
    -m none \
    -- $WORKDIR/dnsmasq/src/dnsmasq -C $WORKDIR/dnsmasq.conf
FUZZSCRIPT

sed -i "s|SNPSFUZZER_PLACEHOLDER|export SNPSFUZZER=$SNPSFUZZER|g" run_fuzzing.sh
chmod +x run_fuzzing.sh

# Monitor script
cat > monitor.sh << 'MONSCRIPT'
#!/bin/bash

SNPSFUZZER_PLACEHOLDER

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
MONSCRIPT

sed -i "s|SNPSFUZZER_PLACEHOLDER|export SNPSFUZZER=$SNPSFUZZER|g" monitor.sh
chmod +x monitor.sh

# Replay script
cat > replay_crash.sh << 'REPSCRIPT'
#!/bin/bash

SNPSFUZZER_PLACEHOLDER

if [ -z "$1" ]; then
    echo "Usage: $0 <crash_file>"
    exit 1
fi

echo "[*] Replaying: $1"
$SNPSFUZZER/afl-replay "$1" tcp 127.0.0.1 5353
REPSCRIPT

sed -i "s|SNPSFUZZER_PLACEHOLDER|export SNPSFUZZER=$SNPSFUZZER|g" replay_crash.sh
chmod +x replay_crash.sh

echo "[+] Scripts created"

# =============================================================================
# Done
# =============================================================================

echo ""
echo "=========================================="
echo "[+] Setup Complete!"
echo "=========================================="
echo ""
echo "Directory: $WORKDIR"
echo ""
echo "To start fuzzing:"
echo "  cd $WORKDIR"
echo "  ./run_fuzzing.sh"
echo ""
echo "To monitor (another terminal):"
echo "  cd $WORKDIR"
echo "  ./monitor.sh"
echo ""
echo "To replay a crash:"
echo "  ./replay_crash.sh fuzz_output/replayable-crashes/id:000000*"
echo ""
echo "IMPORTANT NOTES:"
echo "  - DNS fuzzing uses TCP (not UDP) per AFLNet tutorial"
echo "  - Dnsmasq v2.73rc6 has known crashes"
echo "  - Let it run for several hours for best results"
echo ""
echo "=========================================="