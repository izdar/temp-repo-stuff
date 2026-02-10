// dtls_predicate_adapter.c
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

/* Parse DTLS record header.
 * Returns: content_type, or 0 on error
 * Sets: epoch, sequence, record_length via pointers
 */
static uint8_t dtls_parse_record_header(const unsigned char *buf,
                                        unsigned int len,
                                        uint16_t *epoch_out,
                                        uint64_t *sequence_out,
                                        uint16_t *record_len_out)
{
    if (!buf || len < 13) return 0;
    
    uint8_t content_type = buf[0];
    // buf[1-2]: version (should be DTLS 1.2 = 0xFEFD)
    uint16_t epoch = read_u16_be(&buf[3]);
    // Sequence number is 6 bytes (buf[5-10]), we'll just use lower 32 bits
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

/* Parse handshake message type and check if encrypted.
 * Returns: message_type, or 0xFF if encrypted/malformed
 */
static uint8_t dtls_parse_handshake_type(const unsigned char *payload,
                                         uint16_t record_length,
                                         bool *encrypted_out)
{
    if (!payload || record_length < 12) {
        *encrypted_out = false;
        return 0xFF; // malformed
    }
    
    uint8_t hs_msg_type = payload[0];
    // Fragment length is at bytes 9-11 (3 bytes)
    uint32_t frag_length = read_u24_be(&payload[9]);
    
    // Heuristic: if record_length - frag_length == 12, it's unencrypted
    if (record_length >= frag_length + 12 && 
        record_length - frag_length == 12) {
        *encrypted_out = false;
        return hs_msg_type;
    } else {
        *encrypted_out = true;
        return 0xFF; // encrypted, unknown type
    }
}

/* --- Mapping message type → request/response enum strings --- */

static const char *dtls_request_enum_for_type(uint8_t content_type,
                                               uint8_t message_type)
{
    if (content_type == DTLS_CT_HANDSHAKE) {
        switch (message_type) {
            case DTLS_MT_CLIENT_HELLO:
                if (g_cookie_exchange_done) {
                    return "c2s_ClientHello_with_cookie";
                } else {
                    return "c2s_ClientHello";
                }
            case DTLS_MT_CLIENT_KEY_EXCHANGE:
                return "c2s_ClientKeyExchange";
            case DTLS_MT_CERTIFICATE_VERIFY:
                return "c2s_CertificateVerify";
            case DTLS_MT_CERTIFICATE:
                // Could be client certificate
                if (g_certificate_request_sent) {
                    return "c2s_Certificate";
                }
                break;
            case DTLS_MT_FINISHED:
                if (g_client_ccs_received && !g_client_finished_received) {
                    return "c2s_Finished";
                }
                break;
        }
    } else if (content_type == DTLS_CT_CHANGE_CIPHER_SPEC) {
        if (g_client_key_exchange_received && !g_client_ccs_received) {
            return "c2s_ChangeCipherSpec";
        }
    } else if (content_type == DTLS_CT_ALERT) {
        return "c2s_Alert";
    } else if (content_type == DTLS_CT_APPLICATION_DATA) {
        return "c2s_ApplicationData";
    }
    
    return "requestNotSet";
}

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
                if (g_server_ccs_sent && !g_server_finished_sent) {
                    return "s2c_Finished";
                }
                break;
        }
    } else if (content_type == DTLS_CT_CHANGE_CIPHER_SPEC) {
        if (g_client_finished_received && !g_server_ccs_sent) {
            return "s2c_ChangeCipherSpec";
        }
    } else if (content_type == DTLS_CT_ALERT) {
        return "s2c_Alert";
    } else if (content_type == DTLS_CT_APPLICATION_DATA) {
        return "s2c_ApplicationData";
    }
    
    return "responseNotSet";
}

/* --- Public builder --- */

void dtls_build_pred_line(const unsigned char *buf,
                          unsigned int len,
                          char *out,
                          size_t out_sz)
{
    if (!g_session_initialized) {
        dtls_predicate_reset_session();
    }
    
    if (!buf || len < 13 || !out || out_sz == 0) {
        snprintf(out, out_sz, "request=requestNotSet response=responseNotSet");
        return;
    }
    
    /* Parse DTLS record header */
    uint16_t epoch = 0;
    uint64_t sequence = 0;
    uint16_t record_length = 0;
    uint8_t content_type = dtls_parse_record_header(buf, len, &epoch, &sequence, &record_length);
    
    if (content_type == 0 || len < 13 + record_length) {
        snprintf(out, out_sz, "request=requestNotSet response=responseNotSet");
        return;
    }
    
    const unsigned char *payload = buf + 13;
    uint8_t message_type = 0;
    bool encrypted = (epoch > 0);  // Simple heuristic
    bool mac_ok = true;  // Assume valid for received records
    
    /* Parse message type based on content type */
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
            message_type = payload[1]; // alert description
        } else {
            encrypted = true;
            message_type = 0xFF;
        }
    } else if (content_type == DTLS_CT_APPLICATION_DATA) {
        encrypted = true;
        message_type = 0xFF;
    }
    
    /* Determine direction and update state */
    const char *req_enum = dtls_request_enum_for_type(content_type, message_type);
    const char *resp_enum = dtls_response_enum_for_type(content_type, message_type);
    
    /* Update session state based on detected messages */
    if (content_type == DTLS_CT_HANDSHAKE) {
        switch (message_type) {
            case DTLS_MT_HELLO_VERIFY_REQUEST:
                g_cookie_exchange_done = true;
                break;
            case DTLS_MT_SERVER_HELLO:
                g_server_hello_sent = true;
                g_cipher_negotiated = true;
                break;
            case DTLS_MT_CERTIFICATE_REQUEST:
                g_certificate_request_sent = true;
                break;
            case DTLS_MT_SERVER_HELLO_DONE:
                g_server_hello_done_sent = true;
                break;
            case DTLS_MT_CLIENT_KEY_EXCHANGE:
                g_client_key_exchange_received = true;
                break;
            case DTLS_MT_FINISHED:
                if (strcmp(req_enum, "c2s_Finished") == 0) {
                    g_client_finished_received = true;
                } else if (strcmp(resp_enum, "s2c_Finished") == 0) {
                    g_server_finished_sent = true;
                }
                if (g_client_finished_received && g_server_finished_sent) {
                    g_handshake_complete = true;
                }
                break;
        }
    } else if (content_type == DTLS_CT_CHANGE_CIPHER_SPEC) {
        if (strcmp(req_enum, "c2s_ChangeCipherSpec") == 0) {
            g_client_ccs_received = true;
        } else if (strcmp(resp_enum, "s2c_ChangeCipherSpec") == 0) {
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
        } else {
            cookie_present = false;
            cookie_valid = false;
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
}