// rtsp_predicate_adapter.c
#include "rtsp_predicate_adapter.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* ================================================================
 * Static per-session state
 * ================================================================ */

static char   g_session_id[256] = "";
static bool   g_session_established = false;
static int    g_setup_success_count = 0;
static int    g_play_success_count = 0;
static int    g_total_tracks = 0;
static bool   g_session_initialized = false;

/* Last request state, correlated with the next response */
static int    g_last_req_cseq = -1;
static char   g_last_req_method[32] = "mNotSet";
static bool   g_last_req_has_session = false;
static char   g_last_req_session_id[256] = "";
static bool   g_last_req_transport_udp = false;
static bool   g_last_req_transport_tcp = false;
static bool   g_last_req_client_ports = false;
static bool   g_last_req_malformed = false;
static char   g_last_req_uri[512] = "";

/* Per-session URI tracking for req_uri_in_session */
static char   g_setup_uris[RTSP_MAX_SETUP_URIS][512];
static int    g_setup_uri_count = 0;

static unsigned int g_msg_id = 0;

/* ================================================================
 * Hex trace helper (appends packet hex dump to predicate line)
 * ================================================================ */

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

/* ================================================================
 * Session reset
 * ================================================================ */

void rtsp_predicate_reset_session(void)
{
    g_session_id[0] = '\0';
    g_session_established = false;
    g_setup_success_count = 0;
    g_play_success_count = 0;
    g_last_req_cseq = -1;
    strcpy(g_last_req_method, "mNotSet");
    g_last_req_has_session = false;
    g_last_req_session_id[0] = '\0';
    g_last_req_transport_udp = false;
    g_last_req_transport_tcp = false;
    g_last_req_client_ports = false;
    g_last_req_malformed = false;
    g_last_req_uri[0] = '\0';
    g_total_tracks = 0;
    g_setup_uri_count = 0;
    memset(g_setup_uris, 0, sizeof(g_setup_uris));
    g_session_initialized = true;
}

/* ================================================================
 * Text parsing helpers
 * ================================================================ */

static inline bool starts_with(const char *str, const char *prefix)
{
    return strncasecmp(str, prefix, strlen(prefix)) == 0;
}

static inline void skip_whitespace(const char **p)
{
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

static inline void extract_line(const unsigned char *buf, unsigned int len,
                                unsigned int *offset, char *line, size_t line_sz)
{
    size_t i = 0;
    while (*offset < len && i < line_sz - 1) {
        char c = buf[(*offset)++];
        if (c == '\r') continue;
        if (c == '\n') break;
        line[i++] = c;
    }
    line[i] = '\0';
}

/* ================================================================
 * Request line parsing
 * ================================================================ */

/* Parse method from request line: "SETUP rtsp://... RTSP/1.0" */
static void parse_request_method(const char *line, char *method, size_t method_sz)
{
    const char *p = line;
    skip_whitespace(&p);

    size_t i = 0;
    while (*p && !isspace((unsigned char)*p) && i < method_sz - 1) {
        method[i++] = *p++;
    }
    method[i] = '\0';

    /* Convert to enum string format with 'm' prefix */
    if      (strcasecmp(method, "OPTIONS") == 0)       strcpy(method, "mOPTIONS");
    else if (strcasecmp(method, "DESCRIBE") == 0)      strcpy(method, "mDESCRIBE");
    else if (strcasecmp(method, "SETUP") == 0)         strcpy(method, "mSETUP");
    else if (strcasecmp(method, "PLAY") == 0)          strcpy(method, "mPLAY");
    else if (strcasecmp(method, "PAUSE") == 0)         strcpy(method, "mPAUSE");
    else if (strcasecmp(method, "TEARDOWN") == 0)      strcpy(method, "mTEARDOWN");
    else if (strcasecmp(method, "ANNOUNCE") == 0)      strcpy(method, "mANNOUNCE");
    else if (strcasecmp(method, "GET_PARAMETER") == 0) strcpy(method, "mGET_PARAMETER");
    else if (strcasecmp(method, "SET_PARAMETER") == 0) strcpy(method, "mSET_PARAMETER");
    else if (strcasecmp(method, "REDIRECT") == 0)      strcpy(method, "mREDIRECT");
    else if (strcasecmp(method, "RECORD") == 0)        strcpy(method, "mRECORD");
    else                                               strcpy(method, "mNotSet");
}

/* Parse URI from request line: "METHOD <uri> RTSP/1.0" */
static void parse_request_uri(const char *line, char *uri, size_t uri_sz)
{
    uri[0] = '\0';
    const char *p = line;
    skip_whitespace(&p);

    /* Skip method token */
    while (*p && !isspace((unsigned char)*p)) p++;
    skip_whitespace(&p);

    /* Copy URI until next whitespace */
    size_t i = 0;
    while (*p && !isspace((unsigned char)*p) && i < uri_sz - 1) {
        uri[i++] = *p++;
    }
    uri[i] = '\0';
}

/* ================================================================
 * Header parsing
 * ================================================================ */

/* Parse CSeq header value, return numeric value (-1 if absent/unparseable) */
static int parse_cseq(const char *line)
{
    const char *p = strchr(line, ':');
    if (!p) return -1;
    p++;
    skip_whitespace(&p);
    return atoi(p);
}

/*
 * Check if the CSeq value after "CSeq:" is a valid decimal integer.
 * RFC 2326 s12.17: CSeq = "CSeq" ":" 1*DIGIT
 * Returns false if the value contains any non-digit characters or is empty.
 */
static bool check_cseq_valid(const char *line)
{
    const char *p = strchr(line, ':');
    if (!p) return false;
    p++;
    skip_whitespace(&p);

    if (*p == '\0') return false;  /* empty value */

    bool has_digit = false;
    while (*p && !isspace((unsigned char)*p)) {
        if (!isdigit((unsigned char)*p)) return false;
        has_digit = true;
        p++;
    }
    return has_digit;
}

/* Parse Session header value (extract session ID before ';' or whitespace) */
static void parse_session(const char *line, char *session_id, size_t sz)
{
    const char *p = strchr(line, ':');
    if (!p) {
        session_id[0] = '\0';
        return;
    }
    p++;
    skip_whitespace(&p);

    size_t i = 0;
    while (*p && *p != ';' && !isspace((unsigned char)*p) && i < sz - 1) {
        session_id[i++] = *p++;
    }
    session_id[i] = '\0';
}

/* Parse Transport header for UDP/TCP, port presence, and zero-port detection */
static void parse_transport(const char *line,
                            bool *has_udp, bool *has_tcp,
                            bool *has_client_ports, bool *has_server_ports,
                            bool *has_port_zero)
{
    *has_udp = false;
    *has_tcp = false;
    *has_client_ports = false;
    *has_server_ports = false;
    *has_port_zero = false;

    const char *p = strchr(line, ':');
    if (!p) return;
    p++;

    /* Check transport type */
    if (strstr(p, "RTP/AVP/TCP") || strstr(p, "interleaved")) {
        *has_tcp = true;
    } else if (strstr(p, "RTP/AVP")) {
        *has_udp = true;
    }

    /* Check for port fields and extract values */
    const char *cp = strstr(p, "client_port=");
    if (cp) {
        *has_client_ports = true;
        cp += strlen("client_port=");
        int port = atoi(cp);
        if (port == 0) *has_port_zero = true;
    }

    const char *sp = strstr(p, "server_port=");
    if (sp) {
        *has_server_ports = true;
        sp += strlen("server_port=");
        int port = atoi(sp);
        if (port == 0) *has_port_zero = true;
    }

    /* Also check bare "port=" for multicast */
    const char *mp = strstr(p, ";port=");
    if (mp) {
        mp += strlen(";port=");
        int port = atoi(mp);
        if (port == 0) *has_port_zero = true;
    }
}

/* Parse status code from response line: "RTSP/1.0 200 OK" */
static int parse_response_status(const char *line)
{
    const char *p = line;
    while (*p && !isspace((unsigned char)*p)) p++;
    skip_whitespace(&p);
    return atoi(p);
}

/* Determine status class string from numeric status code */
static const char *get_status_class(int code)
{
    if (code == 0)                    return "scNotSet";
    if (code >= 100 && code < 200)    return "scINFO";
    if (code >= 200 && code < 300)    return "scSUCCESS";
    if (code >= 300 && code < 400)    return "scREDIRECT";
    if (code >= 400 && code < 500)    return "scCLIENT_ERR";
    if (code >= 500 && code < 600)    return "scSERVER_ERR";
    return "scNotSet";
}

/* Helper: is status code a 2xx success? (avoids string comparison bug) */
static inline bool is_success(int code)
{
    return code >= 200 && code < 300;
}

/* ================================================================
 * Malformed message detection
 * ================================================================ */

static bool is_malformed_request(const unsigned char *buf, unsigned int len)
{
    if (len < 4) return true;
    if (!isupper(buf[0])) return true;
    if (!memmem(buf, len, "RTSP/1.0", 8)) return true;
    return false;
}

static bool is_malformed_response(const unsigned char *buf, unsigned int len)
{
    if (len < 12) return true;
    if (memcmp(buf, "RTSP/1.0 ", 9) != 0) return true;
    return false;
}

/* ================================================================
 * URI-in-session tracking
 * ================================================================ */

/* Record a URI as successfully SETUPed in the current session */
static void record_setup_uri(const char *uri)
{
    if (uri[0] == '\0') return;
    if (g_setup_uri_count >= RTSP_MAX_SETUP_URIS) return;

    /* Avoid duplicates */
    for (int i = 0; i < g_setup_uri_count; i++) {
        if (strcmp(g_setup_uris[i], uri) == 0) return;
    }

    strncpy(g_setup_uris[g_setup_uri_count], uri,
            sizeof(g_setup_uris[0]) - 1);
    g_setup_uris[g_setup_uri_count][sizeof(g_setup_uris[0]) - 1] = '\0';
    g_setup_uri_count++;
}

/*
 * Check if a request URI belongs to the current session's SETUPed streams.
 *
 * Matches if:
 *   - The request URI exactly matches a SETUPed URI, OR
 *   - A SETUPed URI starts with the request URI (aggregate PLAY/TEARDOWN).
 *     e.g. SETUP rtsp://host/foo/track1, PLAY rtsp://host/foo/ -> match
 *
 * Does NOT match if the request URI is for a completely different stream.
 *     e.g. SETUP rtsp://host/mp3Audio/track1, PLAY rtsp://host/wavAudio/ -> no match
 */
static bool check_uri_in_session(const char *uri)
{
    if (uri[0] == '\0') return false;
    if (g_setup_uri_count == 0) return false;

    size_t uri_len = strlen(uri);

    for (int i = 0; i < g_setup_uri_count; i++) {
        /* Exact match */
        if (strcmp(g_setup_uris[i], uri) == 0) return true;

        /* Aggregate match: SETUPed URI starts with request URI */
        if (strncmp(g_setup_uris[i], uri, uri_len) == 0) return true;

        /* Reverse: request URI starts with SETUPed URI (track-level TEARDOWN) */
        size_t setup_len = strlen(g_setup_uris[i]);
        if (strncmp(uri, g_setup_uris[i], setup_len) == 0) return true;
    }
    return false;
}

/* ================================================================
 * Public API: request predicate builder
 * ================================================================ */

void rtsp_build_request_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz)
{
    if (!g_session_initialized) {
        rtsp_predicate_reset_session();
    }

    /* Default values */
    char method[32] = "mNotSet";
    char req_uri[512] = "";
    int req_cseq = -1;
    bool req_has_session = false;
    char req_session_id[256] = "";
    bool req_malformed = is_malformed_request(buf, len);
    bool transport_req_udp = false;
    bool transport_req_tcp = false;
    bool transport_client_ports = false;
    bool keepalive_getparam = false;

    if (!req_malformed && len > 0) {
        unsigned int offset = 0;
        char line[1024];

        /* Parse request line */
        extract_line(buf, len, &offset, line, sizeof(line));
        parse_request_method(line, method, sizeof(method));
        parse_request_uri(line, req_uri, sizeof(req_uri));

        /* Parse headers */
        while (offset < len) {
            extract_line(buf, len, &offset, line, sizeof(line));
            if (strlen(line) == 0) break;

            if (starts_with(line, "CSeq:")) {
                req_cseq = parse_cseq(line);
            } else if (starts_with(line, "Session:")) {
                req_has_session = true;
                parse_session(line, req_session_id, sizeof(req_session_id));
            } else if (starts_with(line, "Transport:")) {
                bool dummy_server, dummy_zero;
                parse_transport(line, &transport_req_udp, &transport_req_tcp,
                                &transport_client_ports, &dummy_server, &dummy_zero);
            }
        }

        /* Detect keep-alive GET_PARAMETER */
        if (strcmp(method, "mGET_PARAMETER") == 0 && req_has_session) {
            keepalive_getparam = true;
        }
    }

    /* Save request state for correlation with response */
    g_last_req_cseq = req_cseq;
    strncpy(g_last_req_method, method, sizeof(g_last_req_method) - 1);
    g_last_req_method[sizeof(g_last_req_method) - 1] = '\0';
    g_last_req_has_session = req_has_session;
    strncpy(g_last_req_session_id, req_session_id, sizeof(g_last_req_session_id) - 1);
    g_last_req_session_id[sizeof(g_last_req_session_id) - 1] = '\0';
    g_last_req_transport_udp = transport_req_udp;
    g_last_req_transport_tcp = transport_req_tcp;
    g_last_req_client_ports = transport_client_ports;
    g_last_req_malformed = req_malformed;
    strncpy(g_last_req_uri, req_uri, sizeof(g_last_req_uri) - 1);
    g_last_req_uri[sizeof(g_last_req_uri) - 1] = '\0';

    /* Session ID match (only relevant if session established) */
    bool session_id_match = false;
    if (g_session_established && req_has_session) {
        session_id_match = (strcmp(req_session_id, g_session_id) == 0);
    }

    /* TEARDOWN analysis */
    bool teardown_for_existing = false;
    bool teardown_without_session = false;
    if (strcmp(method, "mTEARDOWN") == 0) {
        if (req_has_session && g_session_established && session_id_match) {
            teardown_for_existing = true;
        }
        if (!req_has_session) {
            teardown_without_session = true;
        }
    }

    /* req_uri_in_session: check against SETUPed URIs (for context in request line) */
    bool uri_in_session = false;
    if (g_session_established && req_uri[0] != '\0') {
        uri_in_session = check_uri_in_session(req_uri);
    }

    /* Build predicate line */
    snprintf(out, out_sz,
             "rtsp_method=%s status_class=scNotSet "
             "req_cseq=%d resp_cseq=-1 resp_status_code=0 "
             "req_malformed=%s resp_malformed=false cseq_match=false "
             "req_has_session=%s resp_has_session=false "
             "session_established=%s session_id_match=%s session_id_changed=false "
             "teardown_for_existing_session=%s teardown_without_session=%s "
             "transport_req_udp=%s transport_req_tcp=%s "
             "transport_resp_udp=false transport_resp_tcp=false "
             "transport_client_ports_present=%s transport_server_ports_present=false "
             "setup_success_count=%d play_success_count=%d all_tracks_setup=%s "
             "keepalive_getparam=%s keepalive_failed=false timeout=false "
             "transport_resp_port_zero=false resp_cseq_valid=true "
             "req_uri_in_session=%s resp_empty=false",
             method,
             req_cseq,
             req_malformed ? "true" : "false",
             req_has_session ? "true" : "false",
             g_session_established ? "true" : "false",
             session_id_match ? "true" : "false",
             teardown_for_existing ? "true" : "false",
             teardown_without_session ? "true" : "false",
             transport_req_udp ? "true" : "false",
             transport_req_tcp ? "true" : "false",
             transport_client_ports ? "true" : "false",
             g_setup_success_count,
             g_play_success_count,
             (g_total_tracks > 0 && g_setup_success_count >= g_total_tracks)
                 ? "true" : "false",
             keepalive_getparam ? "true" : "false",
             uri_in_session ? "true" : "false");

    append_packet_trace(out, out_sz, buf, len, "C2S");
}

/* ================================================================
 * Public API: response predicate builder
 * ================================================================ */

void rtsp_build_response_pred_line(const unsigned char *buf,
                                   unsigned int len,
                                   char *out,
                                   size_t out_sz)
{
    if (!g_session_initialized) {
        rtsp_predicate_reset_session();
    }

    /* Default values */
    int resp_status_code = 0;
    int resp_cseq = -1;
    bool resp_has_session = false;
    char resp_session_id[256] = "";
    bool resp_malformed = false;
    bool transport_resp_udp = false;
    bool transport_resp_tcp = false;
    bool transport_server_ports = false;
    bool transport_resp_port_zero = false;
    bool resp_cseq_valid = true;       /* assume valid until proven otherwise */
    bool resp_empty = (len == 0);
    bool timeout = (len == 0);

    if (len > 0) {
        resp_malformed = is_malformed_response(buf, len);
    }

    if (!resp_malformed && len > 0) {
        unsigned int offset = 0;
        char line[1024];
        bool found_cseq = false;

        /* Parse status line */
        extract_line(buf, len, &offset, line, sizeof(line));
        resp_status_code = parse_response_status(line);

        /* Parse headers */
        while (offset < len) {
            extract_line(buf, len, &offset, line, sizeof(line));
            if (strlen(line) == 0) break;

            if (starts_with(line, "CSeq:")) {
                resp_cseq = parse_cseq(line);
                resp_cseq_valid = check_cseq_valid(line);
                found_cseq = true;
            } else if (starts_with(line, "Session:")) {
                resp_has_session = true;
                parse_session(line, resp_session_id, sizeof(resp_session_id));
            } else if (starts_with(line, "Transport:")) {
                bool dummy_client;
                parse_transport(line, &transport_resp_udp, &transport_resp_tcp,
                                &dummy_client, &transport_server_ports,
                                &transport_resp_port_zero);
            }
        }

        /* No CSeq header at all -> invalid */
        if (!found_cseq) {
            resp_cseq_valid = false;
        }
    } else if (len == 0) {
        resp_cseq_valid = false;
    }

    const char *status_class = get_status_class(resp_status_code);
    bool cseq_match = (g_last_req_cseq > 0 && resp_cseq > 0 &&
                       g_last_req_cseq == resp_cseq);

    /*
     * Update session state.
     * BUG FIX: use numeric status code range and "mXXX" method prefix.
     * The original adapter compared strcmp(status_class, "SUCCESS") against
     * strings like "scSUCCESS", and strcmp(method, "SETUP") against "mSETUP",
     * so session tracking never fired.
     */
    bool session_id_changed = false;

    if (strcmp(g_last_req_method, "mSETUP") == 0 &&
        is_success(resp_status_code) && resp_has_session) {

        if (!g_session_established) {
            /* First successful SETUP establishes session */
            strncpy(g_session_id, resp_session_id, sizeof(g_session_id) - 1);
            g_session_id[sizeof(g_session_id) - 1] = '\0';
            g_session_established = true;
            g_total_tracks = 1;
        } else {
            /* Check if session ID changed */
            if (strcmp(g_session_id, resp_session_id) != 0) {
                session_id_changed = true;
            }
        }
        g_setup_success_count++;

        /* NEW: record this URI as SETUPed in the current session */
        record_setup_uri(g_last_req_uri);
    }

    /* Track successful PLAY */
    if (strcmp(g_last_req_method, "mPLAY") == 0 && is_success(resp_status_code)) {
        g_play_success_count++;
    }

    /* Session ID match with request */
    bool session_id_match = false;
    if (g_last_req_has_session && g_session_established) {
        session_id_match = (strcmp(g_last_req_session_id, g_session_id) == 0);
    }

    /* TEARDOWN tracking */
    bool teardown_for_existing = false;
    bool teardown_without_session = false;
    if (strcmp(g_last_req_method, "mTEARDOWN") == 0) {
        if (g_last_req_has_session && g_session_established && session_id_match) {
            teardown_for_existing = true;
        }
        if (!g_last_req_has_session) {
            teardown_without_session = true;
        }

        /* Successful TEARDOWN ends session */
        if (is_success(resp_status_code)) {
            g_session_established = false;
        }
    }

    /* Keep-alive failure detection */
    bool keepalive_failed = false;
    if (strcmp(g_last_req_method, "mGET_PARAMETER") == 0 &&
        g_last_req_has_session) {
        if (timeout || resp_status_code >= 400) {
            keepalive_failed = true;
        }
    }

    /* NEW: req_uri_in_session for the response line */
    bool uri_in_session = false;
    if (g_session_established && g_last_req_uri[0] != '\0') {
        uri_in_session = check_uri_in_session(g_last_req_uri);
    }

    /* Build predicate line */
    snprintf(out, out_sz,
             "rtsp_method=%s status_class=%s "
             "req_cseq=%d resp_cseq=%d resp_status_code=%d "
             "req_malformed=%s resp_malformed=%s cseq_match=%s "
             "req_has_session=%s resp_has_session=%s "
             "session_established=%s session_id_match=%s session_id_changed=%s "
             "teardown_for_existing_session=%s teardown_without_session=%s "
             "transport_req_udp=%s transport_req_tcp=%s "
             "transport_resp_udp=%s transport_resp_tcp=%s "
             "transport_client_ports_present=%s transport_server_ports_present=%s "
             "setup_success_count=%d play_success_count=%d all_tracks_setup=%s "
             "keepalive_getparam=false keepalive_failed=%s timeout=%s "
             "transport_resp_port_zero=%s resp_cseq_valid=%s "
             "req_uri_in_session=%s resp_empty=%s",
             g_last_req_method,
             status_class,
             g_last_req_cseq,
             resp_cseq,
             resp_status_code,
             g_last_req_malformed ? "true" : "false",
             resp_malformed ? "true" : "false",
             cseq_match ? "true" : "false",
             g_last_req_has_session ? "true" : "false",
             resp_has_session ? "true" : "false",
             g_session_established ? "true" : "false",
             session_id_match ? "true" : "false",
             session_id_changed ? "true" : "false",
             teardown_for_existing ? "true" : "false",
             teardown_without_session ? "true" : "false",
             g_last_req_transport_udp ? "true" : "false",
             g_last_req_transport_tcp ? "true" : "false",
             transport_resp_udp ? "true" : "false",
             transport_resp_tcp ? "true" : "false",
             g_last_req_client_ports ? "true" : "false",
             transport_server_ports ? "true" : "false",
             g_setup_success_count,
             g_play_success_count,
             (g_total_tracks > 0 && g_setup_success_count >= g_total_tracks)
                 ? "true" : "false",
             keepalive_failed ? "true" : "false",
             timeout ? "true" : "false",
             transport_resp_port_zero ? "true" : "false",
             resp_cseq_valid ? "true" : "false",
             uri_in_session ? "true" : "false",
             resp_empty ? "true" : "false");

    append_packet_trace(out, out_sz, buf, len, "S2C");
}
