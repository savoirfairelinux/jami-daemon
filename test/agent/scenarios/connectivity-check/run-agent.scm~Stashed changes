#!/usr/bin/env -S ./agent.exe -e run-agent -s
!#

;;; Scenario connectivity-check - ICE/PUNATH - connectivity check stuck in infinite loop.
;;;
;;; let A=Alice, B=Bob
;;;
;;; 1. A enables its Jami account
;;  2. B enables its Jami account and wait for the presence indicator of A to turn green
;;; 3. A disables its account
;;; 4. B makes a call to A while presence indicator is still green
;;; 5. B disables account while searching (do not terminate the call)
;;; 6. A re-enables its account
;;;
;;; While we want this scenario to used Jami's signal, for now we will use a
;;; grace-period to order the events.
;;;
;;; The following timeline is scaled according to `grace-period`:
;;;
;;; event: |  A and B enables their account| B wait for the presence of A| A disables its account| B makes a call| B disables its account| A re-enables its account
;;; axis:  |--.------------------------------.-----------------------------.-----------------------.---------------.-----------------------.-----------------------
;;; time:  |  0                              1                             2                       3               4                       5


(use-modules (ice-9 threads)
             (ice-9 match)
             ((agent) #:prefix agent:)
             ((jami logger) #:prefix jami:)
             ((jami account) #:prefix account:)
             ((jami signal) #:prefix jami:)
             ((jami call) #:prefix call:))


(define this-account (fluid-ref agent:account-id))

(define grace-period 3)

(define (wait-for-peer peer-uri timeout)
  (sleep timeout))

(define (ensure-contact account)
  (let ((contacts (account:get-contacts account)))
    (if (= (vector-length contacts) 0)
        (begin
          (jami:error "No contacts available")
          (exit 1))
        (assoc-ref (array-ref contacts 0) "id"))))

(define (alice)

  (define bob-uri (ensure-contact this-account))

  (jami:info "Alice waits to see bob")
  ;;(wait-for-peer bob-uri)
  ;; Timeline: 0 -> 2
  (sleep (* 2 grace-period))
  (jami:info "3. Alice disables her account")
  (account:send-register this-account #f)

  ;; Timeline: 2 -> 5
  (sleep (* 3 grace-period))
  (jami:info "6. Alice re-enables her account")
  (account:send-register this-account #t))

(define (bob)

  (define alice-uri (ensure-contact this-account))

  ;; Timeline: 0 -> 1
  (sleep grace-period)
  (jami:info "2. Bob waits for the presence indicator of Alice to turn green")
  ;;(wait-for-peer alice-uri)

  ;; Timeline: 1 -> 3
  (sleep (* 2 grace-period))
  (jami:info "4. Bob makes a call to Alice while presence indicator is still green")
  (call:place-call this-account alice-uri)

  ;; Timeline: 3 -> 4
  (sleep grace-period)
  (jami:info "5. Bob disables his account while searching (do not terminate the call)")
  (account:send-register this-account #f)

  ;; Timeline: 4 -> 5
  (sleep grace-period))

(define (run-agent% args)

  (let ((agent-name (cadr args)))

    (jami:info "Running agent <~a>" agent-name)

    (display agent-name)
    (newline)

    (agent:ensure-account-from-archive
     (format #f "~a.gz" agent-name)
     #:wait-for-announcement? #f)

    ;; Wait for contacts to be added to account
    (sleep 10)

    (jami:info "1. Alice and Bob enable their account")
    (account:send-register this-account #t)

    ;; Timeline = 0
    (match agent-name
      ("alice" (alice))
      ("bob" (bob)))

    ;; Wait a few seconds to gather logs
    (sleep 60)))

(define (run-agent args)
  (catch #t
    (lambda ()
      (run-agent% args)
      (jami:info "Agent done")
      (exit 0))
    (lambda (key . args)
      (jami:error "Exception ~a: ~a" key args)
      (exit 1))))
