// monitor_bridge.h - COMPLETE REPLACEMENT VERSION
#ifndef MONITOR_BRIDGE_H
#define MONITOR_BRIDGE_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

/*
 * Bridge to the external LTL evaluator with violation detection.
 *
 * Usage:
 *
 *   monitor_handle_t *h = monitor_start("/usr/local/bin/ltl_evaluator",
 *                                       "/home/ubuntu/monitor/ssh_semantics_reqresp.ltl",
 *                                       "ssh");
 *
 *   monitor_emit_line(h, "request=c2s_kexinit encrypted=false ...");
 *   ...
 *   monitor_end_session(h);  // sends "__END_SESSION__" and checks for violations
 *   
 *   if (monitor_check_violation(h)) {
 *       // Violation detected! Save test case
 *       monitor_clear_violation(h);
 *   }
 *   
 *   monitor_stop(h);         // closes pipe, waits for evaluator
 */

typedef struct monitor_handle {
    FILE *eval_stdin;          // Write predicates to monitor
    FILE *eval_stdout;         // Read violation signals from monitor (NEW)
    pid_t eval_pid;
    int violation_detected;    // Flag: 1 if violation in current session (NEW)
} monitor_handle_t;

/* Start evaluator process: eval_path spec_path protocol_tag.
 * Returns NULL on failure.
 */
monitor_handle_t *monitor_start(const char *eval_path,
                                const char *spec_path,
                                const char *protocol_tag);

/* Emit a single predicate line "k=v k=v ..." to evaluator. */
void monitor_emit_line(monitor_handle_t *h, const char *line);

/* Mark end of one logical test sequence and check for violations. */
void monitor_end_session(monitor_handle_t *h);

/* Tear down evaluator; returns its exit status (as from waitpid). */
int monitor_stop(monitor_handle_t *h);

/* Check if a violation was detected in the last session. (NEW) */
int monitor_check_violation(monitor_handle_t *h);

/* Clear the violation flag after handling it. (NEW) */
void monitor_clear_violation(monitor_handle_t *h);

void monitor_save_bitvectors(monitor_handle_t *h, unsigned int snapshot_id);
void monitor_restore_bitvectors(monitor_handle_t *h, unsigned int snapshot_id);

#endif /* MONITOR_BRIDGE_H */