(define-module (jami signal)
  #:use-module (jami signal bindings)
  #:use-module (ice-9 atomic)
  #:re-export (on-signal)
  #:export (with-signal-handler))

(define-syntax with-signal-hanler
  (syntax-rules ()
    ([_ signal proc body body* ...]
     (let ([continue-box (make-atomic-box #t)]
           [handler proc])
       (dynamic-wind
         (lambda ()
           (jami:on-signal signal
                           (lambda args
                             (let ([continue? (atomic-box-ref continue-box)])
                               (when continue?
                                 (apply handler args))
                               continue?))))
         (lambda () body body* ...)
         (lambda () (atomic-box-set! continue-box #f)))))))
