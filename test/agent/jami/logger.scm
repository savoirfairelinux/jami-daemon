(define-module (jami logger)
  #:use-module (jami)
  #:use-module ((jami logger bindings) #:prefix ffi:)
  #:export-syntax (debug
                   error
                   info
                   warning))

(define-syntax-rule (logging% lvl fmt args ...)
  (let* ((source-location (current-source-location))
         (filename (or (assq-ref source-location 'filename) "<guile>"))
         (line (or (assq-ref source-location 'line) -1)))
    (ffi:log lvl
             filename
             (+ line 1)
             (format #f fmt args ...))))

(define-syntax-rule (debug fmt args ...)
  (logging% ffi:LOG_DEBUG fmt args ...))

(define-syntax-rule (info fmt args ...)
  (logging% ffi:LOG_INFO fmt args ...))

(define-syntax-rule (warning fmt args ...)
  (logging% ffi:LOG_WARNING fmt args ...))

(define-syntax-rule (error fmt args ...)
  (logging% ffi:LOG_ERR fmt args ...))
