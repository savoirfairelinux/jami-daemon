#!/usr/bin/env -S ./agent.exe -e configure -s
!#

(include "common.scm")

(use-modules (ice-9 threads)
             ((agent) #:prefix agent:)
             ((jami account) #:prefix account:)
             ((jami signal) #:prefix jami:))

(define-syntax-rule (as-someone account-id thunk)
  (with-fluids
      ((agent:account-id account-id))
    (thunk)))

(define alice-account "afafafafafafafaf")
(define bob-account "b0b0b0b0b0b0b0b0")

(define alice-peer-id #f)
(define bob-peer-id #f)

(define (configure . args)

  (define (make-account account-id)
    (as-someone account-id (lambda ()
                             (agent:ensure-account)
                             (account:set-details account-id
                                                  '(("TURN.enable" . "true")
                                                    ("Account.upnpEnabled" . "true")))
                             (fluid-ref agent:peer-id))))

  (progress "bootstrap phase ...")

  (progress "making alice account")
  (set! alice-peer-id (make-account alice-account))

  (progress "making bob account")
  (set! bob-peer-id (make-account bob-account))

  (let ((alice-thread
         (begin-thread
          (let ((mtx (make-mutex))
                (cnd (make-condition-variable)))
            (jami:on-signal 'incoming-trust-request
                            (lambda (account-id conversation-id from payload received)
                              (if (string= account-id alice-account)
                                  (with-mutex mtx
                                    (account:accept-trust-request account-id from)
                                    (signal-condition-variable cnd)
                                    #f)
                                  #t)))
            (progress "Alice wait for Bob's friend request")
            (with-mutex mtx (wait-condition-variable cnd mtx))
            (progress "Alice is done"))))
        (bob-thread
         (begin-thread
          (let ((mtx (make-mutex))
                (cnd (make-condition-variable)))
            (account:send-trust-request bob-account alice-peer-id)
            (jami:on-signal 'contact-added
                            (lambda (account-id uri confirmed)
                              (if (and confirmed (string= account-id bob-account))
                                  (with-mutex mtx
                                    (signal-condition-variable cnd)
                                    #f)
                                  #t)))
            (progress "Bob send a friend request to Alice and wait for conversation to be ready")
            (with-mutex mtx (wait-condition-variable cnd mtx))
            (progress "Bob is done")))))
    (progress "waiting for alice and bob...")
    (join-thread alice-thread)
    (join-thread bob-thread))

  (progress "Disable accounts and archive them")
  (for-each (lambda (account agent-name)
              ;(account:send-register account #f)
              (account:account->archive account (format #f "~a.gz" agent-name)))
            (list alice-account bob-account)
            (list "alice" "bob"))

  (exit 0))
