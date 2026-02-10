#!/bin/bash

# SNPSFuzzer OpenSSH Fuzzing Setup Script with LTL Monitor
# This script sets up and runs fuzzing for OpenSSH with runtime LTL monitoring

set -e  # Exit on error

echo "[*] Starting SNPSFuzzer OpenSSH setup with LTL monitoring..."

# =============================================================================
# Configuration
# =============================================================================

export WORKDIR=$(pwd)/openssh_workspace
# export SNPSFUZZER=$(pwd)/SNPSFuzzer  # Update this to your SNPSFuzzer path

# OpenSSH configuration (matching AFLNet version)
# vegard/openssh-portable uses 'afl' as default branch (not master)
# The latest commit on 'afl' branch IS the AFLNet version
OPENSSH_BRANCH="afl"  # Default branch with AFLNet patches
OPENSSL_VERSION="OpenSSL_1_0_2u"  # Matching OpenSSL version
OPENSSH_PORT=2222

# Create workspace
mkdir -p $WORKDIR
cd $WORKDIR

echo "[+] Workspace created at: $WORKDIR"

# =============================================================================
# Step 0a: Build OpenSSL 1.0.2u (dependency for OpenSSH)
# =============================================================================

if [ -f "$WORKDIR/openssl-install/lib/libssl.so" ]; then
    echo "[*] Step 0a: OpenSSL already built at $WORKDIR/openssl-install"
    echo "[+] Skipping OpenSSL build..."
else
    echo "[*] Step 0a: Building OpenSSL 1.0.2u..."

    # Clone and build OpenSSL
    if [ ! -d "openssl" ]; then
        echo "[*] Cloning OpenSSL..."
        git clone https://github.com/openssl/openssl.git
    else
        echo "[*] OpenSSL directory already exists, skipping clone..."
    fi

    cd openssl
    git checkout $OPENSSL_VERSION

    echo "[*] Configuring OpenSSL..."
    ./Configure linux-x86_64-clang shared --prefix=$WORKDIR/openssl-install

    echo "[*] Building OpenSSL..."
    make clean
    make -j$(nproc)
    make install

    cd $WORKDIR

    echo "[+] OpenSSL 1.0.2u built successfully!"
fi

# Set LD_LIBRARY_PATH for OpenSSL
export LD_LIBRARY_PATH="$WORKDIR/openssl-install/lib:$LD_LIBRARY_PATH"

# =============================================================================
# Step 0b: Build OpenSSH with SNPSFuzzer instrumentation
# =============================================================================

echo "[*] Step 0b: Building OpenSSH..."

# Clone vegard/openssh-portable (AFLNet version)
if [ ! -d "openssh-portable" ]; then
    echo "[*] Cloning OpenSSH portable (vegard fork)..."
    git clone https://github.com/vegard/openssh-portable.git
else
    echo "[*] OpenSSH directory already exists, skipping clone..."
fi

cd openssh-portable

# Checkout the 'afl' branch (default branch with AFLNet patches)
echo "[*] Checking out '$OPENSSH_BRANCH' branch..."
git checkout $OPENSSH_BRANCH

echo "[*] Installing OpenSSH build dependencies..."
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    autoconf \
    automake \
    libtool \
    zlib1g-dev \
    libpam0g-dev \
    libselinux1-dev

# Apply AFLNet randomization patch
# Check multiple locations for the rand files
if [ -f "$SNPSFUZZER/rand.patch" ] && [ -f "$SNPSFUZZER/rand.inc" ]; then
    echo "[*] Applying AFLNet randomization patch from SNPSFuzzer root..."
    cp $SNPSFUZZER/rand.inc .
    
    # Check if the file to patch exists before applying
    if [ -f "openbsd-compat/bsd-arc4random.c" ]; then
        patch -p1 < $SNPSFUZZER/rand.patch || echo "[!] Patch already applied or failed"
    else
        echo "[!] Warning: openbsd-compat/bsd-arc4random.c not found"
        echo "[!] vegard's fork may have different structure - checking for alternatives..."
        
        # vegard's fork might already be deterministic, so just copy rand.inc
        echo "[*] Copying rand.inc for manual inclusion if needed..."
    fi
    
elif [ -f "$SNPSFUZZER/tutorials/openssh/rand.patch" ] && [ -f "$SNPSFUZZER/tutorials/openssh/rand.inc" ]; then
    echo "[*] Applying AFLNet randomization patch from tutorials..."
    cp $SNPSFUZZER/tutorials/openssh/rand.inc .
    
    if [ -f "openbsd-compat/bsd-arc4random.c" ]; then
        patch -p1 < $SNPSFUZZER/tutorials/openssh/rand.patch || echo "[!] Patch already applied or failed"
    else
        echo "[!] vegard's fork may already be deterministic"
    fi
    
else
    echo "[*] AFLNet patches not found in SNPSFuzzer"
    echo "[*] Note: vegard/openssh-portable 'afl' branch may already include deterministic patches"
    echo "[+] Continuing without additional patches..."
fi

# Set up SNPSFuzzer compiler
export AFL_PATH=$SNPSFUZZER

# Determine which AFL compiler to use
# Priority: afl-clang-fast > afl-clang > afl-gcc
if [ -x "$SNPSFUZZER/afl-clang-fast" ]; then
    echo "[*] Using afl-clang-fast for compilation (LLVM mode)"
    export CC=$SNPSFUZZER/afl-clang-fast
    export CXX=$SNPSFUZZER/afl-clang-fast++
elif [ -x "$SNPSFUZZER/afl-clang" ]; then
    echo "[*] Using afl-clang for compilation"
    export CC=$SNPSFUZZER/afl-clang
    export CXX=$SNPSFUZZER/afl-clang++
elif [ -x "$SNPSFUZZER/afl-gcc" ]; then
    echo "[*] Using afl-gcc for compilation"
    export CC=$SNPSFUZZER/afl-gcc
    export CXX=$SNPSFUZZER/afl-g++
else
    echo "[!] ERROR: No AFL compiler found!"
    echo "[!] Please build SNPSFuzzer first:"
    echo "    cd $SNPSFUZZER && make"
    exit 1
fi

echo "[+] Compiler: $CC"

echo "[*] Configuring OpenSSH with SNPSFuzzer instrumentation..."
autoreconf
./configure \
    CC="$CC" \
    CFLAGS="-g -O3 -I$WORKDIR/openssl-install/include" \
    --prefix=$WORKDIR/openssh-install \
    --with-openssl=$WORKDIR/openssl-install \
    --with-ldflags="-L$WORKDIR/openssl-install/lib" \
    --with-privsep-path=$WORKDIR/openssh-install/var-empty \
    --with-sandbox=no \
    --with-privsep-user=$(whoami) \
    --sysconfdir=$WORKDIR/openssh-install/etc/ssh

echo "[*] Building OpenSSH..."
AFL_USE_ASAN=1 make clean
AFL_USE_ASAN=1 make -j$(nproc)
make install

cd $WORKDIR

echo "[+] OpenSSH built successfully at: $WORKDIR/openssh-install"

# =============================================================================
# Step 1: Configure OpenSSH for fuzzing
# =============================================================================

echo "[*] Step 1: Configuring OpenSSH for fuzzing..."

# Create necessary directories
mkdir -p openssh-install/var/empty
mkdir -p openssh-install/var/run
mkdir -p openssh-install/etc/ssh
mkdir -p openssh-install/home/testuser/.ssh

# Generate host keys (only RSA for simplicity, like AFLNet)
echo "[*] Generating SSH host key..."
ssh-keygen -t rsa -f openssh-install/etc/ssh/ssh_host_rsa_key -N "" -q

# Generate test user keys
echo "[*] Generating test user keys..."
ssh-keygen -t rsa -f openssh-install/home/testuser/.ssh/id_rsa -N "" -q

# Create authorized_keys for test user
cp openssh-install/home/testuser/.ssh/id_rsa.pub openssh-install/home/testuser/.ssh/authorized_keys
chmod 600 openssh-install/home/testuser/.ssh/authorized_keys

# Create sshd_config for fuzzing
cat > openssh-install/etc/ssh/sshd_config << 'SSHEOF'
# SNPSFuzzer OpenSSH Configuration (AFLNet-compatible)
Port 2222
ListenAddress 127.0.0.1

# Host keys
HostKey WORKDIR_PLACEHOLDER/openssh-install/etc/ssh/ssh_host_rsa_key

# Authentication
PermitRootLogin yes
PermitEmptyPasswords yes
PasswordAuthentication yes
PubkeyAuthentication yes
ChallengeResponseAuthentication no
UsePAM no

# Disable privilege separation for fuzzing
UsePrivilegeSeparation no

# Logging
LogLevel DEBUG3

# Security (relaxed for fuzzing)
StrictModes no
MaxAuthTries 6

# No pidfile (running in foreground)
PidFile none
SSHEOF

# Replace placeholder with actual workspace
sed -i "s|WORKDIR_PLACEHOLDER|$WORKDIR|g" openssh-install/etc/ssh/sshd_config

echo "[+] OpenSSH configured for fuzzing!"

# =============================================================================
# Step 2: Test OpenSSH installation
# =============================================================================

echo "[*] Step 2: Testing OpenSSH installation..."

# Create cleanup script
cat > cleanup_sshd.sh << 'CLEANEOF'
#!/bin/bash
killall -9 sshd 2>/dev/null || true
killall -9 ssh 2>/dev/null || true
rm -f WORKDIR_PLACEHOLDER/openssh-install/var/run/sshd.pid
CLEANEOF

sed -i "s|WORKDIR_PLACEHOLDER|$WORKDIR|g" cleanup_sshd.sh
chmod +x cleanup_sshd.sh

# Clean up any existing processes
./cleanup_sshd.sh

# Start test server
echo "[*] Starting SSH server on port $OPENSSH_PORT..."
$WORKDIR/openssh-install/sbin/sshd -f $WORKDIR/openssh-install/etc/ssh/sshd_config -D &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Test connection
echo "[*] Testing SSH connection..."
timeout 5 ssh -p $OPENSSH_PORT \
    -o "StrictHostKeyChecking=no" \
    -o "UserKnownHostsFile=/dev/null" \
    -i $WORKDIR/openssh-install/home/testuser/.ssh/id_rsa \
    testuser@localhost "echo 'SSH connection successful'" || true

# Kill test server
kill $SERVER_PID 2>/dev/null || true
./cleanup_sshd.sh

echo "[+] OpenSSH test completed!"

# =============================================================================
# Step 3: Set up LTL Monitor
# =============================================================================

echo "[*] Step 3: Setting up LTL monitor..."

# Link to existing LTL specifications in monitor-bin
mkdir -p ltl_specs

if [ -f "$SNPSFUZZER/monitor-bin/ssh.txt" ]; then
    echo "[*] Linking to existing SSH LTL spec from monitor-bin..."
    ln -sf $SNPSFUZZER/monitor-bin/ssh.txt ltl_specs/ssh.ltl
    echo "[+] Using LTL spec: $SNPSFUZZER/monitor-bin/ssh.txt"
else
    echo "[!] Warning: ssh.txt not found in $SNPSFUZZER/monitor-bin"
    echo "[!] Please ensure monitor LTL specs are in monitor-bin directory"
    exit 1
fi

# Also link RTSP and FTP specs if available
if [ -f "$SNPSFUZZER/monitor-bin/rtsp.txt" ]; then
    ln -sf $SNPSFUZZER/monitor-bin/rtsp.txt ltl_specs/rtsp.ltl
    echo "[+] RTSP spec linked"
fi

if [ -f "$SNPSFUZZER/monitor-bin/ftp.txt" ]; then
    ln -sf $SNPSFUZZER/monitor-bin/ftp.txt ltl_specs/ftp.ltl
    echo "[+] FTP spec linked"
fi

# Check if formula_parser exists
if [ ! -f "$SNPSFUZZER/formula_parser" ]; then
    echo "[!] Warning: formula_parser not found in SNPSFuzzer"
    echo "[*] You may need to build the monitor first:"
    echo "    cd $SNPSFUZZER && make formula_parser"
fi

# =============================================================================
# Step 4: Set up fuzzing directories and seeds
# =============================================================================

echo "[*] Step 4: Setting up fuzzing environment..."

# Create fuzzing directories
mkdir -p fuzz_input
mkdir -p fuzz_output

# Check if SSH handshake captures exist
if [ -d "$SNPSFUZZER/tutorials/openssh/handshake_captures" ]; then
    echo "[*] Copying handshake captures from SNPSFuzzer..."
    cp -r $SNPSFUZZER/tutorials/openssh/handshake_captures/* fuzz_input/
else
    echo "[*] Creating basic SSH seed inputs..."
    
    # Create SSH-2.0 version exchange seed
    echo -en 'SSH-2.0-OpenSSH_8.9\r\n' > fuzz_input/seed_version.bin
    
    # Create basic SSH packet structure seed
    python3 << 'PYEOF'
import struct

# SSH packet: length + padding_length + payload + padding + mac
# Simple SSH_MSG_IGNORE message

payload = b'\x02'  # SSH_MSG_IGNORE
payload += b'\x00\x00\x00\x10'  # String length (16 bytes)
payload += b'A' * 16  # Ignore data

padding_length = 8
padding = b'\x00' * padding_length

packet_length = 1 + len(payload) + padding_length  # padding_length byte + payload + padding

packet = struct.pack('>I', packet_length)
packet += struct.pack('B', padding_length)
packet += payload
packet += padding

with open('fuzz_input/seed_packet.bin', 'wb') as f:
    f.write(packet)

print("Created SSH packet seed")
PYEOF

fi

echo "[+] Fuzzing environment ready!"

# =============================================================================
# Step 5: Create fuzzing script with LTL monitoring
# =============================================================================

echo "[*] Creating fuzzing script with LTL monitoring..."

cat > run_fuzzing.sh << 'FUZZEOF'
#!/bin/bash

# Set environment
export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER
export AFL_PATH=$SNPSFUZZER

# LTL Monitor configuration
export LTL_EVAL_PATH=$SNPSFUZZER/formula_parser
export LTL_SPEC_PATH=$WORKDIR/ltl_specs/ssh.ltl
export MONITOR_VERBOSE=1

# Fuzzing parameters
INPUT_DIR="$WORKDIR/fuzz_input"
OUTPUT_DIR="$WORKDIR/fuzz_output"
TARGET="$WORKDIR/openssh-install/sbin/sshd"
SSHD_CONFIG="$WORKDIR/openssh-install/etc/ssh/sshd_config"

# Clean up any existing processes
$WORKDIR/cleanup_sshd.sh

echo "[*] Starting SNPSFuzzer for OpenSSH with LTL monitoring..."
echo "[*] Input: $INPUT_DIR"
echo "[*] Output: $OUTPUT_DIR"
echo "[*] Target: $TARGET"
echo "[*] Config: $SSHD_CONFIG"
echo "[*] LTL Monitor: ENABLED"
echo "[*] LTL Spec: $LTL_SPEC_PATH"
echo ""

$SNPSFUZZER/afl-fuzz \
    -d \
    -i $INPUT_DIR \
    -o $OUTPUT_DIR \
    -N tcp://127.0.0.1/2222 \
    -P SSH \
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
    -- $TARGET -f $SSHD_CONFIG -D -e
FUZZEOF

sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" run_fuzzing.sh
chmod +x run_fuzzing.sh

echo "[+] Fuzzing script created: run_fuzzing.sh"

# =============================================================================
# Step 6: Create monitoring scripts
# =============================================================================

echo "[*] Creating monitoring scripts..."

# Main fuzzing monitor
cat > monitor_fuzzing.sh << 'MONEOF'
#!/bin/bash

export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER

echo "=== SNPSFuzzer OpenSSH Monitoring ==="
echo ""
echo "Press Ctrl+C to exit"
echo ""

while true; do
    clear
    echo "=== Fuzzing Status ==="
    $SNPSFUZZER/afl-whatsup $WORKDIR/fuzz_output 2>/dev/null || echo "No fuzzing data yet..."
    echo ""
    echo "=== LTL Violations ==="
    if [ -d "$WORKDIR/fuzz_output/violations" ]; then
        echo "Violation count: $(ls $WORKDIR/fuzz_output/violations 2>/dev/null | wc -l)"
        ls -lht $WORKDIR/fuzz_output/violations/ 2>/dev/null | head -5
    else
        echo "No violations directory yet"
    fi
    echo ""
    echo "=== Recent Crashes ==="
    if [ -d "$WORKDIR/fuzz_output/replayable-crashes" ]; then
        echo "Crash count: $(ls $WORKDIR/fuzz_output/replayable-crashes 2>/dev/null | wc -l)"
        ls -lht $WORKDIR/fuzz_output/replayable-crashes/ 2>/dev/null | head -5
    else
        echo "No crashes directory yet"
    fi
    echo ""
    echo "=== Monitor Log (last 10 lines) ==="
    if [ -f "/tmp/monitor.log" ]; then
        tail -n 10 /tmp/monitor.log
    else
        echo "No monitor log yet"
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

# LTL violation monitor
cat > monitor_violations.sh << 'VIOLEOF'
#!/bin/bash

export WORKDIR=$(pwd)

echo "=== LTL Violation Monitor ==="
echo "Watching for new violations..."
echo ""

inotifywait -m -e create --format '%f' $WORKDIR/fuzz_output/violations 2>/dev/null | while read file
do
    echo "[$(date)] NEW VIOLATION: $file"
    
    # Show violation details if available
    if [ -f "/tmp/monitor_violations.log" ]; then
        echo "Recent violation log:"
        tail -n 20 /tmp/monitor_violations.log
    fi
    
    echo "---"
done
VIOLEOF

chmod +x monitor_violations.sh

echo "[+] Monitoring scripts created!"

# =============================================================================
# Step 7: Create helper scripts
# =============================================================================

echo "[*] Creating helper scripts..."

# Crash replay script
cat > replay_crash.sh << 'REPEOF'
#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <crash_file>"
    exit 1
fi

export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER

# Clean up
$WORKDIR/cleanup_sshd.sh

# Replay with aflnet-replay
$SNPSFUZZER/aflnet-replay "$1" tcp 127.0.0.1 2222 1
REPEOF

sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" replay_crash.sh
chmod +x replay_crash.sh

# Violation replay script
cat > replay_violation.sh << 'VIOLREPEOF'
#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <violation_file>"
    exit 1
fi

export WORKDIR=$(pwd)
export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER
export LTL_EVAL_PATH=$SNPSFUZZER/formula_parser
export LTL_SPEC_PATH=$WORKDIR/ltl_specs/ssh.ltl
export MONITOR_VERBOSE=1

echo "[*] Replaying violation with LTL monitor..."
echo "[*] Violation file: $1"
echo ""

# Clean up
$WORKDIR/cleanup_sshd.sh

# Start server
$WORKDIR/openssh-install/sbin/sshd \
    -f $WORKDIR/openssh-install/etc/ssh/sshd_config \
    -D -e &
SERVER_PID=$!

sleep 1

# Replay
$SNPSFUZZER/aflnet-replay "$1" tcp 127.0.0.1 2222 1

# Clean up
kill $SERVER_PID 2>/dev/null || true
$WORKDIR/cleanup_sshd.sh

echo ""
echo "[+] Replay complete. Check /tmp/monitor.log and /tmp/monitor_violations.log for details"
VIOLREPEOF

sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" replay_violation.sh
chmod +x replay_violation.sh

# Monitor log viewer
cat > view_monitor_log.sh << 'LOGEOF'
#!/bin/bash

echo "=== Monitor Main Log ==="
if [ -f "/tmp/monitor.log" ]; then
    tail -f /tmp/monitor.log
else
    echo "No monitor log found at /tmp/monitor.log"
fi
LOGEOF

chmod +x view_monitor_log.sh

# Test LTL monitor
cat > test_monitor.sh << 'TESTMON'
#!/bin/bash

export SNPSFUZZER=SNPSFUZZER_PATH_PLACEHOLDER
export WORKDIR=$(pwd)

echo "[*] Testing LTL monitor..."

if [ ! -f "$SNPSFUZZER/formula_parser" ]; then
    echo "[!] ERROR: formula_parser not found at $SNPSFUZZER/formula_parser"
    echo "[!] Build it with: cd $SNPSFUZZER && make formula_parser"
    exit 1
fi

if [ ! -f "$WORKDIR/ltl_specs/ssh.ltl" ]; then
    echo "[!] ERROR: LTL spec not found"
    exit 1
fi

echo "[+] formula_parser found"
echo "[+] LTL spec found"
echo ""
echo "[*] Testing monitor with sample input..."

# Test with sample predicates
echo "REQ_VERSION
RESP_VERSION
REQ_KEXINIT
RESP_KEXINIT
REQ_NEWKEYS
RESP_NEWKEYS
REQ_SERVICE
RESP_SERVICE_ACCEPT
REQ_AUTH
RESP_AUTH_SUCCESS
__END_SESSION__" | $SNPSFUZZER/formula_parser $WORKDIR/ltl_specs/ssh.ltl ssh

echo ""
echo "[+] Monitor test complete!"
TESTMON

sed -i "s|SNPSFUZZER_PATH_PLACEHOLDER|$SNPSFUZZER|g" test_monitor.sh
chmod +x test_monitor.sh

echo "[+] Helper scripts created!"

# =============================================================================
# Step 8: Create README
# =============================================================================

cat > README.md << 'READEOF'
# SNPSFuzzer OpenSSH Fuzzing with LTL Monitoring

This workspace contains a complete setup for fuzzing OpenSSH with SNPSFuzzer and runtime LTL monitoring.

**Versions (AFLNet-compatible):**
- OpenSSH: vegard/openssh-portable @ 'afl' branch (AFLNet version with deterministic patches)
- OpenSSL: 1.0.2u
- Protocol: SSH 2.0
- Repository: https://github.com/vegard/openssh-portable (uses 'afl' as default branch)

## Quick Start

### 1. Test the Monitor
```bash
./test_monitor.sh
```

### 2. Start Fuzzing
```bash
./run_fuzzing.sh
```

### 3. Monitor Progress (in another terminal)
```bash
./monitor_fuzzing.sh
```

### 4. Watch for Violations (optional, in another terminal)
```bash
./monitor_violations.sh
```

## Directory Structure

```
openssh_workspace/
├── openssh-portable/          # OpenSSH source
├── openssh-install/           # Installed OpenSSH
├── fuzz_input/                # Seed inputs
├── fuzz_output/               # Fuzzing results
│   ├── queue/                 # Test cases
│   ├── crashes/               # Crashes
│   ├── hangs/                 # Hangs
│   └── violations/            # LTL violations
├── ltl_specs/                 # LTL specifications
│   └── openssh.ltl           # SSH protocol properties
└── *.sh                       # Helper scripts
```

## LTL Monitoring

The fuzzer runs with LTL runtime monitoring enabled. Violations of protocol properties are logged to:
- `/tmp/monitor.log` - Main monitor log
- `/tmp/monitor_violations.log` - Violation details
- `fuzz_output/violations/` - Violating inputs

### LTL Properties Monitored

See `ltl_specs/ssh.ltl` (linked from monitor-bin/ssh.txt) for the complete specification. Key properties:

1. Version exchange must occur before key exchange
2. Key exchange must complete before authentication
3. Authentication must receive response
4. Maximum authentication attempts enforced

## Helper Scripts

- `run_fuzzing.sh` - Start fuzzing with LTL monitoring
- `monitor_fuzzing.sh` - Real-time fuzzing statistics
- `monitor_violations.sh` - Watch for LTL violations
- `test_monitor.sh` - Test LTL monitor setup
- `replay_crash.sh <file>` - Replay a crash
- `replay_violation.sh <file>` - Replay LTL violation
- `view_monitor_log.sh` - Tail monitor logs
- `cleanup_sshd.sh` - Kill leftover SSH processes

## Analyzing Results

### View Crashes
```bash
ls -lh fuzz_output/replayable-crashes/
```

### View Violations
```bash
ls -lh fuzz_output/violations/
```

### Replay a Crash
```bash
./replay_crash.sh fuzz_output/replayable-crashes/id:000000,*
```

### Replay a Violation
```bash
./replay_violation.sh fuzz_output/violations/id:000000,*
```

## Troubleshooting

### Monitor not starting
Check that formula_parser is built:
```bash
cd $SNPSFUZZER
make formula_parser
```

### SSH server issues
Clean up processes:
```bash
./cleanup_sshd.sh
```

### View monitor logs
```bash
./view_monitor_log.sh
```

## Environment Variables

- `LTL_EVAL_PATH` - Path to formula_parser
- `LTL_SPEC_PATH` - Path to LTL specification
- `MONITOR_VERBOSE` - Enable verbose monitor logging (0/1)

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
echo "  1. Test monitor:  ./test_monitor.sh"
echo "  2. Start fuzzing: ./run_fuzzing.sh"
echo "  3. Monitor:       ./monitor_fuzzing.sh (in another terminal)"
echo ""
echo "Helper scripts:"
echo "  - monitor_violations.sh  - Watch for LTL violations"
echo "  - replay_crash.sh       - Replay crashes"
echo "  - replay_violation.sh   - Replay violations"
echo "  - view_monitor_log.sh   - View monitor logs"
echo "  - cleanup_sshd.sh       - Clean up SSH processes"
echo ""
echo "LTL Specification: ltl_specs/ssh.ltl -> monitor-bin/ssh.txt"
echo "Monitor logs: /tmp/monitor.log, /tmp/monitor_violations.log"
echo ""
echo "=========================================="