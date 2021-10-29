(use-modules (ice-9 ftw))

(define agent-exe "./agent.exe")

(define-syntax-rule (progress fmt args ...)
  (begin
    (format #t fmt args ...)
    (newline)))

(define after-fork-hooks (make-hook))

(define (with-fork child-do parent-do)
  (let ((cpid (primitive-fork)))
    (when (= 0 cpid)
      (run-hook after-fork-hooks)
      (exit (child-do)))
    (parent-do cpid)))

;;; Taken from Guix - guix/build/utils.scm
(define* (delete-file-recursively dir
                                  #:key follow-mounts?)
  "Delete DIR recursively, like `rm -rf', without following symlinks.  Don't
follow mount points either, unless FOLLOW-MOUNTS? is true.  Report but ignore
errors."
  (let ((dev (stat:dev (lstat dir))))
    (file-system-fold (lambda (dir stat result)    ; enter?
                        (or follow-mounts?
                            (= dev (stat:dev stat))))
                      (lambda (file stat result)   ; leaf
                        (delete-file file))
                      (const #t)                   ; down
                      (lambda (dir stat result)    ; up
                        (rmdir dir))
                      (const #t)                   ; skip
                      (lambda (file stat errno result)
                        (format (current-error-port)
                                "warning: failed to delete ~a: ~a~%"
                                file (strerror errno)))
                      #t
                      dir

                      ;; Don't follow symlinks.
                      lstat)))

(define (setup-xdg-directory)
  (let ((tmp (mkdtemp "/tmp/jami-agent-XXXXXX")))
    (add-hook! exit-hook
               (lambda ()
             (delete-file-recursively tmp)))
      (setenv "XDG_CONFIG_HOME" tmp)
      (setenv "XDG_CACHE_HOME" tmp)
      (setenv "XDG_DATA_HOME" tmp)))

(add-hook! after-fork-hooks setup-xdg-directory)
