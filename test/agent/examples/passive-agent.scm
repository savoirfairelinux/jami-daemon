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
  (agent:wait))

(define (scenario/passive:change-details)
  "Agent changes its account details once in a while"
  (for-each (lambda (details)
              (agent:disable)
              (agent:set-details details)
              (agent:enable)
              (agent:wait 15))
            details-matrix))

(agent:ensure-account)
(scenario/passive)
