#!/bin/bash
# SNPSFuzzer Live555 RTSP Fuzzing Setup Script
set -e
echo "[*] Starting SNPSFuzzer Live555 setup..."
export WORKDIR=$(pwd)/live555_workspace
mkdir -p $WORKDIR && cd $WORKDIR
echo "[+] Workspace: $WORKDIR"

# Build Live555
echo "[*] Building Live555..."
if [ ! -d "live555" ]; then
    git clone https://github.com/rgaufman/live555.git
fi
cd live555 && git checkout ceeb4f4
if [ -f "$SNPSFUZZER/tutorials/live555/ceeb4f4_states_decomposed.patch" ]; then
    patch -p1 < $SNPSFUZZER/tutorials/live555/ceeb4f4_states_decomposed.patch
fi
export CC=$SNPSFUZZER/afl-clang-fast CXX=$SNPSFUZZER/afl-clang-fast++
export CFLAGS="-DXMALLOC=malloc -DXFREE=free" CXXFLAGS="$CFLAGS"
./genMakefiles linux && make clean && make all
cd testProgs && cp $SNPSFUZZER/tutorials/live555/sample_media_sources/*.* ./ 2>/dev/null || echo "No media files"
cd $WORKDIR

# Test
echo "[*] Testing Live555..."
./live555/testProgs/testOnDemandRTSPServer 8554 &
SERVER_PID=$! && sleep 3
(echo -e "OPTIONS rtsp://127.0.0.1:8554/test.264 RTSP/1.0\r\nCSeq: 1\r\n\r\n"; sleep 1) | nc -w 2 127.0.0.1 8554
kill $SERVER_PID 2>/dev/null && sleep 1

# Setup fuzzing
mkdir -p fuzz_input fuzz_output
if [ -d "$SNPSFUZZER/tutorials/live555/in-rtsp" ]; then
    cp -r $SNPSFUZZER/tutorials/live555/in-rtsp/* fuzz_input/
else
    for method in OPTIONS DESCRIBE SETUP PLAY TEARDOWN; do
        echo -e "$method rtsp://127.0.0.1:8554/test.264 RTSP/1.0\r\nCSeq: 1\r\n\r\n" > fuzz_input/rtsp_${method,,}.txt
    done
fi

# Create run script
cat > run_fuzzing.sh << 'FUZZ'
#!/bin/bash
export SNPSFUZZER=SNPSFUZZER_PH WORKDIR=$(pwd)
MONITOR_BIN="$SNPSFUZZER/monitor-bin"
USE_MONITOR=0
if [ -f "$MONITOR_BIN/rtsp.ltl" ] && [ -x "$MONITOR_BIN/main_continuous" ]; then
    USE_MONITOR=1 && export LTL_MONITOR_SPEC="$MONITOR_BIN/rtsp.ltl" LTL_MONITOR_BIN="$MONITOR_BIN/main_continuous"
fi
CMD="$SNPSFUZZER/afl-fuzz -d -i fuzz_input -o fuzz_output -N tcp://127.0.0.1/8554 -P RTSP -D 10000 -q 3 -s 3 -E -K -t 1000+ -m none"
[ $USE_MONITOR -eq 1 ] && CMD="$CMD -L $LTL_MONITOR_BIN -M $LTL_MONITOR_SPEC"
$CMD -- $WORKDIR/live555/testProgs/testOnDemandRTSPServer 8554
FUZZ
sed -i "s|SNPSFUZZER_PH|$SNPSFUZZER|g" run_fuzzing.sh && chmod +x run_fuzzing.sh

# Monitor script
cat > monitor_fuzzing.sh << 'MON'
#!/bin/bash
while true; do
    clear && $SNPSFUZZER/afl-whatsup fuzz_output 2>/dev/null
    echo -e "\n=== Violations ===" && grep -c "VIOLATION" /tmp/monitor_violations.log 2>/dev/null || echo "0"
    sleep 5
done
MON
sed -i "s|SNPSFUZZER|$SNPSFUZZER|g" monitor_fuzzing.sh && chmod +x monitor_fuzzing.sh

echo -e "\n[+] Setup complete!\nStart: ./run_fuzzing.sh\nMonitor: ./monitor_fuzzing.sh"
