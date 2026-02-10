// ftp_trace_replay.c
// Standalone trace replayer for the FTP predicate adapter + LTL monitor.
//
// Usage:
//   ./ftp_trace_replay trace.txt                # just print predicate lines
//   ./ftp_trace_replay trace.txt | ./monitor spec.ltl ftp   # pipe to monitor
//   ./ftp_trace_replay --decode trace.txt       # also show decoded ASCII
//
// Trace file format (one per line):
//   msg_id=130065 dir=S2C trace=35333020506c...0d0a
//
// It decodes hex→bytes, routes C2S→ftp_build_command_pred_line,
// S2C→ftp_build_response_pred_line, and emits the predicate line to stdout.

#include "ftp_predicate_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// ---------- hex helpers ----------
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static unsigned int decode_hex(const char *hex, unsigned char *out, unsigned int max_out) {
    unsigned int n = 0;
    while (hex[0] && hex[1] && n < max_out) {
        int hi = hex_val(hex[0]);
        int lo = hex_val(hex[1]);
        if (hi < 0 || lo < 0) break;
        out[n++] = (unsigned char)((hi << 4) | lo);
        hex += 2;
    }
    return n;
}

// ---------- line parser ----------
// Extracts msg_id, dir, trace from a line like:
//   msg_id=130065 dir=S2C trace=35333020...0d0a
typedef struct {
    char msg_id[32];
    char dir[8];        // "C2S" or "S2C"
    char trace[8192];   // hex string
} TraceLine;

static bool parse_trace_line(const char *line, TraceLine *tl) {
    memset(tl, 0, sizeof(*tl));

    // skip leading whitespace
    while (*line && isspace((unsigned char)*line)) line++;
    if (!*line || *line == '#') return false;  // blank / comment

    // tokenize by whitespace, look for key=value
    const char *p = line;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        const char *tok_start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t tok_len = (size_t)(p - tok_start);

        // find '='
        const char *eq = memchr(tok_start, '=', tok_len);
        if (!eq) continue;

        size_t key_len = (size_t)(eq - tok_start);
        const char *val = eq + 1;
        size_t val_len = tok_len - key_len - 1;

        if (key_len == 6 && memcmp(tok_start, "msg_id", 6) == 0) {
            size_t n = val_len < sizeof(tl->msg_id) - 1 ? val_len : sizeof(tl->msg_id) - 1;
            memcpy(tl->msg_id, val, n);
            tl->msg_id[n] = '\0';
        } else if (key_len == 3 && memcmp(tok_start, "dir", 3) == 0) {
            size_t n = val_len < sizeof(tl->dir) - 1 ? val_len : sizeof(tl->dir) - 1;
            memcpy(tl->dir, val, n);
            tl->dir[n] = '\0';
        } else if (key_len == 5 && memcmp(tok_start, "trace", 5) == 0) {
            size_t n = val_len < sizeof(tl->trace) - 1 ? val_len : sizeof(tl->trace) - 1;
            memcpy(tl->trace, val, n);
            tl->trace[n] = '\0';
        }
    }

    return (tl->dir[0] != '\0' && tl->trace[0] != '\0');
}

// ---------- printable ASCII for --decode ----------
static void print_ascii(FILE *f, const unsigned char *buf, unsigned int len) {
    fprintf(f, "  ASCII: \"");
    for (unsigned int i = 0; i < len; i++) {
        if (buf[i] >= 0x20 && buf[i] < 0x7f)
            fputc(buf[i], f);
        else
            fprintf(f, "\\x%02x", buf[i]);
    }
    fprintf(f, "\"\n");
}

// ---------- main ----------
int main(int argc, char **argv) {
    bool decode_mode = false;
    const char *trace_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--decode") == 0 || strcmp(argv[i], "-d") == 0) {
            decode_mode = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr,
                "Usage: %s [--decode] <trace_file>\n"
                "       %s [--decode] < trace_file\n"
                "\n"
                "Replays FTP traces through the predicate adapter.\n"
                "Output predicate lines go to stdout (pipe to monitor).\n"
                "Diagnostic info goes to stderr.\n"
                "\n"
                "  --decode, -d   Also print decoded ASCII of each packet\n"
                "  --help, -h     This message\n"
                "\n"
                "Pipe to monitor:\n"
                "  %s trace.txt | ./monitor spec.ltl ftp\n",
                argv[0], argv[0], argv[0]);
            return 0;
        } else {
            trace_path = argv[i];
        }
    }

    FILE *in = stdin;
    if (trace_path) {
        in = fopen(trace_path, "r");
        if (!in) {
            fprintf(stderr, "ERROR: Cannot open %s\n", trace_path);
            return 1;
        }
    }

    ftp_predicate_reset_session();

    char line_buf[16384];
    int line_no = 0;
    int msg_count = 0;
    int violation_hint = 0;  // not real violations, just a counter for clarity

    fprintf(stderr, "=== FTP Trace Replayer ===\n");
    fprintf(stderr, "Reading trace%s%s...\n",
            trace_path ? " from " : " from stdin",
            trace_path ? trace_path : "");
    fprintf(stderr, "Predicate lines go to stdout.\n\n");

    while (fgets(line_buf, sizeof(line_buf), in)) {
        line_no++;
        TraceLine tl;
        if (!parse_trace_line(line_buf, &tl)) continue;

        msg_count++;

        // Decode hex to raw bytes
        unsigned char raw[4096];
        unsigned int raw_len = decode_hex(tl.trace, raw, sizeof(raw));

        // Build predicate line
        char pred_line[8192];
        memset(pred_line, 0, sizeof(pred_line));

        bool is_command = (strcmp(tl.dir, "C2S") == 0);

        if (is_command) {
            ftp_build_command_pred_line(raw, raw_len, pred_line, sizeof(pred_line));
        } else {
            ftp_build_response_pred_line(raw, raw_len, pred_line, sizeof(pred_line));
        }

        // Strip trailing metadata (msg_id=, dir=, trace=) that the adapter
        // appends via append_packet_trace -- the monitor doesn't know these.
        char *cut = strstr(pred_line, " msg_id=");
        if (cut) *cut = '\0';

        // --- stderr diagnostics ---
        fprintf(stderr, "--- msg #%d (msg_id=%s, %s, %u bytes) ---\n",
                msg_count, tl.msg_id, tl.dir, raw_len);

        if (decode_mode) {
            print_ascii(stderr, raw, raw_len);
        }

        fprintf(stderr, "  PRED: %s\n\n", pred_line);

        // --- stdout: the predicate line the monitor reads ---
        printf("%s\n", pred_line);
        fflush(stdout);
    }

    fprintf(stderr, "=== Done: %d messages processed ===\n", msg_count);

    if (in != stdin) fclose(in);
    return 0;
}