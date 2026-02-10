# test_infra/predicate_adapter.py
# ------------------------------------------------------------------
# Extracts monitor predicates from ResolverFuzz test artifacts.
#
# Key file formats handled:
#   response.txt  – Python bytes-literal hex of DNS wire  b'7eb8ac81...'
#   query.txt     – plain hex of DNS wire                 7eb82e80...
#   tcpdump.pcap  – libpcap binary (parsed without tshark)
#   *.cache.db / cache.json – resolver cache dumps
#
# IP Scheme for ResolverFuzz:
#   172.22.1.X   = BIND9 resolver
#   172.22.2.X   = Unbound resolver
#   172.22.3.X   = PowerDNS resolver
#   172.22.4.X   = Knot resolver
#   172.22.5.X   = MaraDNS resolver
#   172.22.6.X   = Technitium resolver
#   172.22.101.X = Attacker/Fuzzer client
#   172.22.201.X = Forwarder (auth_srv in FWD_GLOBAL mode)
# ------------------------------------------------------------------

import os, re, json, struct, hashlib, binascii
from pathlib import Path

# ---- helpers -----------------------------------------------------

def _hash_rr_key(name: str, qtype: str) -> str:
    return hashlib.sha1(f"{(name or '').lower()}|{qtype}".encode()).hexdigest()[:8]

def _bool(v): return "true" if bool(v) else "false"

_QTYPE_MAP = {
    1: "A", 28: "AAAA", 5: "CNAME", 2: "NS", 15: "MX", 16: "TXT",
    41: "OPT", 255: "ANY", 46: "RRSIG", 48: "DNSKEY", 43: "DS",
    47: "NSEC", 50: "NSEC3",
}

_DNSSEC_TYPES = {46, 48, 43, 47, 50, 51}  # RRSIG, DNSKEY, DS, NSEC, NSEC3, NSEC3PARAM

def _norm_qtype(q):
    if isinstance(q, int):
        if q in _DNSSEC_TYPES:
            return "DNSSEC"
        return _QTYPE_MAP.get(q, "OTHER")
    s = str(q).upper().strip()
    if s.isdigit():
        i = int(s)
        if i in _DNSSEC_TYPES:
            return "DNSSEC"
        return _QTYPE_MAP.get(i, "OTHER")
    return s if s in {"A","AAAA","ANY","OPT","CNAME","NS","MX","TXT","DNSSEC","OTHER"} else "OTHER"

_RCODE_MAP = {0:"NOERROR",1:"FORMERR",2:"SERVFAIL",3:"NXDOMAIN",4:"NOTIMP",5:"REFUSED",
              6:"YXDOMAIN",7:"YXRRSET",8:"NXRRSET",9:"NOTAUTH"}
def _norm_rcode(n):
    s = str(n).strip()
    if s.isdigit():
        return _RCODE_MAP.get(int(s), "rcNotSet")
    s = s.upper()
    return s if s in set(_RCODE_MAP.values()) | {"rcNotSet"} else "rcNotSet"


# ==================================================================
#  DNS wire-format parser (for response.txt and query.txt)
# ==================================================================

def _read_dns_name(raw, offset, max_depth=10):
    """Read a DNS name from wire format, handling compression. Returns (name, next_offset)."""
    if max_depth <= 0 or offset is None or offset >= len(raw):
        return "", None
    
    labels = []
    seen_offsets = set()
    original_offset = offset
    jumped = False
    
    while offset < len(raw):
        if offset in seen_offsets:
            return ".".join(labels), None  # loop
        seen_offsets.add(offset)
        
        label_len = raw[offset]
        if label_len == 0:
            if not jumped:
                offset += 1
            break
        elif (label_len & 0xC0) == 0xC0:  # compression pointer
            if offset + 1 >= len(raw):
                return ".".join(labels), None
            ptr = ((label_len & 0x3F) << 8) | raw[offset + 1]
            if not jumped:
                original_offset = offset + 2
                jumped = True
            offset = ptr
        else:
            offset += 1
            if offset + label_len > len(raw):
                return ".".join(labels), None
            try:
                labels.append(raw[offset:offset + label_len].decode('ascii', errors='replace'))
            except:
                labels.append("?")
            offset += label_len
    
    name = ".".join(labels) + "." if labels else "."
    return name, original_offset if jumped else offset


def _skip_dns_name(raw, offset):
    """Advance past a DNS name (handles compression pointers). Returns offset after name."""
    if offset is None or offset >= len(raw):
        return None
    for _ in range(128):
        if offset >= len(raw):
            return None
        label_len = raw[offset]
        if label_len == 0:
            return offset + 1
        if (label_len & 0xC0) == 0xC0:
            return offset + 2
        offset += 1 + label_len
    return None


def _parse_rr(raw, offset):
    """Parse one RR starting at *offset*. Returns dict or None."""
    name, name_end = _read_dns_name(raw, offset)
    if name_end is None or name_end + 10 > len(raw):
        return None
    rtype   = (raw[name_end]     << 8) | raw[name_end + 1]
    rclass  = (raw[name_end + 2] << 8) | raw[name_end + 3]
    ttl_raw = (raw[name_end + 4] << 24) | (raw[name_end + 5] << 16) | \
              (raw[name_end + 6] << 8)  |  raw[name_end + 7]
    rdlen   = (raw[name_end + 8] << 8) | raw[name_end + 9]
    nxt     = name_end + 10 + rdlen
    if nxt > len(raw):
        return None
    
    rdata = raw[name_end + 10:nxt]
    return {
        "name": name, "type": rtype, "class": rclass, "ttl": ttl_raw,
        "rdlength": rdlen, "rdata": rdata, "next_offset": nxt
    }


def _get_parent_zone(name):
    """Get parent zone from a DNS name. foo.bar.com. -> bar.com."""
    if not name or name == ".":
        return "."
    parts = name.rstrip(".").split(".")
    if len(parts) <= 1:
        return "."
    return ".".join(parts[1:]) + "."


def _is_in_bailiwick(rr_name, query_name):
    """Check if rr_name is in bailiwick of query_name's zone."""
    if not rr_name or not query_name:
        return True  # can't determine, assume ok
    rr = rr_name.lower().rstrip(".")
    q = query_name.lower().rstrip(".")
    # RR should be same domain or subdomain of query zone
    # e.g., query for foo.example.com -> example.com is bailiwick
    # anything under example.com is in-bailiwick
    q_parts = q.split(".")
    if len(q_parts) < 2:
        return True  # TLD query, everything is in-bailiwick
    # Zone is the last 2+ labels
    zone = ".".join(q_parts[-2:]) if len(q_parts) >= 2 else q
    return rr.endswith(zone) or rr == zone


def _parse_dns_wire(raw, query_name=""):
    """Parse raw DNS message bytes into a predicate dict."""
    d = {
        "present": True, "timeout": False, "malformed": False,
        "resp_size": len(raw),  # CP3/RC5: track response size
    }
    if len(raw) < 12:
        d["malformed"] = True
        return d

    # --- header ---
    d["resp_id"] = (raw[0] << 8) | raw[1]
    flags = (raw[2] << 8) | raw[3]

    d["qr"]    = bool((flags >> 15) & 1)
    d["aa"]    = bool((flags >> 10) & 1)
    d["tc"]    = bool((flags >> 9) & 1)
    d["rd"]    = bool((flags >> 8) & 1)
    d["ra"]    = bool((flags >> 7) & 1)
    d["ad"]    = bool((flags >> 5) & 1)
    d["cd"]    = bool((flags >> 4) & 1)
    d["do"]    = False
    d["rcode"] = _norm_rcode(flags & 0xF)

    qdcount = (raw[4]  << 8) | raw[5]
    d["ancount"] = (raw[6]  << 8) | raw[7]
    d["nscount"] = (raw[8]  << 8) | raw[9]
    d["arcount"] = (raw[10] << 8) | raw[11]

    d["transport"] = "UDP"

    # --- parse question section ---
    offset = 12
    d["qname"] = ""
    d["qtype"] = 0
    for _ in range(qdcount):
        qname, offset = _read_dns_name(raw, offset)
        if offset is None or offset + 4 > len(raw):
            offset = None
            break
        qtype = (raw[offset] << 8) | raw[offset + 1]
        d["qname"] = qname
        d["qtype"] = qtype
        offset += 4

    if not query_name and d["qname"]:
        query_name = d["qname"]

    # --- parse resource records ---
    ttl_vals = []
    opt_present = False
    dnssec_present = False
    total_rrs = d["ancount"] + d["nscount"] + d["arcount"]
    
    # CP1/CP4: Track out-of-bailiwick and unsolicited records
    out_of_bailiwick_records = []
    unsolicited_ns_records = []
    unsolicited_a_records = []
    authority_records = []
    additional_records = []
    
    # RC4: CNAME tracking
    cname_targets = {}
    cname_loop_detected = False
    cname_chase_count = 0
    
    rr_index = 0
    for _ in range(total_rrs):
        if offset is None or offset >= len(raw):
            break
        rr = _parse_rr(raw, offset)
        if rr is None:
            break
        offset = rr["next_offset"]
        rr_index += 1

        # Determine section
        in_answer = rr_index <= d["ancount"]
        in_authority = d["ancount"] < rr_index <= d["ancount"] + d["nscount"]
        in_additional = rr_index > d["ancount"] + d["nscount"]

        if rr["type"] == 41:  # OPT (EDNS)
            opt_present = True
            opt_flags = rr["ttl"] & 0xFFFF
            if (opt_flags >> 15) & 1:
                d["do"] = True
        elif rr["type"] in _DNSSEC_TYPES:
            dnssec_present = True

        # Collect TTLs for non-OPT records
        if rr["type"] != 41:
            ttl_vals.append(rr["ttl"])

        # CP1: Check bailiwick for authority/additional section records
        if (in_authority or in_additional) and rr["type"] != 41:
            if not _is_in_bailiwick(rr["name"], query_name):
                out_of_bailiwick_records.append({
                    "name": rr["name"], "type": rr["type"], "section": "authority" if in_authority else "additional"
                })
        
        # CP4: Track NS and A records in authority/additional (unsolicited)
        if in_authority:
            authority_records.append(rr)
            if rr["type"] == 2:  # NS
                unsolicited_ns_records.append(rr["name"])
        if in_additional:
            additional_records.append(rr)
            if rr["type"] in (1, 28):  # A or AAAA
                unsolicited_a_records.append(rr["name"])

        # RC4: CNAME loop detection
        if rr["type"] == 5:  # CNAME
            cname_chase_count += 1
            # Parse CNAME target from rdata
            if rr["rdata"]:
                target, _ = _read_dns_name(raw, offset - rr["rdlength"])
                if target:
                    # Self-referential CNAME?
                    if target.lower() == rr["name"].lower():
                        cname_loop_detected = True
                    # Circular reference?
                    if rr["name"].lower() in cname_targets:
                        cname_loop_detected = True
                    cname_targets[rr["name"].lower()] = target.lower()

    d["opt_present"] = opt_present
    d["dnssec_present"] = dnssec_present
    d["cname_loop_detected"] = cname_loop_detected
    d["cname_chase_count"] = cname_chase_count
    
    # CP1: Out-of-bailiwick detection
    d["out_of_bailiwick_count"] = len(out_of_bailiwick_records)
    d["out_of_bailiwick_cached"] = len(out_of_bailiwick_records) > 0
    
    # CP4: Unsolicited records
    d["unsolicited_ns_count"] = len(unsolicited_ns_records)
    d["unsolicited_a_count"] = len(unsolicited_a_records)
    d["has_unsolicited_ns"] = len(unsolicited_ns_records) > 0
    d["has_unsolicited_glue"] = len(unsolicited_a_records) > 0
    
    # RC2: Check for invalid record types in authority section
    # RFC allows only NS, SOA, and DNSSEC types in authority
    invalid_auth_types = []
    for rr in authority_records:
        if rr["type"] not in (2, 6, 46, 48, 43, 47, 50, 51):  # NS, SOA, RRSIG, DNSKEY, DS, NSEC, NSEC3, NSEC3PARAM
            invalid_auth_types.append(rr["type"])
    d["invalid_authority_types"] = len(invalid_auth_types)
    d["has_invalid_authority"] = len(invalid_auth_types) > 0

    if ttl_vals:
        d["resp_min_ttl"] = min(ttl_vals)
        d["ttl_zero"] = 0 in ttl_vals
    else:
        d["resp_min_ttl"] = -1
        d["ttl_zero"] = False

    d["resp_port"] = 53
    return d


def _extract_dns_bytes(txt):
    """Try every known encoding of response.txt and return raw bytes or None."""
    # Format 1: b'<hex>' or b"<hex>"
    for q in ("'", '"'):
        prefix = f"b{q}"
        if txt.startswith(prefix) and txt.endswith(q):
            inner = txt[len(prefix):-1]
            try:
                return bytes.fromhex(inner)
            except ValueError:
                pass
            try:
                import ast
                return ast.literal_eval(txt)
            except Exception:
                pass

    # Format 2: plain hex string
    clean = re.sub(r'\s+', '', txt)
    if len(clean) >= 24 and all(c in '0123456789abcdefABCDEF' for c in clean):
        try:
            return bytes.fromhex(clean)
        except ValueError:
            pass

    return None


# ==================================================================
#  Public parse functions
# ==================================================================

_EMPTY_RESPONSE = {
    "present": True, "timeout": False, "malformed": True,
    "rcode": "rcNotSet", "resp_id": 0, "ancount": 0,
    "nscount": 0, "arcount": 0, "tc": False, "aa": False,
    "ad": False, "do": False, "rd": False, "ra": False, "cd": False,
    "opt_present": False, "dnssec_present": False, "resp_min_ttl": -1,
    "ttl_zero": False, "transport": "UDP", "resp_port": 53,
    "resp_size": 0, "cname_loop_detected": False, "cname_chase_count": 0,
    "out_of_bailiwick_count": 0, "out_of_bailiwick_cached": False,
    "unsolicited_ns_count": 0, "unsolicited_a_count": 0,
    "has_unsolicited_ns": False, "has_unsolicited_glue": False,
    "invalid_authority_types": 0, "has_invalid_authority": False,
}


def parse_response_txt(p, query_name=""):
    """Parse DNS response from response.txt (hex wire format)."""
    p = Path(p)
    if not p.exists():
        return None
    try:
        txt = p.read_text(errors="ignore").strip()
    except Exception:
        return None
    if not txt:
        return None

    upper = txt.upper()
    if "TIMEOUT" in upper or "TIMED OUT" in upper or "NO RESPONSE" in upper:
        d = dict(_EMPTY_RESPONSE)
        d["timeout"] = True
        d["malformed"] = False
        return d

    raw = _extract_dns_bytes(txt)
    if raw is None:
        return dict(_EMPTY_RESPONSE)

    if len(raw) < 12:
        return dict(_EMPTY_RESPONSE)

    return _parse_dns_wire(raw, query_name)


# ---- DNS query parsing (from hex) --------------------------------

def _parse_dns_query_hex(h):
    """Minimal DNS query parser from hex string."""
    res = {"id": 0, "qtype": "OTHER", "do": False, "rd": True, "qname": ""}
    try:
        raw = binascii.unhexlify(h.strip())
        if len(raw) < 12:
            return res
        qid = (raw[0] << 8) | raw[1]
        flags = (raw[2] << 8) | raw[3]
        qdcount = (raw[4] << 8) | raw[5]
        res["id"] = qid
        res["rd"] = bool((flags >> 8) & 1)  # RC3: RD flag
        if qdcount < 1:
            return res
        # Read QNAME
        qname, i = _read_dns_name(raw, 12)
        res["qname"] = qname
        if i and i + 4 <= len(raw):
            qtype = (raw[i] << 8) | raw[i + 1]
            res["qtype"] = _norm_qtype(qtype)
        return res
    except Exception:
        return res


def parse_query_hex_file(p):
    """Read query.txt (single hex line). Returns {qid, qtype, do, rd, qname}."""
    p = Path(p)
    d = {"qid": 0, "qtype": "OTHER", "do": False, "rd": True, "qname": ""}
    if not p.exists():
        return d
    try:
        hexline = p.read_text(errors="ignore").strip()
        if not hexline:
            return d
        parsed = _parse_dns_query_hex(hexline)
        d["qid"] = parsed["id"]
        d["qtype"] = parsed["qtype"]
        d["do"] = parsed["do"]
        d["rd"] = parsed["rd"]
        d["qname"] = parsed["qname"]
        return d
    except Exception:
        return d


# ==================================================================
#  PCAP parser (pure Python — no tshark dependency)
# ==================================================================

def _read_pcap_packets(pcap_path):
    """Read a libpcap file. Returns list of packet tuples."""
    results = []
    try:
        data = Path(pcap_path).read_bytes()
    except Exception:
        return results

    if len(data) < 24:
        return results

    magic = struct.unpack_from('<I', data, 0)[0]
    if magic == 0xa1b2c3d4:
        e = '<'
    elif magic == 0xd4c3b2a1:
        e = '>'
    else:
        return results

    network = struct.unpack_from(e + 'I', data, 20)[0]

    if network == 1:
        lhdr = 14; etype_off = 12
    elif network == 113:
        lhdr = 16; etype_off = 14
    elif network == 276:
        lhdr = 20; etype_off = 0
    elif network in (12, 101, 228):
        lhdr = 0; etype_off = -1
    else:
        lhdr = 14; etype_off = 12

    pos = 24
    while pos + 16 <= len(data):
        _ts_sec, _ts_usec, incl_len, _orig_len = struct.unpack_from(e + 'IIII', data, pos)
        pos += 16
        if pos + incl_len > len(data):
            break
        pkt = data[pos:pos + incl_len]
        pos += incl_len

        if len(pkt) < lhdr + 20:
            continue

        if etype_off >= 0 and len(pkt) > etype_off + 1:
            et = (pkt[etype_off] << 8) | pkt[etype_off + 1]
            if et != 0x0800:
                continue

        ip_off = lhdr
        if (pkt[ip_off] >> 4) != 4:
            continue

        ihl = (pkt[ip_off] & 0xF) * 4
        if len(pkt) < ip_off + ihl:
            continue

        proto = pkt[ip_off + 9]
        src_ip = '.'.join(str(b) for b in pkt[ip_off + 12:ip_off + 16])
        dst_ip = '.'.join(str(b) for b in pkt[ip_off + 16:ip_off + 20])

        tp_off = ip_off + ihl

        if proto == 17:  # UDP
            if len(pkt) < tp_off + 8:
                continue
            sp = (pkt[tp_off] << 8) | pkt[tp_off + 1]
            dp = (pkt[tp_off + 2] << 8) | pkt[tp_off + 3]
            udp_len = (pkt[tp_off + 4] << 8) | pkt[tp_off + 5]
            dns_qr = None
            dns_off = tp_off + 8
            if len(pkt) >= dns_off + 4:
                flags = (pkt[dns_off + 2] << 8) | pkt[dns_off + 3]
                dns_qr = (flags >> 15) & 1
            results.append((src_ip, dst_ip, proto, sp, dp, dns_qr, udp_len))

        elif proto == 6:  # TCP
            if len(pkt) < tp_off + 4:
                continue
            sp = (pkt[tp_off] << 8) | pkt[tp_off + 1]
            dp = (pkt[tp_off + 2] << 8) | pkt[tp_off + 3]
            results.append((src_ip, dst_ip, proto, sp, dp, None, 0))

    return results


_RESOLVER_IP_PREFIXES = {
    "bind9":      "172.22.1",
    "unbound":    "172.22.2",
    "powerdns":   "172.22.3",
    "knot":       "172.22.4",
    "maradns":    "172.22.5",
    "technitium": "172.22.6",
}


def parse_pcap(pcap_path, unit_no, resolver_name="", role_ip_base="172.22"):
    """
    Analyse a resolver-container pcap (no tshark needed).
    
    Returns dict of boolean/integer predicates for forward-mode bug detection.
    """
    out = {
        "udp_q": False, "tcp_retry": False, "resp_seen": False,
        "auth_q": False, "auth_rsp": False, "auth_target_correct": False,
        "forwarder_q": False, "unexpected_external": False,
        # RC6/RC7: Query counting
        "udp_query_count": 0, "tcp_query_count": 0,
        # CP3/RC5: Response size tracking
        "max_udp_resp_size": 0,
    }

    pcap_path = Path(pcap_path)
    if not pcap_path.exists() or pcap_path.stat().st_size < 25:
        return out

    try:
        packets = _read_pcap_packets(pcap_path)
    except Exception:
        return out

    forwarder_ip = f"172.22.201.{unit_no}"
    client_ip = f"172.22.101.{unit_no}"
    
    resolver_prefix = _RESOLVER_IP_PREFIXES.get(resolver_name.lower(), "")
    resolver_ip = f"{resolver_prefix}.{unit_no}" if resolver_prefix else ""

    for pkt in packets:
        src_ip, dst_ip, proto, sp, dp, dns_qr = pkt[:6]
        pkt_size = pkt[6] if len(pkt) > 6 else 0

        if proto == 17:  # UDP
            is_from_resolver = (resolver_ip and src_ip == resolver_ip) or \
                               (not resolver_ip and src_ip.startswith(role_ip_base) and 
                                not src_ip.startswith("172.22.101") and 
                                not src_ip.startswith("172.22.201"))
            
            if is_from_resolver and dp == 53 and dns_qr in (0, None):
                out["udp_q"] = True
                out["udp_query_count"] += 1
                
                if dst_ip == forwarder_ip:
                    out["forwarder_q"] = True
                    out["auth_target_correct"] = True
                elif dst_ip == client_ip:
                    pass
                elif dst_ip.startswith("172.22.201"):
                    out["forwarder_q"] = True
                elif dst_ip.startswith(role_ip_base):
                    out["auth_q"] = True
                else:
                    out["unexpected_external"] = True

            is_to_resolver = (resolver_ip and dst_ip == resolver_ip) or \
                             (not resolver_ip and dst_ip.startswith(role_ip_base) and
                              not dst_ip.startswith("172.22.101") and
                              not dst_ip.startswith("172.22.201"))
            
            if is_to_resolver and sp == 53 and dns_qr in (1, None):
                out["resp_seen"] = True
                if pkt_size > out["max_udp_resp_size"]:
                    out["max_udp_resp_size"] = pkt_size
                if src_ip == forwarder_ip or src_ip.startswith("172.22.201"):
                    out["auth_rsp"] = True

        elif proto == 6:  # TCP
            if dp == 53:
                out["tcp_retry"] = True
                out["tcp_query_count"] += 1
            if sp == 53:
                out["resp_seen"] = True

    return out


# ==================================================================
#  DNSTap parser
# ==================================================================

def parse_dnstap(dnstap_dir, qname="", qtype=""):
    """Scan DNSTap JSON/JSONL for query & response evidence."""
    out = {"query_seen": False, "response_seen": False}
    dnstap_dir = Path(dnstap_dir)
    if not dnstap_dir.exists():
        return out
    for f in dnstap_dir.glob("*.json*"):
        try:
            for line in f.read_text(errors="ignore").splitlines():
                if '"CLIENT_QUERY"' in line:
                    out["query_seen"] = True
                if '"CLIENT_RESPONSE"' in line:
                    out["response_seen"] = True
                if out["query_seen"] and out["response_seen"]:
                    return out
        except Exception:
            continue
    try:
        if any(dnstap_dir.iterdir()):
            out["query_seen"] = True
            out["response_seen"] = True
    except Exception:
        pass
    return out


# ==================================================================
#  Cache-dump parser
# ==================================================================

_CACHE_DUMP_FILES = [
    "named_dump.db", "unbound.cache.db", "powerdns.cache.db", "cache.json",
]

_LOG_FILES = [
    "knot.log", "maradns.log", "bind.log", "unbound.log", "powerdns.log", "log.txt",
]


def parse_cache(resolver_dir, query_name=""):
    """
    Inspect resolver artifacts for cache state.
    
    CP1/CP4: Detect out-of-bailiwick and unsolicited cached records.
    RC2: Detect invalid record types in cache.
    """
    resolver_dir = Path(resolver_dir)
    d = {
        "written": False, "neg_present": False, "min_ttl": -1, "any_rrset": False,
        "cached_ns_count": 0, "cached_a_count": 0,
        "out_of_bailiwick_in_cache": False,
        "invalid_types_in_cache": False,
    }

    # Determine expected zone from query_name
    query_zone = ""
    if query_name:
        parts = query_name.lower().rstrip(".").split(".")
        if len(parts) >= 2:
            query_zone = ".".join(parts[-2:])

    def _scan_text(txt):
        for m in re.findall(r'\bTTL\s+(\d+)', txt, re.I):
            try:
                v = int(m)
                d["min_ttl"] = v if d["min_ttl"] < 0 else min(d["min_ttl"], v)
            except ValueError:
                pass
        for m in re.findall(r'\t(\d+)\t(?:IN|CH|HS)\t', txt):
            try:
                v = int(m)
                d["min_ttl"] = v if d["min_ttl"] < 0 else min(d["min_ttl"], v)
            except ValueError:
                pass
        if re.search(r'\bNXDOMAIN\b|\bNEGATIVE\b|\bNSEC\b', txt, re.I):
            d["neg_present"] = True
        if re.search(r'\b(?:IN|CH|HS)\s+(?:A|AAAA|MX|NS|CNAME|TXT|SOA)\b', txt):
            d["any_rrset"] = True
        
        # Count NS and A records
        d["cached_ns_count"] += len(re.findall(r'\bIN\s+NS\b', txt, re.I))
        d["cached_a_count"] += len(re.findall(r'\bIN\s+(?:A|AAAA)\b', txt, re.I))
        
        # CP1: Check for out-of-bailiwick entries
        if query_zone:
            for line in txt.splitlines():
                # Match cache entries like "name TTL IN TYPE ..."
                m = re.match(r'^(\S+)\s+\d+\s+IN\s+', line, re.I)
                if m:
                    cached_name = m.group(1).lower().rstrip(".")
                    if not cached_name.endswith(query_zone) and cached_name != query_zone:
                        # Check if it's a parent zone (allowed for delegation)
                        if not query_zone.endswith(cached_name):
                            d["out_of_bailiwick_in_cache"] = True

    def _scan_json(txt):
        try:
            obj = json.loads(txt)
        except Exception:
            _scan_text(txt)
            return
        _walk_json(obj)

    def _walk_json(obj):
        if isinstance(obj, dict):
            for k, v in obj.items():
                kl = k.lower()
                if kl == "ttl" and isinstance(v, (int, float)):
                    iv = int(v)
                    d["min_ttl"] = iv if d["min_ttl"] < 0 else min(d["min_ttl"], iv)
                if kl == "type" and isinstance(v, str):
                    vup = v.upper()
                    if vup in ("A", "AAAA", "MX", "NS", "CNAME", "TXT", "SOA"):
                        d["any_rrset"] = True
                    if vup == "NS":
                        d["cached_ns_count"] += 1
                    if vup in ("A", "AAAA"):
                        d["cached_a_count"] += 1
                if kl == "rcode" and isinstance(v, str) and v.upper() == "NXDOMAIN":
                    d["neg_present"] = True
                _walk_json(v)
        elif isinstance(obj, list):
            for item in obj:
                _walk_json(item)

    for fname in _CACHE_DUMP_FILES:
        p = resolver_dir / fname
        if not p.exists():
            continue
        d["written"] = True
        try:
            txt = p.read_text(errors="ignore")
        except Exception:
            continue
        if fname.endswith(".json"):
            _scan_json(txt)
        else:
            _scan_text(txt)

    for fname in _LOG_FILES:
        p = resolver_dir / fname
        if not p.exists():
            continue
        try:
            txt = p.read_text(errors="ignore")
            _scan_text(txt)
        except Exception:
            continue

    return d
