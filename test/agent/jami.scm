(define-module (jami)
  #:use-module (system foreign-library)
  #:export (init
            initialized
            fini
            logging
            platform
            start
            version
            JAMI_FLAG_DEBUG
            JAMI_FLAG_CONSOLE_LOG
            JAMI_FLAG_AUTOANSWER
            JAMI_FLAG_IOS_EXTENSION)
  #:export-syntax (with-jami))

(let* ((libjami (load-foreign-library "libguile-jami"))
       (bootstrap (foreign-library-function libjami "bootstrap")))
  (bootstrap))

(define-syntax with-jami
  (syntax-rules ()
    ((_ config-file (init-flags ...) thunk)
     (dynamic-wind
       (lambda ()
         (init (logior init-flags ...))
         (start config-file))
       thunk
       fini))
    ((_ (init-flags ...) thunk)
     (with-jami "" (init-flags ...) thunk))
    ((_ thunk)
     (with-jami "" (0) thunk))))

(define JAMI_FLAG_DEBUG (ash 1 0))
(define JAMI_FLAG_CONSOLE_LOG (ash 1 1))
(define JAMI_FLAG_AUTOANSWER (ash 1 2))
(define JAMI_FLAG_IOS_EXTENSION (ash 1 3))
