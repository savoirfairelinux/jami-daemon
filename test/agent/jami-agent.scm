(define-syntax-rule (with-log:file context to expr ...)
  (begin
    (agent:install-file-logger context to)
    (catch #t
      (lambda ()
        (begin expr ...)
        (agent:remove-logger))
      (lambda (key . args)
        (agent:remove-logger)
        (throw key)))))

(define-syntax-rule (with-log:net context to port expr ...)
  (begin
    (agent:install-net-logger context to port)
    (catch #t
      (lambda ()
        (begin expr ...)
        (agent:remove-logger))
      (lambda (key . args)
        (agent:remove-logger)
        (throw key)))))

(define android "c5ccf4affa049e0279dad0ba0f397859a9edf2a7")
(define peers #("0c446cbae211a110e236053fcce4c375461322d8"))

(define details-matrix
  '((("Account.upnpEnabled" . "true")
     ("TURN.enable" . "true"))

    (("Account.upnpEnabled" . "false")
     ("TURN.enable" . "true"))

    (("Account.upnpEnabled" . "false")
     ("TURN.enable" . "false"))))

(define (scenario/call someone)
  "The agent try to place a call someone"
  (for-each (lambda (details)
              (agent:set-details details)
              (with-log:net (format #f "~a: ~a " someone details) "127.0.0.1" 8080
                            (or (agent:place-call someone) (throw 'bad-call)))
              (agent:wait 10))
            details-matrix))

(define (scenario/ping conversation)
  "The agent try to ping at periodically."
  (let ((conversation (agent:some-conversation)))
    (for-each (lambda (details)
                (agent:set-details details)
                (with-log:net (format #f "~a " details) "127.0.0.1" 8080
                              (or (agent:ping conversation) (throw 'bad-ping)))
                (agent:wait 5))
              details-matrix)))

(define (scenario/call:android)
  (for-each (lambda (details)
              (agent:set-details details)
              (agent:search-for-peers (make-vector 1 android))
              (with-log:net (format #f "~a" details)  "127.0.0.1" 8080
                            (agent:place-call android)))
            details-matrix))

;;(scenario/call:android)

(scenario/call "0c446cbae211a110e236053fcce4c375461322d8")
