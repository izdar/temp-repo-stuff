// ssh_predicate_adapter.h
#ifndef SSH_PREDICATE_ADAPTER_H
#define SSH_PREDICATE_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * SSH predicate adapter for request/response → "k=v ..." lines.
 *
 * This is deliberately conservative: it only parses enough structure
 * to set the enum tags (request=..., response=...) and a few scalars.
 * Other booleans are filled in with safe defaults for now.
 *
 * It matches the spec:
 *
 *   enum request { requestNotSet, c2s_kexinit, ... };
 *   enum response { responseNotSet, s2c_kexinit, ... };
 *
 * plus scalar fields:
 *
 *   bool encrypted, mac_ok, hostkey_present, sig_ok;
 *   int  pkt_len, pad_len, chan_data_len, auth_attempts;
 *   bool is_auth_method_none;
 *
 * Lines look like:
 *
 *   "request=c2s_userauth_request response=responseNotSet "
 *   "encrypted=false mac_ok=true hostkey_present=false sig_ok=false "
 *   "pkt_len=120 pad_len=8 chan_data_len=0 "
 *   "auth_attempts=1 is_auth_method_none=false"
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Reset per-session adapter state (NEWKEYS seen, auth_attempts, etc.). */
void ssh_predicate_reset_session(void);

/* Build predicate line for a client→server packet ("request=..."). */
void ssh_build_request_pred_line(const unsigned char *buf,
                                 unsigned int len,
                                 char *out,
                                 size_t out_sz);

/* Build predicate line for a server→client packet ("response=..."). */
void ssh_build_response_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* SSH_PREDICATE_ADAPTER_H */
