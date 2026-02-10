# LIVE555 RTSP Fuzzer — Trace IDs for Bug Reproduction

Source: `last300.log` (482 violations across 84 sessions)
Timestamp range: `1770085095` (Tue Feb 03 2026 ~02:18 UTC)


## Bug 1: Corrupted Transport Acceptance (HIGH)

**What:** Fuzzed SETUP with byte `0x89` replacing `'r'` in `client_port` →
LIVE555 fails to parse, assigns `client_port=0-1; server_port=0-0` instead of
rejecting with 400 Bad Request. Creates zombie sessions incapable of RTP delivery.

**17 instances (deduplicated)**

### Trace Pairs (C2S request → S2C response)

| # | Request msg_id | Response msg_id | Session |
|---|----------------|-----------------|---------|
| 1 | 62252176 | 62252177 | #1464784 |
| 2 | 62252236 | 62252237 | #1464784 |
| 3 | 62252356 | 62252357 | #1464786 |
| 4 | 62252416 | 62252417 | #1464787 |
| 5 | 62252476 | 62252477 | #1464788 |
| 6 | 62252638 | 62252639 | #1464792 |
| 7 | 62252698 | 62252699 | #1464793 |
| 8 | 62252818 | 62252819 | #1464795 |
| 9 | 62252878 | 62252879 | #1464796 |
| 10 | 62252938 | 62252939 | #1464797 |
| 11 | 62253178 | 62253179 | #1464801 |
| 12 | 62253298 | 62253299 | #1464803 |
| 13 | 62253358 | 62253359 | #1464804 |
| 14 | 62253418 | 62253419 | #1464805 |
| 15 | 62253538 | 62253539 | #1464807 |
| 16 | 62263085 | 62263086 | #1465088 |
| 17 | 62263355 | 62263356 | #1465096 |

### Reproduction Template

```
C2S: SETUP rtsp://127.0.0.1:8554/mp3AudioTest/track1 RTSP/1.0\r\n
     CSeq: 3\r\n
     Transport: RTP/AVP;unicast;client_po\x89t=43712-43713\r\n
     \r\n

S2C: RTSP/1.0 201 OK\r\n               ← should be 400 Bad Request
     Transport: ...client_port=0-1;server_port=0-0\r\n
     Session: 000022B8;timeout=65\r\n
```

**Best single trace for repro:** msg_id `62252176` → `62252177`


## Bug 2: CSeq Garbage Echo (LOW)

**What:** Server echoes unparseable bytes from fuzzed CSeq fields instead of
rejecting. RFC 2326 §12.17 requires `CSeq = "CSeq" ":" 1*DIGIT`.

**243 instances (deduplicated)**

### Garbage Values

| Garbage CSeq Value | Count | Sample msg_ids |
|--------------------|-------|----------------|
| `+P: ./testRTInt  ./test_TSPClient (P0lient v2…` | 83 | 62252221, 62252341, 62252401, 62252461, 62252623, 62252683 |
| `Date: Tue, Feb 03 2026 02:18:23 GMT` | 22 | 62256345, 62256426, 62256468, 62256489, 62256510, 62256531 |
| `Date: Tue, Feb 03 2026 02:18:22 GMT` | 19 | 62255904, 62255925, 62255946, 62255967, 62255988, 62256009 |
| `Date: Tue, Feb 03 2026 02:18:16 GMT` | 16 | 62252363, 62252403, 62252423, 62252463, 62252625, 62252645 |
| `+P: ./testRTI` | 12 | 62252181, 62252241, 62252361, 62252421, 62252643, 62252703 |
| `r-R` | 10 | 62264964, 62265690, 62267274, 62269921, 62270713, 62270845 |
| `Date:` (various timestamps) | 68 | (distributed across seconds 15-42) |
| `Q-R` | 1 | 62265162 |
| `edia v2018.08.28)` | 1 | 62265624 |
| `3\xf1` | 1 | 62271439 |
| `\x14` | 1 | 62256334 |

**Best single trace for repro:** msg_id `62252181` (CSeq: `+P: ./testRTI` echoed in 405 response)


## Bug 3: Cross-Stream Session Confusion (MEDIUM)

**What:** PLAY/TEARDOWN for `wavAudioTest/` (never SETUPed) gets 2xx success
because LIVE555 doesn't validate that the request URI belongs to a stream
SETUPed under the active session. Server even returns RTP-Info for the
wrong stream.

**46 PLAY + 43 TEARDOWN = 89 instances**

### SETUPed URIs (legitimate targets)
- `rtsp://127.0.0.1:8554/mp3AudioTest/track1`
- `rtsp://127.0.0.1:8554/aacAudioTest/track1`

### Cross-stream target
- `rtsp://127.0.0.1:8554/wavAudioTest/` ← never SETUPed

### Cross-Stream PLAY Pairs

| # | Request msg_id | Response msg_id | Session |
|---|----------------|-----------------|---------|
| 1 | 62255917 | 62255918 | #1464914 |
| 2 | 62255938 | 62255939 | #1464915 |
| 3 | 62255959 | 62255960 | #1464916 |
| 4 | 62255980 | 62255981 | #1464917 |
| 5 | 62256001 | 62256002 | #1464918 |
| 6 | 62256022 | 62256023 | #1464919 |
| 7 | 62256064 | 62256065 | #1464921 |
| 8 | 62256106 | 62256107 | #1464923 |
| 9 | 62256127 | 62256128 | #1464924 |
| 10 | 62256148 | 62256149 | #1464925 |

(+ 36 more: 62256169→62256170 through 62263357→62263358)

### Cross-Stream TEARDOWN Pairs

| # | Request msg_id | Response msg_id | Session |
|---|----------------|-----------------|---------|
| 1 | 62255919 | 62255920 | #1464914 |
| 2 | 62255940 | 62255941 | #1464915 |
| 3 | 62255961 | 62255962 | #1464916 |
| 4 | 62255982 | 62255983 | #1464917 |
| 5 | 62256003 | 62256004 | #1464918 |
| 6 | 62256024 | 62256025 | #1464919 |
| 7 | 62256066 | 62256067 | #1464921 |
| 8 | 62256108 | 62256109 | #1464923 |
| 9 | 62256129 | 62256130 | #1464924 |
| 10 | 62256150 | 62256151 | #1464925 |

(+ 33 more: 62256171→62256172 through 62257008→62257009)

### Reproduction Template

```
Step 1 — SETUP mp3AudioTest (legitimate):
  C2S: SETUP rtsp://127.0.0.1:8554/mp3AudioTest/track1 RTSP/1.0\r\n
       CSeq: 3\r\n
       Transport: RTP/AVP;unicast;client_port=43712-43713\r\n\r\n
  S2C: RTSP/1.0 201 OK   (Session: 000022B8)

Step 2 — PLAY wavAudioTest (never SETUPed):
  C2S: PLAY rtsp://127.0.0.1:8554/wavAudioTest/ RTSP/1.0\r\n
       CSeq: 4\r\n
       Session: 000022B8\r\n\r\n
  S2C: RTSP/1.0 202 OK   ← should be 454 Session Not Found or 459
       RTP-Info: url=rtsp://127.0.0.1:8554/wavAudioTest/track1;seq=0;rtptime=0

Step 3 — TEARDOWN wavAudioTest (never SETUPed):
  C2S: TEARDOWN rtsp://127.0.0.1:8554/wavAudioTest/ RTSP/1.0\r\n
       CSeq: 5\r\n
       Session: 000022B8\r\n\r\n
  S2C: RTSP/1.0 206 OK   ← should be 454 Session Not Found
```

**Best session for repro:** Session #1464914, msg_ids `62255917`→`62255920`
(PLAY + TEARDOWN back-to-back for wavAudioTest)


## Bug 5: Compound Responses (Adapter Issue)

**What:** Two RTSP responses concatenated in a single TCP segment. Always
`405 Method Not Allowed` + `400 Bad Request`. The adapter only parses the
first response, missing the second. Not a LIVE555 bug per se (RFC 2326
explicitly allows PDU packing), but the adapter must handle it.

**50 instances**

### Sample msg_ids

| msg_id | Status codes |
|--------|--------------|
| 62252183 | 405 + 400 |
| 62252243 | 405 + 400 |
| 62252363 | 405 + 400 |
| 62252423 | 405 + 400 |
| 62252645 | 405 + 400 |
| 62252705 | 405 + 400 |
| 62252825 | 405 + 400 |
| 62252885 | 405 + 400 |
| 62252945 | 405 + 400 |
| 62253185 | 405 + 400 |

All 50: 62252183, 62252243, 62252363, 62252423, 62252645, 62252705,
62252825, 62252885, 62252945, 62253185, 62253305, 62253365, 62253425,
62261854, 62264712, 62264742, 62264976, 62265006, 62265174, 62265204,
62265438, 62265468, 62265636, 62265666, 62265702, 62265732, 62266164,
62266194, 62266824, 62266854, 62267286, 62267316, 62269471, 62269501,
62269669, 62269699, 62269933, 62269963, 62270725, 62270755, 62270857,
62270887, 62270989, 62271019, 62271319, 62271349, 62271451, 62271481,
62271656, 62271686


## Golden Sessions (Multiple Bug Types)

These sessions contain 3+ bug types and are ideal for end-to-end reproduction:

| Session | Violations | Bugs Present | msg_id Range |
|---------|------------|--------------|--------------|
| **#1464784** | 3 | B1 zero-port, B2 cseq-garbage, B5 compound | 62252176–62252243 |
| **#1464914** | 2 | B2 cseq-garbage, B3 cross-play, B3 cross-teardown | 62255904–62255920 |
| **#1465096** | 5 | B1 zero-port, B2 cseq-garbage, B3 cross-play | 62263342–62263366 |
| **#1465067** | 1 | B2 cseq-garbage, B3 cross-play, B5 compound | 62261844–62261858 |

### Recommended First Targets

1. **Session #1464784** for Bug 1 + Bug 2 + Bug 5 (zero-port, garbage CSeq, compound)
   - Start at msg_id `62252176`, follow through `62252243`
2. **Session #1464914** for Bug 3 (cross-stream confusion)
   - Start at msg_id `62255904`, follow through `62255920`
3. **Session #1465096** for all three real LIVE555 bugs together
   - Start at msg_id `62263342`, follow through `62263366`
