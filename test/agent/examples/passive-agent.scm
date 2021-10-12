;;; This is an example of a passive agent.
;;;
;;; The passive agent ensure that an account is created and then wait for
;;; incomming call of any peer.

(use-modules ((agent) #:prefix agent:)
             ((jami signal) #:prefix jami:)
             ((jami call) #:prefix call:))

(agent:ensure-account)

(jami:on-signal 'incomming-call
                (lambda (account-id call-id peer-display-name media-list)
                  (when (string= account-id agent:account-id)
                    (format #t
                            "Incoming [call:~a] from peer ~a~%"
                            call-id peer-display-name)
                    (call:accept call-id media-list))
                  #t))
