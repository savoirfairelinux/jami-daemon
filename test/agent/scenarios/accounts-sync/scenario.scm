#!/usr/bin/env -S ./agent.exe -e main -s
!#

(use-modules
 (ice-9 match)
 (ice-9 threads)
 (ice-9 atomic)
 (ice-9 format)
 ((srfi srfi-19) #:prefix srfi-19:)
 ((agent) #:prefix agent:)
 ((jami account) #:prefix account:)
 ((jami call) #:prefix call:)
 ((jami signal) #:prefix jami:)
 ((jami logger) #:prefix jami:))

(define *accounts-contacts* (make-hash-table 31))
(define mtx (make-mutex))

(define (has-contact? account peer)
  (with-mutex mtx
    (not (null? (member peer (hash-ref *accounts-contacts* account))))))

(define (add-account! account)
  (jami:info "Adding account: ~a" account)
  (with-mutex mtx
    (hash-set! *accounts-contacts* #nil)))

(define (add-contact! account-id uri)
  (with-mutex mtx
    (append! (hash-ref *accounts-contacts* account-id)
             (list uri)))
  (account:add-contact account-id uri))

(define (remove-contact! account-id uri)
  (with-mutex mtx
    (delq! uri (hash-ref *accounts-contacts* account-id)))
  (account:remove-contact account-id uri))

(define (make-someone)
  (parameterize ((agent:account-id #f))
    (agent:ensure-account)
    (add-account! (agent:account-id))))

(define (main args))
