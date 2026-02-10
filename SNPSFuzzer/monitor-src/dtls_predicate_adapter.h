// dtls_predicate_adapter.h
#ifndef DTLS_PREDICATE_ADAPTER_H
#define DTLS_PREDICATE_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * DTLS predicate adapter for DTLS records -> "k=v ..." lines.
 *
 * IMPORTANT: Use the direction-specific builders, not the generic one.
 *
 *   dtls_build_request_pred_line()   — for C2S packets (before net_send)
 *   dtls_build_response_pred_line()  — for S2C packets (after net_recv)
 *
 * The generic dtls_build_pred_line() is DEPRECATED and should not be used
 * in new code. It tries to guess direction from packet bytes, which fails
 * on fuzzed data and causes false LTL violations.
 *
 * Enum mapping:
 *
 *   enum request  { requestNotSet, c2s_ClientHello, c2s_ClientHello_with_cookie, ... };
 *   enum response { responseNotSet, s2c_HelloVerifyRequest, s2c_ServerHello, ... };
 *
 * The request builder ONLY sets request=c2s_X, always response=responseNotSet.
 * The response builder ONLY sets response=s2c_X, always request=requestNotSet.
 * This prevents fuzzed C2S packets from polluting the response enum.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Reset per-session adapter state (cookie exchange, handshake state, etc.). */
void dtls_predicate_reset_session(void);

/* Build predicate line for a C2S (client-to-server) DTLS record.
 * Always sets response=responseNotSet regardless of packet content.
 *
 * Parameters:
 *   buf:    Raw DTLS record (13-byte header + payload)
 *   len:    Total record length
 *   out:    Output buffer for predicate line
 *   out_sz: Size of output buffer
 */
void dtls_build_request_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz);

/* Build predicate line for an S2C (server-to-client) DTLS record.
 * Always sets request=requestNotSet regardless of packet content.
 *
 * Parameters:
 *   buf:    Raw DTLS record (13-byte header + payload)
 *   len:    Total record length
 *   out:    Output buffer for predicate line
 *   out_sz: Size of output buffer
 */
void dtls_build_response_pred_line(const unsigned char *buf,
                                   unsigned int len,
                                   char *out,
                                   size_t out_sz);

/* DEPRECATED: Generic builder that guesses direction from packet bytes.
 * Kept for backward compatibility. New code should use the
 * direction-specific builders above.
 */
void dtls_build_pred_line(const unsigned char *buf,
                          unsigned int len,
                          char *out,
                          size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* DTLS_PREDICATE_ADAPTER_H */
