#!/usr/bin/env -S ./agent.exe -s
!#

;;; This is an example of a passive agent.
;;;
;;; The passive agent ensure that an account is created and accept all trust
;;; requests and incoming calls.


(use-modules (agent)
             ((jami account) #:prefix account:)
             ((jami signal) #:prefix jami:)
             ((jami call) #:prefix call:)
             ((jami logger) #:prefix jami:))

(define this-agent (make-agent "afafafafafafafaf"))

(let ([account (account-id this-agent)])
  ;; Accept all incoming calls with media.
  (jami:on-signal 'incoming-call/media
                  (lambda (account-id call-id peer media-lst)
                    (when (string= account-id account)
                      (jami:info "Incoming [call:~a] with media ~a from peer ~a~%"
                                 call-id media-lst peer)
                      (call:accept account-id call-id media-lst))
                    #t))
  ;; Accept all incoming calls.
  (jami:on-signal 'incoming-call
                  (lambda (account-id call-id peer)
                    (when (string= account-id account)
                      (jami:info "Incoming [call:~a] from peer ~a~%" call-id peer)
                      (call:accept account-id call-id))
                    #t))

  ;;; Accept all trust requests.
  (jami:on-signal 'incoming-trust-request
                  (lambda (account-id conversation-id peer-id payload received)
                    (when (string= account-id account)
                      (jami:info "accepting trust request: ~a ~a" account-id peer-id)
                      (account:accept-trust-request account-id peer-id))
                    #t)))


(jami:info "~a" this-agent)

(while #t (pause))
