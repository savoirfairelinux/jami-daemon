#!/usr/bin/env -S ./agent.exe --no-auto-compile -s
!#

;;; This is an example of an active agent.
;;;
;;; The active agent ensure that an account is created and then call its peer
;;; every minute.

(use-modules
 (ice-9 threads)
 (agent)
 ((jami account) #:prefix account:)
 ((jami signal) #:prefix jami:)
 ((jami call) #:prefix call:)
 ((jami logger) #:prefix jami:))

(define (make-a-call from to)

  (jami:info "Placing call from:~a to:~a" from to)

  (let ((mtx (make-mutex))
        (cnd (make-condition-variable))
        (this-call-id "")
        (success #f)
        (over #f))

    (with-mutex mtx
      (jami:on-signal 'state-changed
                      (lambda (_ call-id state code)
                        (with-mutex mtx
                          (let ([ret (cond
                                      ((not (string= call-id this-call-id)) #t)
                                      ((string= state "CURRENT") (begin
                                                                   (set! success #t)
                                                                   #t))
                                      ((string= state "OVER") (begin
                                                                (set! over #t)
                                                                #f))
                                      (else #t))])
                            (signal-condition-variable cnd)
                            ret))))
      (set! this-call-id (call:place-call/media from to))
      (while (not (wait-condition-variable cnd mtx (+ (current-time) 30)))))

    (with-mutex mtx
      (when success
        (call:hang-up from this-call-id))
      (while (not over)
        (wait-condition-variable cnd mtx (+ (current-time) 30))))

    success))

(define peer "2a0b2c20e2eea0d86b81916405952ed4ea649445")

(when (string= peer "FIXME")
  (error "peer not set!")
  (quit 1))

(define agent (make-agent "bfbfbfbfbfbfbfbf"))

(make-friend agent peer)

(let loop ([account (account-id agent)])
  (make-a-call account peer)
  (jami:info "Disabling account")
  (account:send-register account #f)
  (sleep 30)
  (jami:info "Enabling account")
  (account:send-register account #t)
  (sleep 30)
  (loop account))
