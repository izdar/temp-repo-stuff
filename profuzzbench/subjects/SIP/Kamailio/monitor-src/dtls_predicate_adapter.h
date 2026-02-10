// dtls_predicate_adapter.h
#ifndef DTLS_PREDICATE_ADAPTER_H
#define DTLS_PREDICATE_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * DTLS predicate adapter for DTLS records → "k=v ..." lines.
 *
 * This parses DTLS 1.2 record structure to extract:
 * - Content type (handshake, alert, CCS, application data)
 * - Message type (ClientHello, ServerHello, etc.)
 * - Epoch (for tracking encryption boundaries)
 * - Encryption status
 *
 * It matches the spec:
 *
 *   enum request { requestNotSet, c2s_ClientHello, c2s_ClientHello_with_cookie, ... };
 *   enum response { responseNotSet, s2c_HelloVerifyRequest, s2c_ServerHello, ... };
 *
 * plus scalar fields:
 *
 *   int content_type, handshake_type;
 *   int alert_level, alert_description;
 *   bool cookie_present, cookie_valid;
 *   bool encrypted, mac_ok;
 *   bool handshake_complete;
 *   int epoch, sequence;
 *   int record_length, fragment_length;
 *   bool cipher_negotiated;
 *
 * Lines look like:
 *
 *   "request=c2s_ClientHello response=responseNotSet "
 *   "content_type=22 handshake_type=1 "
 *   "cookie_present=false cookie_valid=false "
 *   "encrypted=false mac_ok=true epoch=0 "
 *   "handshake_complete=false cipher_negotiated=false"
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Reset per-session adapter state (cookie exchange, handshake state, etc.). */
void dtls_predicate_reset_session(void);

/* Build predicate line for a DTLS record. 
 * The direction (client/server) is inferred from message type and session state.
 * 
 * Parameters:
 *   buf: Raw DTLS record (13-byte header + payload)
 *   len: Total record length
 *   out: Output buffer for predicate line
 *   out_sz: Size of output buffer
 */
void dtls_build_pred_line(const unsigned char *buf,
                          unsigned int len,
                          char *out,
                          size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* DTLS_PREDICATE_ADAPTER_H */