#!/usr/bin/env -S ./agent.exe -s
!#

;;; Summary:
;;;
;;; Create an account then call a peer in a loop
;;;

;;; We import here Jami's primitives.
(use-modules
 (ice-9 threads)
 (agent)
 ((jami account) #:prefix account:)
 ((jami signal) #:prefix jami:)
 ((jami call) #:prefix call:)
 ((jami logger) #:prefix jami:))

(define* (make-a-call from to #:key (timeout 30) (media-flow 10))
  "Make a call from account id FROM to peer id TO.
If call is not in state CURRENT before TIMEOUT, returns #f, otherwise the call is
hang up after MEDIA-FLOW seconds and #t is returned.
"
  (jami:info "Placing call from:~a to:~a" from to)

  (let ([mtx (make-recursive-mutex)]
        [cnd (make-condition-variable)]
        [this-call-id ""]
        [continue #t])

    (jami:on-signal 'state-changed
                    (lambda (account-id call-id state code)
                      (with-mutex mtx
                        (when (and continue
                                   (string= account-id from)
                                   (string= call-id this-call-id)
                                   (string= "CURRENT" state))
                          (signal-condition-variable cnd))
                        continue)))
    (with-mutex mtx
      (set! this-call-id (call:place-call/media from to))

      (let ([success (wait-condition-variable cnd mtx
                                              (+ (current-time) timeout))])
        (when success
          (sleep media-flow) ; Wait for media to flow between peers.
          (call:hang-up from this-call-id))

        (set! continue #f)
        success))))

;;; Change FIXME for the peer id you want to contact.
(define peer "8b3f62c43c9ba8339e7c41839ce819897b1b912c")

;;; This will create your agent and wait for its announcement on the DHT (see
;;; (agent)).
(define agent (make-agent "bfbfbfbfbfbfbfbf"))

;;; This will make you friend with the other peer.  You need to accept the trust
;;; request within a certain amount of time (see (agent)).
(make-friend agent peer)

;;; You can change the value of MEDIA-FLOW and GRACE-PERIOD.
(let loop ([account (account-id agent)]
           [media-flow 4]
           [grace-period 4]
           [call-count 1])

  (jami:info "Call count:~a" call-count)

  (make-a-call account peer #:media-flow media-flow)
  (sleep grace-period)
  (loop account media-flow grace-period (1+ call-count)))
