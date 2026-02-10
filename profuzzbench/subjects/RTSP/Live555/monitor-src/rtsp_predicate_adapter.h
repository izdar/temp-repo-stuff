// rtsp_predicate_adapter.h
#ifndef RTSP_PREDICATE_ADAPTER_H
#define RTSP_PREDICATE_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * RTSP predicate adapter for Live555 fuzzing.
 *
 * Maps RTSP request/response pairs to predicates matching rtsp.txt spec.
 *
 * Enums:
 *   - rtsp_method: mNotSet, OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, etc.
 *   - status_class: scNotSet, INFO, SUCCESS, REDIRECT, CLIENT_ERR, SERVER_ERR
 *
 * Scalars:
 *   - req_cseq, resp_cseq, resp_status_code
 *   - setup_success_count, play_success_count
 *
 * Booleans:
 *   - req_malformed, resp_malformed, cseq_match
 *   - req_has_session, resp_has_session, session_established
 *   - session_id_match, session_id_changed
 *   - teardown_for_existing_session, teardown_without_session
 *   - transport_req_udp, transport_req_tcp, transport_resp_udp, transport_resp_tcp
 *   - transport_client_ports_present, transport_server_ports_present
 *   - all_tracks_setup, keepalive_getparam, keepalive_failed
 *   - timeout
 *   - transport_resp_port_zero  (NEW: response Transport has port value 0)
 *   - resp_cseq_valid           (NEW: response CSeq is valid decimal integer)
 *   - req_uri_in_session        (NEW: request URI was SETUPed in current session)
 *   - resp_empty                (NEW: server sent zero-length response)
 *
 * Output format: "key=value key=value ..."
 *
 * Bug fixes over original adapter:
 *   - Method comparisons use "mXXX" prefix consistently
 *   - Status class checks use numeric code ranges, not string comparison
 *   - Both fixes restore session state tracking (setup_success_count,
 *     session_established, play_success_count) which were permanently
 *     stuck at zero/false in the original due to string mismatch.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of SETUPed URIs tracked per session. */
#define RTSP_MAX_SETUP_URIS 32

/* Reset per-session adapter state (session ID, counters, URI set, etc.). */
void rtsp_predicate_reset_session(void);

/* Build predicate line for a client->server RTSP request. */
void rtsp_build_request_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz);

/* Build predicate line for a server->client RTSP response. */
void rtsp_build_response_pred_line(const unsigned char *buf,
                                   unsigned int len,
                                   char *out,
                                   size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* RTSP_PREDICATE_ADAPTER_H */
