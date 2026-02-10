// dtls_predicate_adapter.c - FIXED: Split into separate C2S/S2C builders
// 
// ROOT CAUSE FIX: The old single dtls_build_pred_line() was assigned to BOTH
// build_request_pred_line and build_response_pred_line function pointers.
// It tried to infer direction from packet bytes, but fuzzed C2S packets could
// decode as server message types (e.g., s2c_ApplicationData), triggering
// false LTL violations that stuck permanently via H() operator.
//
// FIX: Two separate functions. The send loop already knows the direction
// (request = before send, response = after recv), so we trust that and
// ONLY populate the enum for the known direction.

#include "dtls_predicate_adapter.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* --- DTLS Constants (from RFC 6347) --- */

#define DTLS_CT_CHANGE_CIPHER_SPEC  20
#define DTLS_CT_ALERT               21
#define DTLS_CT_HANDSHAKE           22
#define DTLS_CT_APPLICATION_DATA    23
#define DTLS_CT_HEARTBEAT           24

#define DTLS_MT_HELLO_REQUEST         0
#define DTLS_MT_CLIENT_HELLO          1
#define DTLS_MT_SERVER_HELLO          2
#define DTLS_MT_HELLO_VERIFY_REQUEST  3
#define DTLS_MT_CERTIFICATE          11
#define DTLS_MT_SERVER_KEY_EXCHANGE  12
#define DTLS_MT_CERTIFICATE_REQUEST  13
#define DTLS_MT_SERVER_HELLO_DONE    14
#define DTLS_MT_CERTIFICATE_VERIFY   15
#define DTLS_MT_CLIENT_KEY_EXCHANGE  16
#define DTLS_MT_FINISHED             20

#define DTLS_ALERT_LEVEL_WARNING  1
#define DTLS_ALERT_LEVEL_FATAL    2

/* --- Static per-session state --- */

static bool g_cookie_exchange_done = false;
static bool g_server_hello_sent = false;
static bool g_server_hello_done_sent = false;
static bool g_client_key_exchange_received = false;
static bool g_client_ccs_received = false;
static bool g_server_ccs_sent = false;
static bool g_client_finished_received = false;
static bool g_server_finished_sent = false;
static bool g_handshake_complete = false;
static bool g_cipher_negotiated = false;
static bool g_certificate_request_sent = false;
static bool g_session_initialized = false;

static unsigned int g_msg_id = 0;

void dtls_predicate_reset_session(void)
{
    g_cookie_exchange_done = false;
    g_server_hello_sent = false;
    g_server_hello_done_sent = false;
    g_client_key_exchange_received = false;
    g_client_ccs_received = false;
    g_server_ccs_sent = false;
    g_client_finished_received = false;
    g_server_finished_sent = false;
    g_handshake_complete = false;
    g_cipher_negotiated = false;
    g_certificate_request_sent = false;
    g_session_initialized = true;
    g_msg_id = 0;
}

/* --- Helper: Append packet trace --- */
static void append_packet_trace(char *out, size_t out_sz,
                                const unsigned char *buf,
                                unsigned int len,
                                const char *direction)
{
    g_msg_id++;
    
    size_t current_len = strlen(out);
    size_t remaining = out_sz - current_len;
    
    if (remaining < 50) return;
    
    int written = snprintf(out + current_len, remaining,
                          " msg_id=%u dir=%s trace=",
                          g_msg_id, direction);
    
    if (written < 0 || (size_t)written >= remaining) return;
    
    current_len += written;
    remaining -= written;
    
    unsigned int trace_len = len;
    if (trace_len > 256) trace_len = 256;
    
    for (unsigned int i = 0; i < trace_len && remaining > 2; i++) {
        snprintf(out + current_len, remaining, "%02x", buf[i]);
        current_len += 2;
        remaining -= 2;
    }
}

/* --- Helpers --- */

static inline uint32_t read_u16_be(const unsigned char *p)
{
    return ((uint32_t)p[0] << 8) | ((uint32_t)p[1]);
}

static inline uint32_t read_u24_be(const unsigned char *p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2]);
}

/* Parse DTLS record header */
static uint8_t dtls_parse_record_header(const unsigned char *buf,
                                        unsigned int len,
                                        uint16_t *epoch_out,
                                        uint64_t *sequence_out,
                                        uint16_t *record_len_out)
{
    if (!buf || len < 13) return 0;
    
    uint8_t content_type = buf[0];
    uint16_t epoch = read_u16_be(&buf[3]);
    uint64_t sequence = 0;
    for (int i = 0; i < 6; i++) {
        sequence = (sequence << 8) | buf[5 + i];
    }
    uint16_t record_length = read_u16_be(&buf[11]);
    
    if (epoch_out) *epoch_out = epoch;
    if (sequence_out) *sequence_out = sequence;
    if (record_len_out) *record_len_out = record_length;
    
    return content_type;
}

/* Parse handshake message type with encryption detection */
static uint8_t dtls_parse_handshake_type(const unsigned char *payload,
                                         uint16_t record_length,
                                         bool *encrypted_out)
{
    if (!payload || record_length < 12) {
        *encrypted_out = false;
        return 0xFF;
    }
    
    uint8_t hs_msg_type = payload[0];
    uint32_t msg_length = read_u24_be(&payload[1]);
    uint32_t frag_offset = read_u24_be(&payload[6]);
    uint32_t frag_length = read_u24_be(&payload[9]);
    
    if (record_length == 12 + frag_length && 
        frag_offset == 0 &&
        msg_length == frag_length) {
        *encrypted_out = false;
        return hs_msg_type;
    } else {
        *encrypted_out = true;
        return 0xFF;
    }
}

/* --- Direction-specific enum mapping --- */

/* Map message type to C2S request enum. Only called for known C2S packets. */
static const char *dtls_request_enum_for_type(uint8_t content_type,
                                               uint8_t message_type)
{
    if (content_type == DTLS_CT_HANDSHAKE) {
        switch (message_type) {
            case DTLS_MT_CLIENT_HELLO:
                return g_cookie_exchange_done
                    ? "c2s_ClientHello_with_cookie"
                    : "c2s_ClientHello";
            case DTLS_MT_CLIENT_KEY_EXCHANGE:
                return "c2s_ClientKeyExchange";
            case DTLS_MT_CERTIFICATE_VERIFY:
                return "c2s_CertificateVerify";
            case DTLS_MT_CERTIFICATE:
                if (g_certificate_request_sent)
                    return "c2s_Certificate";
                break;
            case DTLS_MT_FINISHED:
                if (g_client_ccs_received && !g_client_finished_received)
                    return "c2s_Finished";
                break;
        }
    } else if (content_type == DTLS_CT_CHANGE_CIPHER_SPEC) {
        if (g_client_key_exchange_received && !g_client_ccs_received)
            return "c2s_ChangeCipherSpec";
    } else if (content_type == DTLS_CT_ALERT) {
        return "c2s_Alert";
    } else if (content_type == DTLS_CT_APPLICATION_DATA) {
        return "c2s_ApplicationData";
    }
    
    return "requestNotSet";
}

/* Map message type to S2C response enum. Only called for known S2C packets. */
static const char *dtls_response_enum_for_type(uint8_t content_type,
                                                uint8_t message_type)
{
    if (content_type == DTLS_CT_HANDSHAKE) {
        switch (message_type) {
            case DTLS_MT_HELLO_VERIFY_REQUEST:
                return "s2c_HelloVerifyRequest";
            case DTLS_MT_SERVER_HELLO:
                return "s2c_ServerHello";
            case DTLS_MT_CERTIFICATE:
                return "s2c_Certificate";
            case DTLS_MT_SERVER_KEY_EXCHANGE:
                return "s2c_ServerKeyExchange";
            case DTLS_MT_CERTIFICATE_REQUEST:
                return "s2c_CertificateRequest";
            case DTLS_MT_SERVER_HELLO_DONE:
                return "s2c_ServerHelloDone";
            case DTLS_MT_FINISHED:
                if (g_server_ccs_sent && !g_server_finished_sent)
                    return "s2c_Finished";
                break;
        }
    } else if (content_type == DTLS_CT_CHANGE_CIPHER_SPEC) {
        if (g_client_finished_received && !g_server_ccs_sent)
            return "s2c_ChangeCipherSpec";
    } else if (content_type == DTLS_CT_ALERT) {
        return "s2c_Alert";
    } else if (content_type == DTLS_CT_APPLICATION_DATA) {
        return "s2c_ApplicationData";
    }
    
    return "responseNotSet";
}

/* --- Internal: shared parsing + predicate assembly --- */

typedef enum {
    DTLS_DIR_C2S,
    DTLS_DIR_S2C
} dtls_direction_t;

static void dtls_build_pred_line_internal(const unsigned char *buf,
                                          unsigned int len,
                                          char *out,
                                          size_t out_sz,
                                          dtls_direction_t direction)
{
    if (!g_session_initialized) {
        dtls_predicate_reset_session();
    }
    
    const char *dir_str = (direction == DTLS_DIR_C2S) ? "C2S" : "S2C";
    
    /* Error path: unparseable packet */
    if (!buf || len < 13 || !out || out_sz == 0) {
        snprintf(out, out_sz,
                 "request=requestNotSet response=responseNotSet "
                 "content_type=0 handshake_type=0 "
                 "cookie_present=false cookie_valid=false "
                 "encrypted=false mac_ok=true "
                 "epoch=0 sequence=0 "
                 "record_length=0 fragment_length=0 "
                 "handshake_complete=%s cipher_negotiated=%s "
                 "alert_level=0 alert_description=0",
                 g_handshake_complete ? "true" : "false",
                 g_cipher_negotiated ? "true" : "false");
        if (buf && len > 0) {
            append_packet_trace(out, out_sz, buf, len, dir_str);
        }
        return;
    }
    
    /* Parse DTLS record header */
    uint16_t epoch = 0;
    uint64_t sequence = 0;
    uint16_t record_length = 0;
    uint8_t content_type = dtls_parse_record_header(buf, len, &epoch, &sequence, &record_length);
    
    if (content_type == 0 || len < 13 + record_length) {
        snprintf(out, out_sz,
                 "request=requestNotSet response=responseNotSet "
                 "content_type=0 handshake_type=0 "
                 "cookie_present=false cookie_valid=false "
                 "encrypted=false mac_ok=true "
                 "epoch=0 sequence=0 "
                 "record_length=0 fragment_length=0 "
                 "handshake_complete=%s cipher_negotiated=%s "
                 "alert_level=0 alert_description=0",
                 g_handshake_complete ? "true" : "false",
                 g_cipher_negotiated ? "true" : "false");
        append_packet_trace(out, out_sz, buf, len, dir_str);
        return;
    }
    
    const unsigned char *payload = buf + 13;
    uint8_t message_type = 0;
    bool encrypted = (epoch > 0);
    bool mac_ok = true;
    
    /* MAC heuristic for encrypted data */
    if (encrypted) {
        if (record_length == 0 || record_length > 16384) {
            mac_ok = false;
        }
    }
    
    /* Parse message type based on content type.
     *
     * DIRECTION-AWARE ENCRYPTION DETECTION:
     *
     * C2S (fuzzed data): Trust epoch ONLY. The structural heuristic
     *   (record_length == 12 + frag_length) is designed for well-formed
     *   messages. The fuzzer mutates length fields constantly, causing
     *   the check to fail on plaintext packets and falsely set
     *   encrypted=true. With epoch=0 this fires
     *   H((encrypted=true) -> (epoch>0)) and the violation sticks
     *   permanently via H().
     *
     * S2C (server responses): Use both epoch AND structural detection.
     *   The server should send well-formed messages, so structural
     *   mismatches genuinely indicate encryption or corruption.
     */
    if (direction == DTLS_DIR_C2S) {
        /* --- C2S: epoch-only encryption, best-effort type parsing --- */
        if (content_type == DTLS_CT_HANDSHAKE) {
            if (!encrypted && record_length >= 12) {
                /* Plaintext (epoch=0): read handshake type directly.
                 * Don't validate structure — the fuzzer breaks it. */
                message_type = payload[0];
            } else {
                /* Encrypted (epoch>0): can't read through ciphertext */
                message_type = 0xFF;
            }
        } else if (content_type == DTLS_CT_CHANGE_CIPHER_SPEC) {
            if (!encrypted && record_length >= 1) {
                message_type = payload[0];
            } else {
                message_type = 0xFF;
            }
        } else if (content_type == DTLS_CT_ALERT) {
            if (!encrypted && record_length >= 2) {
                message_type = payload[1];
            } else {
                message_type = 0xFF;
            }
        } else if (content_type == DTLS_CT_APPLICATION_DATA) {
            encrypted = true;
            message_type = 0xFF;
        }
        /* encrypted remains epoch-based for all C2S types */
    } else {
        /* --- S2C: full structural + epoch detection --- */
        if (content_type == DTLS_CT_HANDSHAKE) {
            bool hs_encrypted = false;
            message_type = dtls_parse_handshake_type(payload, record_length, &hs_encrypted);
            encrypted = hs_encrypted || (epoch > 0);
        } else if (content_type == DTLS_CT_CHANGE_CIPHER_SPEC) {
            if (record_length == 1) {
                message_type = payload[0];
                encrypted = false;
            } else {
                encrypted = true;
                message_type = 0xFF;
            }
        } else if (content_type == DTLS_CT_ALERT) {
            if (record_length == 2 && !encrypted) {
                message_type = payload[1];
                encrypted = false;
            } else {
                encrypted = true;
                message_type = 0xFF;
            }
        } else if (content_type == DTLS_CT_APPLICATION_DATA) {
            encrypted = true;
            message_type = 0xFF;
        }
    }
    
    /* --- DIRECTION-AWARE enum assignment --- */
    const char *req_enum = "requestNotSet";
    const char *resp_enum = "responseNotSet";
    
    if (direction == DTLS_DIR_C2S) {
        /* We KNOW this is C2S. Only set request enum. */
        req_enum = dtls_request_enum_for_type(content_type, message_type);
        /* resp_enum stays "responseNotSet" — no matter what the bytes look like */
    } else {
        /* We KNOW this is S2C. Only set response enum. */
        resp_enum = dtls_response_enum_for_type(content_type, message_type);
        /* req_enum stays "requestNotSet" — no matter what the bytes look like */
    }
    
    /* --- DIRECTION-AWARE state updates --- */
    if (content_type == DTLS_CT_HANDSHAKE) {
        switch (message_type) {
            case DTLS_MT_CLIENT_HELLO:
                /* Only update if this is actually C2S */
                break;
            case DTLS_MT_HELLO_VERIFY_REQUEST:
                if (direction == DTLS_DIR_S2C)
                    g_cookie_exchange_done = true;
                break;
            case DTLS_MT_SERVER_HELLO:
                if (direction == DTLS_DIR_S2C) {
                    g_server_hello_sent = true;
                    g_cipher_negotiated = true;
                }
                break;
            case DTLS_MT_CERTIFICATE_REQUEST:
                if (direction == DTLS_DIR_S2C)
                    g_certificate_request_sent = true;
                break;
            case DTLS_MT_SERVER_HELLO_DONE:
                if (direction == DTLS_DIR_S2C)
                    g_server_hello_done_sent = true;
                break;
            case DTLS_MT_CLIENT_KEY_EXCHANGE:
                if (direction == DTLS_DIR_C2S)
                    g_client_key_exchange_received = true;
                break;
            case DTLS_MT_FINISHED:
                if (direction == DTLS_DIR_C2S) {
                    g_client_finished_received = true;
                } else {
                    g_server_finished_sent = true;
                }
                if (g_client_finished_received && g_server_finished_sent) {
                    g_handshake_complete = true;
                }
                break;
        }
    } else if (content_type == DTLS_CT_CHANGE_CIPHER_SPEC) {
        if (direction == DTLS_DIR_C2S) {
            g_client_ccs_received = true;
        } else {
            g_server_ccs_sent = true;
        }
    }
    
    /* Extract additional predicates */
    bool cookie_present = false;
    bool cookie_valid = false;
    
    if (content_type == DTLS_CT_HANDSHAKE && message_type == DTLS_MT_CLIENT_HELLO) {
        if (g_cookie_exchange_done) {
            cookie_present = true;
            cookie_valid = true;
        }
    }
    
    int alert_level = 0;
    int alert_description = 0;
    if (content_type == DTLS_CT_ALERT && record_length >= 2 && !encrypted) {
        alert_level = payload[0];
        alert_description = payload[1];
    }
    
    int fragment_length = 0;
    if (content_type == DTLS_CT_HANDSHAKE && record_length >= 12 && !encrypted) {
        fragment_length = read_u24_be(&payload[9]);
    }
    
    /* Build predicate line */
    snprintf(out, out_sz,
             "request=%s response=%s "
             "content_type=%d handshake_type=%d "
             "cookie_present=%s cookie_valid=%s "
             "encrypted=%s mac_ok=%s "
             "epoch=%u sequence=%llu "
             "record_length=%u fragment_length=%d "
             "handshake_complete=%s cipher_negotiated=%s "
             "alert_level=%d alert_description=%d",
             req_enum,
             resp_enum,
             content_type,
             message_type,
             cookie_present ? "true" : "false",
             cookie_valid ? "true" : "false",
             encrypted ? "true" : "false",
             mac_ok ? "true" : "false",
             epoch,
             (unsigned long long)sequence,
             record_length,
             fragment_length,
             g_handshake_complete ? "true" : "false",
             g_cipher_negotiated ? "true" : "false",
             alert_level,
             alert_description);
    
    append_packet_trace(out, out_sz, buf, len, dir_str);
}

/* --- Public API --- */

void dtls_build_request_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz)
{
    dtls_build_pred_line_internal(buf, len, out, out_sz, DTLS_DIR_C2S);
}

void dtls_build_response_pred_line(const unsigned char *buf,
                                   unsigned int len,
                                   char *out,
                                   size_t out_sz)
{
    dtls_build_pred_line_internal(buf, len, out, out_sz, DTLS_DIR_S2C);
}

/* Backward-compatible wrapper — DEPRECATED, do not use in new code.
 * Kept so old code that references dtls_build_pred_line still compiles,
 * but it falls back to the old ambiguous behavior. */
void dtls_build_pred_line(const unsigned char *buf,
                          unsigned int len,
                          char *out,
                          size_t out_sz)
{
    /* Best-effort: parse record to guess direction, then delegate.
     * This is the OLD behavior and is only here for link compatibility. */
    if (!g_session_initialized) {
        dtls_predicate_reset_session();
    }
    
    /* Try to guess direction from content — the exact bug we're fixing,
     * but kept for any callers that haven't been updated yet. */
    if (buf && len >= 13) {
        const unsigned char *payload = buf + 13;
        uint16_t record_length = read_u16_be(&buf[11]);
        uint8_t content_type = buf[0];
        
        if (content_type == DTLS_CT_HANDSHAKE && len >= 13 + 12 && record_length >= 12) {
            uint8_t hs_type = payload[0];
            switch (hs_type) {
                case DTLS_MT_CLIENT_HELLO:
                case DTLS_MT_CLIENT_KEY_EXCHANGE:
                case DTLS_MT_CERTIFICATE_VERIFY:
                    dtls_build_pred_line_internal(buf, len, out, out_sz, DTLS_DIR_C2S);
                    return;
                case DTLS_MT_HELLO_VERIFY_REQUEST:
                case DTLS_MT_SERVER_HELLO:
                case DTLS_MT_SERVER_KEY_EXCHANGE:
                case DTLS_MT_SERVER_HELLO_DONE:
                case DTLS_MT_CERTIFICATE_REQUEST:
                    dtls_build_pred_line_internal(buf, len, out, out_sz, DTLS_DIR_S2C);
                    return;
            }
        }
    }
    
    /* Fallback: unknown direction — use C2S so we don't set any response= values
     * that could trigger false violations on H() rules. */
    dtls_build_pred_line_internal(buf, len, out, out_sz, DTLS_DIR_C2S);
}
