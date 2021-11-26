(use-modules
 (ice-9 threads)
 ((jami account) #:prefix account:)
 ((jami call) #:prefix call: #:select (accept place-call hang-up))
 ((jami signal) #:prefix jami:)
 ((jami logger) #:prefix jami:)
 ((agent) #:prefix agent:))

;;; The (agent) module exports the procedure `ensure-account` that create an
;;; account with some default settings.  The account-id will be the one defined
;;; by the parameter agent:account-id.  Thus here, we parameterize this variable
;;; so that our account has the ID that we wanted.
;;;
;;; The `ensure-account` also takes as optional keyword a boolean if you want to
;;; wait for the announcement of the account on the DHT.
(define (make-account account-id)
  (parameterize ((agent:account-id account-id))
    (jami:info "Make account: ~a" (agent:account-id))
    (agent:ensure-account)
    (agent:peer-id)))

(define alice-account-id "afafafafafafafaf")
(define bob-account-id   "bfbfbfbfbfbfbfbf")

(define alice-peer-id (make-account alice-account-id))
(define bob-peer-id   (make-account bob-account-id))

;;; Here, Bob will log every call he receives, but will only accept ones from
;;; Alice.
(jami:on-signal 'incoming-call/media
                (lambda (account-id call-id peer media-lst)
                  (when (string= account-id bob-account-id)
                    (jami:info "Bob has a call from ~a" peer)
                    (when (string-contains peer alice-peer-id)
                      (jami:info "Accepting call")
                      (call:accept alice-account-id call-id media-lst)
                      (call:hang-up alice-account-id call-id)
                      ))
                  #t))

;;; Here, Alice spam Bob with some call, with many threads.
(define (spam-bob)
  (while #t
    (let ((call-id (call:place-call alice-account-id bob-peer-id)))
      (jami:info "ring ring..")
      (usleep 100000))))

;;; Spawn spammers for Bob on N-1 CPU
;; (do ((ncpu (current-processor-count) (1- ncpu)))
;;     ((<= ncpu 1))
;;   (jami:info "Spawning spammer")
;;   (begin-thread
;;    ))
(spam-bob)
