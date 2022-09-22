(define-module (jami)
  #:use-module (system foreign-library))

;;; Call libjami::init so that every other bindings work.
;;;
;;; Since libjami::fini is not safe in atexit callback, we also register a exit
;;; hook for finalizing Jami with libjami::fini.
(let* ((libjami (load-foreign-library "libguile-jami"))
       (init (foreign-library-function libjami "init"))
       (fini (foreign-library-function libjami "fini")))
  (init)
  (add-hook! exit-hook fini))
