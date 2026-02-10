// rtsp_predicate_adapter.c
#include "rtsp_predicate_adapter.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* --- Static per-session state --- */

static char   g_session_id[256] = "";      // Active Session ID
static bool   g_session_established = false;
static int    g_setup_success_count = 0;
static int    g_play_success_count = 0;
static int    g_last_req_cseq = -1;
static char   g_last_req_method[32] = "mNotSet";
static bool   g_last_req_has_session = false;
static char   g_last_req_session_id[256] = "";
static bool   g_last_req_transport_udp = false;
static bool   g_last_req_transport_tcp = false;
static bool   g_last_req_client_ports = false;
static int    g_total_tracks = 0;         // Heuristic: count SETUP successes
static bool   g_session_initialized = false;

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
    g_total_tracks = 0;
    g_session_initialized = true;
}

/* --- Helpers for parsing RTSP text --- */

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
    
    // Convert to enum string format with m prefix
    if (strcasecmp(method, "OPTIONS") == 0) strcpy(method, "mOPTIONS");
    else if (strcasecmp(method, "DESCRIBE") == 0) strcpy(method, "mDESCRIBE");
    else if (strcasecmp(method, "SETUP") == 0) strcpy(method, "mSETUP");
    else if (strcasecmp(method, "PLAY") == 0) strcpy(method, "mPLAY");
    else if (strcasecmp(method, "PAUSE") == 0) strcpy(method, "mPAUSE");
    else if (strcasecmp(method, "TEARDOWN") == 0) strcpy(method, "mTEARDOWN");
    else if (strcasecmp(method, "ANNOUNCE") == 0) strcpy(method, "mANNOUNCE");
    else if (strcasecmp(method, "GET_PARAMETER") == 0) strcpy(method, "mGET_PARAMETER");
    else if (strcasecmp(method, "SET_PARAMETER") == 0) strcpy(method, "mSET_PARAMETER");
    else if (strcasecmp(method, "REDIRECT") == 0) strcpy(method, "mREDIRECT");
    else if (strcasecmp(method, "RECORD") == 0) strcpy(method, "mRECORD");
    else strcpy(method, "mNotSet");
}

/* Parse CSeq header value */
static int parse_cseq(const char *line)
{
    const char *p = strchr(line, ':');
    if (!p) return -1;
    p++;
    skip_whitespace(&p);
    return atoi(p);
}

/* Parse Session header value (extract session ID) */
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
    // Session ID ends at ';' or whitespace
    while (*p && *p != ';' && !isspace((unsigned char)*p) && i < sz - 1) {
        session_id[i++] = *p++;
    }
    session_id[i] = '\0';
}

/* Parse Transport header for UDP/TCP and port info */
static void parse_transport(const char *line, bool *has_udp, bool *has_tcp,
                           bool *has_client_ports, bool *has_server_ports)
{
    *has_udp = false;
    *has_tcp = false;
    *has_client_ports = false;
    *has_server_ports = false;
    
    const char *p = strchr(line, ':');
    if (!p) return;
    p++;
    
    // Check for RTP/AVP (UDP) vs RTP/AVP/TCP (interleaved)
    if (strstr(p, "RTP/AVP/TCP") || strstr(p, "interleaved")) {
        *has_tcp = true;
    } else if (strstr(p, "RTP/AVP")) {
        *has_udp = true;
    }
    
    // Check for client_port= or server_port=
    if (strstr(p, "client_port")) *has_client_ports = true;
    if (strstr(p, "server_port")) *has_server_ports = true;
}

/* Parse status code from response line: "RTSP/1.0 200 OK" */
static int parse_response_status(const char *line)
{
    const char *p = line;
    // Skip "RTSP/1.0 "
    while (*p && !isspace((unsigned char)*p)) p++;
    skip_whitespace(&p);
    return atoi(p);
}

/* Determine status class from status code */
static const char* get_status_class(int code)
{
    if (code == 0) return "scNotSet";
    if (code >= 100 && code < 200) return "scINFO";
    if (code >= 200 && code < 300) return "scSUCCESS";
    if (code >= 300 && code < 400) return "scREDIRECT";
    if (code >= 400 && code < 500) return "scCLIENT_ERR";
    if (code >= 500 && code < 600) return "scSERVER_ERR";
    return "scNotSet";
}

/* Basic malformed check: must start with valid RTSP marker */
static bool is_malformed_request(const unsigned char *buf, unsigned int len)
{
    if (len < 4) return true;
    // Valid requests start with method name (all caps)
    if (!isupper(buf[0])) return true;
    // Should contain "RTSP/1.0"
    const char *rtsp_marker = "RTSP/1.0";
    if (!memmem(buf, len, rtsp_marker, strlen(rtsp_marker))) return true;
    return false;
}

static bool is_malformed_response(const unsigned char *buf, unsigned int len)
{
    if (len < 12) return true;
    // Must start with "RTSP/1.0 "
    if (memcmp(buf, "RTSP/1.0 ", 9) != 0) return true;
    return false;
}

/* --- Public builders --- */

void rtsp_build_request_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz)
{
    if (!g_session_initialized) {
        rtsp_predicate_reset_session();
    }
    
    // Default values
    char method[32] = "mNotSet";
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
        
        // Parse request line (first line)
        extract_line(buf, len, &offset, line, sizeof(line));
        parse_request_method(line, method, sizeof(method));
        
        // Parse headers
        while (offset < len) {
            extract_line(buf, len, &offset, line, sizeof(line));
            if (strlen(line) == 0) break; // Empty line = end of headers
            
            if (starts_with(line, "CSeq:")) {
                req_cseq = parse_cseq(line);
            } else if (starts_with(line, "Session:")) {
                req_has_session = true;
                parse_session(line, req_session_id, sizeof(req_session_id));
            } else if (starts_with(line, "Transport:")) {
                bool dummy_server_ports;
                parse_transport(line, &transport_req_udp, &transport_req_tcp,
                              &transport_client_ports, &dummy_server_ports);
            }
        }
        
        // Check if this is GET_PARAMETER with Session (keep-alive)
        if (strcmp(method, "GET_PARAMETER") == 0 && req_has_session) {
            keepalive_getparam = true;
        }
    }
    
    // Save request state for correlation with response
    g_last_req_cseq = req_cseq;
    strncpy(g_last_req_method, method, sizeof(g_last_req_method) - 1);
    g_last_req_has_session = req_has_session;
    strncpy(g_last_req_session_id, req_session_id, sizeof(g_last_req_session_id) - 1);
    g_last_req_transport_udp = transport_req_udp;
    g_last_req_transport_tcp = transport_req_tcp;
    g_last_req_client_ports = transport_client_ports;
    
    // Check session ID match (only relevant if session established)
    bool session_id_match = false;
    if (g_session_established && req_has_session) {
        session_id_match = (strcmp(req_session_id, g_session_id) == 0);
    }
    
    // TEARDOWN analysis
    bool teardown_for_existing = false;
    bool teardown_without_session = false;
    if (strcmp(method, "TEARDOWN") == 0) {
        if (req_has_session && g_session_established && session_id_match) {
            teardown_for_existing = true;
        }
        if (!req_has_session) {
            teardown_without_session = true;
        }
    }
    
    // Build predicate line
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
             "keepalive_getparam=%s keepalive_failed=false timeout=false",
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
             (g_total_tracks > 0 && g_setup_success_count >= g_total_tracks) ? "true" : "false",
             keepalive_getparam ? "true" : "false");
}

void rtsp_build_response_pred_line(const unsigned char *buf,
                                   unsigned int len,
                                   char *out,
                                   size_t out_sz)
{
    if (!g_session_initialized) {
        rtsp_predicate_reset_session();
    }
    
    // Default values
    int resp_status_code = 0;
    int resp_cseq = -1;
    bool resp_has_session = false;
    char resp_session_id[256] = "";
    bool resp_malformed = is_malformed_response(buf, len);
    bool transport_resp_udp = false;
    bool transport_resp_tcp = false;
    bool transport_server_ports = false;
    bool timeout = false;
    
    if (!resp_malformed && len > 0) {
        unsigned int offset = 0;
        char line[1024];
        
        // Parse status line
        extract_line(buf, len, &offset, line, sizeof(line));
        resp_status_code = parse_response_status(line);
        
        // Parse headers
        while (offset < len) {
            extract_line(buf, len, &offset, line, sizeof(line));
            if (strlen(line) == 0) break;
            
            if (starts_with(line, "CSeq:")) {
                resp_cseq = parse_cseq(line);
            } else if (starts_with(line, "Session:")) {
                resp_has_session = true;
                parse_session(line, resp_session_id, sizeof(resp_session_id));
            } else if (starts_with(line, "Transport:")) {
                bool dummy_client_ports;
                parse_transport(line, &transport_resp_udp, &transport_resp_tcp,
                              &dummy_client_ports, &transport_server_ports);
            }
        }
    } else if (len == 0) {
        // Empty response = timeout
        timeout = true;
    }
    
    const char *status_class = get_status_class(resp_status_code);
    bool cseq_match = (g_last_req_cseq > 0 && resp_cseq > 0 && 
                       g_last_req_cseq == resp_cseq);
    
    // Update session state
    bool session_id_changed = false;
    if (strcmp(g_last_req_method, "SETUP") == 0 && 
        strcmp(status_class, "SUCCESS") == 0 && resp_has_session) {
        
        if (!g_session_established) {
            // First successful SETUP establishes session
            strncpy(g_session_id, resp_session_id, sizeof(g_session_id) - 1);
            g_session_established = true;
            g_total_tracks = 1;
        } else {
            // Check if session ID changed
            if (strcmp(g_session_id, resp_session_id) != 0) {
                session_id_changed = true;
            }
        }
        g_setup_success_count++;
    }
    
    // Track successful PLAY
    if (strcmp(g_last_req_method, "PLAY") == 0 && strcmp(status_class, "SUCCESS") == 0) {
        g_play_success_count++;
    }
    
    // Check session ID match with request
    bool session_id_match = false;
    if (g_last_req_has_session && g_session_established) {
        session_id_match = (strcmp(g_last_req_session_id, g_session_id) == 0);
    }
    
    // TEARDOWN tracking
    bool teardown_for_existing = false;
    bool teardown_without_session = false;
    if (strcmp(g_last_req_method, "TEARDOWN") == 0) {
        if (g_last_req_has_session && g_session_established && session_id_match) {
            teardown_for_existing = true;
        }
        if (!g_last_req_has_session) {
            teardown_without_session = true;
        }
        
        // Successful TEARDOWN ends session
        if (strcmp(status_class, "SUCCESS") == 0) {
            g_session_established = false;
        }
    }
    
    // Keep-alive failure detection
    bool keepalive_failed = false;
    if (strcmp(g_last_req_method, "GET_PARAMETER") == 0 && 
        g_last_req_has_session) {
        if (timeout || strcmp(status_class, "CLIENT_ERR") == 0 || 
            strcmp(status_class, "SERVER_ERR") == 0) {
            keepalive_failed = true;
        }
    }
    
    // Build predicate line
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
             "keepalive_getparam=false keepalive_failed=%s timeout=%s",
             g_last_req_method,
             status_class,
             g_last_req_cseq,
             resp_cseq,
             resp_status_code,
             "false", // req_malformed was already checked in request
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
             (g_total_tracks > 0 && g_setup_success_count >= g_total_tracks) ? "true" : "false",
             keepalive_failed ? "true" : "false",
             timeout ? "true" : "false");
}