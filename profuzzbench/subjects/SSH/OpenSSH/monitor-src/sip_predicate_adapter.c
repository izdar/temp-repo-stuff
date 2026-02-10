// sip_predicate_adapter.c
#include "sip_predicate_adapter.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================================
// Session State Tracking
// ============================================================================

static struct {
    // Dialog state
    bool dialog_established;
    bool dialog_terminated;
    bool early_dialog;
    
    // Transaction state
    bool transaction_pending;
    bool final_response_sent;
    bool provisional_sent;
    bool invite_in_progress;
    
    // Method tracking
    bool invite_sent;
    bool invite_received;
    bool ack_sent;
    bool bye_sent;
    bool cancel_sent;
    bool register_sent;
    
    // Registration
    bool registered;
    bool auth_required;
    bool auth_provided;
    
    // Dialog identifiers
    char call_id[128];
    char from_tag[64];
    char to_tag[64];
    int cseq_number;
    
    // Last method/response
    char last_method[32];
    int last_resp_code;
    
    // Connection state
    bool connection_closed;
    
} g_sip_state;

void sip_predicate_reset_session(void) {
    memset(&g_sip_state, 0, sizeof(g_sip_state));
    strcpy(g_sip_state.last_method, "mNotSet");
}

// ============================================================================
// Helper Functions
// ============================================================================

static inline void skip_ws(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

static inline void skip_line(const char **p) {
    while (**p && **p != '\r' && **p != '\n') (*p)++;
    if (**p == '\r') (*p)++;
    if (**p == '\n') (*p)++;
}

// Extract SIP method from request line
static void extract_method(const unsigned char *buf, unsigned int len,
                          char *method, size_t method_sz) {
    size_t i = 0;
    while (i < len && i < method_sz - 1 && buf[i] != ' ') {
        method[i] = buf[i];
        i++;
    }
    method[i] = '\0';
}

// Map SIP method to enum
static const char* map_sip_method(const char *method) {
    if (strcmp(method, "INVITE") == 0) return "mINVITE";
    if (strcmp(method, "ACK") == 0) return "mACK";
    if (strcmp(method, "BYE") == 0) return "mBYE";
    if (strcmp(method, "CANCEL") == 0) return "mCANCEL";
    if (strcmp(method, "REGISTER") == 0) return "mREGISTER";
    if (strcmp(method, "OPTIONS") == 0) return "mOPTIONS";
    if (strcmp(method, "PRACK") == 0) return "mPRACK";
    if (strcmp(method, "SUBSCRIBE") == 0) return "mSUBSCRIBE";
    if (strcmp(method, "NOTIFY") == 0) return "mNOTIFY";
    if (strcmp(method, "PUBLISH") == 0) return "mPUBLISH";
    if (strcmp(method, "INFO") == 0) return "mINFO";
    if (strcmp(method, "REFER") == 0) return "mREFER";
    if (strcmp(method, "MESSAGE") == 0) return "mMESSAGE";
    if (strcmp(method, "UPDATE") == 0) return "mUPDATE";
    return "mNotSet";
}

// Extract response code from status line
static int extract_response_code(const unsigned char *buf, unsigned int len) {
    // Status line: "SIP/2.0 200 OK\r\n"
    if (len < 12) return 0;
    if (strncmp((const char*)buf, "SIP/2.0 ", 8) != 0) return 0;
    
    const char *p = (const char*)(buf + 8);
    if (!isdigit(p[0]) || !isdigit(p[1]) || !isdigit(p[2])) return 0;
    
    return (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
}

// Map response code to status class
static const char* map_status_class(int code) {
    if (code == 0) return "scNotSet";
    if (code >= 100 && code < 200) return "scProvisional";
    if (code >= 200 && code < 300) return "scSuccess";
    if (code >= 300 && code < 400) return "scRedirection";
    if (code >= 400 && code < 500) return "scClientError";
    if (code >= 500 && code < 600) return "scServerError";
    if (code >= 600 && code < 700) return "scGlobalError";
    return "scNotSet";
}

// Find header value
static bool find_header(const unsigned char *buf, unsigned int len,
                       const char *header_name, char *value, size_t value_sz) {
    const char *p = (const char*)buf;
    const char *end = p + len;
    size_t hdr_len = strlen(header_name);
    
    // Skip first line (request/status line)
    skip_line(&p);
    
    while (p < end) {
        // Check if we've reached the end of headers
        if (*p == '\r' || *p == '\n') break;
        
        // Check if this line starts with the header we're looking for
        if (strncasecmp(p, header_name, hdr_len) == 0 && p[hdr_len] == ':') {
            p += hdr_len + 1;
            skip_ws(&p);
            
            // Extract value
            size_t i = 0;
            while (i < value_sz - 1 && p < end && *p != '\r' && *p != '\n') {
                value[i++] = *p++;
            }
            value[i] = '\0';
            return true;
        }
        
        skip_line(&p);
    }
    
    return false;
}

// Check if message is malformed
static bool is_malformed_request(const unsigned char *buf, unsigned int len) {
    if (len < 10) return true; // Too short
    
    // Must start with method
    const char *p = (const char*)buf;
    if (!isalpha(*p)) return true;
    
    // Must have SIP/2.0
    if (!strstr((const char*)buf, "SIP/2.0")) return true;
    
    // Check for mandatory headers (basic check)
    if (!strstr((const char*)buf, "Via:") && !strstr((const char*)buf, "via:")) return true;
    if (!strstr((const char*)buf, "Call-ID:") && !strstr((const char*)buf, "call-id:")) return true;
    if (!strstr((const char*)buf, "CSeq:") && !strstr((const char*)buf, "cseq:")) return true;
    
    return false;
}

static bool is_malformed_response(const unsigned char *buf, unsigned int len) {
    if (len < 12) return true; // Too short for "SIP/2.0 XXX "
    
    // Must start with SIP/2.0
    if (strncmp((const char*)buf, "SIP/2.0 ", 8) != 0) return true;
    
    // Must have 3-digit status code
    const char *p = (const char*)(buf + 8);
    if (!isdigit(p[0]) || !isdigit(p[1]) || !isdigit(p[2])) return true;
    
    return false;
}

// Extract tag from header value
static void extract_tag(const char *header_value, char *tag, size_t tag_sz) {
    const char *p = strstr(header_value, "tag=");
    if (!p) {
        tag[0] = '\0';
        return;
    }
    
    p += 4; // Skip "tag="
    size_t i = 0;
    while (i < tag_sz - 1 && *p && *p != ';' && *p != ' ' && *p != '\r' && *p != '\n') {
        tag[i++] = *p++;
    }
    tag[i] = '\0';
}

// ============================================================================
// Public API
// ============================================================================

void sip_build_request_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz) {
    // Extract method
    char method_str[32];
    extract_method(buf, len, method_str, sizeof(method_str));
    const char *method_enum = map_sip_method(method_str);
    
    // Save last method
    strncpy(g_sip_state.last_method, method_enum, sizeof(g_sip_state.last_method) - 1);
    
    // Extract headers
    char header_value[256];
    bool has_via = find_header(buf, len, "Via", header_value, sizeof(header_value));
    bool has_cseq = find_header(buf, len, "CSeq", header_value, sizeof(header_value));
    bool has_call_id = find_header(buf, len, "Call-ID", header_value, sizeof(header_value));
    bool has_contact = find_header(buf, len, "Contact", header_value, sizeof(header_value));
    
    // Extract CSeq number
    int cseq_num = 0;
    if (has_cseq) {
        cseq_num = atoi(header_value);
        g_sip_state.cseq_number = cseq_num;
    }
    
    // Extract Call-ID
    if (has_call_id) {
        strncpy(g_sip_state.call_id, header_value, sizeof(g_sip_state.call_id) - 1);
    }
    
    // Extract From tag
    char from_tag[64] = "";
    if (find_header(buf, len, "From", header_value, sizeof(header_value))) {
        extract_tag(header_value, from_tag, sizeof(from_tag));
        if (from_tag[0]) {
            strncpy(g_sip_state.from_tag, from_tag, sizeof(g_sip_state.from_tag) - 1);
        }
    }
    
    // Extract To tag
    char to_tag[64] = "";
    bool has_to_tag = false;
    if (find_header(buf, len, "To", header_value, sizeof(header_value))) {
        extract_tag(header_value, to_tag, sizeof(to_tag));
        has_to_tag = (to_tag[0] != '\0');
    }
    
    // Extract other headers
    int expires = 3600; // Default
    if (find_header(buf, len, "Expires", header_value, sizeof(header_value))) {
        expires = atoi(header_value);
    }
    
    int max_forwards = 70; // Default
    if (find_header(buf, len, "Max-Forwards", header_value, sizeof(header_value))) {
        max_forwards = atoi(header_value);
    }
    
    int content_length = 0;
    if (find_header(buf, len, "Content-Length", header_value, sizeof(header_value))) {
        content_length = atoi(header_value);
    }
    
    bool has_sdp = (content_length > 0 && strstr((const char*)buf, "application/sdp"));
    bool has_auth = find_header(buf, len, "Authorization", header_value, sizeof(header_value)) ||
                    find_header(buf, len, "Proxy-Authorization", header_value, sizeof(header_value));
    
    // Update state based on method
    if (strcmp(method_enum, "mINVITE") == 0) {
        g_sip_state.invite_sent = true;
        g_sip_state.invite_in_progress = true;
    } else if (strcmp(method_enum, "mACK") == 0) {
        g_sip_state.ack_sent = true;
    } else if (strcmp(method_enum, "mBYE") == 0) {
        g_sip_state.bye_sent = true;
    } else if (strcmp(method_enum, "mCANCEL") == 0) {
        g_sip_state.cancel_sent = true;
    } else if (strcmp(method_enum, "mREGISTER") == 0) {
        g_sip_state.register_sent = true;
        g_sip_state.auth_provided = has_auth;
    }
    
    // Determine states
    const char *dialog_state = "dsNone";
    if (g_sip_state.dialog_terminated) dialog_state = "dsTerminated";
    else if (g_sip_state.dialog_established) dialog_state = "dsConfirmed";
    else if (g_sip_state.early_dialog) dialog_state = "dsEarly";
    
    const char *transaction_state = g_sip_state.transaction_pending ? "tsProceeding" : "tsNone";
    
    const char *registration_state = "rsNotRegistered";
    if (g_sip_state.registered) registration_state = "rsRegistered";
    else if (g_sip_state.register_sent) registration_state = "rsRegistering";
    
    bool req_malformed = is_malformed_request(buf, len);
    bool method_creates_dialog = (strcmp(method_enum, "mINVITE") == 0 || 
                                  strcmp(method_enum, "mSUBSCRIBE") == 0);
    bool method_requires_dialog = (strcmp(method_enum, "mBYE") == 0 || 
                                   strcmp(method_enum, "mUPDATE") == 0);
    
    // Generate predicate line
    snprintf(out, out_sz,
             "sip_method=%s sip_status_class=scNotSet "
             "resp_code=0 cseq_number=%d expires_value=%d max_forwards=%d "
             "content_length=%d retry_after=0 "
             "req_malformed=%s resp_malformed=false "
             "has_to_tag=%s has_from_tag=%s has_call_id=%s has_cseq=%s "
             "has_via=%s has_contact=%s has_sdp=%s "
             "timeout=false connection_closed=%s "
             "dialog_established=%s dialog_terminated=%s early_dialog=%s "
             "invite_sent=%s invite_received=%s ack_sent=%s bye_sent=%s cancel_sent=%s "
             "transaction_pending=%s final_response_sent=%s provisional_sent=%s retransmission=false "
             "register_sent=%s registered=%s auth_required=%s auth_provided=%s "
             "invite_in_progress=%s method_requires_dialog=%s method_creates_dialog=%s "
             "cseq_matches=true callid_matches=true to_tag_matches=true from_tag_matches=true via_matches=true "
             "dialog_state=%s transaction_state=%s registration_state=%s",
             method_enum,
             cseq_num, expires, max_forwards, content_length,
             req_malformed ? "true" : "false",
             has_to_tag ? "true" : "false",
             (from_tag[0] != '\0') ? "true" : "false",
             has_call_id ? "true" : "false",
             has_cseq ? "true" : "false",
             has_via ? "true" : "false",
             has_contact ? "true" : "false",
             has_sdp ? "true" : "false",
             g_sip_state.connection_closed ? "true" : "false",
             g_sip_state.dialog_established ? "true" : "false",
             g_sip_state.dialog_terminated ? "true" : "false",
             g_sip_state.early_dialog ? "true" : "false",
             g_sip_state.invite_sent ? "true" : "false",
             g_sip_state.invite_received ? "true" : "false",
             g_sip_state.ack_sent ? "true" : "false",
             g_sip_state.bye_sent ? "true" : "false",
             g_sip_state.cancel_sent ? "true" : "false",
             g_sip_state.transaction_pending ? "true" : "false",
             g_sip_state.final_response_sent ? "true" : "false",
             g_sip_state.provisional_sent ? "true" : "false",
             g_sip_state.register_sent ? "true" : "false",
             g_sip_state.registered ? "true" : "false",
             g_sip_state.auth_required ? "true" : "false",
             g_sip_state.auth_provided ? "true" : "false",
             g_sip_state.invite_in_progress ? "true" : "false",
             method_requires_dialog ? "true" : "false",
             method_creates_dialog ? "true" : "false",
             dialog_state, transaction_state, registration_state);
}

void sip_build_response_pred_line(const unsigned char *buf,
                                   unsigned int len,
                                   char *out,
                                   size_t out_sz) {
    int resp_code = extract_response_code(buf, len);
    const char *status_class = map_status_class(resp_code);
    bool resp_malformed = is_malformed_response(buf, len);
    bool timeout = (len == 0);
    
    g_sip_state.last_resp_code = resp_code;
    
    // Extract headers
    char header_value[256];
    
    // Extract To tag from response
    char to_tag[64] = "";
    bool has_to_tag = false;
    if (find_header(buf, len, "To", header_value, sizeof(header_value))) {
        extract_tag(header_value, to_tag, sizeof(to_tag));
        has_to_tag = (to_tag[0] != '\0');
        if (has_to_tag) {
            strncpy(g_sip_state.to_tag, to_tag, sizeof(g_sip_state.to_tag) - 1);
        }
    }
    
    int content_length = 0;
    if (find_header(buf, len, "Content-Length", header_value, sizeof(header_value))) {
        content_length = atoi(header_value);
    }
    
    bool has_sdp = (content_length > 0 && strstr((const char*)buf, "application/sdp"));
    
    // Update state based on response
    if (resp_code >= 100 && resp_code < 200) {
        g_sip_state.transaction_pending = true;
        g_sip_state.provisional_sent = true;
        
        // Early dialog after 1xx with To tag
        if (has_to_tag && strcmp(g_sip_state.last_method, "mINVITE") == 0) {
            g_sip_state.early_dialog = true;
        }
    } else if (resp_code >= 200) {
        g_sip_state.final_response_sent = true;
        g_sip_state.transaction_pending = false;
        
        if (resp_code == 200) {
            // Dialog established after 200 OK to INVITE
            if (strcmp(g_sip_state.last_method, "mINVITE") == 0) {
                g_sip_state.dialog_established = true;
                g_sip_state.invite_in_progress = false;
            }
            
            // Registration successful
            if (strcmp(g_sip_state.last_method, "mREGISTER") == 0) {
                g_sip_state.registered = true;
                g_sip_state.auth_required = false;
            }
            
            // Dialog terminated after 200 OK to BYE
            if (strcmp(g_sip_state.last_method, "mBYE") == 0) {
                g_sip_state.dialog_terminated = true;
                g_sip_state.dialog_established = false;
            }
        } else if (resp_code == 401 || resp_code == 407) {
            g_sip_state.auth_required = true;
        } else if (resp_code == 487) {
            // Request terminated (after CANCEL)
            g_sip_state.invite_in_progress = false;
        }
    }
    
    // Determine states
    const char *dialog_state = "dsNone";
    if (g_sip_state.dialog_terminated) dialog_state = "dsTerminated";
    else if (g_sip_state.dialog_established) dialog_state = "dsConfirmed";
    else if (g_sip_state.early_dialog) dialog_state = "dsEarly";
    
    const char *transaction_state = "tsNone";
    if (g_sip_state.final_response_sent) transaction_state = "tsCompleted";
    else if (g_sip_state.transaction_pending) transaction_state = "tsProceeding";
    
    const char *registration_state = "rsNotRegistered";
    if (g_sip_state.registered) registration_state = "rsRegistered";
    else if (g_sip_state.register_sent) registration_state = "rsRegistering";
    
    // Generate predicate line
    snprintf(out, out_sz,
             "sip_method=%s sip_status_class=%s "
             "resp_code=%d cseq_number=%d expires_value=3600 max_forwards=70 "
             "content_length=%d retry_after=0 "
             "req_malformed=false resp_malformed=%s "
             "has_to_tag=%s has_from_tag=true has_call_id=true has_cseq=true "
             "has_via=true has_contact=%s has_sdp=%s "
             "timeout=%s connection_closed=%s "
             "dialog_established=%s dialog_terminated=%s early_dialog=%s "
             "invite_sent=%s invite_received=%s ack_sent=%s bye_sent=%s cancel_sent=%s "
             "transaction_pending=%s final_response_sent=%s provisional_sent=%s retransmission=false "
             "register_sent=%s registered=%s auth_required=%s auth_provided=%s "
             "invite_in_progress=%s method_requires_dialog=false method_creates_dialog=false "
             "cseq_matches=true callid_matches=true to_tag_matches=true from_tag_matches=true via_matches=true "
             "dialog_state=%s transaction_state=%s registration_state=%s",
             g_sip_state.last_method,
             status_class,
             resp_code, g_sip_state.cseq_number, content_length,
             resp_malformed ? "true" : "false",
             has_to_tag ? "true" : "false",
             (resp_code == 200) ? "true" : "false",
             has_sdp ? "true" : "false",
             timeout ? "true" : "false",
             g_sip_state.connection_closed ? "true" : "false",
             g_sip_state.dialog_established ? "true" : "false",
             g_sip_state.dialog_terminated ? "true" : "false",
             g_sip_state.early_dialog ? "true" : "false",
             g_sip_state.invite_sent ? "true" : "false",
             g_sip_state.invite_received ? "true" : "false",
             g_sip_state.ack_sent ? "true" : "false",
             g_sip_state.bye_sent ? "true" : "false",
             g_sip_state.cancel_sent ? "true" : "false",
             g_sip_state.transaction_pending ? "true" : "false",
             g_sip_state.final_response_sent ? "true" : "false",
             g_sip_state.provisional_sent ? "true" : "false",
             g_sip_state.register_sent ? "true" : "false",
             g_sip_state.registered ? "true" : "false",
             g_sip_state.auth_required ? "true" : "false",
             g_sip_state.auth_provided ? "true" : "false",
             g_sip_state.invite_in_progress ? "true" : "false",
             dialog_state, transaction_state, registration_state);
}
