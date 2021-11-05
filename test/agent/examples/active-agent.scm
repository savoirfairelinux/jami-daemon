;;; This is an example of an active agent.
;;;
;;; The active agent ensure that an account is created and then call its peer
;;; every minute.

(use-modules
 (ice-9 threads)
 ((agent) #:prefix agent:)
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

    (jami:on-signal 'state-changed
                    (lambda (call-id state code)
                      (with-mutex mtx
                        (let ((ret (cond
                                    ((not (string= call-id this-call-id)) #t)
                                    ((string= state "CURRENT") (begin
                                                                 (set! success #t)
                                                                 #t))
                                    ((string= state "OVER") (begin
                                                              (set! over #t)
                                                              #f))
                                    (else #t))))
                          (signal-condition-variable cnd)
                          ret))))

    (set! this-call-id (call:place-call from to))

    (with-mutex mtx
      (while (not (or success over))
        (wait-condition-variable cnd mtx (+ (current-time) 30))))

    (when success
      (call:hang-up this-call-id))

    (unless over
      (with-mutex mtx
        (while (not over)
          (wait-condition-variable cnd mtx (+ (current-time) 30)))))

    success))

(agent:ensure-account)

(let ((account (fluid-ref agent:account-id))
      (peer "FIXME"))

  (when (string= peer "FIXME")
    (throw 'bad-peer
           "Peer was not set! Please set variable `peer` to a valid Jami's ID"))
  (while #t
    (begin
      (make-a-call account peer)
      (jami:info "Disabling account")
      (account:send-register account #f)
      (sleep 30)
      (jami:info "Enabling account")
      (account:send-register account #t)
      (sleep 30))))
