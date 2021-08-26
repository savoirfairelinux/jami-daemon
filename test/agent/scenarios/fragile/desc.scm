;; SPDX-License-Identifier: GPL-3.0-or-later
;;
;; Copyright (c) 2021 Olivier Dion <olivier.dion@savoirfairelinux.com>
;;
;; scenarios/fragile/desc.scm - Simulate a fragile connection between two peer.
;;
;; Summary:
;;   Alice call Bob once.  The call is interrupted because Bob lost the connection
;;   (e.g., bad reception).  Bob recalls Alice after getting its connection up again.
;;
;; Pre-conditions:
;;   - Alice and Bob are on different networks.
;;   - Alice has UPNP, TURN, STUN disabled.
;;   - Bob has TURN enabled.
;;
;; Post-conditions:
;;   - Both calls should work.
;;   - Each call has its own TCP connection pairs.
;;   - ICE nominated candidates should be of types
;;     [host] <-> [relay] (from Alice's point of view).
;;
;; Note:
;;   To simulate Bob's fragile connection, Bob should activate/deactivate its
;;   account after the first call for a period of time then recall Alice.

(use-modules (ice-9 match)
             (ice-9 local-eval)
             (srfi srfi-26))

;; Set these to the Jami's IDs of both accounts.
;;
;; You also need to have the accounts archived under alice.gz and bob.gz.
(define alice:id "FIXME")
(define bob:id   "FIXME")

(define* (simulate-lost-of-connection #:optional (duration 30))
  (agent:disable)
  (agent:wait duration)
  (agent:enable))

(define (alice)
  (archive->agent "alice.gz")
  (agent:set-details '(("Account.upnpEnabled" . "false")
                       ("TURN.enable" . "false")))
  (agent:place-call bob:id)
  (agent:wait-for-call-state "CURRENT"))

(define (bob)

  (archive->agent "bob.gz")
  (agent:set-details '(("Account.upnpEnabled" . "false")
                       ("TURN.enable" . "true")))
  (agent:wait-for-call-state "CURRENT")
  (agent:wait 5)
  (simulate-lost-of-connection)
  (agent:place-call alice:id))

(catch #t
  (lambda ()
    (match (or (getenv "AGENT")
               (throw 'undefined-environment "AGENT"))
      ("alice" (alice))
      ("bob" (bob))
      (other (throw 'bad-agent other))))
  (lambda (key . args)
    (format #t "Scenario failed with exception - ~a: ~a~%" key args)
    (quit 1)))

(quit 0)
