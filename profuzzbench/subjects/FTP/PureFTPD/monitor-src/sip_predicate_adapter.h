// sip_predicate_adapter.h
#ifndef SIP_PREDICATE_ADAPTER_H
#define SIP_PREDICATE_ADAPTER_H

#include <stddef.h>
#include <stdbool.h>

/*
 * SIP predicate adapter for Kamailio semantic monitoring.
 *
 * Tracks SIP protocol state and generates predicate lines matching
 * the kamailio.txt LTL specification.
 *
 * Usage in aflnet.c extract functions:
 *   - Call sip_build_request_pred_line() for client requests
 *   - Call sip_build_response_pred_line() for server responses
 *   - Call sip_predicate_reset_session() on new connection/dialog
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Reset adapter state for a new SIP session/dialog */
void sip_predicate_reset_session(void);

/* Build predicate line from SIP request (client → server) */
void sip_build_request_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz);

/* Build predicate line from SIP response (server → client) */
void sip_build_response_pred_line(const unsigned char *buf,
                                   unsigned int len,
                                   char *out,
                                   size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* SIP_PREDICATE_ADAPTER_H */
