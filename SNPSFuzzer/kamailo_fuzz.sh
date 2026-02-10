#!/bin/bash

# SNPSFuzzer Kamailio SIP Fuzzing Setup Script
# This script sets up and runs fuzzing for Kamailio SIP server

set -e  # Exit on error

echo "[*] Starting SNPSFuzzer Kamailio setup..."

# =============================================================================
# Configuration
# =============================================================================

export WORKDIR=$(pwd)/kamailio_workspace
# export SNPSFUZZER=$(pwd)/SNPSFuzzer  # Update this to your SNPSFuzzer path

# Create workspace
mkdir -p $WORKDIR
cd $WORKDIR

echo "[+] Workspace created at: $WORKDIR"

# =============================================================================
# Step 0: Copy patches from SNPSFuzzer directory
# =============================================================================

echo "[*] Step 0: Copying patches from SNPSFuzzer..."

if [ ! -f "$SNPSFUZZER/kamailio.patch" ] || [ ! -f "$SNPSFUZZER/pjsip.patch" ]; then
    echo "[!] Error: Patches not found in SNPSFuzzer directory!"
    echo "[!] Expected files:"
    echo "    $SNPSFUZZER/kamailio.patch"
    echo "    $SNPSFUZZER/pjsip.patch"
    exit 1
fi

cp $SNPSFUZZER/kamailio.patch .
cp $SNPSFUZZER/pjsip.patch .
echo "[+] Patches copied from SNPSFuzzer directory!"

# =============================================================================
# Step 1: Build Kamailio with SNPSFuzzer instrumentation
# =============================================================================

echo "[*] Step 1: Building Kamailio with SNPSFuzzer..."

# Set up SNPSFuzzer compiler
export CC=$SNPSFUZZER/afl-clang-fast
export CXX=$SNPSFUZZER/afl-clang-fast++
export AFL_PATH=$SNPSFUZZER

# Set ASAN options
export ASAN_OPTIONS='abort_on_error=1:symbolize=0:detect_leaks=0:detect_stack_use_after_return=1:detect_container_overflow=0:poison_array_cookie=0:malloc_fill_byte=0:max_malloc_fill_size=16777216'

# Clone and build Kamailio for fuzzing
if [ ! -d "kamailio" ]; then
    echo "[*] Cloning Kamailio..."
    git clone https://github.com/kamailio/kamailio.git
    cd kamailio
    git checkout 2648eb3
    patch -p1 < $WORKDIR/kamailio.patch
else
    echo "[*] Kamailio directory already exists..."
    cd kamailio
fi

echo "[*] Building Kamailio with SNPSFuzzer instrumentation..."
make clean || true
CC=$SNPSFUZZER/afl-clang-fast make MEMPKG=sys cfg
AFL_USE_ASAN=1 CC=$SNPSFUZZER/afl-clang-fast make all -j$(nproc)

cd $WORKDIR

echo "[+] Kamailio built successfully!"

# =============================================================================
# Step 2: Build PJSIP client (for testing)
# =============================================================================

echo "[*] Step 2: Building PJSIP client..."

if [ ! -d "pjproject" ]; then
    echo "[*] Cloning PJSIP..."
    git clone https://github.com/pjsip/pjproject.git
    cd pjproject
    git checkout bba95b8
    patch -p1 < $WORKDIR/pjsip.patch
else
    echo "[*] PJSIP directory already exists..."
    cd pjproject
fi

echo "[*] Building PJSIP..."
./configure
make dep && make clean && make -j$(nproc)

cd $WORKDIR

echo "[+] PJSIP client built successfully!"

# =============================================================================
# Step 3: Optional - Build Kamailio for coverage analysis
# =============================================================================

read -p "[?] Do you want to build Kamailio for coverage analysis? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "[*] Building Kamailio with coverage instrumentation..."
    
    if [ ! -d "kamailio-gcov" ]; then
        git clone https://github.com/kamailio/kamailio.git kamailio-gcov
        cd kamailio-gcov
        git checkout 2648eb3
        patch -p1 < $WORKDIR/kamailio.patch
    else
        cd kamailio-gcov
    fi
    
    make clean || true
    make CC_EXTRA_OPTS="-fprofile-arcs -ftest-coverage" \
         LD_EXTRA_OPTS="-fprofile-arcs -ftest-coverage" \
         MEMPKG=sys cfg
    make CC_EXTRA_OPTS="-fprofile-arcs -ftest-coverage" \
         LD_EXTRA_OPTS="-fprofile-arcs -ftest-coverage" \
         all -j$(nproc)
    
    cd $WORKDIR
    echo "[+] Coverage build completed!"
fi

# =============================================================================
# Step 4: Set up fuzzing directories and seeds
# =============================================================================

echo "[*] Step 4: Setting up fuzzing environment..."

# Create fuzzing directories
mkdir -p fuzz_input
mkdir -p fuzz_output

# Check if SIP seeds exist in SNPSFuzzer
if [ -d "$SNPSFUZZER/tutorials/kamailio/in-sip" ]; then
    echo "[*] Copying SIP seeds from SNPSFuzzer..."
    cp -r $SNPSFUZZER/tutorials/kamailio/in-sip/* fuzz_input/
else
    echo "[!] Warning: No SIP seeds found in SNPSFuzzer"
    echo "[*] Creating basic SIP seed inputs..."
    
    # Create basic SIP INVITE message
    cat > fuzz_input/invite.sip << 'SIPEOF'
INVITE sip:bob@127.0.0.1:5060 SIP/2.0
Via: SIP/2.0/UDP 127.0.0.1:5061;branch=z9hG4bK776asdhds
Max-Forwards: 70
To: Bob <sip:bob@127.0.0.1:5060>
From: Alice <sip:alice@127.0.0.1:5061>;tag=1928301774
Call-ID: a84b4c76e66710@127.0.0.1
CSeq: 314159 INVITE
Contact: <sip:alice@127.0.0.1:5061>
Content-Type: application/sdp
Content-Length: 142

v=0
o=user1 53655765 2353687637 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 6000 RTP/AVP 0
a=rtpmap:0 PCMU/8000
SIPEOF

    # Create basic SIP REGISTER message
    cat > fuzz_input/register.sip << 'SIPEOF'
REGISTER sip:127.0.0.1:5060 SIP/2.0
Via: SIP/2.0/UDP 127.0.0.1:5061;branch=z9hG4bK.eAV4o0nXr
Max-Forwards: 70
From: <sip:alice@127.0.0.1>;tag=SW50m6c5r
To: <sip:alice@127.0.0.1>
Call-ID: vhKbd5fb28
CSeq: 20 REGISTER
Contact: <sip:alice@127.0.0.1:5061>
Expires: 3600
Content-Length: 0

SIPEOF

    # Create basic SIP OPTIONS message
    cat > fuzz_input/options.sip << 'SIPEOF'
OPTIONS sip:127.0.0.1:5060 SIP/2.0
Via: SIP/2.0/UDP 127.0.0.1:5061;branch=z9hG4bK776asdhds
Max-Forwards: 70
To: <sip:127.0.0.1:5060>
From: Alice <sip:alice@127.0.0.1:5061>;tag=1928301774
Call-ID: a84b4c76e66710@127.0.0.1
CSeq: 63104 OPTIONS
Contact: <sip:alice@127.0.0.1:5061>
Accept: application/sdp
Content-Length: 0

SIPEOF

    echo "[+] Created basic SIP seed inputs"
fi

echo "[+] Fuzzing environment ready!"

# =============================================================================
# Step 5: Create Kamailio configuration
# =============================================================================

echo "[*] Step 5: Creating Kamailio configuration..."

cat > kamailio.cfg << 'KAMEOF'
#!KAMAILIO

# Minimal Kamailio configuration for fuzzing
# Based on AFLNet tutorial setup

####### Global Parameters #########

debug=2
log_stderror=yes
fork=no
children=1

port=5060
listen=udp:127.0.0.1:5060

####### Modules Section ########

mpath="kamailio/src/modules/"

loadmodule "sl.so"
loadmodule "tm.so"
loadmodule "rr.so"
loadmodule "pv.so"
loadmodule "maxfwd.so"
loadmodule "textops.so"
loadmodule "siputils.so"
loadmodule "xlog.so"

####### Routing Logic ########

request_route {
    # Log requests
    xlog("L_INFO", "Received $rm from $si:$sp\n");
    
    # Max forwards check
    if (!mf_process_maxfwd_header("10")) {
        sl_send_reply("483", "Too Many Hops");
        exit;
    }
    
    # Record routing
    if (method!="REGISTER") {
        record_route();
    }
    
    # Reply to OPTIONS
    if (method==OPTIONS) {
        sl_send_reply("200", "OK");
        exit;
    }
    
    # Accept REGISTER
    if (method==REGISTER) {
        sl_send_reply("200", "OK");
        exit;
    }
    
    # Accept other requests
    sl_send_reply("200", "OK");
    exit;
}
KAMEOF

echo "[+] Kamailio configuration created!"

# =============================================================================
# Step 6: Test Kamailio installation
# =============================================================================

echo "[*] Step 6: Testing Kamailio installation..."

# Start Kamailio in background
echo "[*] Starting Kamailio on port 5060..."
./kamailio/src/kamailio -f kamailio.cfg -DD > /tmp/kamailio-test.log 2>&1 &
KAMAILIO_PID=$!

# Wait for server to start
sleep 3

# Test with SIP OPTIONS using netcat
echo "[*] Testing SIP OPTIONS..."
timeout 5 bash -c 'echo -e "OPTIONS sip:127.0.0.1:5060 SIP/2.0\r\nVia: SIP/2.0/UDP 127.0.0.1:5061\r\nFrom: <sip:test@127.0.0.1>\r\nTo: <sip:test@127.0.0.1>\r\nCall-ID: test123\r\nCSeq: 1 OPTIONS\r\nContent-Length: 0\r\n\r\n" | nc -u -w 1 127.0.0.1 5060' || true

# Kill test server
kill $KAMAILIO_PID 2>/dev/null || true
sleep 1

echo "[+] Kamailio test completed!"

# =============================================================================
# Step 7: Create fuzzing script
# =============================================================================

echo "[*] Step 7: Creating fuzzing script..."

cat > run_fuzzing.sh << 'FUZZEOF'
#!/bin/bash

# Set environment
export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER
export AFL_PATH=$SNPSFUZZER

# Set ASAN options
export ASAN_OPTIONS='abort_on_error=1:symbolize=0:detect_leaks=0:detect_stack_use_after_return=1:detect_container_overflow=0:poison_array_cookie=0:malloc_fill_byte=0:max_malloc_fill_size=16777216'

# Fuzzing parameters
INPUT_DIR="$WORKDIR/fuzz_input"
OUTPUT_DIR="$WORKDIR/fuzz_output"
TARGET="$WORKDIR/kamailio/src/kamailio"
CONFIG="$WORKDIR/kamailio.cfg"

# SIP typically runs on UDP port 5060
SIP_PORT=5060

echo "[*] Starting SNPSFuzzer for Kamailio..."
echo "[*] Input: $INPUT_DIR"
echo "[*] Output: $OUTPUT_DIR"
echo "[*] Target: $TARGET"
echo "[*] Config: $CONFIG"
echo "[*] Protocol: SIP on UDP port $SIP_PORT"
echo ""

$SNPSFUZZER/afl-fuzz \
    -d \
    -i $INPUT_DIR \
    -o $OUTPUT_DIR \
    -N udp://127.0.0.1/$SIP_PORT \
    -P SIP \
    -D 10000 \
    -q 3 \
    -s 3 \
    -E \
    -K \
    -R \
    -W 100 \
    -a 2 \
    -b 100 \
    -m none \
    -- $TARGET -f $CONFIG -DD
FUZZEOF

# Replace placeholder with actual SNPSFuzzer path
sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" run_fuzzing.sh
chmod +x run_fuzzing.sh

echo "[+] Fuzzing script created: run_fuzzing.sh"

# =============================================================================
# Step 8: Create monitoring script
# =============================================================================

echo "[*] Step 8: Creating monitoring script..."

cat > monitor_fuzzing.sh << 'MONEOF'
#!/bin/bash

export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER

echo "=== SNPSFuzzer Kamailio SIP Monitoring ==="
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
    echo "=== Recent Queue Items ==="
    if [ -d "$WORKDIR/fuzz_output/replayable-queue" ]; then
        ls -lht $WORKDIR/fuzz_output/replayable-queue/ 2>/dev/null | head -5
    else
        echo "No queue directory yet"
    fi
    echo ""
    echo "=== System Info ==="
    echo "CPU: $(top -bn1 | grep "Cpu(s)" | awk '{print $2}')% usage"
    echo "Memory: $(free -h | awk '/^Mem:/ {print $3 "/" $2}')"
    sleep 5
done
MONEOF

sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" monitor_fuzzing.sh
chmod +x monitor_fuzzing.sh

echo "[+] Monitoring script created: monitor_fuzzing.sh"

# =============================================================================
# Step 9: Create helper scripts
# =============================================================================

echo "[*] Step 9: Creating helper scripts..."

# Crash replay script
cat > replay_crash.sh << 'REPEOF'
#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <crash_file>"
    exit 1
fi

export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER

echo "[*] Replaying crash: $1"
$SNPSFUZZER/afl-replay "$1" udp 127.0.0.1 5060
REPEOF

sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" replay_crash.sh
chmod +x replay_crash.sh

# Coverage analysis script
cat > analyze_coverage.sh << 'COVEOF'
#!/bin/bash

export WORKDIR=$(pwd)

if [ ! -d "kamailio-gcov" ]; then
    echo "[!] Error: kamailio-gcov not found. Run setup with coverage option."
    exit 1
fi

echo "[*] Analyzing coverage..."

# Create coverage directory
mkdir -p coverage_results

# Run gcov on instrumented binary
cd kamailio-gcov/src
lcov --capture --directory . --output-file $WORKDIR/coverage_results/coverage.info
genhtml $WORKDIR/coverage_results/coverage.info --output-directory $WORKDIR/coverage_results/html

cd $WORKDIR

echo "[+] Coverage analysis complete!"
echo "[*] Open coverage_results/html/index.html in a browser to view results"
COVEOF

chmod +x analyze_coverage.sh

# SIP message parser
cat > parse_sip.sh << 'PARSEEOF'
#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <sip_message_file>"
    exit 1
fi

echo "=== SIP Message Parser ==="
echo ""
cat "$1"
echo ""
echo "=== Basic Analysis ==="
echo "Method: $(head -1 "$1" | awk '{print $1}')"
echo "Request-URI: $(head -1 "$1" | awk '{print $2}')"
echo "SIP Version: $(head -1 "$1" | awk '{print $3}')"
echo "Content-Length: $(grep -i "^Content-Length:" "$1" | awk '{print $2}' | tr -d '\r')"
PARSEEOF

chmod +x parse_sip.sh

# Clean output script
cat > clean_output.sh << 'CLEANEOF'
#!/bin/bash

echo "[*] Cleaning fuzzing output..."

read -p "[?] This will delete all fuzzing output. Continue? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf fuzz_output/*
    echo "[+] Fuzzing output cleaned!"
else
    echo "[*] Cancelled"
fi
CLEANEOF

chmod +x clean_output.sh

echo "[+] Helper scripts created!"

# =============================================================================
# Step 10: Create README
# =============================================================================

cat > README.md << 'READEOF'
# Kamailio SIP Fuzzing with SNPSFuzzer

This workspace contains everything needed to fuzz Kamailio SIP server with SNPSFuzzer.

## Directory Structure

```
kamailio_workspace/
├── kamailio/              # Instrumented Kamailio for fuzzing
├── kamailio-gcov/         # Kamailio with coverage instrumentation (optional)
├── pjproject/             # PJSIP client for testing
├── fuzz_input/            # Seed inputs for fuzzing
├── fuzz_output/           # Fuzzing results
├── kamailio.cfg           # Kamailio configuration
└── *.sh                   # Helper scripts
```

## Quick Start

1. **Start fuzzing:**
   ```bash
   ./run_fuzzing.sh
   ```

2. **Monitor progress (in another terminal):**
   ```bash
   ./monitor_fuzzing.sh
   ```

## Scripts

- `run_fuzzing.sh` - Start the fuzzer
- `monitor_fuzzing.sh` - Monitor fuzzing progress
- `replay_crash.sh <file>` - Replay a crash
- `parse_sip.sh <file>` - Parse and analyze SIP messages
- `analyze_coverage.sh` - Generate coverage report (requires gcov build)
- `clean_output.sh` - Clean fuzzing output directory

## Fuzzing Parameters

The fuzzing configuration in `run_fuzzing.sh`:
- Protocol: SIP (Session Initiation Protocol)
- Transport: UDP
- Port: 5060 (standard SIP port)
- Polling timeout: 100ms
- Response timeout: 10000ms (10s)

## Understanding Results

- **fuzz_output/replayable-crashes/** - Crashing inputs
- **fuzz_output/replayable-queue/** - Interesting queue inputs
- **fuzz_output/plot_data** - Statistics for plotting

## Troubleshooting

1. **Fuzzer won't start:**
   - Check that SNPSFUZZER path is set correctly
   - Ensure port 5060 is not in use: `sudo lsof -i :5060`

2. **No coverage:**
   - Make sure you built with coverage option during setup
   - Check that test cases were executed against gcov binary

3. **Server crashes immediately:**
   - Check `/tmp/kamailio-test.log` for errors
   - Verify kamailio.cfg is valid
   - Check ASAN output in fuzzing terminal

## SIP Protocol Basics

Common SIP methods in seeds:
- INVITE - Initiate a call
- REGISTER - Register with SIP server
- OPTIONS - Query server capabilities
- ACK - Acknowledge response
- BYE - Terminate call
- CANCEL - Cancel pending request

## References

- SNPSFuzzer: https://github.com/SNPS-Security/SNPSFuzzer
- Kamailio: https://www.kamailio.org/
- SIP RFC 3261: https://tools.ietf.org/html/rfc3261
READEOF

echo "[+] README created!"

# =============================================================================
# Finish
# =============================================================================

echo ""
echo "=========================================="
echo "[+] Setup completed successfully!"
echo "=========================================="
echo ""
echo "Directory: $WORKDIR"
echo ""
echo "Quick start:"
echo "  cd $WORKDIR"
echo "  ./run_fuzzing.sh"
echo ""
echo "To monitor fuzzing (in another terminal):"
echo "  cd $WORKDIR"
echo "  ./monitor_fuzzing.sh"
echo ""
echo "Other commands:"
echo "  ./replay_crash.sh <crash_file>      # Replay a crash"
echo "  ./parse_sip.sh <sip_file>           # Parse SIP message"
echo "  ./analyze_coverage.sh               # Generate coverage report"
echo "  ./clean_output.sh                   # Clean fuzzing output"
echo ""
echo "Documentation:"
echo "  cat README.md"
echo ""
echo "=========================================="