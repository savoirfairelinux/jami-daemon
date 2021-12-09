#!/usr/bin/env -S ./agent.exe -s
!#

;;; This is an example of an active agent.
;;;
;;; The active agent ensure that an account is created and then call its peer
;;; every minute.

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
If call not in state CURRENT before TIMEOUT, returns #f, otherwise the call is
hang up after MEDIA-FLOW seconds and #t is returned.
"
  (jami:info "Placing call from:~a to:~a" from to)

  (let ([mtx (make-recursive-mutex)]
        [cnd (make-condition-variable)]
        [this-call-id ""]
        [continue #t])

    (with-mutex mtx
      (jami:on-signal 'state-changed
                      (lambda (account-id call-id state code)
                        (with-mutex mtx
                          (when (and continue
                                     (string= account-id from)
                                     (string= call-id this-call-id)
                                     (string= "CURRENT" state))
                            (signal-condition-variable cnd))
                          continue)))

      (set! this-call-id (call:place-call/media from to))

      (let ([success (wait-condition-variable cnd mtx
                                              (+ (current-time) timeout))])
        (when success
          (sleep media-flow) ; Wait for media to flow between peers.
          (call:hang-up from this-call-id))
        (set! continue #f)
        success))))

(define peer "76456e495548bbd18f73989df68237fd84186fe1")

(define agent (make-agent "bfbfbfbfbfbfbfbf"))

(make-friend agent peer)

(let loop ([account (account-id agent)]
           [grace-period 30])

  (make-a-call account peer #:media-flow media-flow)

  (jami:info "Disabling account")
  (account:send-register account #f)
  (sleep grace-period)

  (jami:info "Enabling account")
  (account:send-register account #t)
  (sleep grace-period)

  (loop account grace-period media-flow))
