// dtls_violation_check.c
// 
// Reads a monitor violation log, extracts hex traces, runs each through
// the DTLS predicate adapter, and emits evaluator-compatible "k=v k=v ..."
// lines on stdout — pipe straight into main.cpp.
//
// Usage:
//   ./dtls_violation_check < monitor_violations.log | ./monitor dtls.txt dtls
//   ./dtls_violation_check --violation 3 < monitor_violations.log | ./monitor dtls.txt dtls
//
// Reads msg_id=N dir=C2S|S2C trace=HEXHEX lines from stdin.
// Emits RESET as __END_SESSION__ between violations.

#include "dtls_predicate_adapter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

static int hex_to_bytes(const char *hex, unsigned char *out, int max_out)
{
    int len = 0;
    while (*hex && len < max_out) {
        while (*hex && isspace((unsigned char)*hex)) hex++;
        if (!*hex) break;
        char hi = *hex++;
        if (!*hex) break;
        char lo = *hex++;
        unsigned int byte;
        char tmp[3] = {hi, lo, '\0'};
        if (sscanf(tmp, "%02x", &byte) != 1) break;
        out[len++] = (unsigned char)byte;
    }
    return len;
}

/* Find "key=value" in a line, write value into dst. Returns true if found. */
static bool extract_field(const char *line, const char *key, char *dst, size_t dst_sz)
{
    size_t klen = strlen(key);
    const char *p = line;
    while ((p = strstr(p, key)) != NULL) {
        /* Check it's preceded by whitespace or start-of-line */
        if (p != line && !isspace((unsigned char)p[-1])) { p += klen; continue; }
        if (p[klen] != '=') { p += klen; continue; }
        const char *val = p + klen + 1;
        size_t i = 0;
        while (val[i] && !isspace((unsigned char)val[i]) && i < dst_sz - 1) {
            dst[i] = val[i];
            i++;
        }
        dst[i] = '\0';
        return true;
    }
    return false;
}

int main(int argc, char **argv)
{
    int target_violation = -1; /* -1 = all */
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--violation") == 0 || strcmp(argv[i], "-v") == 0) {
            if (i + 1 < argc) target_violation = atoi(argv[++i]);
        }
    }
    
    char line[131072];
    char pred_buf[8192];
    unsigned char pkt[65536];
    
    bool in_violation = false;
    bool in_packet_window = false;
    int current_violation = 0;
    bool emitting = false;
    bool session_started = false;
    
    while (fgets(line, sizeof(line), stdin)) {
        /* Detect violation header */
        if (strstr(line, "[VIOLATION #")) {
            int vid = 0;
            if (sscanf(strstr(line, "#") + 1, "%d", &vid) == 1) {
                current_violation = vid;
                in_violation = true;
                in_packet_window = false;
                emitting = (target_violation == -1 || target_violation == vid);
                
                if (emitting) {
                    if (session_started) {
                        printf("__END_SESSION__\n");
                    }
                    dtls_predicate_reset_session();
                    session_started = true;
                    fprintf(stderr, "--- Violation #%d ---\n", vid);
                }
            }
            continue;
        }
        
        /* Detect packet window section */
        if (strstr(line, "Recent packet window")) {
            in_packet_window = true;
            continue;
        }
        
        /* End of packet window: next violation header or blank section */
        if (in_packet_window && line[0] == '\n') {
            in_packet_window = false;
            continue;
        }
        
        if (!in_violation || !in_packet_window || !emitting) continue;
        
        /* Parse: msg_id=N dir=C2S|S2C trace=HEXHEX */
        char msg_id[32] = {0};
        char dir[8] = {0};
        char *trace_start = NULL;
        
        if (!extract_field(line, "msg_id", msg_id, sizeof(msg_id))) continue;
        if (!extract_field(line, "dir", dir, sizeof(dir))) continue;
        
        trace_start = strstr(line, "trace=");
        if (!trace_start) continue;
        trace_start += 6; /* skip "trace=" */
        
        /* Strip trailing whitespace from trace */
        size_t tlen = strlen(trace_start);
        while (tlen > 0 && isspace((unsigned char)trace_start[tlen-1]))
            trace_start[--tlen] = '\0';
        
        /* Empty trace = no data for this msg */
        if (tlen == 0) continue;
        
        int pkt_len = hex_to_bytes(trace_start, pkt, sizeof(pkt));
        if (pkt_len == 0) continue;
        
        int is_c2s = (strcmp(dir, "C2S") == 0);
        
        memset(pred_buf, 0, sizeof(pred_buf));
        if (is_c2s) {
            dtls_build_request_pred_line(pkt, pkt_len, pred_buf, sizeof(pred_buf));
        } else {
            dtls_build_response_pred_line(pkt, pkt_len, pred_buf, sizeof(pred_buf));
        }
        
        /* Output the predicate line (evaluator format) */
        printf("%s\n", pred_buf);
        fflush(stdout);
        
        /* Also print to stderr for visibility */
        /* Truncate trace from stderr output for readability */
        char *trace_in_pred = strstr(pred_buf, " msg_id=");
        if (trace_in_pred) *trace_in_pred = '\0';
        fprintf(stderr, "  [%s] %s %3d bytes → %s\n", msg_id, dir, pkt_len, pred_buf);
    }
    
    if (session_started) {
        printf("__END_SESSION__\n");
    }
    
    return 0;
}
