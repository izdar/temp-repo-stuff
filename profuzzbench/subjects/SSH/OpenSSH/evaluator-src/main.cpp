// main_continuous.cpp - Monitor that continues after violations
// With DTLS and SIP protocol support integrated
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <cctype>
#include <cassert>
#include <fstream>

#include "ast.h"
#include "ast_printer.h"
#include "memory_manager.h"
#include "typechecker.h"
#include "preprocess.h"
#include "evaluator.h"
#include "state.h"

extern FILE *yyin;
extern int yyparse();
extern Spec root;

// ============================================================================
// LOGGING CONFIGURATION
// ============================================================================
static const char* LOG_FILE_PATH = "./monitor.log";
static const char* VIOLATION_LOG_PATH = "./monitor_violations.log";
static std::ofstream g_log_file;
static std::ofstream g_violation_file;
static bool g_verbose = false;

// Recent packet trace references (for joining violations to raw bytes)
struct TraceRef {
    std::string msg_id;
    std::string dir;
    std::string trace;
};
static std::deque<TraceRef> g_recent_traces;
static const size_t TRACE_WINDOW = 20;

// Global state storage
struct EvaluatorState {
    int index;
    size_t event_count;
    size_t session_count;
    std::vector<BitVector> old_bv;
    std::vector<BitVector> new_bv;
};

std::unordered_map<unsigned int, EvaluatorState> saved_states;

static void init_logging() {
    const char* verbose_env = getenv("MONITOR_VERBOSE");
    g_verbose = (verbose_env && std::string(verbose_env) == "1");
    
    g_log_file.open(LOG_FILE_PATH, std::ios::out | std::ios::app);
    if (!g_log_file.is_open()) {
        std::cerr << "[MONITOR] WARNING: Could not open log file: " 
                  << LOG_FILE_PATH << std::endl;
    } else {
        g_log_file << "\n========================================\n";
        g_log_file << "[MONITOR] Started at " << time(nullptr) << "\n";
        g_log_file << "========================================\n";
        g_log_file.flush();
    }
    
    g_violation_file.open(VIOLATION_LOG_PATH, std::ios::out | std::ios::app);
    if (g_violation_file.is_open()) {
        g_violation_file << "\n=== New Monitor Session at " << time(nullptr) << " ===\n";
        g_violation_file.flush();
    }
}

static void log_msg(const std::string& msg, bool to_stderr = false) {
    if (g_log_file.is_open()) {
        g_log_file << msg << std::endl;
        g_log_file.flush();
    }
    if (to_stderr || g_verbose) {
        std::cerr << msg << std::endl;
    }
    return;
}

static void log_event(const std::unordered_map<std::string, std::string>& kv) {
    if (!g_verbose && !g_log_file.is_open()) return;
    
    std::ostringstream oss;
    oss << "[EVENT] ";
    bool first = true;
    for (const auto& kvp : kv) {
        if (!first) oss << ", ";
        first = false;
        oss << kvp.first << "=" << kvp.second;
    }
    log_msg(oss.str());
}

static inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::unordered_map<std::string,std::string>
parse_kv_line(const std::string& line) {
    std::unordered_map<std::string,std::string> kv;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) {
        auto eq = tok.find('=');
        if (eq == std::string::npos) continue;
        std::string k = tok.substr(0, eq);
        std::string v = tok.substr(eq + 1);
        kv[k] = v;
    }
    return kv;
}

static inline std::string getOr(const std::unordered_map<std::string,std::string>& kv,
                                const char* key) {
    auto it = kv.find(key);
    return (it == kv.end() ? "-" : it->second);
}

static void print_ssh_context(const std::unordered_map<std::string,std::string>& kv) {
    std::cerr << "ctx: proto=ssh"
              << " request=" << getOr(kv, "request")
              << " response=" << getOr(kv, "response")
              << " encrypted=" << getOr(kv, "encrypted")
              << " auth_attempts=" << getOr(kv, "auth_attempts")
              << "\n";
}

static void print_rtsp_context(const std::unordered_map<std::string,std::string>& kv) {
    std::cerr << "ctx: proto=rtsp"
              << " method=" << getOr(kv, "rtsp_method")
              << " status_class=" << getOr(kv, "status_class")
              << " resp_status_code=" << getOr(kv, "resp_status_code")
              << " req_cseq=" << getOr(kv, "req_cseq")
              << " resp_cseq=" << getOr(kv, "resp_cseq")
              << " session_established=" << getOr(kv, "session_established")
              << "\n";
}

static void print_dtls_context(const std::unordered_map<std::string,std::string>& kv) {
    std::cerr << "ctx: proto=dtls"
              << " request=" << getOr(kv, "request")
              << " response=" << getOr(kv, "response")
              << " content_type=" << getOr(kv, "content_type")
              << " encrypted=" << getOr(kv, "encrypted")
              << " epoch=" << getOr(kv, "epoch")
              << " handshake_complete=" << getOr(kv, "handshake_complete")
              << "\n";
}

static void print_sip_context(const std::unordered_map<std::string,std::string>& kv) {
    std::cerr << "ctx: proto=sip"
              << " msg_type=" << getOr(kv, "sip_msg_type")
              << " method=" << getOr(kv, "sip_method")
              << " status_code=" << getOr(kv, "sip_status_code")
              << " cseq=" << getOr(kv, "sip_cseq")
              << " cseq_method=" << getOr(kv, "sip_cseq_method")
              << " call_id=" << getOr(kv, "sip_call_id")
              << " from_tag=" << getOr(kv, "sip_from_tag")
              << " to_tag=" << getOr(kv, "sip_to_tag")
              << " dialog_state=" << getOr(kv, "dialog_state")
              << " transaction_state=" << getOr(kv, "transaction_state")
              << "\n";
    
    // Additional SIP-specific details
    std::cerr << "sip_details:"
              << " auth_used=" << getOr(kv, "auth_used")
              << " proxy_auth_used=" << getOr(kv, "proxy_auth_used")
              << " contact_present=" << getOr(kv, "contact_present")
              << " via_count=" << getOr(kv, "via_count")
              << " content_length=" << getOr(kv, "content_length")
              << " has_sdp=" << getOr(kv, "has_sdp")
              << "\n";
}

static void print_dnsmasq_context(const std::unordered_map<std::string,std::string>& kv) {
    std::cerr << "ctx: proto=dnsmasq"
              << " message_type=" << getOr(kv, "message_type")
              << " opcode=" << getOr(kv, "opcode")
              << " rcode=" << getOr(kv, "rcode")
              << " qtype=" << getOr(kv, "qtype")
              << " query_id=" << getOr(kv, "query_id")
              << " id_match=" << getOr(kv, "id_match")
              << "\n";
    
    // DNS flags
    std::cerr << "dns_flags:"
              << " rd=" << getOr(kv, "rd")
              << " ra=" << getOr(kv, "ra")
              << " aa=" << getOr(kv, "aa")
              << " tc=" << getOr(kv, "tc")
              << " ad=" << getOr(kv, "ad")
              << " cd=" << getOr(kv, "cd")
              << "\n";
    
    // DNS counts and cache
    std::cerr << "dns_details:"
              << " qdcount=" << getOr(kv, "qdcount")
              << " ancount=" << getOr(kv, "ancount")
              << " nscount=" << getOr(kv, "nscount")
              << " arcount=" << getOr(kv, "arcount")
              << " cache_hit=" << getOr(kv, "cache_hit")
              << " upstream_queried=" << getOr(kv, "upstream_queried")
              << " response_valid=" << getOr(kv, "response_valid")
              << " dnssec_ok=" << getOr(kv, "dnssec_ok")
              << "\n";
}

static void print_generic_context(const std::unordered_map<std::string,std::string>& kv,
                                  const std::string& proto_tag) {
    std::cerr << "ctx: proto=" << proto_tag << "\n";
    std::cerr << "kv: ";
    bool first = true;
    for (const auto& kvp : kv) {
        if (!first) std::cerr << " ";
        first = false;
        std::cerr << kvp.first << "=" << kvp.second;
    }
    std::cerr << "\n";
}

static void print_compact_context(const std::unordered_map<std::string,std::string>& kv,
                                  const std::string& proto_tag) {
    if (proto_tag == "ssh") {
        print_ssh_context(kv);
    } else if (proto_tag == "rtsp") {
        print_rtsp_context(kv);
    } else if (proto_tag == "dtls") {
        print_dtls_context(kv);
    } else if (proto_tag == "sip") {
        print_sip_context(kv);
    } else if (proto_tag == "dnsmasq" || proto_tag == "dns") {
        print_dnsmasq_context(kv);
    } else {
        print_generic_context(kv, proto_tag);
    }
    
    // Always dump full key-value pairs for debugging
    std::cerr << "full_kv: ";
    bool first = true;
    for (const auto& kvp : kv) {
        if (!first) std::cerr << " ";
        first = false;
        std::cerr << kvp.first << "=" << kvp.second;
    }
    std::cerr << "\n";
}

// Format a single event's KV pairs into a compact string for trace logging.
static std::string format_event_kv(const std::unordered_map<std::string,std::string>& kv) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& kvp : kv) {
        if (!first) oss << ", ";
        first = false;
        oss << kvp.first << "=" << kvp.second;
    }
    oss << "}";
    return oss.str();
}

// Dump violating trace in the style of the reference Fuzzer::runtime_monitor_dump.
// Writes violated rule indices + the full session trace to the violation log file
// and to stderr.
static void dump_violation_trace(
    size_t violation_number,
    const std::vector<size_t>& bad_idx,
    const std::vector<std::string>& prop_texts,
    const std::vector<std::string>& session_trace,
    const std::string& proto_tag)
{
    // Build the violated-rule-index string (matches reference: "0 2 5 ")
    std::string idx_str;
    for (size_t i : bad_idx) {
        idx_str += std::to_string(i) + " ";
    }

    // Build the full session trace string
    std::string trace_str;
    for (size_t i = 0; i < session_trace.size(); ++i) {
        trace_str += "(" + std::to_string(i) + ": " + session_trace[i] + ") ";
    }

    // --- Write to violation log file (primary record) ---
    if (g_violation_file.is_open()) {
        g_violation_file << "\n--- Violation #" << violation_number
                         << " [" << proto_tag << "] ---\n";
        g_violation_file << "Violated property indices: " << idx_str << "\n";

        // Print the property text for each violated index
        for (size_t i : bad_idx) {
            g_violation_file << "  Property[" << i << "]";
            if (i < prop_texts.size()) {
                g_violation_file << ": " << prop_texts[i];
            }
            g_violation_file << "\n";
        }

        g_violation_file << "Trace (" << session_trace.size() << " events):\n";
        for (size_t i = 0; i < session_trace.size(); ++i) {
            g_violation_file << "  [" << i << "] " << session_trace[i] << "\n";
        }
        g_violation_file.flush();
    }

    // --- Also write to the general monitor log ---
    log_msg("[VIOLATION_TRACE] indices: " + idx_str);
    log_msg("[VIOLATION_TRACE] trace_length: " + std::to_string(session_trace.size()));

    // --- Stderr summary ---
    std::cerr << "violated_indices: " << idx_str << "\n";
    std::cerr << "trace_length: " << session_trace.size() << "\n";
    if (session_trace.size() <= 30) {
        // Print full trace if short enough
        for (size_t i = 0; i < session_trace.size(); ++i) {
            std::cerr << "  [" << i << "] " << session_trace[i] << "\n";
        }
    } else {
        // Print first 10 and last 10
        for (size_t i = 0; i < 10; ++i) {
            std::cerr << "  [" << i << "] " << session_trace[i] << "\n";
        }
        std::cerr << "  ... (" << (session_trace.size() - 20) << " events omitted) ...\n";
        for (size_t i = session_trace.size() - 10; i < session_trace.size(); ++i) {
            std::cerr << "  [" << i << "] " << session_trace[i] << "\n";
        }
    }

    // --- Also write in the compact reference format to runtime_monitor.txt ---
    FILE *file = fopen("runtime_monitor.txt", "a");
    if (file) {
        fprintf(file, "%s", idx_str.c_str());
        fprintf(file, "%s", trace_str.c_str());
        fprintf(file, "\n");
        fclose(file);
    }
}

int main(int argc, char **argv) {
    init_logging();
    log_msg("[MONITOR] Initializing multi-protocol evaluator (continuous mode)...", true);
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <spec.ltl> [protocol_tag]\n";
        std::cerr << "  protocol_tag: ssh, rtsp, dtls, sip, dnsmasq, or generic (default: generic)\n";
        return 1;
    }

    const char* spec_path = argv[1];
    std::string proto_tag = (argc > 2) ? argv[2] : "generic";

    log_msg(std::string("[MONITOR] Loading spec: ") + spec_path, true);
    log_msg(std::string("[MONITOR] Protocol tag: ") + proto_tag, true);

    yyin = fopen(spec_path, "r");
    if (!yyin) {
        std::cerr << "Could not open spec: " << spec_path << std::endl;
        log_msg(std::string("[MONITOR] ERROR: Could not open spec: ") + spec_path, true);
        return 1;
    }

    log_msg("[MONITOR] Parsing LTL specification...");
    
    if (yyparse() != 0) {
        std::cerr << "Parsing failed." << std::endl;
        log_msg("[MONITOR] ERROR: LTL parsing failed", true);
        fclose(yyin);
        return 1;
    }

    log_msg("[MONITOR] Building type checker and evaluator...");
    
    TypeChecker typeChecker(root);
    Preprocessor preprocessor;
    std::vector<int> serials = preprocessor.DoPreProcess(root.second);
    Evaluator eval(root.second, serials);
    State ltl_state(&typeChecker);

    // Build property texts: verdicts[i] corresponds to root.second[i] directly.
    // (serials[i] are internal preprocessor node IDs, NOT indices into root.second.)
    std::vector<std::string> prop_texts;
    prop_texts.reserve(serials.size());
    
    for (size_t i = 0; i < serials.size(); ++i) {
        if (i < root.second.size()) {
            std::string txt = ASTPrinter::printStuff(root.second[i]);
            prop_texts.push_back(txt);
            log_msg("  Property[" + std::to_string(i) + "] " + txt);
        } else {
            prop_texts.emplace_back("<unknown property>");
            log_msg("  Property[" + std::to_string(i) + "] <unknown property>", true);
        }
    }

    fclose(yyin);

    log_msg(std::string("[MONITOR] Loaded ") + std::to_string(prop_texts.size()) + 
           " LTL properties for protocol: " + proto_tag, true);

    log_msg("[MONITOR] Ready to receive events (continuous mode - won't exit on violations)", true);

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string line;
    size_t event_count = 0;
    size_t session_count = 0;
    size_t total_violations = 0;
    
    // Accumulated trace of events in the current session (for violation dumps).
    // Each entry is the compact KV string for one event, in order.
    std::vector<std::string> session_trace;
    
    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line.empty()) continue;
        
        if (line.substr(0, 14) == "__SAVE_STATE__") {
            unsigned int snap_id = std::stoul(line.substr(15));
            
            EvaluatorState state;
            state.index = eval.get_index();
            state.event_count = event_count;
            state.session_count = session_count;
            state.old_bv = eval.get_old_bv();
            state.new_bv = eval.get_new_bv();
            
            saved_states[snap_id] = state;
            log_msg("[MONITOR] Saved state for snapshot " + std::to_string(snap_id));
            
            std::cout << "STATE_SAVED:" << snap_id << std::endl;
            std::cout.flush();
            continue;
        }
        
        if (line.substr(0, 17) == "__RESTORE_STATE__") {
            unsigned int snap_id = std::stoul(line.substr(18));
            
            auto it = saved_states.find(snap_id);
            if (it == saved_states.end()) {
                log_msg("[MONITOR] ERROR: No saved state for snapshot " + 
                        std::to_string(snap_id), true);
                std::cout << "STATE_RESTORE_FAILED:" << snap_id << std::endl;
                std::cout.flush();
                continue;
            }
            
            EvaluatorState &state = it->second;
            eval.set_index(state.index);
            eval.set_old_bv(state.old_bv);
            eval.set_new_bv(state.new_bv);
            event_count = state.event_count;
            session_count = state.session_count;
            
            // Truncate session trace back to the saved event count
            if (session_trace.size() > event_count) {
                session_trace.resize(event_count);
            }
            
            log_msg("[MONITOR] Restored state from snapshot " + std::to_string(snap_id));
            
            std::cout << "STATE_RESTORED:" << snap_id << std::endl;
            std::cout.flush();
            continue;
        }
        
        if (line == "__END_SESSION__") {
            session_count++;
            log_msg(std::string("[MONITOR] Session #") + std::to_string(session_count) + 
                   " ended. Events: " + std::to_string(event_count) +
                   ", Total violations so far: " + std::to_string(total_violations));
            eval.reset_evaluator();
            
            event_count = 0;
            session_trace.clear();  // Reset trace for next session
            continue;
        }

        auto kv = parse_kv_line(line);
        
        // Track the most recent raw-packet trace references, if present.
        if (kv.count("msg_id") && kv.count("trace")) {
            TraceRef tr;
            tr.msg_id = kv["msg_id"];
            tr.dir = kv.count("dir") ? kv["dir"] : "-";
            tr.trace = kv["trace"];
            g_recent_traces.push_back(std::move(tr));
            if (g_recent_traces.size() > TRACE_WINDOW) g_recent_traces.pop_front();
        }

        // IMPORTANT:
        // The evaluator/state machine must only see predicates that are defined in the
        // spec. We still *log* and *retain* extra metadata (msg_id/dir/trace) so we can
        // join violations back to raw packet blobs, but we must NOT pass these keys to
        // the LTL label state, otherwise the evaluator may throw on unknown predicates.
        static const std::unordered_set<std::string> kMetaKeys = {
            "msg_id", "dir", "trace"
        };

        State ltl_state(&typeChecker);

        event_count++;
        log_event(kv);
        
        // Record this event in the session trace (compact KV format)
        session_trace.push_back(format_event_kv(kv));

        // Protocol-agnostic derived predicates
        bool have_qid = false, have_respid = false;
        long qid = 0, respid = 0;

        for (const auto& kvp : kv) {
            const std::string& k = kvp.first;
            const std::string& v = kvp.second;
            if (k == "q_id")    { have_qid = true;    qid = std::stol(v); }
            if (k == "resp_id") { have_respid = true; respid = std::stol(v); }
        }

        if (!kv.count("id_mismatch") && have_qid && have_respid) {
            kv["id_mismatch"] = (qid != respid) ? "true" : "false";
        }

        for (const auto& kvp : kv) {
            if (kMetaKeys.find(kvp.first) != kMetaKeys.end()) continue;
            if (kvp.first.empty() || kvp.second.empty()) continue;
            ltl_state.addLabel(kvp.first, kvp.second);
        }

        assert(ltl_state.IsSane());
        std::vector<bool> verdicts = eval.EvaluateOneStep(&ltl_state);
        // ltl_state.clearState();

        std::vector<size_t> bad_idx;
        for (size_t i = 0; i < verdicts.size(); ++i) {
            if (!verdicts[i]) bad_idx.push_back(i);
        }

        if (!bad_idx.empty()) {
            bool is_valid_response = false;
            
            if (proto_tag == "dnsmasq" || proto_tag == "dns") {
                // DNS: response_valid=true means actual server response
                is_valid_response = (kv.count("response_valid") && 
                                    kv["response_valid"] == "true");
                
            } else if (proto_tag == "ssh") {
                // SSH: encrypted=true & mac_ok=true
                bool is_encrypted = (kv.count("encrypted") && kv["encrypted"] == "true");
                bool mac_valid = (kv.count("mac_ok") && kv["mac_ok"] == "true");
                is_valid_response = (is_encrypted && mac_valid);
                
            } else if (proto_tag == "rtsp") {
                // RTSP: Valid response (not timeout, not malformed, has status)
                bool not_timeout = (!kv.count("timeout") || kv["timeout"] == "false");
                bool has_status = (kv.count("status_class") && kv["status_class"] != "scNotSet");
                is_valid_response = (not_timeout && has_status);
                
            } else if (proto_tag == "dtls") {
                // DTLS: encrypted=true & mac_ok=true
                is_valid_response = (kv.count("response") && kv["response"] != "responseNotSet");
                
            } else if (proto_tag == "sip") {
                // SIP: msg_type=response & not timeout
                bool is_response = (kv.count("sip_msg_type") && kv["sip_msg_type"] == "response");
                bool not_timeout = (!kv.count("timeout") || kv["timeout"] == "false");
                is_valid_response = (is_response && not_timeout);
                
            } else if (proto_tag == "ftp") {
                // FTP: Valid response (not timeout, not malformed, has status)
                bool not_timeout = (!kv.count("timeout") || kv["timeout"] == "false");
                bool has_status = (kv.count("ftp_status_class") && kv["ftp_status_class"] != "scNotSet");
                is_valid_response = (not_timeout && has_status);
                
            } else {
                // Generic protocol: report all violations
                is_valid_response = true;
            }
            
            // Skip violations on invalid/garbage responses
            if (!is_valid_response) {
                if (g_verbose) {
                    std::ostringstream skip_msg;
                    skip_msg << "[MONITOR] Filtered violation on invalid response";
                    log_msg(skip_msg.str());
                }
                continue; // Skip to next event
            }

            total_violations++;
            std::cout << "VIOLATION_DETECTED:" << total_violations << std::endl;
            std::cout.flush();
            
            std::string viol_msg = std::string("[MONITOR] *** VIOLATION #") +
                                  std::to_string(total_violations) + " *** (" +
                                  std::to_string(bad_idx.size()) + " rule(s), event #" +
                                  std::to_string(event_count) + ", session #" +
                                  std::to_string(session_count) + ")";
            log_msg(viol_msg, true);

            std::cerr << "=== LTL VIOLATION #" << total_violations << " (" << bad_idx.size()
                      << (bad_idx.size() == 1 ? " rule" : " rules")
                      << ") ===\n";

            print_compact_context(kv, proto_tag);

            for (size_t i : bad_idx) {
                std::string rule_text = " - Property " + std::to_string(i);
                if (i < prop_texts.size()) {
                    rule_text += ": " + prop_texts[i];
                } else {
                    rule_text += ": <verdict index " + std::to_string(i) +
                                 " out of range, verdicts.size()=" +
                                 std::to_string(verdicts.size()) +
                                 ", prop_texts.size()=" +
                                 std::to_string(prop_texts.size()) + ">";
                }
                std::cerr << rule_text << "\n";
                log_msg(rule_text, true);
            }

            // Dump the full violating trace (matching reference implementation style)
            dump_violation_trace(total_violations, bad_idx,
                                prop_texts, session_trace, proto_tag);

            // Dump recent raw packet traces if available
            if (g_violation_file.is_open() && !g_recent_traces.empty()) {
                g_violation_file << "Recent packet window (" << TRACE_WINDOW << "):\n";
                for (const auto& tr : g_recent_traces) {
                    g_violation_file << "  msg_id=" << tr.msg_id
                                     << " dir=" << tr.dir
                                     << " trace=" << tr.trace << "\n";
                }
                g_violation_file.flush();
            }

            std::cerr << "=== Continuing monitoring... ===\n";
        }
    }

    log_msg(std::string("[MONITOR] Finished normally. Total sessions: ") + 
           std::to_string(session_count) + ", total events: " + 
           std::to_string(event_count) + ", total violations: " +
           std::to_string(total_violations), true);

    MemoryManager::freeSpec(root);
    
    if (g_log_file.is_open()) {
        g_log_file.close();
    }
    if (g_violation_file.is_open()) {
        g_violation_file.close();
    }
    
    return 0;
}