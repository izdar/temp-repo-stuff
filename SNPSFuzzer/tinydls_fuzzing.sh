#!/bin/bash

# SNPSFuzzer TinyDTLS Fuzzing Setup Script
# This script sets up and runs fuzzing for TinyDTLS DTLS 1.2 server

set -e  # Exit on error

echo "[*] Starting SNPSFuzzer TinyDTLS setup..."

# =============================================================================
# Configuration
# =============================================================================

export WORKDIR=$(pwd)/tinydtls_workspace
# export SNPSFUZZER=$(pwd)/SNPSFuzzer  # Update this to your SNPSFuzzer path

# Create workspace
mkdir -p $WORKDIR
cd $WORKDIR

echo "[+] Workspace created at: $WORKDIR"

# =============================================================================
# Step 0: Build TinyDTLS with SNPSFuzzer instrumentation
# =============================================================================

echo "[*] Step 0: Building TinyDTLS..."

# Clone de-randomized version of TinyDTLS
if [ ! -d "tinydtls" ]; then
    echo "[*] Cloning TinyDTLS fork..."
    git clone https://github.com/assist-project/tinydtls-fuzz.git tinydtls
else
    echo "[*] TinyDTLS directory already exists, skipping clone..."
fi

cd tinydtls/tests
git checkout 06995d4

# Set up SNPSFuzzer compiler
export CC=$SNPSFUZZER/afl-clang-fast
export CXX=$SNPSFUZZER/afl-clang-fast++
export AFL_PATH=$SNPSFUZZER

echo "[*] Building with SNPSFuzzer instrumentation..."
make clean
make all

cd $WORKDIR

echo "[+] TinyDTLS built successfully!"

# =============================================================================
# Step 1: Test TinyDTLS installation
# =============================================================================

echo "[*] Step 1: Testing TinyDTLS installation..."

# Start server in background
echo "[*] Starting DTLS server on port 20220..."
./tinydtls/tests/dtls-server > /tmp/dtls-server-test.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Test with client
echo "[*] Testing client connection..."
timeout 5 ./tinydtls/tests/dtls-client localhost > /tmp/dtls-client-test.log 2>&1 || true

# Kill test server
kill $SERVER_PID 2>/dev/null || true
sleep 1

echo "[+] TinyDTLS test completed!"

# =============================================================================
# Step 2: Optional - Disable EC for faster fuzzing
# =============================================================================

read -p "[?] Do you want to disable EC cipher suite for faster fuzzing? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "[*] Disabling DTLS_ECC in dtls-server.c..."
    
    # Backup original
    cp tinydtls/tests/dtls-server.c tinydtls/tests/dtls-server.c.backup
    
    # Comment out DTLS_ECC definition
    sed -i 's/^#define DTLS_ECC/\/\/ #define DTLS_ECC/g' tinydtls/tests/dtls-server.c
    
    # Rebuild
    cd tinydtls/tests
    make clean
    make all
    cd $WORKDIR
    
    echo "[+] EC disabled and server rebuilt!"
fi

# =============================================================================
# Step 3: Set up fuzzing directories and seeds
# =============================================================================

echo "[*] Step 3: Setting up fuzzing environment..."

# Create fuzzing directories
mkdir -p fuzz_input
mkdir -p fuzz_output

# Check if handshake captures exist in SNPSFuzzer
if [ -d "$SNPSFUZZER/tutorials/tinydtls/handshake_captures" ]; then
    echo "[*] Copying handshake captures from SNPSFuzzer..."
    cp -r $SNPSFUZZER/tutorials/tinydtls/handshake_captures/* fuzz_input/
else
    echo "[!] Warning: No handshake captures found in SNPSFuzzer"
    echo "[*] Creating basic seed inputs..."
    
    # Create basic DTLS ClientHello seed
    python3 << 'PYEOF'
import struct

# Basic DTLS 1.2 ClientHello structure
content_type = b'\x16'  # Handshake
version = b'\xfe\xfd'   # DTLS 1.2
epoch = b'\x00\x00'
sequence = b'\x00' * 6
length = b'\x00\x40'    # Placeholder length

# Handshake header
msg_type = b'\x01'      # ClientHello
msg_length = b'\x00\x00\x34'
msg_seq = b'\x00\x00'
frag_offset = b'\x00\x00\x00'
frag_length = b'\x00\x00\x34'

# ClientHello body
client_version = b'\xfe\xfd'  # DTLS 1.2
random = b'\x00' * 32
session_id_len = b'\x00'
cookie_len = b'\x00'
cipher_suites_len = b'\x00\x02'
cipher_suites = b'\x00\x2f'  # TLS_RSA_WITH_AES_128_CBC_SHA
compression_len = b'\x01'
compression = b'\x00'

payload = (msg_type + msg_length + msg_seq + frag_offset + frag_length + 
           client_version + random + session_id_len + cookie_len + 
           cipher_suites_len + cipher_suites + compression_len + compression)

record = content_type + version + epoch + sequence + length + payload

with open('fuzz_input/seed1.bin', 'wb') as f:
    f.write(record)

print("Created basic seed input")
PYEOF
fi

echo "[+] Fuzzing environment ready!"

# =============================================================================
# Step 4: Create fuzzing script
# =============================================================================

echo "[*] Creating fuzzing script..."

cat > run_fuzzing.sh << 'FUZZEOF'
#!/bin/bash

# Set environment
export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER
export AFL_PATH=$SNPSFUZZER

# Fuzzing parameters
INPUT_DIR="$WORKDIR/fuzz_input"
OUTPUT_DIR="$WORKDIR/fuzz_output"
TARGET="$WORKDIR/tinydtls/tests/dtls-server"

# Check if EC is disabled (faster fuzzing with -W 10)
# If EC is enabled, use -W 30 for slower polling
POLLING_TIMEOUT=30

if grep -q "^// #define DTLS_ECC" $WORKDIR/tinydtls/tests/dtls-server.c; then
    echo "[*] EC cipher suite disabled - using faster timeout"
    POLLING_TIMEOUT=10
else
    echo "[*] EC cipher suite enabled - using slower timeout"
fi

echo "[*] Starting SNPSFuzzer for TinyDTLS..."
echo "[*] Input: $INPUT_DIR"
echo "[*] Output: $OUTPUT_DIR"
echo "[*] Target: $TARGET"
echo "[*] Polling timeout: $POLLING_TIMEOUT ms"
echo ""

$SNPSFUZZER/afl-fuzz \
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
FUZZEOF

# Replace placeholder with actual SNPSFuzzer path
sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" run_fuzzing.sh
chmod +x run_fuzzing.sh

echo "[+] Fuzzing script created: run_fuzzing.sh"

# =============================================================================
# Step 5: Create monitoring script
# =============================================================================

echo "[*] Creating monitoring script..."

cat > monitor_fuzzing.sh << 'MONEOF'
#!/bin/bash

export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER

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
MONEOF

sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" monitor_fuzzing.sh
chmod +x monitor_fuzzing.sh

echo "[+] Monitoring script created: monitor_fuzzing.sh"

# =============================================================================
# Step 6: Create helper scripts
# =============================================================================

# Status code decoder
cat > decode_status.sh << 'DECEOF'
#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <status_code_decimal>"
    echo "Example: $0 5635"
    exit 1
fi

HEX=$(printf '%x' $1)
CONTENT_TYPE=${HEX:0:2}
MESSAGE_TYPE=${HEX:2:2}

echo "Status Code: $1 (decimal) = 0x$HEX (hex)"
echo "Content Type: 0x$CONTENT_TYPE"
echo "Message Type: 0x$MESSAGE_TYPE"
echo ""
echo "Common DTLS Content Types:"
echo "  16 (0x16) - Handshake"
echo "  17 (0x17) - Application Data"
echo "  14 (0x14) - Change Cipher Spec"
echo "  15 (0x15) - Alert"
DECEOF

chmod +x decode_status.sh

# Crash replay script
cat > replay_crash.sh << 'REPEOF'
#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <crash_file>"
    exit 1
fi

export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER

$SNPSFUZZER/afl-replay "$1" udp 127.0.0.1 20220
REPEOF

sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" replay_crash.sh
chmod +x replay_crash.sh

echo "[+] Helper scripts created!"

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
echo "To start fuzzing:"
echo "  cd $WORKDIR"
echo "  ./run_fuzzing.sh"
echo ""
echo "To monitor fuzzing (in another terminal):"
echo "  cd $WORKDIR"
echo "  ./monitor_fuzzing.sh"
echo ""
echo "To decode status codes:"
echo "  ./decode_status.sh <decimal_code>"
echo ""
echo "To replay a crash:"
echo "  ./replay_crash.sh <path_to_crash>"
echo ""
echo "=========================================="