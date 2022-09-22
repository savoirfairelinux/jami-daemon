;;; Commentary:
;;;
;;; This scenario tests calling a peer in a single registration.
;;;
;;; Parameters:
;;;   CALL-COUNT
;;;   GRACE-PERIOD
;;;
;;; Alice's view:
;;;   while call-count < CALL-COUNT:
;;;     call bob
;;;     wait GRACE-PERIOD
;;;     hangup bob
;;;     wait GRACE-PERIOD
;;;   exit okay
;;;
;;; Bob's view:
;;;   while call-count < CALL-COUNT:
;;;     wait-call alice -> 1+ call-count
;;;   exit cal-count = CALL-COUNT
;;;
;;; Code:

(use-modules
 (ice-9 exceptions)
 (ice-9 match)
 (ice-9 threads)
 ((agent) #:prefix agent:)
 ((jami) #:select (with-jami JAMI_FLAG_DEBUG))
 ((jami account) #:prefix account:)
 ((jami call) #:prefix call:)
 ((jami signal) #:prefix jami:)
 ((jami logger) #:prefix jami:))

(define CALL-COUNT 50)
(define GRACE-PERIOD 4)

(define (make-agent account-id)
  (agent:make-agent account-id
                    #:details
                    '(("Account.upnpEnabled" . "false")
                      ("TURN.enable" . "true"))))

(define (alice bob-id)

  (jami:info "Alice is starting with bob ~a" bob-id)

  (define me (make-agent "afafafafafafafaf"))

  (unless (agent:make-friend me bob-id)
    (raise-exception
     (make-exception
      (make-exception-with-message "Can't make friend with bob"))))

  (define (call-bob timeout)
    (let ((this-call-id ""))
      (jami:with-signal-sync
       'state-changed
       (lambda (account-id call-id state code)
         (and (string= account-id (agent:account-id me))
              (string= call-id this-call-id)
              (string= state "CURRENT")))
       timeout
       (set! this-call-id (agent:call-friend me bob-id)))
      this-call-id))

  ;; Synchronize with bob a first time.
  (jami:info "Alice sending call #~a" 1)
  (call:hang-up (agent:account-id me)
                (call-bob #f))

  (let loop ((cnt 2))
    (when (<= cnt CALL-COUNT)
      (jami:info "Alice sending call #~a" cnt)
      (let ((call-id (call-bob GRACE-PERIOD)))
        (sleep GRACE-PERIOD)
        (call:hang-up (agent:account-id me) call-id)
        (sleep GRACE-PERIOD))
      (loop (1+ cnt)))))

(define (bob)

  (jami:info "Bob is starting")

  (define me (make-agent "bfbfbfbfbfbfbfbf"))

  (jami:info "Bob is ready @~a" (agent:peer-id me))

  ;; Wait for Alice.
  (jami:with-signal-sync
   'incoming-trust-request
   (lambda (account-id conversation-id peer-id payload received)
     (let ((sync? (string= account-id (agent:account-id me))))
       (when sync?
         (jami:info "accepting trust request: ~a ~a" account-id peer-id)
         (account:accept-trust-request account-id peer-id))
       sync?)))


  ;; Accept all incoming calls with media.
  (let ((mtx (make-recursive-mutex))
        (cnd (make-condition-variable))
        (received 0))
    (with-mutex mtx
      (jami:with-signal
       'incoming-call/media
       (lambda (account-id call-id peer media-lst)
         (when (string= account-id (agent:account-id me))
           (call:accept account-id call-id media-lst)
           (with-mutex mtx
             (set! received (1+ received))
             (jami:info "Bob has received: ~a calls" received)
             (when (= received CALL-COUNT)
               (signal-condition-variable cnd)))))
       (let ((success?
              (wait-condition-variable
               cnd mtx (+ (current-time) (* 4 GRACE-PERIOD CALL-COUNT)))))
         (jami:info "Summary: ~a%" (* 100 (/ received CALL-COUNT)))
         (exit success?))))))

(define (main args)

  (with-jami "" (JAMI_FLAG_DEBUG)
             (match (cdr args)
               (("alice" bob-id)
                (lambda ()
                  (alice bob-id)
                  (jami:info "bye bye")))
               (("bob")
                (lambda ()
                  (bob)
                  (jami:info "bye bye")))
               (_
                (jami:error "Invalid arguments: ~a" args)
                (jami:error "Usage: ~a alice|bob (ARG)\n" (car args))
                (exit EXIT_FAILURE))))

  (exit EXIT_SUCCESS))
