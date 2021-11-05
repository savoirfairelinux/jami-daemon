;;; This is an example of a passive agent.
;;;
;;; The passive agent ensure that an account is created and then wait for
;;; incomming call of any peer.

(use-modules ((agent) #:prefix agent:)
             ((jami signal) #:prefix jami:)
             ((jami call) #:prefix call:)
             ((jami logger) #:prefix jami:))

(agent:ensure-account)

(jami:info "Agent peer-id: ~a" (fluid-ref agent:peer-id))

(let ((account (fluid-ref agent:account-id)))
  (jami:on-signal 'incoming-call/media
                  (lambda (account-id call-id peer media-lst)
                    (when (string= account-id account)
                      (jami:info "Incoming [call:~a] from peer ~a~%" call-id peer)
                      (call:accept call-id media-lst))
                    #t)))
(while #t (pause))
