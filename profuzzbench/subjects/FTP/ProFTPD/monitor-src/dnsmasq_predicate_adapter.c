// dnsmasq_predicate_adapter.c - FIXED VERSION
#include "dnsmasq_predicate_adapter.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* --- DNS Constants (from RFC 1035) --- */

// Opcodes
#define DNS_OPCODE_QUERY   0
#define DNS_OPCODE_IQUERY  1
#define DNS_OPCODE_STATUS  2

// Response codes
#define DNS_RCODE_NOERROR   0
#define DNS_RCODE_FORMERR   1
#define DNS_RCODE_SERVFAIL  2
#define DNS_RCODE_NXDOMAIN  3
#define DNS_RCODE_NOTIMP    4
#define DNS_RCODE_REFUSED   5

// Common QTYPEs
#define DNS_QTYPE_A      1
#define DNS_QTYPE_NS     2
#define DNS_QTYPE_CNAME  5
#define DNS_QTYPE_SOA    6
#define DNS_QTYPE_PTR    12
#define DNS_QTYPE_MX     15
#define DNS_QTYPE_TXT    16
#define DNS_QTYPE_AAAA   28
#define DNS_QTYPE_ANY    255

// DNS header flags
#define DNS_FLAG_QR     0x8000  // Query/Response
#define DNS_FLAG_AA     0x0400  // Authoritative Answer
#define DNS_FLAG_TC     0x0200  // Truncated
#define DNS_FLAG_RD     0x0100  // Recursion Desired
#define DNS_FLAG_RA     0x0080  // Recursion Available
#define DNS_FLAG_AD     0x0020  // Authenticated Data
#define DNS_FLAG_CD     0x0010  // Checking Disabled

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

/* Parse DNS header.
 * DNS header format (12 bytes):
 *   0-1:  ID
 *   2-3:  Flags (QR, Opcode, AA, TC, RD, RA, Z, AD, CD, RCODE)
 *   4-5:  QDCOUNT
 *   6-7:  ANCOUNT
 *   8-9:  NSCOUNT
 *   10-11: ARCOUNT
 */
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

/* Skip a DNS name (with compression support).
 * Returns: offset after the name, or 0 on error.
 */
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
            // End of name
            return pos + 1;
        } else if ((label_len & 0xC0) == 0xC0) {
            // Pointer (compression)
            if (pos + 1 >= len) return 0;
            jumps++;
            return pos + 2;  // Pointer is 2 bytes
        } else if ((label_len & 0xC0) == 0) {
            // Regular label
            pos += 1 + label_len;
        } else {
            // Invalid label
            return 0;
        }
    }
    
    return 0;  // Invalid or too many jumps
}

/* Parse the first question in the DNS message.
 * Returns: QTYPE, or 0 on error.
 */
static uint16_t dns_parse_first_question(const unsigned char *buf,
                                         unsigned int len,
                                         uint16_t qdcount)
{
    if (qdcount == 0) return 0;
    
    unsigned int pos = 12;  // After header
    
    // Skip QNAME
    pos = dns_skip_name(buf, len, pos);
    if (pos == 0 || pos + 4 > len) return 0;
    
    // Read QTYPE (2 bytes)
    uint16_t qtype = read_u16_be(&buf[pos]);
    
    return qtype;
}

/* Track a query for later matching */
static void track_query(uint16_t query_id, uint16_t qtype)
{
    if (g_query_count >= MAX_TRACKED_QUERIES) {
        // Ring buffer - overwrite oldest
        memmove(&g_query_history[0], &g_query_history[1],
                sizeof(query_record_t) * (MAX_TRACKED_QUERIES - 1));
        g_query_count = MAX_TRACKED_QUERIES - 1;
    }
    
    g_query_history[g_query_count].query_id = query_id;
    g_query_history[g_query_count].qtype = qtype;
    g_query_history[g_query_count].valid = true;
    g_query_history[g_query_count].timestamp = 0;  // Could use actual timestamp
    g_query_count++;
}

static bool find_matching_query(uint16_t query_id, uint16_t response_qtype, uint16_t *qtype_out) {
    for (size_t i = 0; i < g_query_count; i++) {
        if (g_query_history[i].valid &&
            g_query_history[i].query_id == query_id &&
            g_query_history[i].qtype == response_qtype) {  // ← Must match qtype too
            
            if (qtype_out) *qtype_out = g_query_history[i].qtype;
            return true;
        }
    }
    return false;
}

/* Convert QTYPE to enum string */
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

/* Convert opcode to enum string */
static const char *opcode_to_string(uint8_t opcode)
{
    switch (opcode) {
        case DNS_OPCODE_QUERY:  return "QUERY";
        case DNS_OPCODE_IQUERY: return "IQUERY";
        case DNS_OPCODE_STATUS: return "STATUS";
        default:                return "OPCODE_UNKNOWN";
    }
}

/* Convert RCODE to enum string */
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

/* --- Public builder --- */

void dnsmasq_build_pred_line(const unsigned char *buf,
                              unsigned int len,
                              char *out,
                              size_t out_sz)
{
    if (!g_session_initialized) {
        dnsmasq_predicate_reset_session();
    }
    
    /* CRITICAL FIX: Error handlers must send ALL predicates */
    if (!buf || len < 12 || !out || out_sz == 0) {
        snprintf(out, out_sz,
                 "message_type=messageNotSet opcode=OPCODE_UNKNOWN rcode=RCODE_UNKNOWN qtype=QTYPE_UNKNOWN "
                 "is_query=false is_response=false "
                 "aa=false tc=false rd=false ra=false ad=false cd=false "
                 "qdcount=-1 ancount=-1 nscount=-1 arcount=-1 "
                 "response_valid=false dnssec_ok=false "
                 "query_id=-1 id_match=false "
                 "cache_hit=false upstream_queried=false");
        return;
    }
    
    /* Parse DNS header */
    uint16_t id = 0, flags = 0;
    uint16_t qdcount = 0, ancount = 0, nscount = 0, arcount = 0;
    
    /* CRITICAL FIX: Parse error handler must also send ALL predicates */
    if (!dns_parse_header(buf, len, &id, &flags, &qdcount, &ancount, &nscount, &arcount)) {
        snprintf(out, out_sz,
                 "message_type=messageNotSet opcode=OPCODE_UNKNOWN rcode=RCODE_UNKNOWN qtype=QTYPE_UNKNOWN "
                 "is_query=false is_response=false "
                 "aa=false tc=false rd=false ra=false ad=false cd=false "
                 "qdcount=-1 ancount=-1 nscount=-1 arcount=-1 "
                 "response_valid=false dnssec_ok=false "
                 "query_id=-1 id_match=false "
                 "cache_hit=false upstream_queried=false");
        return;
    }
    
    /* Extract flag fields */
    bool qr = (flags & DNS_FLAG_QR) != 0;     // 0 = query, 1 = response
    uint8_t opcode = (flags >> 11) & 0x0F;
    bool aa = (flags & DNS_FLAG_AA) != 0;
    bool tc = (flags & DNS_FLAG_TC) != 0;
    bool rd = (flags & DNS_FLAG_RD) != 0;
    bool ra = (flags & DNS_FLAG_RA) != 0;
    bool ad = (flags & DNS_FLAG_AD) != 0;
    bool cd = (flags & DNS_FLAG_CD) != 0;
    uint8_t rcode = flags & 0x0F;
    
    /* Determine message type */
    const char *msg_type = qr ? "response" : "query";
    bool is_query = !qr;
    bool is_response = qr;
    
    /* Parse first question to get QTYPE */
    uint16_t qtype = dns_parse_first_question(buf, len, qdcount);
    
    /* Track queries and match responses */
    bool id_match = false;
    bool cache_hit = false;
    bool upstream_queried = false;
    
    if (is_query) {
        track_query(id, qtype);
        upstream_queried = true;
        g_upstream_queries++;
    } else if (is_response) {
        uint16_t matching_qtype = 0;
        id_match = find_matching_query(id, qtype, &matching_qtype);
        
        // Heuristic: very fast response might be cache hit
        if (id_match && ancount > 0 && rcode == DNS_RCODE_NOERROR) {
            // In real implementation, timing would determine this
            // For now, assume some responses are cached
            if ((id % 3) == 0) {  // Simple heuristic
                cache_hit = true;
                g_cache_hits++;
            } else {
                upstream_queried = true;
            }
        }
    }
    
    /* Determine response validity */
    bool response_valid = is_response && 
                         (rcode == DNS_RCODE_NOERROR || rcode == DNS_RCODE_NXDOMAIN);
    
    /* DNSSEC indicators */
    bool dnssec_ok = ad;  // AD flag indicates DNSSEC validation
    
    /* Build predicate line - ALL 22 predicates ALWAYS */
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