// dnsmasq_predicate_adapter.h
#ifndef DNSMASQ_PREDICATE_ADAPTER_H
#define DNSMASQ_PREDICATE_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * DNSMASQ predicate adapter for DNS messages → "k=v ..." lines.
 *
 * This parses DNS protocol messages (RFC 1035) to extract:
 * - Message type (query vs response)
 * - Question and answer sections
 * - Resource record types
 * - Error conditions (NXDOMAIN, SERVFAIL, etc.)
 * - DNSSEC validation states
 * - Cache behavior
 *
 * It matches the spec:
 *
 *   enum message_type { messageNotSet, query, response };
 *   enum opcode { QUERY, IQUERY, STATUS };
 *   enum rcode { NOERROR, FORMERR, SERVFAIL, NXDOMAIN, NOTIMP, REFUSED };
 *   enum qtype { A, AAAA, MX, NS, CNAME, PTR, SOA, TXT, ANY };
 *   
 * plus scalar fields:
 *
 *   bool is_query, is_response;
 *   bool aa, tc, rd, ra, ad, cd;
 *   int qdcount, ancount, nscount, arcount;
 *   bool response_valid;
 *   bool dnssec_ok;
 *   int query_id;
 *   bool id_match;
 *   bool cache_hit;
 *   bool upstream_queried;
 *
 * Lines look like:
 *
 *   "message_type=query opcode=QUERY qtype=A "
 *   "is_query=true rd=true qdcount=1 query_id=12345"
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Reset per-session adapter state (query tracking, cache state, etc.). */
void dnsmasq_predicate_reset_session(void);

/* Build predicate line for a DNS message.
 * 
 * Parameters:
 *   buf: Raw DNS message (12-byte header + questions/answers)
 *   len: Total message length
 *   out: Output buffer for predicate line
 *   out_sz: Size of output buffer
 */
void dnsmasq_build_pred_line(const unsigned char *buf,
                              unsigned int len,
                              char *out,
                              size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* DNSMASQ_PREDICATE_ADAPTER_H */
