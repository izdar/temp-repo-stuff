// monitor_bridge.c - COMPLETE REPLACEMENT VERSION
#define _GNU_SOURCE
#include "monitor_bridge.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>  // NEW: for select()
#include <errno.h>
#include <signal.h>

monitor_handle_t *monitor_start(const char *eval_path,
                                const char *spec_path,
                                const char *protocol_tag)
{
    int pipefd_in[2];   // AFL -> monitor (stdin)
    int pipefd_out[2];  // monitor -> AFL (stdout) - NEW
    
    if (pipe(pipefd_in) < 0) {
        perror("monitor_start: pipe in");
        return NULL;
    }
    
    // NEW: Create second pipe for reading from monitor
    if (pipe(pipefd_out) < 0) {
        perror("monitor_start: pipe out");
        close(pipefd_in[0]);
        close(pipefd_in[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("monitor_start: fork");
        close(pipefd_in[0]);
        close(pipefd_in[1]);
        close(pipefd_out[0]);
        close(pipefd_out[1]);
        return NULL;
    }

    if (pid == 0) {
        // child: evaluator
        if (dup2(pipefd_in[0], STDIN_FILENO) < 0) {
            perror("monitor_start: dup2 stdin");
            _exit(127);
        }
        
        // NEW: Redirect monitor's stdout to pipe
        if (dup2(pipefd_out[1], STDOUT_FILENO) < 0) {
            perror("monitor_start: dup2 stdout");
            _exit(127);
        }
        
        close(pipefd_in[0]);
        close(pipefd_in[1]);
        close(pipefd_out[0]);
        close(pipefd_out[1]);

        // argv: eval_path spec_path protocol_tag
        execl(eval_path, eval_path, spec_path, protocol_tag, (char *)NULL);
        // If execl fails:
        perror("monitor_start: execl");
        _exit(127);
    }

    // parent
    close(pipefd_in[0]);
    close(pipefd_out[1]);  // NEW

    monitor_handle_t *h = (monitor_handle_t *)calloc(1, sizeof(*h));
    if (!h) {
        close(pipefd_in[1]);
        close(pipefd_out[0]);  // NEW
        kill(pid, SIGKILL);
        return NULL;
    }

    h->eval_pid = pid;
    h->eval_stdin = fdopen(pipefd_in[1], "w");
    if (!h->eval_stdin) {
        perror("monitor_start: fdopen stdin");
        close(pipefd_in[1]);
        close(pipefd_out[0]);  // NEW
        kill(pid, SIGKILL);
        free(h);
        return NULL;
    }

    // NEW: Open stdout for reading
    h->eval_stdout = fdopen(pipefd_out[0], "r");
    if (!h->eval_stdout) {
        perror("monitor_start: fdopen stdout");
        fclose(h->eval_stdin);
        close(pipefd_out[0]);
        kill(pid, SIGKILL);
        free(h);
        return NULL;
    }

    // line buffered
    setvbuf(h->eval_stdin, NULL, _IOLBF, 0);
    setvbuf(h->eval_stdout, NULL, _IOLBF, 0);  // NEW
    
    h->violation_detected = 0;  // NEW
    return h;
}

void monitor_emit_line(monitor_handle_t *h, const char *line)
{
    if (!h || !h->eval_stdin || !line) return;
    fprintf(h->eval_stdin, "%s\n", line);
    // line buffering should flush, but be safe:
    fflush(h->eval_stdin);
}

void monitor_end_session(monitor_handle_t *h)
{
    if (!h || !h->eval_stdin) return;
    fprintf(h->eval_stdin, "__END_SESSION__\n");
    fflush(h->eval_stdin);
    
    // NEW: Check for violation signal from monitor
    if (h->eval_stdout) {
        char response[256];
        fd_set readfds;
        struct timeval tv = {0, 10000}; // 10ms timeout
        
        FD_ZERO(&readfds);
        int fd = fileno(h->eval_stdout);
        FD_SET(fd, &readfds);
        
        int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(fd, &readfds)) {
            if (fgets(response, sizeof(response), h->eval_stdout)) {
                if (strstr(response, "VIOLATION_DETECTED")) {
                    h->violation_detected = 1;
                }
            }
        }
    }
}

// NEW FUNCTION
int monitor_check_violation(monitor_handle_t *h)
{
    if (!h) return 0;
    return h->violation_detected;
}

// NEW FUNCTION
void monitor_clear_violation(monitor_handle_t *h)
{
    if (h) h->violation_detected = 0;
}

int monitor_stop(monitor_handle_t *h)
{
    if (!h) return -1;

    if (h->eval_stdin) {
        fclose(h->eval_stdin);  // send EOF
        h->eval_stdin = NULL;
    }
    
    // NEW: Close stdout
    if (h->eval_stdout) {
        fclose(h->eval_stdout);
        h->eval_stdout = NULL;
    }

    int status = 0;
    if (h->eval_pid > 0) {
        if (waitpid(h->eval_pid, &status, 0) < 0) {
            perror("monitor_stop: waitpid");
        }
    }

    free(h);
    return status;
}

void monitor_save_bitvectors(monitor_handle_t *h, unsigned int snapshot_id) {
    if (!h || !h->eval_stdin) return;
    
    fprintf(h->eval_stdin, "__SAVE_STATE__ %u\n", snapshot_id);
    fflush(h->eval_stdin);
    
    // Optional: Wait for acknowledgment
    if (h->eval_stdout) {
        char ack[128];
        fd_set readfds;
        struct timeval tv = {0, 50000}; // 50ms timeout
        
        FD_ZERO(&readfds);
        int fd = fileno(h->eval_stdout);
        FD_SET(fd, &readfds);
        
        if (select(fd + 1, &readfds, NULL, NULL, &tv) > 0) {
            if (fgets(ack, sizeof(ack), h->eval_stdout)) {
                // State saved successfully
            }
        }
    }
}

void monitor_restore_bitvectors(monitor_handle_t *h, unsigned int snapshot_id) {
    if (!h || !h->eval_stdin) return;
    
    fprintf(h->eval_stdin, "__RESTORE_STATE__ %u\n", snapshot_id);
    fflush(h->eval_stdin);
    
    // Optional: Wait for acknowledgment
    if (h->eval_stdout) {
        char ack[128];
        fd_set readfds;
        struct timeval tv = {0, 50000}; // 50ms timeout
        
        FD_ZERO(&readfds);
        int fd = fileno(h->eval_stdout);
        FD_SET(fd, &readfds);
        
        if (select(fd + 1, &readfds, NULL, NULL, &tv) > 0) {
            if (fgets(ack, sizeof(ack), h->eval_stdout)) {
                // State restored successfully
            }
        }
    }
}