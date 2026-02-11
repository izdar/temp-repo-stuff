#!/bin/bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------
export SNPSFUZZER="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export AFL_PATH=$SNPSFUZZER

# Your existing LightFTP build directory
export LIGHTFTP_DIR=$SNPSFUZZER/LightFTP/Source/Release
export WORKDIR=$SNPSFUZZER/LightFTP_WORKDIR

# Target server + config
TARGET="$LIGHTFTP_DIR/fftp"
CONF="$LIGHTFTP_DIR/fftp.conf"
PORT=2200

# -----------------------------------------------------------------------------
# LTL Monitor Configuration (absolute paths)
# -----------------------------------------------------------------------------
export LTL_EVAL_PATH=$SNPSFUZZER/formula_parser
export LTL_SPEC_PATH=$SNPSFUZZER/monitor-bin/ftp.txt
export MONITOR_VERBOSE=1

# -----------------------------------------------------------------------------
# AFL/SNPSFuzzer configuration
# -----------------------------------------------------------------------------
export AFL_SKIP_CPUFREQ=1
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
export AFL_NO_AFFINITY=1

# -----------------------------------------------------------------------------
# Corpus / output / dict / cleanup script
# -----------------------------------------------------------------------------
INPUT_DIR="$WORKDIR/in-ftp"
OUTPUT_DIR="$WORKDIR/out-lightftp"

# If you have AFLNet tutorial assets somewhere, point these to them.
# Example (AFLNet style):
#   SEEDS_DIR="$AFLNET/tutorials/lightftp/in-ftp"
#   DICT="$AFLNET/tutorials/lightftp/ftp.dict"
#   CLEAN="$AFLNET/tutorials/lightftp/ftpclean.sh"
#
# If you copied them locally, point to your local copies instead.
DICT="$WORKDIR/ftp.dict"
CLEAN="$LIGHTFTP_DIR/ftpclean.sh"

POLLING_TIMEOUT=10000   # ms (AFLNet's -D 10000 is also used; keep both consistent)

# -----------------------------------------------------------------------------
# Prep
# -----------------------------------------------------------------------------
mkdir -p "$INPUT_DIR" "$OUTPUT_DIR"

if [[ ! -x "$TARGET" ]]; then
  echo "[-] Target not found or not executable: $TARGET"
  exit 1
fi

if [[ ! -f "$CONF" ]]; then
  echo "[-] Missing config: $CONF"
  exit 1
fi

if [[ ! -f "$DICT" ]]; then
  echo "[-] Missing dict: $DICT"
  echo "    Put ftp.dict at: $DICT (or change DICT to wherever it lives)"
  exit 1
fi

if [[ ! -x "$CLEAN" ]]; then
  echo "[-] Missing cleanup script (ftpclean.sh) or not executable: $CLEAN"
  echo "    If you have it, run: chmod +x $CLEAN"
  exit 1
fi

echo "[*] Starting SNPSFuzzer for LightFTP"
echo "    Target:   $TARGET"
echo "    Conf:     $CONF"
echo "    Port:     $PORT"
echo "    Input:    $INPUT_DIR"
echo "    Output:   $OUTPUT_DIR"
echo "    Dict:     $DICT"
echo "    Clean:    $CLEAN"
echo "    Monitor:  $LTL_EVAL_PATH  Spec: $LTL_SPEC_PATH"

# -----------------------------------------------------------------------------
# Run
# -----------------------------------------------------------------------------
sudo -E "$SNPSFUZZER/afl-fuzz" \
  -d \
  -i "$INPUT_DIR" \
  -o "$OUTPUT_DIR" \
  -N "tcp://127.0.0.1/$PORT" \
  -x "$DICT" \
  -P FTP \
  -D 10000 \
  -q 3 \
  -s 3 \
  -E \
  -K \
  -R \
  -W "$POLLING_TIMEOUT" \
  -a 1 \
  -b 100 \
  -m none \
  -c "$CLEAN" \
  -- "$TARGET" "$CONF" "$PORT"
