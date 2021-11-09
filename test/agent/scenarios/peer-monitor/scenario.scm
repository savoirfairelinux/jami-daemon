#!/usr/bin/env -S ./agent.exe -e main -s
!#

(use-modules
 (ice-9 match)
 (ice-9 threads)
 (ice-9 atomic)
 (ice-9 format)
 ((srfi srfi-19) #:prefix srfi-19:)
 ((agent) #:prefix agent:)
 ((jami account) #:prefix account:)
 ((jami call) #:prefix call:)
 ((jami signal) #:prefix jami:)
 ((jami logger) #:prefix jami:))

(define seconds-per-hour (* 60 60))
(define seconds-per-day (* 24 second-per-hour))

(define* (time-until-midnight #:optional (tz 0))
  "Returns the number of seconds before midnight at timezone offset TZ."
  (let ((now (srfi-19:current-date 0)))
    (+ (* 3600 (- 23 (srfi-19:date-hour now)))
       (* 60 (- 59 (srfi-19:date-minute now)))
       (- 60 (srfi-19:date-second now)))))

(define next-details!
  "Returns the next details in the matrix of account's details."
  (let* ((details-matrix
          #((("Account.upnpEnabled" . "true")
             ("TURN.enable"         . "true"))

            (("Account.upnpEnabled" . "true")
             ("TURN.enable"         . "false"))

            (("Account.upnpEnabled" . "false")
             ("TURN.enable"         . "true"))

            (("Account.upnpEnabled" . "false")
             ("TURN.enable"         . "false"))))
         (i 0)
         (len (array-length details-matrix)))
    (lambda ()
      (let ((details (array-ref details-matrix i)))
        (set! i (euclidean-remainder (1+ i) len))
        details))))

(define (timestamp)
  (srfi-19:date->string (srfi-19:current-date) "[~5]"))

(define-syntax-rule (progress fmt args ...)
  (jami:info "~a ~a" (timestamp) (format #f fmt args ...)))

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

                       (let ((account (agent:account-id))
                             (details (next-details!)))
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

(define resume #f)

(define (active peer)

  (define (call-peer timeout)
    (let ((mtx (make-mutex))
          (cnd (make-condition-variable))
          (this-call-id "")
          (continue (make-atomic-box #t)))
      (jami:on-signal 'state-changed
                      (lambda (call-id state code)
                        (if (atomic-box-ref continue)
                            (with-mutex mtx
                              (if (and (string= this-call-id call-id)
                                       (string= state "CURRENT"))
                                  (begin
                                    (signal-condition-variable cnd)
                                    #f)
                                  #t))
                            #f)))
      (with-mutex mtx
        (set! this-call-id (call:place-call (agent:account-id) peer))
        (let ((ret (wait-condition-variable cnd mtx
                                            (+ (current-time) timeout))))
          (unless ret (atomic-box-set! continue #f))
          ret))))

  (let ((account (agent:account-id))
        (success 0)
        (failure 0)
        (reset (call/cc (lambda (k)
                          (set! resume k)
                          #f))))

    (when reset
        (let ((total (+ success failure)))
          (progress "'(summary (total-call . ~a) (success-rate . ~a) (failure-rate . ~a))"
                     total
                     (/ success total)
                     (/ failure total))
          (set! success 0)
          (set! failure 0)))

    (while #t
      (let ((result  (call-peer 30)))
        (progress "Call: ~a"  (if result "PASS" "FAIL"))
        (if result
            (set! success (1+ success))
            (set! failure (1+ failure)))
        (account:send-register account #f)
        (sleep 30)
        (account:send-register account #t)
        (sleep 30)))))

(define (passive)

  (jami:on-signal 'incoming-call
                  (lambda (account-id call-id peer)
                    (progress "Receiving call from ~a" peer)
                    (call:accept call-id)
                    #t))

  (jami:on-signal 'incoming-call/media
                  (lambda (account-id call-id peer media-lst)
                    (call:accept call-id media-lst)
                    #t))

  (let ((continue (call/cc (lambda (k)
                             (set! resume k)
                             #t))))
        (while continue
            (pause))))

(define (main args)

  (let ((behavior (cdr args)))
    (set! resume
          (match behavior
            (("passive" archive) (lambda _
                                   (agent:ensure-account-from-archive
                                    archive)
                                   (passive)))
            (("active" archive peer) (lambda _
                                       (agent:ensure-account-from-archive
                                        archive)
                                       (active peer)))
            (_ (throw 'bad-argument args)))))

  (setup-timer))
