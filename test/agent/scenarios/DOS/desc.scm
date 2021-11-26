(use-modules (agent)
             (jami call))

(define victim "8653188c7adb75f8eed4c26e0f432e3e224f40c6")

(ensure-account)
(place-call (account-id) victim)
