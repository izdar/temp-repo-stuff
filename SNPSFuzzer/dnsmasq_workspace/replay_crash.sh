#!/bin/bash

export SNPSFUZZER=/home/pirwani/SNPSFuzzer/

if [ -z "$1" ]; then
    echo "Usage: $0 <crash_file>"
    exit 1
fi

echo "[*] Replaying: $1"
$SNPSFUZZER/afl-replay "$1" tcp 127.0.0.1 5353
