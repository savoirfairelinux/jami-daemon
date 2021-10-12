;;; This is an example of an active agent.
;;;
;;; The active agent ensure that an account is created and then call its peer
;;; every minute.

(use-modules
 (ice-9 threads)
 ((agent) #:prefix agent:)
 ((jami account) #:prefix account:)
 ((jami signal) #:prefix jami:)
 ((jami call) #:prefix call:))

(define (make-a-call from to)
  (let ((mtx (make-mutex))
        (cnd (make-condition-variable))
        (this-call-id "")
        (success #f)
        (over #f))

    (jami:on-signal 'call-state-changed
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

(while #t
  (begin
    (make-a-call agent:account-id "bcebc2f134fc15eb06c64366c1882de2e0f1e54f")
    (account:send-register agent:account-id #f)
    (sleep 30)
    (account:send-register agent:account-id #t)
    (sleep 30)))
