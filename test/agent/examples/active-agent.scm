;;; This is an example of an active agent.

;;; Here we define a simple variable named `peer` that references a string.
;;;
;;; You should change this to the Jami ID of an other agent or one of your
;;; account.

(define my-peer "358d385fc78edde27981caac4ec40fb963c8a066")
;;(define my-peer "d21bba3b6f5d894431b1b99a3829bad1f1ecaa10")
(define sfl:ios "aa64a2135eb3af24493f70f1f5446206dfbb9b73")

;;; Here we define a variable named `details-matrix` that references a list of
;;; association lists.
;;;
;;; An association list is a list that has pairs of (key . value).
(define details-matrix
  '((("Account.upnpEnabled" . "true")
     ("TURN.enable"         . "true"))

    (("Account.upnpEnabled" . "false")
     ("TURN.enable"         . "true"))

    (("Account.upnpEnabled" . "true")
     ("TURN.enable"         . "false"))

    (("Account.upnpEnabled" . "false")
     ("TURN.enable"         . "false"))))

(define (scenario/call:details someone)
  "
  pre-conditions: SOMEONE is a string that references a valid contact in the agent account.

  Here we define a variable named `scenario/call` that references a procedure
  that takes a single argument called `someone`.

  This procedure iterates over the details matrix defined above and changes the
  details of the account, using `agent:set-details` for every association list in
  the matrix.  If it fails to place a call, then the `bad-call` exception is
  thrown.  Finally, the agent sleeps for 10 seconds using `agent:wait`.
  "
  (for-each (lambda (details)
              (agent:set-details details)
              (or (agent:place-call someone)
                  (throw 'bad-call)))
            (agent:wait 10)
            details-matrix))

(define (scenario/call:periodic someone)
  "
  pre-conditions: SOMEONE is a string that references a valid contact in the agent account.

  Place a call to SOMEONE periodically until it fails.

  NOTE!  This is an example of a recursive procedure.  In Scheme, tail
  recursions are free."
  (when (agent:place-call someone)
        (when pause
      (agent:wait pause))
    (scenario/call:periodic someone pause)))

(define (scenario/call:nth someone cnt)
  "
  pre-conditions: SOMEONE is a string that references a valid contact in the agent account.
                   LEFT is a positive number.

  Place a call to SOMEONE CNT time.
  "
  (let ((fail 0))
    (do ((i 0 (1+ i)))
        ((> i cnt))
      (unless (agent:place-call someone)
        (set! fail (1+ fail))))
    fail)
)

(define (scenario/ping conversation)
  "
  pre-conditions: CONVERSATION is a string that references a valid conversation in the agent account.

  Here we define a variable named `scenario/ping` that references a procedure
  that takes a single argument called `conversation`.

  This procedure work just like `scenario/call`, but will send a random
  message to CONVERSATION and expect to receive a pong of it, instead of
  placing a call.
  "
  (for-each (lambda (details)
              (agent:set-details details)
              (or (agent:ping conversation) (throw 'bad-ping))
              (agent:wait 5))
            details-matrix))

;;; Set default account's details.
;(agent:set-details '(("Account.upnpEnabled" . "true") ("TURN.enable" . "true")))

;;; Search for our peer.
;(agent:search-for-peers my-peer)

;;; Let's call our peer 50 times.
;(scenario/call:nth my-peer 50)

;;; Wait for 100000 seconds.
(agent:ensure-account)
(agent:search-for-peers my-peer)

(agent:set-details '(("Account.upnpEnabled" . "false")
                     ("TURN.enable"         . "false")))

(while #t
  (for-each (lambda (details)
              (agent:set-details details)
              (display details)
              (newline)
              (agent:place-call sfl:ios)
              (agent:wait 10)
              )
            details-matrix))
