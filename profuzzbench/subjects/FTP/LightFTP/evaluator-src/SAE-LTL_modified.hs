-- request = {
--     commit_success,
--     commit_bad,
--     commit_error,
--     commit_reuse,
--     commit_reflect,
--     unknown,
--     confirm_success,
--     confirm_bad,
--     association_request,
--     commit_success_ac_token
-- }
-- group = int
-- send_confirm = int

-- status_code = {
--     success,
--     h2e,
--     unsupported_cyclic_group,
--     unknown_password_identifier
-- }

-- rg_container = Bool
-- ac_container = Bool
-- pi_container = Bool

-- rg = {
--     supported,
--     unsupported
-- }
-- ac = Bool
-- pi = Bool

-- response = {
--     commit_success,
--     commit_error,
--     timeout,
--     confirm_success,
--     confirm_error,
--     commit_ac_token
-- }
-- ap_group = int
-- ap_send_confirm = int

-- ap_status_code = {
--     success,
--     h2e,
--     unsupported_cyclic_group,
--     unknown_password_identifier
-- }

-- ap_pi_container = Bool
-- ap_pi = Bool

--  can encode multiple interpretations as a hierarchy of properties

-- 1 direct representation with edge case
-- A party may commit at any time
-- RL: Looks good
H (
    (
        (request=commit_success | request=commit_bad) -> ! response=timeout
    ) 
    |
    (
        (request=commit_success & rg_container & rg=supported) -> response=timeout
    )
)

-- 1 the general case as taken from the standard
-- A party may commit at any time
-- RL: Looks good
H (
    ((request=commit_success | request=commit_bad) -> ! response=timeout) 
)

-- 2 
-- A party confirms after it has committed and its peer has committed
-- RL: Do "request=commit_success" and "response=commit_success" have to occur in the same timestep?
H (
    response=confirm_success -> (O request=commit_success) & (O response=commit_success)
)

-- 3
-- A party accepts authentication only after a peer has confirmed
-- RL: Analogous to previous. Are the parens nested properly?
H (
    response=association_response -> O (request=confirm_success)
)

-- 4
-- The protocol successfully terminates after each peer has accepted
-- Upon receipt of an SAE Commit message, the parent process checks whether a protocol instance for the peer MAC address exists in the database. 
--   If one does, and it is in either Committed state or Confirmed state the frame shall be passed to the protocol instance. 
--   If one does and it is in Accepted state, the scalar in the received frame is checked against the peer-scalar used in authentication 
--   of the existing protocol instance (in Accepted state) If it is identical, the frame shall be dropped
-- RL: Will need to talk to Daniyal to understand this
H (
    (request=commit_reuse & O (response=association_response)) -> response=timeout
)

-- 5
-- The parent process checks the value of Open. If Open is greater than dot11RSNASAEAntiCloggingThreshold, 
--   the parent process shall check for the presence of an Anti-Clogging Token. If an Anti-Clogging Token exists and is correct, 
--   the parent process shall create a protocol instance. If the Anti-Clogging Token is incorrect, the frame shall be silently discarded.
-- RL: This doesn't look correct. This states that if the antecedent holds once, then every remaining timestep along the trace needs response=commit_success.
--     I bet this should either be an instantaneous response, or a bounded eventually.
-- RL: In the previous comment, I was mis-parsing the precedence of "Once" in the antecedent
H (
    ((O response=commit_ac_token) & request=commit_success_ac_token) -> response=commit_success
)

-- 18
-- The other case of the previous requirement
-- RL: Ditto for previous.
H (
    ((O response=commit_ac_token) & request=commit_success) -> response=timeout
)

-- 6 top-layer, all cases wrapped around disjunctions
-- Upon receipt of a Com event, the protocol instance shall check the Status of the Authentication frame. 
--   If the Status code is not SUCCESS, the frame shall be silently discarded and a Del event shall be sent to the parent process. 
--   Otherwise, the frame shall be processed by first checking the finite cyclic group field to see if the requested group is supported. 
--   If not, BadGrp shall be set and the protocol instance shall construct and transmit a an Authentication frame 
--   with Status code UNSUPPORTED_FINITE_CYCLIC_GROUP indicating rejection with the finite cyclic group field set to the rejected group, 
--   and shall send the parent process a Del event.
H (
    (
        ((status != 0 & status != 126) & (request=commit_success | request=commit_error)) -> response=timeout
    )
)

H (
    (
        ((status = 0 | status = 126) & (group < 19 | group > 21))
        -> (response=commit_error & ap_status_code=unsupported_cyclic_group)
    )
)

-- 7 
-- If the Status code is UNSUPPORTED_FINITE_CYCLIC_GROUP, the protocol instance shall check the finite cyclic group field being rejected. 
--   If the rejected group does not match the last offered group the protocol instance shall silently discard the message and set the t0 (retransmission) timer.
-- RL: The requirement text does not seem to mention the "request=commit_error" part of the formalization. Again, maybe there are two possible interpretations
--     of this requirement, where one of them more strictly follows the standard but will flag counterexamples that are not security issues.
H (
    ((request=commit_error | request=commit_bad | request=commit_success) & status=unsupported_cyclic_group & group != ap_group)
    -> response=timeout
)

-- 8
-- If the rejected group matches the last offered group, the protocol instance shall choose a different group and generate the PWE and the secret values according to 12.4.5.2; 
--   it then generates and transmits a new SAE Commit message to the peer, zeros Sync, sets the t0 (retransmission) timer, and remains in Committed state.
-- RL: Might want to discuss "last offered group".
-- RL: Similar comment to previous--"request=commit_error" and "response=timeout" don't seem to be mentioned in the requirement text.
H (
    ((request=commit_error | request=commit_bad | request=commit_success) & status=unsupported_cyclic_group & group = ap_group)
    -> 
    (response=timeout | (response=commit_success & ap_group >= 19 & ap_group <= 21))
)

-- RL: Strict interpretation of the standard; violation maybe doesn't cause a security issue.
H (
    ((request=commit_error | request=commit_bad | request=commit_success) & status=unsupported_cyclic_group & group = ap_group)
    -> 
    (response=commit_success & ap_group >= 19 & ap_group <= 21)
)

-- 9
-- The protocol instance checks the peer-commit-scalar and PEER-COMMIT-ELEMENT from the message. 
--   If they match those sent as part of the protocol instance’s own SAE Commit message, the frame shall be silently discarded 
--   (because it is evidence of a reflection attack) and the t0 (retransmission) timer shall be set.
-- RL: Where does "O (response=commit_success)" come from?
H (
    (request=commit_reflect & O (response=commit_success)) -> response=timeout
)

-- RL: Can maybe encode it this way (closer to the requirement text)
H (
    request=commit_reflect -> response=timeout
)

-- 10
-- If Sync is not greater than dot11RSNASAESync, the protocol instance shall verify that the finite cyclic group is the same as the previously received Commit frame. 
--   If not, the frame shall be silently discarded. If so, the protocol instance shall increment Sync, increment Sc, and transmit its Commit and Confirm 
--   (with the new Sc value) messages. It then shall set the t0 (retransmission) timer.
-- RL: A lot of this requirement doesn't seem encoded in the formula; need to talk to Daniyal.
H (
    (O (response=commit_success) & (request=commit_success & group=ap_group))
    -> response=commit_success
)

-- 11
-- If processing is successful and the SAE Confirm message has been verified, the Rc variable shall be set to the send-confirm portion of the frame, 
--   Sc shall be set to the value 2^16 – 1, the t1 (key expiration) timer shall be set, and the protocol instance shall transition to Accepted state. 
--   verified, the Rc variable shall be set to the send-confirm portion of the frame, Sc shall be set to the value 2^16 – 1, the t1 (key expiration) timer shall be set, 
--   and the protocol instance shall transition to Accepted state.
-- RL: Similar comment to requirement 5 relating to a Once operator in the antecedent of an implication.
H (
    ((O response=commit_success) & request=confirm_success) -> 
        (response=confirm_success & ap_send_confirm = 65535)
)

-- 12
-- If the value of Sync is not greater than dot11RSNASAESync, the value of send-confirm shall be checked. 
--   If the value is not greater than Rc or is equal to 216 – 1, the received frame shall be silently discarded.
-- RL: Same comment as previous about the Once operator + implication
H (
    ((O response=commit_success) & request=confirm_success &
    (send_confirm=65535 | send_confirm <= ap_send_confirm))
    -> response=timeout
)

-- 13 == conflicts with 12
-- The Confirm portion of the frame shall be checked according to 12.4.5.6. If the verification fails, the received frame shall be silently discarded. 
--   If the verification succeeds, the Rc variable shall be set to the send-confirm portion of the frame, 
--   the Sync shall be incremented and a new SAE Confirm message shall be constructed (with Sc set to 2^16 – 1) and sent to the peer. 
--   The protocol instance shall remain in Accepted state.
-- RL: Similar as previous.
-- RL: Also, why are we not capturing "Sync shall be incremented"? Maybe this doesn't relate to the AP?
--     Not visible.
H (
    ((O response=confirm_success) & request=confirm_success &
    (send_confirm < 65535 | send_confirm > ap_send_confirm))
    -> (response=confirm & ap_send_confirm = 65535)
)

-- 14
-- If an SAE Commit message is received with status code equal to SAE_HASH_TO_ELEMENT the peer shall generate the PWE using [hash to element] 
--   and reply with its own SAE Commit message with status code set to SAE_HASH_TO_ELEMENT.
-- RL: Looks good.
H (
    (status=126 & (request=commit_success | request=commit_error | request=commit_bad))
    -> (ap_status=126 & (response=commit_ac_token | response=commit_success | response=commit_error))
)

-- 15
-- If a password identifier is present in the peer’s SAE Commit message and there is no password with the given identifier a STA shall fail authentication.
-- RL: The formalization looks like it will reject any commit message with a password identifier--is this intended? 
-- RL: Kind of pedantic question, but does "peer's SAE Commit message" imply a commit_success? Why not a commit_error, or otherwise?
--     May be another case where we can have multiple interpretations.

-- RL: Cases without commit_success are not very meaningful.
H (
    (request=commit_success & pi_container) -> (response=commit_error & ap_status_code = unknown_password_identifier)
)

-- 16 
-- If the peer’s SAE Commit message contains a Rejected Groups element, 
--   the list of rejected groups shall be checked to ensure that all of the groups in the list are groups that would be rejected. 
--   If any groups in the list would not be rejected then processing of the SAE Commit message terminates and the STA shall reject the peer’s authentication.
-- RL: Same pedantic question as previous.
H (
    (request=commit_success & rg_container & rg = unsupported) -> 
    (response=timeout | response=commit_error)
)

-- 17 -- two layers
-- When the PWE is derived using the hash-to-element method, the Anti-Clogging Token field is encapsulated in an Anti-Clogging Token Container element; 
--   otherwise, the Anti-Clogging Token field is included in the frame outside of an element as described in Table 9-41
-- RL: Same comment as 5 about "Once" operator.
-- RL: Similar comment about 6 with disjunction of implications. Maybe could also be split into two requirements.
--     The "otherwise" makes it seems like the antecedent should be negated.
H (
    (
        ((O response=commit_ac_token) & request=commit_success & status = 126) -> (ac_container & ac)
    ) 
)
H (
    (
        ((O response=commit_ac_token) & request=commit_success_ac_token & status = 0) -> (!ac_container)
    )
)


