// ftp_predicate_adapter.c
#include "ftp_predicate_adapter.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================================
// Session State Tracking
// ============================================================================

static struct {
    // Authentication
    bool user_logged_in;
    bool user_sent;
    bool pass_sent;
    bool login_successful;
    bool login_failed;
    
    // Data connection
    bool port_sent;
    bool pasv_sent;
    bool pasv_response_received;
    bool port_accepted;
    bool data_connection_open;
    
    // Transfer
    bool retr_sent;
    bool stor_sent;
    bool transfer_started;
    bool transfer_complete;
    bool transfer_aborted;
    bool transfer_in_progress;
    
    // Rename
    bool rnfr_sent;
    bool rnfr_accepted;
    bool rnto_sent;
    
    // Session
    bool session_initialized;
    bool quit_sent;
    bool reinit_sent;
    bool connection_closed;
    
    // Counters
    int sequence_number;
    int rest_position;
    
    // Last command for state transitions
    char last_command[16];
} g_ftp_state;

void ftp_predicate_reset_session(void) {
    memset(&g_ftp_state, 0, sizeof(g_ftp_state));
    strcpy(g_ftp_state.last_command, "cmdNotSet");
}

// ============================================================================
// Helper Functions
// ============================================================================

static inline void skip_ws(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

static void extract_command(const unsigned char *buf, unsigned int len,
                           char *cmd, size_t cmd_sz) {
    size_t i = 0;
    while (i < len && i < cmd_sz - 1 && 
           buf[i] != ' ' && buf[i] != '\r' && buf[i] != '\n') {
        cmd[i] = toupper(buf[i]);
        i++;
    }
    cmd[i] = '\0';
}

static const char* map_ftp_command(const char *cmd) {
    if (strcmp(cmd, "USER") == 0) return "cmdUSER";
    if (strcmp(cmd, "PASS") == 0) return "cmdPASS";
    if (strcmp(cmd, "ACCT") == 0) return "cmdACCT";
    if (strcmp(cmd, "CWD") == 0) return "cmdCWD";
    if (strcmp(cmd, "CDUP") == 0) return "cmdCDUP";
    if (strcmp(cmd, "SMNT") == 0) return "cmdSMNT";
    if (strcmp(cmd, "QUIT") == 0) return "cmdQUIT";
    if (strcmp(cmd, "REIN") == 0) return "cmdREIN";
    if (strcmp(cmd, "PORT") == 0) return "cmdPORT";
    if (strcmp(cmd, "PASV") == 0) return "cmdPASV";
    if (strcmp(cmd, "TYPE") == 0) return "cmdTYPE";
    if (strcmp(cmd, "STRU") == 0) return "cmdSTRU";
    if (strcmp(cmd, "MODE") == 0) return "cmdMODE";
    if (strcmp(cmd, "RETR") == 0) return "cmdRETR";
    if (strcmp(cmd, "STOR") == 0) return "cmdSTOR";
    if (strcmp(cmd, "STOU") == 0) return "cmdSTOU";
    if (strcmp(cmd, "APPE") == 0) return "cmdAPPE";
    if (strcmp(cmd, "ALLO") == 0) return "cmdALLO";
    if (strcmp(cmd, "REST") == 0) return "cmdREST";
    if (strcmp(cmd, "RNFR") == 0) return "cmdRNFR";
    if (strcmp(cmd, "RNTO") == 0) return "cmdRNTO";
    if (strcmp(cmd, "ABOR") == 0) return "cmdABOR";
    if (strcmp(cmd, "DELE") == 0) return "cmdDELE";
    if (strcmp(cmd, "RMD") == 0) return "cmdRMD";
    if (strcmp(cmd, "MKD") == 0) return "cmdMKD";
    if (strcmp(cmd, "PWD") == 0) return "cmdPWD";
    if (strcmp(cmd, "LIST") == 0) return "cmdLIST";
    if (strcmp(cmd, "NLST") == 0) return "cmdNLST";
    if (strcmp(cmd, "SITE") == 0) return "cmdSITE";
    if (strcmp(cmd, "SYST") == 0) return "cmdSYST";
    if (strcmp(cmd, "STAT") == 0) return "cmdSTAT";
    if (strcmp(cmd, "HELP") == 0) return "cmdHELP";
    if (strcmp(cmd, "NOOP") == 0) return "cmdNOOP";
    if (strcmp(cmd, "FEAT") == 0) return "cmdFEAT";
    if (strcmp(cmd, "OPTS") == 0) return "cmdOPTS";
    if (strcmp(cmd, "AUTH") == 0) return "cmdAUTH";
    if (strcmp(cmd, "PBSZ") == 0) return "cmdPBSZ";
    if (strcmp(cmd, "PROT") == 0) return "cmdPROT";
    if (strcmp(cmd, "EPRT") == 0) return "cmdEPRT";
    if (strcmp(cmd, "EPSV") == 0) return "cmdEPSV";
    if (strcmp(cmd, "SIZE") == 0) return "cmdSIZE";
    if (strcmp(cmd, "MLSD") == 0) return "cmdMLSD";
    if (strcmp(cmd, "MLST") == 0) return "cmdMLST";
    return "cmdNotSet";
}

static int extract_response_code(const unsigned char *buf, unsigned int len) {
    if (len < 3) return 0;
    if (!isdigit(buf[0]) || !isdigit(buf[1]) || !isdigit(buf[2])) return 0;
    return (buf[0] - '0') * 100 + (buf[1] - '0') * 10 + (buf[2] - '0');
}

static const char* map_status_class(int code) {
    if (code == 0) return "scNotSet";
    if (code >= 100 && code < 200) return "scPreliminary";
    if (code >= 200 && code < 300) return "scSuccess";
    if (code >= 300 && code < 400) return "scIntermediate";
    if (code >= 400 && code < 500) return "scTransientError";
    if (code >= 500 && code < 600) return "scPermanentError";
    return "scNotSet";
}

static const char* get_auth_state(void) {
    if (g_ftp_state.user_logged_in) return "authComplete";
    if (g_ftp_state.login_failed) return "authFailed";
    if (g_ftp_state.pass_sent) return "authPasswordSent";
    if (g_ftp_state.user_sent) return "authUserSent";
    return "authNone";
}

static const char* get_data_state(void) {
    if (g_ftp_state.data_connection_open) return "dataActive";
    if (g_ftp_state.port_sent) return "dataPORT";
    if (g_ftp_state.pasv_sent) return "dataPASV";
    return "dataNotSet";
}

static bool is_cmd_malformed(const unsigned char *buf, unsigned int len) {
    if (len == 0) return true;
    if (len > 512) return true; // FTP commands shouldn't be this long
    
    // Check for null bytes in middle
    for (unsigned int i = 0; i < len - 1; i++) {
        if (buf[i] == '\0') return true;
    }
    
    // Extract and validate command
    char cmd[16];
    extract_command(buf, len, cmd, sizeof(cmd));
    if (strlen(cmd) == 0) return true;
    
    return false;
}

static bool is_resp_malformed(const unsigned char *buf, unsigned int len) {
    if (len < 4) return true; // Min: "220\r\n"
    if (!isdigit(buf[0]) || !isdigit(buf[1]) || !isdigit(buf[2])) return true;
    return false;
}

// ============================================================================
// Public API
// ============================================================================

void ftp_build_command_pred_line(const unsigned char *buf,
                                 unsigned int len,
                                 char *out,
                                 size_t out_sz) {
    // Extract command
    char cmd_str[16];
    extract_command(buf, len, cmd_str, sizeof(cmd_str));
    const char *cmd_enum = map_ftp_command(cmd_str);
    
    // Save last command
    strncpy(g_ftp_state.last_command, cmd_enum, sizeof(g_ftp_state.last_command) - 1);
    
    // Update state based on command
    if (strcmp(cmd_enum, "cmdUSER") == 0) {
        g_ftp_state.user_sent = true;
    } else if (strcmp(cmd_enum, "cmdPASS") == 0) {
        g_ftp_state.pass_sent = true;
    } else if (strcmp(cmd_enum, "cmdPORT") == 0) {
        g_ftp_state.port_sent = true;
        g_ftp_state.pasv_sent = false;
    } else if (strcmp(cmd_enum, "cmdPASV") == 0) {
        g_ftp_state.pasv_sent = true;
        g_ftp_state.port_sent = false;
    } else if (strcmp(cmd_enum, "cmdRETR") == 0) {
        g_ftp_state.retr_sent = true;
    } else if (strcmp(cmd_enum, "cmdSTOR") == 0) {
        g_ftp_state.stor_sent = true;
    } else if (strcmp(cmd_enum, "cmdRNFR") == 0) {
        g_ftp_state.rnfr_sent = true;
    } else if (strcmp(cmd_enum, "cmdRNTO") == 0) {
        g_ftp_state.rnto_sent = true;
    } else if (strcmp(cmd_enum, "cmdABOR") == 0) {
        g_ftp_state.transfer_aborted = true;
        g_ftp_state.transfer_in_progress = false;
    } else if (strcmp(cmd_enum, "cmdQUIT") == 0) {
        g_ftp_state.quit_sent = true;
    } else if (strcmp(cmd_enum, "cmdREIN") == 0) {
        g_ftp_state.reinit_sent = true;
    } else if (strcmp(cmd_enum, "cmdREST") == 0) {
        // Extract restart position
        const char *p = (const char *)buf + strlen(cmd_str);
        skip_ws(&p);
        g_ftp_state.rest_position = atoi(p);
    }
    
    g_ftp_state.sequence_number++;
    
    bool cmd_malformed = is_cmd_malformed(buf, len);
    bool timeout = (len == 0);
    
    // Generate predicate line
    snprintf(out, out_sz,
             "ftp_command=%s ftp_status_class=scNotSet "
             "resp_code=0 sequence_number=%d port_number=0 file_size=0 rest_position=%d "
             "cmd_malformed=%s resp_malformed=false "
             "user_logged_in=%s data_connection_open=%s transfer_in_progress=%s "
             "timeout=%s connection_closed=%s "
             "user_sent=%s pass_sent=%s login_successful=%s login_failed=%s "
             "port_sent=%s pasv_sent=%s pasv_response_received=%s port_accepted=%s "
             "retr_sent=%s stor_sent=%s transfer_started=%s transfer_complete=%s transfer_aborted=%s "
             "rnfr_sent=%s rnfr_accepted=%s rnto_sent=%s "
             "session_initialized=%s quit_sent=%s reinit_sent=%s "
             "auth_state=%s data_state=%s transfer_type=typeNotSet",
             cmd_enum,
             g_ftp_state.sequence_number,
             g_ftp_state.rest_position,
             cmd_malformed ? "true" : "false",
             g_ftp_state.user_logged_in ? "true" : "false",
             g_ftp_state.data_connection_open ? "true" : "false",
             g_ftp_state.transfer_in_progress ? "true" : "false",
             timeout ? "true" : "false",
             g_ftp_state.connection_closed ? "true" : "false",
             g_ftp_state.user_sent ? "true" : "false",
             g_ftp_state.pass_sent ? "true" : "false",
             g_ftp_state.login_successful ? "true" : "false",
             g_ftp_state.login_failed ? "true" : "false",
             g_ftp_state.port_sent ? "true" : "false",
             g_ftp_state.pasv_sent ? "true" : "false",
             g_ftp_state.pasv_response_received ? "true" : "false",
             g_ftp_state.port_accepted ? "true" : "false",
             g_ftp_state.retr_sent ? "true" : "false",
             g_ftp_state.stor_sent ? "true" : "false",
             g_ftp_state.transfer_started ? "true" : "false",
             g_ftp_state.transfer_complete ? "true" : "false",
             g_ftp_state.transfer_aborted ? "true" : "false",
             g_ftp_state.rnfr_sent ? "true" : "false",
             g_ftp_state.rnfr_accepted ? "true" : "false",
             g_ftp_state.rnto_sent ? "true" : "false",
             g_ftp_state.session_initialized ? "true" : "false",
             g_ftp_state.quit_sent ? "true" : "false",
             g_ftp_state.reinit_sent ? "true" : "false",
             get_auth_state(),
             get_data_state());
}

void ftp_build_response_pred_line(const unsigned char *buf,
                                  unsigned int len,
                                  char *out,
                                  size_t out_sz) {
    int resp_code = extract_response_code(buf, len);
    const char *status_class = map_status_class(resp_code);
    bool resp_malformed = is_resp_malformed(buf, len);
    bool timeout = (len == 0);
    
    // Update state based on response
    if (resp_code == 220 && g_ftp_state.sequence_number == 0) {
        g_ftp_state.session_initialized = true;
    } else if (resp_code == 230) {
        g_ftp_state.user_logged_in = true;
        g_ftp_state.login_successful = true;
    } else if (resp_code == 530) {
        g_ftp_state.login_failed = true;
        g_ftp_state.user_logged_in = false;
    } else if (resp_code == 200 && strcmp(g_ftp_state.last_command, "cmdPORT") == 0) {
        g_ftp_state.port_accepted = true;
    } else if (resp_code == 227) {
        g_ftp_state.pasv_response_received = true;
    } else if (resp_code == 150) {
        g_ftp_state.transfer_started = true;
        g_ftp_state.transfer_in_progress = true;
        g_ftp_state.data_connection_open = true;
    } else if (resp_code == 226) {
        g_ftp_state.transfer_complete = true;
        g_ftp_state.transfer_in_progress = false;
        g_ftp_state.data_connection_open = false;
    } else if (resp_code == 350 && strcmp(g_ftp_state.last_command, "cmdRNFR") == 0) {
        g_ftp_state.rnfr_accepted = true;
    } else if (resp_code == 220 && strcmp(g_ftp_state.last_command, "cmdREIN") == 0) {
        // REIN resets session
        ftp_predicate_reset_session();
        g_ftp_state.session_initialized = true;
    } else if (resp_code == 221) {
        g_ftp_state.connection_closed = true;
    }
    
    // Generate predicate line
    snprintf(out, out_sz,
             "ftp_command=%s ftp_status_class=%s "
             "resp_code=%d sequence_number=%d port_number=0 file_size=0 rest_position=%d "
             "cmd_malformed=false resp_malformed=%s "
             "user_logged_in=%s data_connection_open=%s transfer_in_progress=%s "
             "timeout=%s connection_closed=%s "
             "user_sent=%s pass_sent=%s login_successful=%s login_failed=%s "
             "port_sent=%s pasv_sent=%s pasv_response_received=%s port_accepted=%s "
             "retr_sent=%s stor_sent=%s transfer_started=%s transfer_complete=%s transfer_aborted=%s "
             "rnfr_sent=%s rnfr_accepted=%s rnto_sent=%s "
             "session_initialized=%s quit_sent=%s reinit_sent=%s "
             "auth_state=%s data_state=%s transfer_type=typeNotSet",
             g_ftp_state.last_command,
             status_class,
             resp_code,
             g_ftp_state.sequence_number,
             g_ftp_state.rest_position,
             resp_malformed ? "true" : "false",
             g_ftp_state.user_logged_in ? "true" : "false",
             g_ftp_state.data_connection_open ? "true" : "false",
             g_ftp_state.transfer_in_progress ? "true" : "false",
             timeout ? "true" : "false",
             g_ftp_state.connection_closed ? "true" : "false",
             g_ftp_state.user_sent ? "true" : "false",
             g_ftp_state.pass_sent ? "true" : "false",
             g_ftp_state.login_successful ? "true" : "false",
             g_ftp_state.login_failed ? "true" : "false",
             g_ftp_state.port_sent ? "true" : "false",
             g_ftp_state.pasv_sent ? "true" : "false",
             g_ftp_state.pasv_response_received ? "true" : "false",
             g_ftp_state.port_accepted ? "true" : "false",
             g_ftp_state.retr_sent ? "true" : "false",
             g_ftp_state.stor_sent ? "true" : "false",
             g_ftp_state.transfer_started ? "true" : "false",
             g_ftp_state.transfer_complete ? "true" : "false",
             g_ftp_state.transfer_aborted ? "true" : "false",
             g_ftp_state.rnfr_sent ? "true" : "false",
             g_ftp_state.rnfr_accepted ? "true" : "false",
             g_ftp_state.rnto_sent ? "true" : "false",
             g_ftp_state.session_initialized ? "true" : "false",
             g_ftp_state.quit_sent ? "true" : "false",
             g_ftp_state.reinit_sent ? "true" : "false",
             get_auth_state(),
             get_data_state());
}
