;;; This is an example of a passive agent
;;;
;;; It will accept all trust request and answer every call

(define details-matrix
  '((("Account.upnpEnabled" . "true")
     ("TURN.enable" . "true"))

    (("Account.upnpEnabled" . "false")
     ("TURN.enable" . "true"))

    (("Account.upnpEnabled" . "false")
     ("TURN.enable" . "false"))))

(define (scenario/passive)
  "Agent does nothing"
  (while #t (agent:wait 100000000)))

(define (behavior/passive:change-details)
  "Agent changes its account details once in a while"
  (for-each (lambda (details)
              (agent:set-details details)
              (agent:wait 40))
            details-matrix))

(scenario/passive)
