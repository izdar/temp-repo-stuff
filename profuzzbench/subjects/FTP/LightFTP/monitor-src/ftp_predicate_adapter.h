// ftp_predicate_adapter.h
#ifndef FTP_PREDICATE_ADAPTER_H
#define FTP_PREDICATE_ADAPTER_H

#include <stddef.h>
#include <stdbool.h>

/*
 * FTP predicate adapter for LightFTP semantic monitoring.
 *
 * Tracks FTP protocol state and generates predicate lines matching
 * the lightftp.txt LTL specification.
 *
 * Usage in aflnet.c extract functions:
 *   - Call ftp_build_command_pred_line() for client commands
 *   - Call ftp_build_response_pred_line() for server responses
 *   - Call ftp_predicate_reset_session() on new connection
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Reset adapter state for a new FTP session */
void ftp_predicate_reset_session(void);

/* Build predicate line from FTP command (client → server) */
void ftp_build_command_pred_line(const unsigned char *buf,
                                 unsigned int len,
                                 char *out,
                                 size_t out_sz);

/* Build predicate line from FTP response (server → client) */
void ftp_build_response_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* FTP_PREDICATE_ADAPTER_H */
