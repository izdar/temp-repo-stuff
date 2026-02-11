# test_infra/monitor_bridge.py
# ------------------------------------------------------------------
# Bridge between ResolverFuzz test artifacts and the LTL monitor.
# Collects predicates from response.txt, query.txt, tcpdump.pcap,
# cache dumps, and logs, then streams them to the monitor process.
# ------------------------------------------------------------------

import os
import sys
import json
import time
import subprocess
import threading
from pathlib import Path
from predicate_adapter import (
    parse_response_txt, parse_pcap, parse_dnstap, parse_cache,
    parse_query_hex_file, _bool, _norm_qtype, _norm_rcode
)

END = "__END_SESSION__"

# -------- CONFIGURATION --------
MONITOR_BIN = "../ltl-parser/formula_parser"
MONITOR_SPEC = "../ltl-parser/dns-infra-spec.txt"
MONITOR_CMD = [MONITOR_BIN, MONITOR_SPEC]

MONITOR_LOG_DIR = Path("../monitor_logs/")
MONITOR_LOG_DIR.mkdir(parents=True, exist_ok=True)

MONITOR_STARTUP_WAIT = 0.1

# --------------------------------

_MODE_RESULT_DIRS = {
    "cdns":          "cdns_test_res",
    "cdns_fallback": "cdns_fallback_test_res",
    "fwd_global":    "fwd_global_test_res",
    "recursive":     "recursive_test_res",
}

def results_root_for_mode(mode: str) -> Path:
    return Path(_MODE_RESULT_DIRS.get(mode, "cdns_test_res"))


def _make_log_files(tag=""):
    ts = int(time.time())
    label = f"monitor_{os.getpid()}_{tag}_{ts}" if tag else f"monitor_{os.getpid()}_{ts}"
    out = MONITOR_LOG_DIR / f"{label}.out"
    err = MONITOR_LOG_DIR / f"{label}.err"
    return out.open("a"), err.open("a")


class MonitorBridge:
    def __init__(self, tag=""):
        self._lock = threading.Lock()
        self._tag = tag
        self._start_proc()

    def _start_proc(self):
        out_fp, err_fp = _make_log_files(self._tag)
        try:
            self.p = subprocess.Popen(
                MONITOR_CMD,
                stdin=subprocess.PIPE,
                stdout=out_fp,
                stderr=err_fp,
                text=True,
                bufsize=1
            )
            time.sleep(MONITOR_STARTUP_WAIT)
        except Exception as e:
            self.p = None
            print(f"[monitor_bridge] failed to start monitor: {e}", file=sys.stderr)

    def _ensure_proc(self):
        if getattr(self, "p", None) is None:
            self._start_proc()
            return
        if self.p.poll() is not None:
            self._start_proc()

    def _is_broken(self):
        return getattr(self, "p", None) is None or (self.p.poll() is not None)

    def send_line(self, kv: dict):
        parts = []
        for k, v in kv.items():
            s = str(v)
            if ' ' in s:
                s = s.replace(' ', '_')
            parts.append(f"{k}={s}")
        line = " ".join(parts)

        with self._lock:
            try:
                self._ensure_proc()
                if self._is_broken():
                    print("[monitor_bridge] warning: monitor unavailable, skipping send_line", file=sys.stderr)
                    return
                self.p.stdin.write(line + "\n")
                self.p.stdin.flush()
            except Exception as e:
                print(f"[monitor_bridge] write failed, restarting monitor: {e}", file=sys.stderr)
                try:
                    self._start_proc()
                except Exception as e2:
                    print(f"[monitor_bridge] restart failed: {e2}", file=sys.stderr)

    def end_session(self):
        with self._lock:
            try:
                self._ensure_proc()
                if self._is_broken():
                    return
                self.p.stdin.write(END + "\n")
                self.p.stdin.flush()
            except Exception as e:
                print(f"[monitor_bridge] end_session write failed: {e}", file=sys.stderr)
                try:
                    self._start_proc()
                except Exception:
                    pass

    def terminate(self):
        with self._lock:
            try:
                if getattr(self, "p", None) is not None:
                    self.p.terminate()
                    try:
                        self.p.wait(timeout=1.0)
                    except subprocess.TimeoutExpired:
                        self.p.kill()
            except Exception:
                pass


class MonitorPool:
    def __init__(self):
        self._bridges: dict[str, MonitorBridge] = {}

    def get_bridge(self, resolver_name: str) -> MonitorBridge:
        if resolver_name not in self._bridges:
            self._bridges[resolver_name] = MonitorBridge(tag=resolver_name)
        return self._bridges[resolver_name]

    def end_all_sessions(self):
        for bridge in self._bridges.values():
            bridge.end_session()

    def terminate_all(self):
        for bridge in self._bridges.values():
            bridge.terminate()


# ---------------- predicate builder & streaming ------------------

# Thresholds for resource consumption bug detection
CNAME_CHASE_THRESHOLD = 10       # RC4: Normal is ~2-3, MaraDNS does 113, Technitium does 289
UDP_QUERY_THRESHOLD = 5          # RC6/RC7: Normal is 1-2 queries per resolution
TCP_QUERY_THRESHOLD = 5          # RC7: Knot does 100 TCP queries
MAX_UDP_RESPONSE_SIZE = 1232     # CP3/RC5: EDNS default, larger enables fragmentation attacks


def build_predicates(mode: str, unit_no: int, round_no: int, resolver_name: str, base_dir: Path):
    """
    Collect predicates from RF artifacts under base_dir/<unit>/<round>/<resolver>.
    
    Includes predicates for forward-mode bug detection from ResolverFuzz paper:
      CP1: Out-of-bailiwick cache poisoning
      CP2: In-bailiwick cache poisoning (PowerDNS NS cache lookup)
      CP3: Fragmentation-based cache poisoning (large responses)
      CP4: Iterative subdomain caching (unsolicited NS/A)
      RC1: Excessive cache search (PowerDNS)
      RC2: Unlimited cache store (invalid auth types)
      RC3: Ignoring RD flag
      RC4: Self-CNAME reference / CNAME loop
      RC5: Large responses to clients
      RC6: Overlong waiting / excessive UDP retries
      RC7: Excessive TCP queries
    """
    kv = {}
    
    # Mode and resolver identification
    try:
        kv["mode"] = {
            "cdns": "CDNS",
            "cdns_fallback": "CDNS_FALLBACK",
            "fwd_global": "FWD_GLOBAL",
            "recursive": "RECURSIVE",
        }[mode] if mode else "CDNS"
    except Exception:
        kv["mode"] = "CDNS"
    kv["resolver"] = resolver_name.upper()
    kv["unit_no"] = unit_no
    kv["round_no"] = round_no

    unit_dir = base_dir / str(unit_no) / str(round_no)
    res_dir = unit_dir / resolver_name

    # ---- query.txt (hex wire format) ----
    query_info = parse_query_hex_file(unit_dir / "query.txt")
    kv["q_id"] = query_info["qid"]
    kv["q_type"] = query_info["qtype"]
    kv["query_do"] = _bool(query_info["do"])
    kv["query_rd"] = _bool(query_info["rd"])  # RC3: RD flag tracking
    # NOTE: qname is used internally for bailiwick calculations but NOT emitted
    # because the LTL parser doesn't support string types
    query_name = query_info.get("qname", "")

    # Presence flags
    kv["query_payload_sent"] = _bool((unit_dir / "query.txt").exists())
    kv["auth_payload_deployed"] = _bool((unit_dir / "auth_payload.txt").exists())

    # DNSTap
    dnstap_dir = unit_dir / "dnstap"
    dnst = parse_dnstap(dnstap_dir) if dnstap_dir.exists() else {"query_seen": False, "response_seen": False}
    kv["dnstap_query_seen"] = _bool(dnst.get("query_seen", False))
    kv["dnstap_response_seen"] = _bool(dnst.get("response_seen", False))

    # ---- response.txt - resolver's response to client ----
    resp_file = res_dir / "response.txt"
    resp = parse_response_txt(resp_file, query_name=query_name) or {}
    kv["response_txt_present"] = _bool(bool(resp))
    kv["response_timeout_reported"] = _bool(resp.get("timeout", False))
    kv["response_tcp_used"] = _bool(resp.get("transport", "UDP") == "TCP")

    # Core DNS response fields
    kv["q_transport"] = resp.get("transport", "UDP")
    kv["resp_id"] = int(resp.get("resp_id", 0))
    kv["rcode"] = resp.get("rcode", "rcNotSet")
    kv["resp_tc"] = _bool(resp.get("tc", False))
    kv["ancount"] = int(resp.get("ancount", 0))
    kv["nscount"] = int(resp.get("nscount", 0))
    kv["arcount"] = int(resp.get("arcount", 0))
    kv["resp_ad"] = _bool(resp.get("ad", False))
    kv["resp_do"] = _bool(resp.get("do", False))
    kv["resp_aa"] = _bool(resp.get("aa", False))
    kv["resp_malformed"] = _bool(resp.get("malformed", False))
    kv["opt_present"] = _bool(resp.get("opt_present", True))
    kv["timeout"] = _bool(resp.get("timeout", False))
    kv["dnssec_present"] = _bool(resp.get("dnssec_present", False))

    # CP3/RC5: Response size tracking
    resp_size = int(resp.get("resp_size", 0))
    kv["resp_size"] = resp_size
    kv["resp_exceeds_1232"] = _bool(resp_size > MAX_UDP_RESPONSE_SIZE)
    kv["resp_exceeds_4096"] = _bool(resp_size > 4096)

    # CP1: Out-of-bailiwick detection
    kv["out_of_bailiwick_count"] = int(resp.get("out_of_bailiwick_count", 0))
    kv["out_of_bailiwick_cached"] = _bool(resp.get("out_of_bailiwick_cached", False))

    # CP4: Unsolicited NS/A record detection
    kv["unsolicited_ns_count"] = int(resp.get("unsolicited_ns_count", 0))
    kv["unsolicited_a_count"] = int(resp.get("unsolicited_a_count", 0))
    kv["has_unsolicited_ns"] = _bool(resp.get("has_unsolicited_ns", False))
    kv["has_unsolicited_glue"] = _bool(resp.get("has_unsolicited_glue", False))

    # RC2: Invalid authority section types
    kv["invalid_authority_types"] = int(resp.get("invalid_authority_types", 0))
    kv["has_invalid_authority"] = _bool(resp.get("has_invalid_authority", False))

    # RC4: CNAME loop detection
    cname_chase = int(resp.get("cname_chase_count", 0))
    kv["cname_chase_count"] = cname_chase
    kv["cname_loop_detected"] = _bool(resp.get("cname_loop_detected", False))
    kv["excessive_cname_chase"] = _bool(cname_chase > CNAME_CHASE_THRESHOLD)

    # Derived predicates
    kv["id_mismatch"] = _bool(kv["q_id"] != kv["resp_id"])
    is_nxdomain = (kv["rcode"] == "NXDOMAIN")
    has_authority = (kv["nscount"] > 0)
    kv["negative_cache_indicator"] = _bool(is_nxdomain and has_authority)
    kv["ttl_zero_exposed"] = _bool(resp.get("ttl_zero", False))
    kv["resp_min_ttl"] = int(resp.get("resp_min_ttl", -1))

    # ---- PCAP analysis ----
    pcap = res_dir / "tcpdump.pcap"
    net = parse_pcap(pcap, unit_no, resolver_name=resolver_name) if pcap.exists() else {
        "udp_q": False, "tcp_retry": False, "resp_seen": False,
        "auth_q": False, "auth_rsp": False, "auth_target_correct": False,
        "forwarder_q": False, "unexpected_external": False,
        "udp_query_count": 0, "tcp_query_count": 0, "max_udp_resp_size": 0,
    }
    
    kv["pcap_udp_query_seen"] = _bool(net["udp_q"])
    kv["pcap_tcp_retry_seen"] = _bool(net["tcp_retry"])
    kv["pcap_response_seen"] = _bool(net["resp_seen"])
    kv["auth_ns_query_seen"] = _bool(net["auth_q"])
    kv["auth_ns_response_seen"] = _bool(net["auth_rsp"])
    kv["auth_target_correct"] = _bool(net["auth_target_correct"])
    kv["forwarder_query_seen"] = _bool(net["forwarder_q"])
    kv["unexpected_external_query"] = _bool(net["unexpected_external"])

    # RC6/RC7: Query counting
    udp_q_count = int(net.get("udp_query_count", 0))
    tcp_q_count = int(net.get("tcp_query_count", 0))
    kv["udp_query_count"] = udp_q_count
    kv["tcp_query_count"] = tcp_q_count
    kv["excessive_udp_queries"] = _bool(udp_q_count > UDP_QUERY_THRESHOLD)
    kv["excessive_tcp_queries"] = _bool(tcp_q_count > TCP_QUERY_THRESHOLD)

    # RC3: Forwarding despite RD=0
    # In forward mode, if query has RD=0, resolver should NOT forward
    rd_zero_but_forwarded = (not query_info["rd"]) and net["forwarder_q"]
    kv["rd_zero_but_forwarded"] = _bool(rd_zero_but_forwarded)

    # CP3: Large upstream response accepted
    max_upstream_size = int(net.get("max_udp_resp_size", 0))
    kv["max_upstream_resp_size"] = max_upstream_size
    kv["large_upstream_accepted"] = _bool(max_upstream_size > MAX_UDP_RESPONSE_SIZE)

    # Derived PCAP predicates
    kv["tcp_fallback_seen"] = _bool(net["tcp_retry"])
    kv["auth_source_expected"] = _bool(net["auth_target_correct"])
    
    resp_port = int(resp.get("resp_port", 53))
    kv["resp_port"] = resp_port
    kv["resp_from_unexpected_port"] = _bool(resp_port != 53 and resp_port != 0)

    # ---- Log analysis ----
    kv["log_error_seen"] = _bool(
        (res_dir / "bind.log").exists() or
        (res_dir / "unbound.log").exists() or
        (res_dir / "knot.log").exists() or
        (res_dir / "powerdns.log").exists() or
        (res_dir / "log.txt").exists()
    )
    kv["log_tcp_fallback_decided"] = _bool(False)
    kv["log_formerr_or_notimp"] = _bool(False)

    # ---- Cache analysis ----
    cache = parse_cache(res_dir, query_name=query_name)
    kv["cache_written"] = _bool(cache["written"])
    kv["cache_neg_present"] = _bool(cache["neg_present"])
    kv["cache_min_ttl"] = cache["min_ttl"]
    kv["any_cache_rrset_present"] = _bool(cache["any_rrset"])
    
    # CP1/CP4: Cache poisoning indicators
    kv["cached_ns_count"] = int(cache.get("cached_ns_count", 0))
    kv["cached_a_count"] = int(cache.get("cached_a_count", 0))
    kv["out_of_bailiwick_in_cache"] = _bool(cache.get("out_of_bailiwick_in_cache", False))
    kv["invalid_types_in_cache"] = _bool(cache.get("invalid_types_in_cache", False))

    # Aggregate predicates
    kv["any_ans_rrset_present"] = _bool(kv["ancount"] > 0)

    # Safe defaults for complex predicates
    kv["server_behaved_nonidempotent"] = _bool(False)
    kv["opt_padding_unusual"] = _bool(False)
    kv["prev_negative_ttl"] = -1
    kv["time_since_prev"] = 0
    kv["rr_kind"] = "RR_OTHER"

    return kv


def stream_round_to_monitor(mode: str, unit_no: int, round_no: int, base_dir: Path, pool: "MonitorPool"):
    """
    Iterate resolver subfolders and send one event per resolver to the monitor.
    """
    for resolver_name in ("bind9", "unbound", "knot", "powerdns", "technitium", "maradns"):
        res_dir = base_dir / str(unit_no) / str(round_no) / resolver_name
        if not res_dir.exists():
            continue
        kv = build_predicates(mode, unit_no, round_no, resolver_name, base_dir)
        pool.get_bridge(resolver_name).send_line(kv)
