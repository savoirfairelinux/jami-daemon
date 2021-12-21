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

(define* (make-a-call from to #:optional (timeout 30))
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
          (call:hang-up from this-call-id))
        (set! continue #f)
        success))))

(agent:ensure-account)

(let ((account (agent:account-id))
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
