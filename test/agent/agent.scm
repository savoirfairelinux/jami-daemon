;; Copyright (C) 2021-2023 Savoir-faire Linux Inc.

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

  (let ([mtx (make-recursive-mutex)]
        [cnd (make-condition-variable)])
    (with-mutex mtx
      (jami:on-signal 'volatile-details-changed
                      (lambda (account-id details)
                        (with-mutex mtx
                          (let ([done?
                                 (and (string= account-id this-account-id)
                                      (string= "true" (or (assoc-ref
                                                           details
                                                           "Account.deviceAnnounced")
                                                          "false")))])
                            (when done?
                              (signal-condition-variable cnd))
                            (not done?)))))

      (unless (wait-condition-variable cnd mtx
                                       (+ (current-time) timeout))
        (throw 'make-account-timeout this-account-id account-details))))

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

(define-method (display (self <agent>) port)
  (format port "<agent: account=~a, peer-id=~a>"
          (account-id self)
          (peer-id self)))

(define* (make-agent account-id #:key (details #nil) (timeout 60))
  "Make an agent with ACCOUNT-ID and additional DETAILS.  If not announced on
the DHT before TIMEOUT, throw 'make-account-timeout."

  (jami:info "making agent: ~a" account-id)

  (let ([full-details (append details %default-details)])
    (receive (account-id peer-id)
        (make-account account-id full-details timeout)
      (make <agent>
        #:account account-id
        #:peer peer-id))))


(define-method (call-friend (A <agent>) (peer <string>))
  "Agent A calls  PEER.  Returns the call id."
  (call:place-call/media (account-id A) peer))

(define-method (call-friend (A <agent>) (B <agent>))
  "Agent A calls agent B.  Returns the call id."
  (call-friend A (peer-id B)))

(define-method (make-friend (self <agent>) (peer-id <string>) (timeout <number>))

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
      (wait-condition-variable cnd mtx (+ (current-time) timeout)))))

(define-method (make-friend (self <agent>) (peer-id <string>))
  (make-friend self peer-id 30))

(define-method (make-friend (A <agent>) (B <agent>) . args)
  (apply make-friend (append (list A (peer-id B)) args)))
