;;; Common utilities for agents

;;; Install a file logger and execute expression
(define-syntax-rule (with-log:file context to expr ...)
  (begin
    (agent:install-file-logger context to)
    (begin expr ...)
    (agent:remove-logger)))

;;; Install a net logger and execute expression
(define-syntax-rule (with-log:net context to port expr ...)
  (begin
    (agent:install-net-logger context to port)
    (begin expr ...)
    (agent:remove-logger)))
