;;; This is an example of an active agent.

;; Here we include what's common.
;; Take a look of what's available!
(include-from-path "examples/common.scm")

;;; Here we define a simple variable named `peer` that reference a string
;;;
;;; You should change this to the Jami ID of an other agent or one of your
;;; account
(define my-peer "0c446cbae211a110e236053fcce4c375461322d8")

;;; Here we define a variable named `details-matrix` that reference a list of
;;; association lists.
;;;
;;; An association list is a list that has pairs of (key . value)
(define details-matrix
  '((("Account.upnpEnabled" . "true")
     ("TURN.enable" . "true"))

    (("Account.upnpEnabled" . "false")
     ("TURN.enable" . "true"))

    (("Account.upnpEnabled" . "false")
     ("TURN.enable" . "false"))))

;;; Define default log handler
;;;
;;; NOTE!  If you want to use the net logger, you need a server listening for
;;; the logs.  For this, you can do `nc -k -l 127.0.0.1 8080 | tee out.log`
;;(define with:my-logger (make-logger agent:install-net-logger "127.0.0.1" 8080))
;;; Use this logger instead if you just want to capture log to file
(define with:my-logger (make-logger agent:install-file-logger "out.log"))

;;; Pre: `someone` is a string that reference a valid contact in the agent account
;;;
;;; Here we define a variable named `scenario/call` that reference a procedure
;;; that takes a single argument called `someone`.
;;;
;;; This procedure iterates over the details matrix defined above and changes
;;; the details of the account, using `agent:set-details` for every association
;;; list in the matrix.  Once it's done, it installs a log handler using
;;; `with:my-logger` that accepts a string context and a thunk (lambda).  This
;;; thunk is then called and the log handler is removed after.  While capturing
;;; the log, the agent will try to call `someone` using `agent:place-call`.  If
;;; it fails to place a call, then the `bad-call` exception is thrown.  Finally,
;;; the agent sleeps for 10 seconds using `agent:wait`.
(define (scenario/call someone)
  (for-each (lambda (details)
              (with:my-logger (format #f "~a: ~a " someone details)
                              (lambda ()
                                (agent:set-details details)
                                (or (agent:place-call someone)
                                    (throw 'bad-call)))))
            (agent:wait 10)
            details-matrix))

;;; Pre: `conversation` is a string that reference a valid conversation in the agent account
;;;
;;; Here we define a variable named `scenario/ping` that reference a procedure
;;; that takes a single argument called `conversation`.
;;;
;;; This procedure work just like `scenario/call`, but will send a random
;;; message to `conversation` and expect to receive a pong of it, instaed of
;;; placing a call.
(define (scenario/ping conversation)
  (let ((conversation (agent:some-conversation)))
    (for-each (lambda (details)
                (with:my-logger (format #f "~a " details)
                                (lambda ()
                                  (agent:set-details details)
                                  (or (agent:ping conversation) (throw 'bad-ping))))
                (agent:wait 5))
              details-matrix)))

;;; Pre: `someone` is a string that reference a valid contact in the agent account.
;;;
;;; Place a call to `someone` periodically until it fails.
;;;
;;; NOTE!  This is an example of a recursive procedure.  In scheme, tail
;;; recursions are free.
(define (scenario/call:periodic someone)
  (when (agent:place-call someone)
    (scenario/call:periodic someone)))

;;; Let's search for our peer on the network.  This is only necessary for
;;; sending a trust request.
(agent:search-for-peers my-peer)

;;; You try other scenarios here.
(define details '(("Account.upnpEnabled" . "true") ("TURN.enable" . "true")))
(with:my-logger (format #f "~a" details)
                (lambda ()
                  (agent:set-details details)
                  (scenario/call:periodic my-peer)))
