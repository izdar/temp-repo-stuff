// dnsmasq_predicate_adapter.h
#ifndef DNSMASQ_PREDICATE_ADAPTER_H
#define DNSMASQ_PREDICATE_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * DNSMASQ predicate adapter for DNS messages -> "k=v ..." lines.
 *
 * IMPORTANT: Use the direction-specific builders, not the generic one.
 *
 *   dnsmasq_build_request_pred_line()   — for C2S packets (queries)
 *   dnsmasq_build_response_pred_line()  — for S2C packets (responses)
 *
 * The generic dnsmasq_build_pred_line() is DEPRECATED. It reads the QR bit
 * from the DNS header to guess direction, which fails on fuzzed data where
 * the QR bit may be flipped.
 *
 * Direction-aware guarantees:
 *   Request builder:  always is_query=true,  is_response=false, message_type=query
 *   Response builder: always is_query=false, is_response=true,  message_type=response
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Reset per-session adapter state (query tracking, cache state, etc.). */
void dnsmasq_predicate_reset_session(void);

/* Build predicate line for a C2S DNS query.
 * Always sets is_query=true, is_response=false, message_type=query
 * regardless of the QR bit in the packet.
 */
void dnsmasq_build_request_pred_line(const unsigned char *buf,
                                     unsigned int len,
                                     char *out,
                                     size_t out_sz);

/* Build predicate line for an S2C DNS response.
 * Always sets is_query=false, is_response=true, message_type=response
 * regardless of the QR bit in the packet.
 */
void dnsmasq_build_response_pred_line(const unsigned char *buf,
                                      unsigned int len,
                                      char *out,
                                      size_t out_sz);

/* DEPRECATED: Generic builder that reads the QR bit to guess direction.
 * Kept for backward compatibility only.
 */
void dnsmasq_build_pred_line(const unsigned char *buf,
                              unsigned int len,
                              char *out,
                              size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* DNSMASQ_PREDICATE_ADAPTER_H */
