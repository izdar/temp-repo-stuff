#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <crash_file>"
    exit 1
fi

export WORKDIR=$(pwd)
export SNPSFUZZER=/home/pirwani/SNPSFuzzer

$SNPSFUZZER/afl-replay "$1" udp 127.0.0.1 20220
