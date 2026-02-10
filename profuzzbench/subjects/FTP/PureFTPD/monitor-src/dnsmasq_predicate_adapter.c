// dnsmasq_predicate_adapter.c - FIXED: Split into separate C2S/S2C builders
//
// ROOT CAUSE FIX: Same as DTLS — the single dnsmasq_build_pred_line() was
// assigned to both function pointers. It read the QR bit from the DNS header
// to determine query vs response, but fuzzed C2S packets can have QR=1
// (response bit set), causing the adapter to emit is_response=true and
// message_type=response on a client-to-server event.
//
// FIX: Trust the network layer direction, not the packet bytes.

#include "dnsmasq_predicate_adapter.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* --- DNS Constants (from RFC 1035) --- */

#define DNS_OPCODE_QUERY   0
#define DNS_OPCODE_IQUERY  1
#define DNS_OPCODE_STATUS  2

#define DNS_RCODE_NOERROR   0
#define DNS_RCODE_FORMERR   1
#define DNS_RCODE_SERVFAIL  2
#define DNS_RCODE_NXDOMAIN  3
#define DNS_RCODE_NOTIMP    4
#define DNS_RCODE_REFUSED   5

#define DNS_QTYPE_A      1
#define DNS_QTYPE_NS     2
#define DNS_QTYPE_CNAME  5
#define DNS_QTYPE_SOA    6
#define DNS_QTYPE_PTR    12
#define DNS_QTYPE_MX     15
#define DNS_QTYPE_TXT    16
#define DNS_QTYPE_AAAA   28
#define DNS_QTYPE_ANY    255

#define DNS_FLAG_QR     0x8000
#define DNS_FLAG_AA     0x0400
#define DNS_FLAG_TC     0x0200
#define DNS_FLAG_RD     0x0100
#define DNS_FLAG_RA     0x0080
#define DNS_FLAG_AD     0x0020
#define DNS_FLAG_CD     0x0010

/* --- Static per-session state --- */

typedef struct {
    uint16_t query_id;
    uint16_t qtype;
    bool valid;
    uint64_t timestamp;
} query_record_t;

#define MAX_TRACKED_QUERIES 256

static query_record_t g_query_history[MAX_TRACKED_QUERIES];
static size_t g_query_count = 0;
static bool g_session_initialized = false;
static size_t g_cache_hits = 0;
static size_t g_upstream_queries = 0;

void dnsmasq_predicate_reset_session(void)
{
    memset(g_query_history, 0, sizeof(g_query_history));
    g_query_count = 0;
    g_cache_hits = 0;
    g_upstream_queries = 0;
    g_session_initialized = true;
}

/* --- Helpers --- */

static inline uint16_t read_u16_be(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | ((uint16_t)p[1]);
}

static bool dns_parse_header(const unsigned char *buf,
                             unsigned int len,
                             uint16_t *id_out,
                             uint16_t *flags_out,
                             uint16_t *qdcount_out,
                             uint16_t *ancount_out,
                             uint16_t *nscount_out,
                             uint16_t *arcount_out)
{
    if (!buf || len < 12) return false;
    
    if (id_out) *id_out = read_u16_be(&buf[0]);
    if (flags_out) *flags_out = read_u16_be(&buf[2]);
    if (qdcount_out) *qdcount_out = read_u16_be(&buf[4]);
    if (ancount_out) *ancount_out = read_u16_be(&buf[6]);
    if (nscount_out) *nscount_out = read_u16_be(&buf[8]);
    if (arcount_out) *arcount_out = read_u16_be(&buf[10]);
    
    return true;
}

static unsigned int dns_skip_name(const unsigned char *buf,
                                  unsigned int len,
                                  unsigned int offset)
{
    unsigned int pos = offset;
    unsigned int jumps = 0;
    const unsigned int MAX_JUMPS = 5;
    
    while (pos < len && jumps < MAX_JUMPS) {
        uint8_t label_len = buf[pos];
        
        if (label_len == 0) {
            return pos + 1;
        } else if ((label_len & 0xC0) == 0xC0) {
            if (pos + 1 >= len) return 0;
            return pos + 2;
        } else if ((label_len & 0xC0) == 0) {
            pos += 1 + label_len;
        } else {
            return 0;
        }
    }
    
    return 0;
}

static uint16_t dns_parse_first_question(const unsigned char *buf,
                                         unsigned int len,
                                         uint16_t qdcount)
{
    if (qdcount == 0) return 0;
    
    unsigned int pos = 12;
    pos = dns_skip_name(buf, len, pos);
    if (pos == 0 || pos + 4 > len) return 0;
    
    return read_u16_be(&buf[pos]);
}

static void track_query(uint16_t query_id, uint16_t qtype)
{
    if (g_query_count >= MAX_TRACKED_QUERIES) {
        memmove(&g_query_history[0], &g_query_history[1],
                sizeof(query_record_t) * (MAX_TRACKED_QUERIES - 1));
        g_query_count = MAX_TRACKED_QUERIES - 1;
    }
    
    g_query_history[g_query_count].query_id = query_id;
    g_query_history[g_query_count].qtype = qtype;
    g_query_history[g_query_count].valid = true;
    g_query_history[g_query_count].timestamp = 0;
    g_query_count++;
}

static bool find_matching_query(uint16_t query_id, uint16_t response_qtype,
                                uint16_t *qtype_out)
{
    for (size_t i = 0; i < g_query_count; i++) {
        if (g_query_history[i].valid &&
            g_query_history[i].query_id == query_id &&
            g_query_history[i].qtype == response_qtype) {
            if (qtype_out) *qtype_out = g_query_history[i].qtype;
            return true;
        }
    }
    return false;
}

static const char *qtype_to_string(uint16_t qtype)
{
    switch (qtype) {
        case DNS_QTYPE_A:     return "A";
        case DNS_QTYPE_AAAA:  return "AAAA";
        case DNS_QTYPE_MX:    return "MX";
        case DNS_QTYPE_NS:    return "NS";
        case DNS_QTYPE_CNAME: return "CNAME";
        case DNS_QTYPE_PTR:   return "PTR";
        case DNS_QTYPE_SOA:   return "SOA";
        case DNS_QTYPE_TXT:   return "TXT";
        case DNS_QTYPE_ANY:   return "ANY";
        default:              return "QTYPE_UNKNOWN";
    }
}

static const char *opcode_to_string(uint8_t opcode)
{
    switch (opcode) {
        case DNS_OPCODE_QUERY:  return "QUERY";
        case DNS_OPCODE_IQUERY: return "IQUERY";
        case DNS_OPCODE_STATUS: return "STATUS";
        default:                return "OPCODE_UNKNOWN";
    }
}

static const char *rcode_to_string(uint8_t rcode)
{
    switch (rcode) {
        case DNS_RCODE_NOERROR:  return "NOERROR";
        case DNS_RCODE_FORMERR:  return "FORMERR";
        case DNS_RCODE_SERVFAIL: return "SERVFAIL";
        case DNS_RCODE_NXDOMAIN: return "NXDOMAIN";
        case DNS_RCODE_NOTIMP:   return "NOTIMP";
        case DNS_RCODE_REFUSED:  return "REFUSED";
        default:                 return "RCODE_UNKNOWN";
    }
}

/* --- Default error output for all error paths --- */

static void output_default_error(char *out, size_t out_sz,
                                 bool force_query, bool force_response)
{
    snprintf(out, out_sz,
             "message_type=messageNotSet opcode=OPCODE_UNKNOWN rcode=RCODE_UNKNOWN qtype=QTYPE_UNKNOWN "
             "is_query=%s is_response=%s "
             "aa=false tc=false rd=false ra=false ad=false cd=false "
             "qdcount=-1 ancount=-1 nscount=-1 arcount=-1 "
             "response_valid=false dnssec_ok=false "
             "query_id=-1 id_match=false "
             "cache_hit=false upstream_queried=false",
             force_query ? "true" : "false",
             force_response ? "true" : "false");
}

/* --- Internal: shared parsing + predicate assembly --- */

typedef enum {
    DNS_DIR_C2S,  /* Query: client -> dnsmasq */
    DNS_DIR_S2C   /* Response: dnsmasq -> client */
} dns_direction_t;

static void dnsmasq_build_pred_line_internal(const unsigned char *buf,
                                             unsigned int len,
                                             char *out,
                                             size_t out_sz,
                                             dns_direction_t direction)
{
    if (!g_session_initialized) {
        dnsmasq_predicate_reset_session();
    }
    
    bool is_query    = (direction == DNS_DIR_C2S);
    bool is_response = (direction == DNS_DIR_S2C);
    
    /* Error paths */
    if (!buf || len < 12 || !out || out_sz == 0) {
        output_default_error(out, out_sz, is_query, is_response);
        return;
    }
    
    uint16_t id = 0, flags = 0;
    uint16_t qdcount = 0, ancount = 0, nscount = 0, arcount = 0;
    
    if (!dns_parse_header(buf, len, &id, &flags, &qdcount, &ancount, &nscount, &arcount)) {
        output_default_error(out, out_sz, is_query, is_response);
        return;
    }
    
    /* Extract flag fields from the packet (these are factual, not direction) */
    uint8_t opcode = (flags >> 11) & 0x0F;
    bool aa = (flags & DNS_FLAG_AA) != 0;
    bool tc = (flags & DNS_FLAG_TC) != 0;
    bool rd = (flags & DNS_FLAG_RD) != 0;
    bool ra = (flags & DNS_FLAG_RA) != 0;
    bool ad = (flags & DNS_FLAG_AD) != 0;
    bool cd = (flags & DNS_FLAG_CD) != 0;
    uint8_t rcode = flags & 0x0F;
    
    /* Direction comes from the network layer, NOT the QR bit.
     * Fuzzed packets can have QR=1 in a C2S query. */
    const char *msg_type = is_query ? "query" : "response";
    
    /* Parse first question to get QTYPE */
    uint16_t qtype = dns_parse_first_question(buf, len, qdcount);
    
    /* Direction-specific logic */
    bool id_match = false;
    bool cache_hit = false;
    bool upstream_queried = false;
    
    if (is_query) {
        /* C2S: this is a query, track it */
        track_query(id, qtype);
        upstream_queried = true;
        g_upstream_queries++;
    } else {
        /* S2C: this is a response, match against tracked queries */
        uint16_t matching_qtype = 0;
        id_match = find_matching_query(id, qtype, &matching_qtype);
        
        if (id_match && ancount > 0 && rcode == DNS_RCODE_NOERROR) {
            if ((id % 3) == 0) {
                cache_hit = true;
                g_cache_hits++;
            } else {
                upstream_queried = true;
            }
        }
    }
    
    bool response_valid = is_response && 
                         (rcode == DNS_RCODE_NOERROR || rcode == DNS_RCODE_NXDOMAIN);
    
    bool dnssec_ok = ad;
    
    /* Build predicate line */
    snprintf(out, out_sz,
             "message_type=%s opcode=%s rcode=%s qtype=%s "
             "is_query=%s is_response=%s "
             "aa=%s tc=%s rd=%s ra=%s ad=%s cd=%s "
             "qdcount=%u ancount=%u nscount=%u arcount=%u "
             "response_valid=%s dnssec_ok=%s "
             "query_id=%u id_match=%s "
             "cache_hit=%s upstream_queried=%s",
             msg_type,
             opcode_to_string(opcode),
             rcode_to_string(rcode),
             qtype_to_string(qtype),
             is_query ? "true" : "false",
             is_response ? "true" : "false",
             aa ? "true" : "false",
             tc ? "true" : "false",
             rd ? "true" : "false",
             ra ? "true" : "false",
             ad ? "true" : "false",
             cd ? "true" : "false",
             qdcount,
             ancount,
             nscount,
             arcount,
             response_valid ? "true" : "false",
             dnssec_ok ? "true" : "false",
             id,
             id_match ? "true" : "false",
             cache_hit ? "true" : "false",
             upstream_queried ? "true" : "false");
}

/* --- Public API --- */

void dnsmasq_build_request_pred_line(const unsigned char *buf,
                                     unsigned int len,
                                     char *out,
                                     size_t out_sz)
{
    dnsmasq_build_pred_line_internal(buf, len, out, out_sz, DNS_DIR_C2S);
}

void dnsmasq_build_response_pred_line(const unsigned char *buf,
                                      unsigned int len,
                                      char *out,
                                      size_t out_sz)
{
    dnsmasq_build_pred_line_internal(buf, len, out, out_sz, DNS_DIR_S2C);
}

/* Backward-compatible wrapper — DEPRECATED */
void dnsmasq_build_pred_line(const unsigned char *buf,
                              unsigned int len,
                              char *out,
                              size_t out_sz)
{
    if (!g_session_initialized) {
        dnsmasq_predicate_reset_session();
    }
    
    /* Try to guess direction from QR bit (old behavior, unreliable on fuzzed data) */
    if (buf && len >= 12) {
        uint16_t flags = read_u16_be(&buf[2]);
        bool qr = (flags & DNS_FLAG_QR) != 0;
        if (qr) {
            dnsmasq_build_pred_line_internal(buf, len, out, out_sz, DNS_DIR_S2C);
        } else {
            dnsmasq_build_pred_line_internal(buf, len, out, out_sz, DNS_DIR_C2S);
        }
        return;
    }
    
    /* Fallback: unknown direction — default to C2S (safer for H() rules) */
    dnsmasq_build_pred_line_internal(buf, len, out, out_sz, DNS_DIR_C2S);
}
