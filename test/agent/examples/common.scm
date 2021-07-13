;;; Common utilities for agents

;;; Return a procedure that accepts a string context and a thunk to called
;;; in between installation of a log handler
(define (make-logger installer . static-args)
  (lambda (context thunk)
    (apply installer (cons context static-args))
    (thunk)
    (agent:remove-logger)))
