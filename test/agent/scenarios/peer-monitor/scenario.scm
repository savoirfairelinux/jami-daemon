#!/usr/bin/env -S ./agent.exe --debug -e main -s
!#

;;; Commentary:
;;;
;;; This scenario is testing connectivity between two peers.  An active peer and
;;; a passive peer.  The passive peer accepts all incoming trust request and
;;; incoming calls.  The active peer will call the passive peer every minute,
;;; disabling and re-enabling its account in between to force a new connection.
;;;
;;; Every peers in this scenario will synchronize at midnight UTC±00:00.  When
;;; synchronizing, peers will have a grace period of one hour to change their
;;; account' details.  After that, the scenario is resumed.
;;;
;;; The active peer also keep statistics about success rate and failure rate of
;;; calls made during the day.  After synchronization, the active peer flush the
;;; statistics of the day to the file `stats.scm`, which can be further analyze
;;; using Guile.  The rates are then reset to zero for the next day.
;;;
;;; Code:

(use-modules
 (ice-9 match)
 (ice-9 threads)
 (ice-9 atomic)
 (ice-9 format)
 ((srfi srfi-19) #:prefix srfi-19:)
 (agent)
 ((jami account) #:prefix account:)
 ((jami call) #:prefix call:)
 ((jami signal) #:prefix jami:)
 ((jami logger) #:prefix jami:))

(define* (current-date #:optional (tz 0))
  (srfi-19:current-date tz))

(define seconds-per-hour (* 60 60))
(define seconds-per-day (* 24 seconds-per-hour))

(define* (time-until-midnight)
  "Returns the number of seconds before midnight at timezone offset TZ."
  (let ((now (current-date)))
    (+ (* 3600 (- 23 (srfi-19:date-hour now)))
       (* 60 (- 59 (srfi-19:date-minute now)))
       (- 60 (srfi-19:date-second now)))))

(define next-details!
  (let* ([details-matrix #((("Account.upnpEnabled" . "true")
                            ("TURN.enable"         . "true")

                            ("Account.upnpEnabled" . "true")
                            ("TURN.enable"         . "false")

                            ("Account.upnpEnabled" . "false")
                            ("TURN.enable"         . "true")

                            ("Account.upnpEnabled" . "false")
                            ("TURN.enable"         . "false")))]
         [i 0]
         [len (array-length details-matrix)])
    (lambda ()
      "Returns the next details in the matrix of account's details."
      (let ([details (array-ref details-matrix i)])
        (set! i (euclidean-remainder (1+ i) len))
        details))))

(define (timestamp)
  (srfi-19:date->string (current-date) "[~5]"))

(define-syntax-rule (progress fmt args ...)
  (jami:info "~a ~a" (timestamp) (format #f fmt args ...)))

(define stats-output #t)

(define-syntax-rule (stat fmt args ...)
  (format stats-output fmt args ...))

(define (setup-timer)

  (progress "Setting up timer")

  ;; Timer was triggered.  The agent now has a grace period of an hour to change
  ;; its account's details.  After the grace period, the midnight timer is set
  ;; again.
  (sigaction SIGALRM (lambda _
                       ;; Restart in one hour
                       (setitimer ITIMER_REAL
                                  0 0
                                  seconds-per-hour 0)

                       (sigaction SIGALRM (lambda _ (setup-timer)))

                       (let ([account (account-id agent)]
                             [details (next-details!)])
                         (progress "SIGALRM - Changing account details: ~a" details)
                         (account:send-register account #f)
                         (account:set-details account details)
                         (account:send-register account #t))

                       (pause)))

  ;; Setup timer to trigger at next midnigh of UTC+0.  This means that peers can
  ;; have different timezones and still synchronize at the same time.
  (setitimer ITIMER_REAL
             0 0
             (time-until-midnight) 0)

  ;; Resume execution of continuation.
  (resume #t))

(define agent #f)
(define resume #f)

(define (active peer)

  (define (call-peer timeout)
    (let ((mtx (make-recursive-mutex))
          (cnd (make-condition-variable))
          (me (account-id agent))
          (this-call-id "")
          (continue #t))

      (with-mutex mtx
        (jami:on-signal 'state-changed
                        (lambda (account-id call-id state code)
                          (with-mutex mtx
                            (when (and continue
                                       (string= account-id me)
                                       (string= call-id this-call-id)
                                       (string= "CURRENT" state))
                              (signal-condition-variable cnd))
                            continue)))

        (set! this-call-id (call-friend agent peer))

        (let ([success (wait-condition-variable cnd mtx
                                                (+ (current-time) timeout))])
          (when success
            (call:hang-up me this-call-id))
          (set! continue #f)
          success))))

  (let ([account (account-id agent)]
        [success 0]
        [failure 0]
        [date (timestamp)]
        [reset (call/cc (lambda (k)
                          (set! resume k)
                          #f))])

    (when reset
      (let ((total (+ success failure)))
        (stat "'(summary (date . \"~a\") (total-call . ~a) (success-rate . ~a) (failure-rate . ~a))"
              date
              total
              (/ success total)
              (/ failure total))
        (set! date (timestamp))
        (set! success 0)
        (set! failure 0)))

    (while #t
      (let ([success? (call-peer 30)])
        (progress "Call: ~a"  (if success? "PASS" "FAIL"))
        (if success?
            (set! success (1+ success))
            (set! failure (1+ failure)))
        (account:send-register account #f)
        (sleep 30)
        (account:send-register account #t)
        (sleep 30)))))

(define (passive)

  (let ([account (account-id agent)])
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

    ;; Accept all trust requests.
    (jami:on-signal 'incoming-trust-request
                    (lambda (account-id conversation-id peer-id payload received)
                      (when (string= account-id account)
                        (jami:info "accepting trust request: ~a ~a" account-id peer-id)
                        (account:accept-trust-request account-id peer-id))
                      #t)))

  (let ([continue (call/cc (lambda (k)
                             (set! resume k)
                             #t))])
    (while continue (pause))))

(define (main args)

  (set! agent (make-agent
               "afafafafafafafaf"))

  (progress "I am ~a" agent)

  ;; For debugging purpose, you can text the agent.
  (jami:on-signal 'message-received
                  (lambda (account-id conv-id commit)
                    (progress "Message received: ~a ~a: ~a"
                              account-id conv-id commit)
                    #t))

  (let ([behavior (cdr args)])
    (set! resume
          (match behavior
            (("passive") (lambda _ (passive)))
            (("active" peer)
             (lambda _
               (set! stats-output (open-output-file "stats.scm"))
               (setvbuf stats-output 'none)
               (make-friend agent peer)
               (active peer)))
            (_ (throw 'bad-argument args)))))

  (setup-timer))
