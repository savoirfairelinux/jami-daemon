#!/usr/bin/env -S ./agent.exe --no-auto-compile -s
!#

;;; This is an example of a passive agent.
;;;
;;; The passive agent ensure that an account is created and then wait for
;;; incomming call of any peer.

(use-modules (agent)
             ((jami account) #:prefix account:)
             ((jami signal) #:prefix jami:)
             ((jami call) #:prefix call:)
             ((jami logger) #:prefix jami:))

(define this-agent (make-agent "afafafafafafafaf"))

(jami:info "Agent peer-id: ~a" (peer-id this-agent))

(let ((account (account-id this-agent)))
  (jami:on-signal 'incoming-call/media
                  (lambda (account-id call-id peer media-lst)
                    (when (string= account-id account)
                      (jami:info "Incoming [call:~a] from peer ~a~%" call-id peer)
                      (call:accept account-id call-id media-lst))
                    #t))
  (jami:on-signal 'incoming-call
                  (lambda (account-id call-id peer)
                    (when (string= account-id account)
                      (jami:info "Incoming [call:~a] from peer ~a~%" call-id peer)
                      (call:accept account-id call-id))
                    #t)))

;;; Accept all trust requests
(jami:on-signal 'incoming-trust-request
                  (lambda (account-id conversation-id peer-id payload received)
                    (jami:info "accepting trust request: ~a ~a" account-id peer-id)
                    (account:accept-trust-request account-id peer-id)))

(jami:info "~a" this-agent)

(while #t (pause))
