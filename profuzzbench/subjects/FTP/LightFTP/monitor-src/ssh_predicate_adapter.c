// ssh_predicate_adapter.c
#include "ssh_predicate_adapter.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* --- Static per-session state --- */

static int  g_auth_attempts = 0;
static bool g_last_auth_method_none = false;
static bool g_seen_client_newkeys = false;
static bool g_seen_server_newkeys = false;
static bool g_session_initialized = false;

static unsigned int g_msg_id = 0;

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

void ssh_predicate_reset_session(void)
{
    g_auth_attempts = 0;
    g_last_auth_method_none = false;
    g_seen_client_newkeys = false;
    g_seen_server_newkeys = false;
    g_session_initialized = true;
}

/* --- Helpers --- */

static inline uint8_t get_msg_type(const unsigned char *buf, unsigned int len)
{
    if (!buf || len == 0) return 0;
    return buf[0];
}

/* Detect plain-text SSH identification banner "SSH-2.0-..." etc. */
static inline bool is_ssh_banner(const unsigned char *buf, unsigned int len)
{
    if (!buf || len < 4) return false;
    return (memcmp(buf, "SSH-", 4) == 0);
}

static bool read_u32_be(const unsigned char *buf, unsigned int len,
                        unsigned int *off, uint32_t *out)
{
    if (*off + 4 > len) return false;
    const unsigned char *p = buf + *off;
    *out = ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
           ((uint32_t)p[3]);
    *off += 4;
    return true;
}

static bool read_ssh_string(const unsigned char *buf, unsigned int len,
                            unsigned int *off,
                            const unsigned char **str,
                            uint32_t *str_len)
{
    uint32_t l;
    if (!read_u32_be(buf, len, off, &l)) return false;
    if (*off + l > len) return false;
    *str = buf + *off;
    *str_len = l;
    *off += l;
    return true;
}

static bool ssh_parse_userauth_method(const unsigned char *buf,
                                      unsigned int len,
                                      bool *is_none_out)
{
    /* RFC 4252: USERAUTH_REQUEST
       byte      SSH_MSG_USERAUTH_REQUEST (50)
       string    user name
       string    service name
       string    method name
       [method-specific fields...]
    */
    if (len < 2) return false;
    unsigned int off = 1;
    const unsigned char *s;
    uint32_t slen;

    // username
    if (!read_ssh_string(buf, len, &off, &s, &slen)) return false;
    // service
    if (!read_ssh_string(buf, len, &off, &s, &slen)) return false;
    // method name
    if (!read_ssh_string(buf, len, &off, &s, &slen)) return false;

    if (slen == 4 && memcmp(s, "none", 4) == 0) {
        *is_none_out = true;
    } else {
        *is_none_out = false;
    }
    return true;
}

static int ssh_parse_channel_data_len(const unsigned char *buf,
                                      unsigned int len)
{
    /* RFC 4254: CHANNEL_DATA
       byte      SSH_MSG_CHANNEL_DATA (94)
       uint32    recipient channel
       string    data
    */
    if (len < 1 + 4 + 4) return 0;
    unsigned int off = 1;
    uint32_t tmp, data_len;

    if (!read_u32_be(buf, len, &off, &tmp)) return 0;         // recipient
    if (!read_u32_be(buf, len, &off, &data_len)) return 0;    // data length
    if (off + data_len > len) return 0;
    return (int)data_len;
}

/* Decide whether payload should be considered "encrypted" for this step.
   For now we treat messages after both NEWKEYS as encrypted. */
static bool ssh_encrypted_now(void)
{
    return (g_seen_client_newkeys && g_seen_server_newkeys);
}

/* --- Mapping message type → request/response enum strings --- */

static const char *ssh_request_enum_for_type(uint8_t t)
{
    switch (t) {
        case 20: return "c2s_kexinit";              // SSH_MSG_KEXINIT
        case 21: return "c2s_newkeys_request";      // SSH_MSG_NEWKEYS
        case 30: return "c2s_kexdh_init";           // SSH_MSG_KEXDH_INIT / ECDH_INIT
        case 5:  return "c2s_service_request_userauth"; // SSH_MSG_SERVICE_REQUEST (assume ssh-userauth)
        case 50: return "c2s_userauth_request";     // SSH_MSG_USERAUTH_REQUEST
        case 90: return "c2s_channel_open";         // SSH_MSG_CHANNEL_OPEN
        case 93: return "c2s_channel_window_adjust";// SSH_MSG_CHANNEL_WINDOW_ADJUST
        case 94: return "c2s_channel_data";         // SSH_MSG_CHANNEL_DATA
        case 96: return "c2s_channel_eof";          // SSH_MSG_CHANNEL_EOF
        case 97: return "c2s_channel_close";        // SSH_MSG_CHANNEL_CLOSE
        case 98: return "c2s_channel_request";      // SSH_MSG_CHANNEL_REQUEST
        case 80: return "c2s_global_request";       // SSH_MSG_GLOBAL_REQUEST
        case 1:  return "c2s_disconnect";           // SSH_MSG_DISCONNECT
        case 3:  return "c2s_unimplemented";        // SSH_MSG_UNIMPLEMENTED
        default: return "requestNotSet";
    }
}

static const char *ssh_response_enum_for_type(uint8_t t)
{
    switch (t) {
        case 20: return "s2c_kexinit";                 // SSH_MSG_KEXINIT
        case 21: return "s2c_newkeys_response";        // SSH_MSG_NEWKEYS
        case 31: return "s2c_kexdh_reply";             // SSH_MSG_KEXDH_REPLY / ECDH_REPLY
        case 6:  return "s2c_service_accept_userauth"; // SSH_MSG_SERVICE_ACCEPT (assume ssh-userauth)
        case 51: return "s2c_userauth_failure";        // SSH_MSG_USERAUTH_FAILURE
        case 52: return "s2c_userauth_success";        // SSH_MSG_USERAUTH_SUCCESS
        case 53: return "s2c_userauth_banner";         // SSH_MSG_USERAUTH_BANNER
        case 90: return "s2c_channel_open_confirmation";// SSH_MSG_CHANNEL_OPEN_CONFIRMATION
        case 92: return "s2c_channel_open_failure";    // SSH_MSG_CHANNEL_OPEN_FAILURE
        case 93: return "s2c_channel_window_adjust";   // SSH_MSG_CHANNEL_WINDOW_ADJUST
        case 94: return "s2c_channel_data";            // SSH_MSG_CHANNEL_DATA
        case 96: return "s2c_channel_eof";             // SSH_MSG_CHANNEL_EOF
        case 97: return "s2c_channel_close";           // SSH_MSG_CHANNEL_CLOSE
        case 81: return "s2c_global_request_response"; // SSH_MSG_REQUEST_SUCCESS
        case 82: return "s2c_global_request_response"; // SSH_MSG_REQUEST_FAILURE
        case 1:  return "s2c_disconnect";              // SSH_MSG_DISCONNECT
        case 3:  return "s2c_unimplemented";           // SSH_MSG_UNIMPLEMENTED
        default: return "responseNotSet";
    }
}

/* --- Public builders --- */

void ssh_build_request_pred_line(const unsigned char *buf,
                                 unsigned int len,
                                 char *out,
                                 size_t out_sz)
{
    if (!g_session_initialized) {
        ssh_predicate_reset_session();
    }

    /* In practice banners are server->client, but be robust:
       if we see a banner here, treat it as a non-packet event
       with unknown padding. */
    if (is_ssh_banner(buf, len)) {
        snprintf(out, out_sz,
                 "request=requestNotSet response=responseNotSet "
                 "encrypted=false mac_ok=true hostkey_present=false sig_ok=false "
                 "pkt_len=%d pad_len=-1 chan_data_len=0 "
                 "auth_attempts=%d is_auth_method_none=%s",
                 (int)len,
                 g_auth_attempts,
                 g_last_auth_method_none ? "true" : "false");
        return;
    }

    uint8_t t = get_msg_type(buf, len);
    const char *req_enum = ssh_request_enum_for_type(t);
    const char *resp_enum = "responseNotSet";

    bool encrypted = ssh_encrypted_now();
    bool mac_ok    = true;   // we don't verify MACs here (safe default)
    bool hostkey_present = false;
    bool sig_ok   = false;
    int  pkt_len  = (int)len;
    int  pad_len  = -1;      // unknown / not parsed
    int  chan_data_len = 0;

    /* Track NEWKEYS state */
    if (t == 21) {
        g_seen_client_newkeys = true;
        encrypted = ssh_encrypted_now();
    }

    /* Track auth attempts + "none" method */
    if (t == 50) { // USERAUTH_REQUEST
        g_auth_attempts++;
        bool is_none = false;
        if (ssh_parse_userauth_method(buf, len, &is_none)) {
            g_last_auth_method_none = is_none;
        } else {
            g_last_auth_method_none = false;
        }
    }

    /* Channel data length for request side */
    if (t == 94) { // CHANNEL_DATA
        chan_data_len = ssh_parse_channel_data_len(buf, len);
    }

    snprintf(out, out_sz,
             "request=%s response=%s "
             "encrypted=%s mac_ok=%s hostkey_present=%s sig_ok=%s "
             "pkt_len=%d pad_len=%d chan_data_len=%d "
             "auth_attempts=%d is_auth_method_none=%s",
             req_enum,
             resp_enum,
             encrypted ? "true" : "false",
             mac_ok    ? "true" : "false",
             hostkey_present ? "true" : "false",
             sig_ok ? "true" : "false",
             pkt_len,
             pad_len,
             chan_data_len,
             g_auth_attempts,
             g_last_auth_method_none ? "true" : "false");
    append_packet_trace(out, out_sz, buf, len, "C2S");
}

void ssh_build_response_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz)
{
    if (!g_session_initialized) {
        ssh_predicate_reset_session();
    }

    /* First event for a normal connection will be the server banner. */
    if (is_ssh_banner(buf, len)) {
        snprintf(out, out_sz,
                 "request=requestNotSet response=s2c_banner "
                 "encrypted=false mac_ok=true hostkey_present=false sig_ok=false "
                 "pkt_len=%d pad_len=-1 chan_data_len=0 "
                 "auth_attempts=%d is_auth_method_none=%s",
                 (int)len,
                 g_auth_attempts,
                 g_last_auth_method_none ? "true" : "false");
        return;
    }

    uint8_t t = get_msg_type(buf, len);
    const char *req_enum = "requestNotSet";
    const char *resp_enum = ssh_response_enum_for_type(t);

    bool encrypted = ssh_encrypted_now();
    bool mac_ok    = true;   // we don't verify MACs here
    bool hostkey_present = false;
    bool sig_ok   = false;
    int  pkt_len  = (int)len;
    int  pad_len  = -1;      // unknown / not parsed
    int  chan_data_len = 0;

    /* Track NEWKEYS from server */
    if (t == 21) {
        g_seen_server_newkeys = true;
        encrypted = ssh_encrypted_now();
    }

    /* KEX reply: mark hostkey/signature present.
     * (We don't parse them yet, so we set both true to avoid FPs until
     *  we implement deeper parsing.) */
    if (t == 31) { // KEXDH_REPLY / ECDH_REPLY
        hostkey_present = true;
        sig_ok = true;
    }

    /* Channel data length for response side */
    if (t == 94) { // CHANNEL_DATA
        chan_data_len = ssh_parse_channel_data_len(buf, len);
    }

    snprintf(out, out_sz,
             "request=%s response=%s "
             "encrypted=%s mac_ok=%s hostkey_present=%s sig_ok=%s "
             "pkt_len=%d pad_len=%d chan_data_len=%d "
             "auth_attempts=%d is_auth_method_none=%s",
             req_enum,
             resp_enum,
             encrypted ? "true" : "false",
             mac_ok    ? "true" : "false",
             hostkey_present ? "true" : "false",
             sig_ok ? "true" : "false",
             pkt_len,
             pad_len,
             chan_data_len,
             g_auth_attempts,
             g_last_auth_method_none ? "true" : "false");
    append_packet_trace(out, out_sz, buf, len, "S2C");
}
