(use-modules (agent)
	     (jami call)
	     (jami account))

(define peer "54e06a434d7ccefe67108f4a333a9cfc8354a82b")

(ensure-account)

(let ((account (fluid-ref account-id)))
  (while #t
  (begin
    (place-call account peer)
    (usleep 1000))))
