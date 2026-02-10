#!/usr/bin/env python3

import os
import sys

def count_sloc(path):
    sloc = 0
    in_block_comment = False

    with open(path, 'r', errors='ignore') as f:
        for line in f:
            line = line.strip()

            if not line:
                continue

            # Handle block comments
            if in_block_comment:
                if '*/' in line:
                    in_block_comment = False
                continue

            if line.startswith('/*'):
                if '*/' not in line:
                    in_block_comment = True
                continue

            if line.startswith('//'):
                continue

            sloc += 1

    return sloc


def main(root):
    c_files = {}
    h_files = {}

    for dirpath, _, filenames in os.walk(root):
        for name in filenames:
            base, ext = os.path.splitext(name)
            full = os.path.join(dirpath, name)

            if ext == ".c":
                c_files[base] = full
            elif ext == ".h":
                h_files[base] = full

    total = 0

    print(f"{'Module':30} {'C':>6} {'H':>6} {'Total':>6}")
    print("-" * 52)

    for base in sorted(set(c_files) | set(h_files)):
        c_sloc = count_sloc(c_files[base]) if base in c_files else 0
        h_sloc = count_sloc(h_files[base]) if base in h_files else 0
        pair_total = c_sloc + h_sloc
        total += pair_total

        print(f"{base:30} {c_sloc:6} {h_sloc:6} {pair_total:6}")

    print("-" * 52)
    print(f"{'GRAND TOTAL':30} {'':6} {'':6} {total:6}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <directory>")
        sys.exit(1)

    main(sys.argv[1])

