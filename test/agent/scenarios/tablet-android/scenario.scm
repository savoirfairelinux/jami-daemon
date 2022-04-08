;;; Commentary:
;;;
;;; Code:

(use-modules
 (ice-9 exceptions)
 (ice-9 match)
 (ice-9 threads)
 (ice-9 format)
 ((agent) #:prefix agent:)
 ((jami account) #:prefix account:)
 ((jami call) #:prefix call:)
 ((jami signal) #:prefix jami:)
 ((jami logger) #:prefix jami:))

(define GRACE-PERIOD 10)
(define MEDIA-FLOW 10)

(define me (agent:make-agent "afafafafafafafaf"
                             #:details
                             '(("Account.upnpEnabled" . "false")
                               ("TURN.enable" . "false")
                               ("STUN.enable". "false"))))

(define (place-call-sync peer)
  (letrec* ((this-call-id "")
            (success?
             (jami:with-signal-sync
              'state-changed
              (lambda (account-id call-id status _)
                (and (string= account-id (agent:account-id me))
                     (string= call-id this-call-id)
                     (string= status "CURRENT")))
              GRACE-PERIOD
              (set! this-call-id (agent:call-friend me peer)))))
    (sleep MEDIA-FLOW)
    (unless (string= "" this-call-id)
      (call:hang-up (agent:account-id me) this-call-id))
    success?))

(define (reset-connexion proc)
  (lambda args
    (let ((result (apply proc args))
          (id (agent:account-id me)))
      (jami:with-signal-sync
       'volatile-details-changed
       (lambda (account-id details)
         (and (string= account-id id)
              (string= "true"
                       (or (assoc-ref details
                                      "Account.deviceAnnounced")
                           "false"))))
       #f
       (begin
         (account:send-register id #f)
         (account:send-register id #t)))
      result)))

(define* (run-scenario peer #:key (timeout 0) (output "results.txt"))
  (let ((call (reset-connexion place-call-sync)))
    (call-with-output-file output
      (lambda (port)
        (let lp ()
          (format port "~s\n~!" (cons
                                 (strftime "%c" (localtime (current-time)))
                                 (call peer)))
          (sleep timeout)
          (lp))))))
