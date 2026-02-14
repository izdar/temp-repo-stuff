#!/usr/bin/env bash
# =============================================================================
# run_campaigns.sh — Run ResolverFuzz fwd_global mode (all 6 resolvers) with monitor
# =============================================================================
#
# Usage:
#   sudo -i
#   conda activate resolverfuzz
#   bash run_campaigns.sh [--units N] [--payloads N] [--dry-run] [--no-monitor-check]
#
# Or directly (without activating conda first):
#   sudo bash run_campaigns.sh --python /path/to/resolverfuzz/bin/python
#
# =============================================================================

set -euo pipefail

# ========================= CONFIGURATION ====================================

# Root of the ResolverFuzz repo
RF_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Where the test scripts live
INFRA_DIR="${RF_ROOT}/test_infra"

# Monitor paths
MONITOR_BIN="${RF_ROOT}/ltl-parser/formula_parser"
MONITOR_SPEC="${RF_ROOT}/ltl-parser/dns-infra-spec.txt"
MONITOR_LOG_DIR="${RF_ROOT}/monitor_logs"
MONITOR_VIOLATION_DIR="${RF_ROOT}/monitor_violations"

# Campaign defaults (override with flags)
UNIT_SIZE=5
PAYLOAD_NUM=500

# Python interpreter (auto-detected from conda, or pass --python)
PYTHON_BIN=""

# Campaign timestamp for this run
RUN_TS=$(date +%Y%m%d_%H%M%S)

# Log directory for this run
RUN_LOG_DIR="${RF_ROOT}/campaign_logs/${RUN_TS}"

# ========================= CLI PARSING ======================================

DRY_RUN=0
SKIP_MONITOR_CHECK=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --units)        UNIT_SIZE="$2";       shift 2 ;;
        --payloads)     PAYLOAD_NUM="$2";     shift 2 ;;
        --python)       PYTHON_BIN="$2";      shift 2 ;;
        --dry-run)      DRY_RUN=1;            shift   ;;
        --no-monitor-check) SKIP_MONITOR_CHECK=1; shift ;;
        -h|--help)
            echo "Usage: sudo bash run_campaigns.sh [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --units N            Number of units per mode (default: 5, max: 50)"
            echo "  --payloads N         Payloads per unit (default: 500, recommended < 1000)"
            echo "  --python PATH        Path to Python interpreter (auto-detected from conda)"
            echo "  --dry-run            Print commands without executing"
            echo "  --no-monitor-check   Skip monitor binary pre-flight check"
            echo "  -h, --help           Show this help"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ========================= HELPERS ==========================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

log()  { echo -e "${CYAN}[$(date +%H:%M:%S)]${NC} $*"; }
ok()   { echo -e "${GREEN}[✓]${NC} $*"; }
warn() { echo -e "${YELLOW}[!]${NC} $*"; }
err()  { echo -e "${RED}[✗]${NC} $*"; }
die()  { err "$*"; exit 1; }

# ========================= PRE-FLIGHT CHECKS ================================

log "${BOLD}ResolverFuzz Parallel Campaign Runner${NC}"
log "Run ID: ${RUN_TS}"
echo ""

# --- Python interpreter ---
if [[ -z "$PYTHON_BIN" ]]; then
    # Try conda env first
    if command -v conda &>/dev/null; then
        PYTHON_BIN=$(conda run -n resolverfuzz which python 2>/dev/null || true)
    fi
    # Fall back to active python
    if [[ -z "$PYTHON_BIN" || ! -x "$PYTHON_BIN" ]]; then
        PYTHON_BIN=$(which python3 2>/dev/null || which python 2>/dev/null || true)
    fi
fi
[[ -x "$PYTHON_BIN" ]] || die "Python interpreter not found. Use --python /path/to/python"
ok "Python: ${PYTHON_BIN}"

# --- Repo structure ---
[[ -d "$INFRA_DIR" ]] || die "test_infra not found at ${INFRA_DIR}"
for script in main_cdns.py main_cdns_fallback.py main_fwd_global.py main_recursive.py; do
    [[ -f "${INFRA_DIR}/${script}" ]] || die "Missing ${script} in ${INFRA_DIR}"
done
ok "Test scripts present"

# --- Monitor bridge + adapter ---
[[ -f "${INFRA_DIR}/monitor_bridge.py" ]]    || die "monitor_bridge.py missing"
[[ -f "${INFRA_DIR}/predicate_adapter.py" ]] || die "predicate_adapter.py missing"
ok "Monitor bridge + adapter present"

# --- Monitor binary ---
if [[ "$SKIP_MONITOR_CHECK" -eq 0 ]]; then
    [[ -x "$MONITOR_BIN" ]] || die "Monitor binary not found or not executable: ${MONITOR_BIN}\n    Build it first: cd ${RF_ROOT}/ltl-parser && make"
    [[ -f "$MONITOR_SPEC" ]] || die "Monitor spec not found: ${MONITOR_SPEC}"

    # Quick smoke test: feed empty input, should exit 0
    echo "" | "$MONITOR_BIN" "$MONITOR_SPEC" 2>/dev/null
    [[ $? -eq 0 ]] || die "Monitor binary smoke test failed"
    ok "Monitor binary: ${MONITOR_BIN}"
else
    warn "Skipping monitor binary check (--no-monitor-check)"
fi

# --- Docker ---
docker info &>/dev/null || die "Docker not available. Are you running as root / with sudo?"
ok "Docker available"

# --- Docker network ---
docker network inspect test_net_batch &>/dev/null || die "Docker network 'test_net_batch' not found.\n    Create it: docker network create --subnet 172.22.0.0/16 test_net_batch"
ok "Docker network test_net_batch exists"

# --- Docker images ---
REQUIRED_IMAGES=(
    "bind9:9.18.0" "unbound:1.16.0" "knot:5.5.0"
    "powerdns:4.7.0" "maradns:3.5.0022" "technitium:10.0.1"
    "dnstap-listener" "attacker" "auth-srv"
)
MISSING_IMAGES=()
for img in "${REQUIRED_IMAGES[@]}"; do
    if ! docker image inspect "$img" &>/dev/null; then
        MISSING_IMAGES+=("$img")
    fi
done
if [[ ${#MISSING_IMAGES[@]} -gt 0 ]]; then
    die "Missing Docker images: ${MISSING_IMAGES[*]}\n    Pull and tag them per the README."
fi
ok "All ${#REQUIRED_IMAGES[@]} Docker images present"

# --- Parameters ---
log ""
log "${BOLD}Campaign parameters:${NC}"
log "  Units per mode:    ${UNIT_SIZE}"
log "  Payloads per unit: ${PAYLOAD_NUM}"
log "  Modes:             fwd_global (forward-only, all 6 resolvers)"
log "  Results:           ${INFRA_DIR}/<mode>_test_res/"
log "  Monitor logs:      ${MONITOR_LOG_DIR}/"
log "  Violation traces:  ${MONITOR_VIOLATION_DIR}/"
log "  Campaign logs:     ${RUN_LOG_DIR}/"
echo ""

# ========================= SETUP DIRECTORIES ================================

mkdir -p "$MONITOR_LOG_DIR"
mkdir -p "$MONITOR_VIOLATION_DIR"
mkdir -p "$RUN_LOG_DIR"

# Export for the C++ monitor (it reads this env var)
export MONITOR_VIOLATION_DIR

# ========================= DRY RUN =========================================

if [[ "$DRY_RUN" -eq 1 ]]; then
    warn "DRY RUN — commands that would be executed:"
    echo ""
    for mode in fwd_global; do
        echo "  ${PYTHON_BIN} ${INFRA_DIR}/main_${mode}.py \\"
        echo "      --unit_size ${UNIT_SIZE} --payload_num ${PAYLOAD_NUM} \\"
        echo "      > ${RUN_LOG_DIR}/${mode}.log 2>&1 &"
        echo ""
    done
    exit 0
fi

# ========================= LAUNCH CAMPAIGNS =================================

declare -A EXIT_CODES
MODES=(fwd_global)

# Docker containers that are shared across modes and cause conflicts
SHARED_CONTAINERS="dnstap-concurrent"

cleanup_docker() {
    # Remove shared containers left over from the previous mode
    for cname in $SHARED_CONTAINERS; do
        docker rm -f "$cname" 2>/dev/null || true
    done
    # Also remove any stale RF containers from the previous mode
    docker ps -a --filter "network=test_net_batch" -q | xargs -r docker rm -f 2>/dev/null || true
    sleep 2
}

cleanup_signal() {
    log ""
    warn "Caught signal — stopping current campaign..."
    # Kill any running python child
    jobs -p | xargs -r kill -TERM 2>/dev/null || true
    sleep 3
    jobs -p | xargs -r kill -9 2>/dev/null || true
    cleanup_docker
    log "Cleanup done. Check partial results in ${INFRA_DIR}/"
    exit 130
}

trap cleanup_signal SIGINT SIGTERM

cd "$INFRA_DIR"

log "${BOLD}Running fwd_global campaign (forward-only mode, all 6 resolvers)...${NC}"
echo ""

FAILED=0

for mode in "${MODES[@]}"; do
    LOG_FILE="${RUN_LOG_DIR}/${mode}.log"

    # Clean up Docker state from any previous mode
    log "Cleaning Docker state..."
    cleanup_docker

    log "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    log "Starting ${BOLD}${mode}${NC}  (log: ${LOG_FILE})"
    log "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    START_TIME=$(date +%s)

    set +e
    "$PYTHON_BIN" "main_${mode}.py" \
        --unit_size "$UNIT_SIZE" \
        --payload_num "$PAYLOAD_NUM" \
        > "$LOG_FILE" 2>&1
    rc=$?
    set -e

    ELAPSED=$(( $(date +%s) - START_TIME ))
    EXIT_CODES[$mode]=$rc

    if [[ $rc -eq 0 ]]; then
        ok "${mode} completed successfully (${ELAPSED}s)"
    else
        err "${mode} exited with code ${rc} (${ELAPSED}s) — see ${LOG_FILE}"
        FAILED=$((FAILED + 1))
    fi
    echo ""
done

# ========================= SUMMARY ==========================================

echo ""
log "═══════════════════════════════════════════════════════════"
log "${BOLD}CAMPAIGN SUMMARY${NC}  —  Run ${RUN_TS}"
log "═══════════════════════════════════════════════════════════"
echo ""

# Exit codes
for mode in "${MODES[@]}"; do
    rc=${EXIT_CODES[$mode]}
    if [[ $rc -eq 0 ]]; then
        echo -e "  ${GREEN}✓${NC} ${mode}  (exit ${rc})"
    else
        echo -e "  ${RED}✗${NC} ${mode}  (exit ${rc})  — see ${RUN_LOG_DIR}/${mode}.log"
    fi
done
echo ""

# Result sizes
log "Result directories:"
for dir in fwd_test_res; do
    if [[ -d "${INFRA_DIR}/${dir}" ]]; then
        SIZE=$(du -sh "${INFRA_DIR}/${dir}" 2>/dev/null | cut -f1)
        COUNT=$(find "${INFRA_DIR}/${dir}" -name "response.txt" 2>/dev/null | wc -l)
        log "  ${dir}: ${SIZE}  (${COUNT} response files)"
    else
        log "  ${dir}: (not created)"
    fi
done
echo ""

# Monitor violations
VIOLATION_COUNT=$(find "$MONITOR_VIOLATION_DIR" -name "*.trace" 2>/dev/null | wc -l)
if [[ $VIOLATION_COUNT -gt 0 ]]; then
    warn "${BOLD}${VIOLATION_COUNT} property violations detected!${NC}"
    log "  Traces in: ${MONITOR_VIOLATION_DIR}/"
    echo ""

    # Breakdown by property
    log "  Violations by property:"
    find "$MONITOR_VIOLATION_DIR" -name "*.trace" -newer "${RUN_LOG_DIR}" \
        | xargs grep -h "^# failed_properties:" 2>/dev/null \
        | sed 's/# failed_properties://' \
        | tr ' ' '\n' \
        | grep -v '^$' \
        | sort \
        | uniq -c \
        | sort -rn \
        | while read count prop; do
            log "    property ${prop}: ${count} violation(s)"
        done

    echo ""

    # Breakdown by resolver
    log "  Violations by resolver:"
    find "$MONITOR_VIOLATION_DIR" -name "*.trace" -newer "${RUN_LOG_DIR}" \
        | xargs grep -h "^# resolver:" 2>/dev/null \
        | sed 's/# resolver:\s*//' \
        | sort \
        | uniq -c \
        | sort -rn \
        | while read count res; do
            log "    ${res}: ${count} violation(s)"
        done
else
    ok "No property violations detected in this run."
fi
echo ""

# Monitor errors
ERR_COUNT=$(find "$MONITOR_LOG_DIR" -name "*.err" -newer "${RUN_LOG_DIR}" -size +0c 2>/dev/null | wc -l)
if [[ $ERR_COUNT -gt 0 ]]; then
    log "Monitor .err files with content: ${ERR_COUNT}"
    log "  Inspect: ls -lt ${MONITOR_LOG_DIR}/*.err"
fi
echo ""

log "Campaign logs: ${RUN_LOG_DIR}/"
log "═══════════════════════════════════════════════════════════"

if [[ $FAILED -gt 0 ]]; then
    die "${FAILED} campaign(s) failed. Check logs above."
fi

ok "${BOLD}All campaigns completed successfully.${NC}"
