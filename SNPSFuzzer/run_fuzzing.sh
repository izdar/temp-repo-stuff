#!/bin/bash
export WORKDIR=$(pwd)
export SNPSFUZZER=/home/pirwani/SNPSFuzzer
export AFL_PATH=$SNPSFUZZER

mkdir -p runtime_dir

$SNPSFUZZER/afl-fuzz \
    -d \
    -i fuzz_input \
    -o fuzz_output \
    -N udp://127.0.0.1/5060 \
    -l 5061 \
    -P SIP \
    -D 10000 \
    -W 500 \
    -w 100 \
    -t 2000 \
    -q 3 \
    -s 3 \
    -E \
    -K \
    -R \
    -m 2048 \
    -- kamailio/src/kamailio -f kamailio.cfg -L kamailio/src/modules -Y runtime_dir/ -n 1 -D -E
