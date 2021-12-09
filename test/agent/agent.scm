;; Copyright (C) 2021 Savoir-faire Linux Inc.

;; Author: Olivier Dion <olivier.dion@savoirfairelinux.com>

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program; if not, write to the Free Software
;; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.

(define-module (agent)
  #:use-module (ice-9 threads)
  #:use-module (ice-9 receive)
  #:use-module (oop goops)
  #:use-module ((jami logger) #:prefix jami:)
  #:use-module ((jami account) #:prefix account:)
  #:use-module ((jami call)    #:prefix call:)
  #:use-module ((jami signal)  #:prefix jami:)
  #:export (<agent>
            call-friend
            make-agent
            make-friend
            account-id
            peer-id))

(define %default-details
  '(("Account.type"            . "RING")
    ("Account.displayName"     . "AGENT")
    ("Account.alias"           . "AGENT")
    ("Account.archivePassword" . "")
    ("Account.archivePIN"      . "")
    ("Account.archivePath"     . "")))

(define (make-account this-account-id account-details timeout)

  (if (null? (account:get-details this-account-id))
      (account:add account-details this-account-id)
      (account:set-details this-account-id '(("Account.enable" . "true"))))

  (let ([mtx (make-mutex)]
        [cnd (make-condition-variable)])
    (with-mutex mtx
      (jami:on-signal 'volatile-details-changed
                      (lambda (accountID details)
                        (with-mutex mtx
                          (cond
                           ((and (string= accountID this-account-id)
                                 (string= "true" (assoc-ref
                                                  details
                                                  "Account.deviceAnnounced")))
                            (signal-condition-variable cnd)
                            #f)
                           (else #t)))))

      (unless (wait-condition-variable cnd mtx
                                       (+ (current-time) timeout))
        (throw 'timeout))))

  (values this-account-id (assoc-ref (account:get-details this-account-id)
                                     "Account.username")))

(define-class <agent> ()
  (account-id
   #:getter account-id
   #:init-keyword #:account)
  (peer-id
   #:getter peer-id
   #:init-keyword #:peer)
  (account-details
   #:allocation #:virtual
   #:slot-ref (lambda (this)
                (account:get-details (account-id this)))
   #:slot-set! (lambda (this details)
                 (account:set-details (account-id this) details))))

(define* (make-agent account-id
                     #:key
                     (details #nil)
                     (timeout 60))

  (jami:info "making agent: ~a" account-id)

  (let ([full-details (append details %default-details)])
    (receive (account-id peer-id)
        (make-account account-id full-details timeout)
      (make <agent>
        #:account account-id
        #:peer peer-id))))

(define-method (call-friend (A <agent>) (B <agent>))
  (call:place-call/media (account-id A)
                         (peer-id B)))

(define-method (display (self <agent>) port)
  (format port "<agent: account=~a, peer-id=~a>"
          (account-id self)
          (peer-id self)))

(define-method (make-friend (self <agent>) (peer-id <string>))
  (jami:info "making friend between ~a ~a" self peer-id)
  (let ([mtx (make-mutex)]
        [cnd (make-condition-variable)]
        [me (account-id self)]
        [friend peer-id])
    (with-mutex mtx
      (jami:on-signal 'contact-added
                      (lambda (id uri confirmed)
                        (with-mutex mtx
                          (if (and (string= id me)
                                   (string= uri friend)
                                   confirmed)
                              (begin
                                (signal-condition-variable cnd)
                                #f)
                              #t))))
      (account:send-trust-request me friend)
      (wait-condition-variable cnd mtx))))

(define-method (make-friend (A <agent>) (B <agent>))
  (make-friend A (peer-id B)))
