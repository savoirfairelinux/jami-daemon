(define-module (jami signal)
  #:use-module (ice-9 atomic)
  #:use-module (ice-9 format)
  #:use-module (ice-9 threads)
  #:use-module (jami logger)
  #:use-module (jami signal bindings)
  #:re-export (on-signal)
  #:export-syntax (with-signal
                   with-signal-sync)
  #:export (log-signal))

;;; Install for signal SIG an HANDLER and execute bodies.  HANDLER is removed
;;; for SIG at the exit of the dynamic extent.
(define-syntax-rule (with-signal sig handler body body* ...)
  (let ([continue? (make-atomic-box #t)])
    (dynamic-wind
      (const #f)
      (lambda ()
        (on-signal
         sig
         (lambda args
           (let ([continue? (atomic-box-ref continue?)])
             (when continue? (apply handler args))
             continue?)))
        body
        body* ...)
      (lambda () (atomic-box-set! continue? #f)))))

(define* (wait-cnd cnd mtx #:optional timeout)
  (if timeout
      (wait-condition-variable cnd mtx (+ (current-time) timeout))
      (wait-condition-variable cnd mtx)))

;;; Synchronize with Jami's signal.  Install signal handler for SIG and wait for
;;; either PRED to return #t or that TIMEOUT seconds has expired, after TRIGGERS
;;; are been executed.
(define-syntax with-signal-sync
  (syntax-rules ()
    [(_ sig pred) (with-signal-sync sig pred #f #f)]
    [(_ sig pred timeout) (with-signal-sync sig pred timeout #f)]
    [(_ sig pred timeout triggers ...)
     (let ([mtx (make-recursive-mutex)]
           [cnd (make-condition-variable)])
       (with-mutex mtx
         (on-signal sig (lambda args
                          (let ([sync? (apply pred args)])
                            (when sync?
                              (with-mutex mtx
                                (signal-condition-variable cnd)))
                            (not sync?))))
         (begin triggers ...)
         (wait-cnd cnd mtx timeout)))]))

;;; Install signal handler for SIG that print the arguments of the signal.
(define (log-signal sig)
  (on-signal sig (lambda args
                   (info "Signal ~a:~{ ~a~}" sig args)
                   #t)))
