// dtls_trace_replay.c - Replay hex traces through the DTLS predicate adapter
//
// Usage:
//   ./dtls_trace_replay < traces.txt
//   echo "C2S 16fefd..." | ./dtls_trace_replay
//
// Input format (one per line):
//   RESET                          - reset session state
//   C2S <hex>                      - client-to-server record
//   S2C <hex>                      - server-to-client record
//   # comment                      - ignored
//
// Output: the full predicate line the evaluator would see

#include "dtls_predicate_adapter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int hex_to_bytes(const char *hex, unsigned char *out, int max_out)
{
    int len = 0;
    while (*hex && len < max_out) {
        /* skip whitespace */
        while (*hex && isspace((unsigned char)*hex)) hex++;
        if (!*hex) break;
        
        char hi = *hex++;
        if (!*hex) break;
        char lo = *hex++;
        
        unsigned int byte;
        if (sscanf((char[]){hi, lo, '\0'}, "%02x", &byte) != 1)
            break;
        out[len++] = (unsigned char)byte;
    }
    return len;
}

int main(int argc, char **argv)
{
    char line[65536];
    char pred_buf[8192];
    unsigned char pkt[65536];
    int event = 0;
    
    dtls_predicate_reset_session();
    
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        fprintf(stderr, "Usage: %s < traces.txt\n", argv[0]);
        fprintf(stderr, "Input: RESET | C2S <hex> | S2C <hex> | # comment\n");
        return 0;
    }
    
    while (fgets(line, sizeof(line), stdin)) {
        /* strip trailing newline */
        size_t slen = strlen(line);
        while (slen > 0 && (line[slen-1] == '\n' || line[slen-1] == '\r'))
            line[--slen] = '\0';
        
        /* skip empty lines and comments */
        if (slen == 0 || line[0] == '#') continue;
        
        if (strncmp(line, "RESET", 5) == 0) {
            dtls_predicate_reset_session();
            printf("--- SESSION RESET ---\n");
            event = 0;
            continue;
        }
        
        int is_c2s = -1;
        const char *hex_start = NULL;
        
        if (strncmp(line, "C2S ", 4) == 0) {
            is_c2s = 1;
            hex_start = line + 4;
        } else if (strncmp(line, "S2C ", 4) == 0) {
            is_c2s = 0;
            hex_start = line + 4;
        } else {
            fprintf(stderr, "WARNING: skipping unrecognized line: %.40s...\n", line);
            continue;
        }
        
        /* skip leading whitespace in hex */
        while (*hex_start && isspace((unsigned char)*hex_start)) hex_start++;
        
        int pkt_len = hex_to_bytes(hex_start, pkt, sizeof(pkt));
        
        event++;
        memset(pred_buf, 0, sizeof(pred_buf));
        
        if (is_c2s) {
            dtls_build_request_pred_line(pkt, pkt_len, pred_buf, sizeof(pred_buf));
        } else {
            dtls_build_response_pred_line(pkt, pkt_len, pred_buf, sizeof(pred_buf));
        }
        
        /* Print the predicate line, stripping the trace= suffix for readability */
        char *trace_pos = strstr(pred_buf, " msg_id=");
        
        printf("[%3d] %s %d bytes\n", event, is_c2s ? "C2S" : "S2C", pkt_len);
        
        if (trace_pos) {
            /* print predicates without trace blob */
            *trace_pos = '\0';
            printf("      PRED: %s\n", pred_buf);
        } else {
            printf("      PRED: %s\n", pred_buf);
        }
        
        /* Also print the raw hex header for quick visual check */
        if (pkt_len >= 13) {
            printf("      HDR:  ct=%d ver=%02x%02x epoch=%d seq=%d rlen=%d",
                   pkt[0], pkt[1], pkt[2],
                   (pkt[3] << 8) | pkt[4],
                   (int)pkt[9] << 8 | pkt[10],  /* low 2 bytes of seq */
                   (pkt[11] << 8) | pkt[12]);
            if (pkt[0] == 22 && pkt_len >= 14) {
                printf(" hs_type=%d", pkt[13]);
            }
            printf("\n");
        }
        printf("\n");
    }
    
    return 0;
}
