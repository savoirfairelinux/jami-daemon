(define-syntax-rule (define-signal signal-name handler-name arg-cnt)
  (define name (make-hook arg-cnt))
  (jami:on-signal signal-name
                  (lambda (. args)
                    (apply run-hook (cons name args)))))

(define-signal 'new-buddy-notification *on-new-buddy* 4)

(on-new-buddy! (lambda (account-id buddy-uri status line-status)
                 (format #t
                         "Account ID:  ~a
sshqBuddy URI:   ~a
Status:      ~a
Line status: ~a
" account-id buddy-uri status line-status)
                 #t))
