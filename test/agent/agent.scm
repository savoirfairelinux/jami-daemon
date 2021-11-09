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
  #:use-module ((jami logger) #:prefix jami:)
  #:use-module ((jami account) #:prefix account:)
  #:use-module ((jami call)    #:prefix call:)
  #:use-module ((jami signal)  #:prefix jami:)
  #:export (ensure-account
            ensure-account-from-archive))

(define-public account-id (make-parameter "afafafafafafafaf"))
(define-public peer-id (make-parameter #f))

(define* (ensure-account% this-account-id account-details #:optional (wait-for-announcement? #t))
  (if wait-for-announcement?
      (let ((mtx (make-mutex))
            (cnd (make-condition-variable)))
        (jami:on-signal 'volatile-details-changed
                        (lambda (accountID details)
                          (cond
                           ((and (string= accountID this-account-id)
                                 (string= "true" (assoc-ref details "Account.deviceAnnounced")))
                            (with-mutex mtx
                              (signal-condition-variable cnd)
                              #f))
                           (else #t))))
        (when (null? (account:get-details this-account-id))
          (account:add account-details this-account-id))

        (with-mutex mtx
          (wait-condition-variable cnd mtx)))

      (when (null? (account:get-details this-account-id))
        (account:add account-details this-account-id)))

  (let ((details (account:get-details this-account-id)))
    (peer-id (assoc-ref details "Account.username"))))

(define* (ensure-account #:key (wait-for-announcement? #t))
  (jami:info "Ensure account")
  (ensure-account% (account-id) '(("Account.type"            . "RING")
                                  ("Account.displayName"     . "AGENT")
                                  ("Account.alias"           . "AGENT")
                                  ("Account.archivePassword" . "")
                                  ("Account.archivePIN"      . "")
                                  ("Account.archivePath"     . ""))
                   wait-for-announcement?))

(define* (ensure-account-from-archive path #:key (wait-for-announcement? #t))
  (jami:info "Ensure account from archive ~a" path)
  (ensure-account% (account-id) `(("Account.type"            . "RING")
                                  ("Account.displayName"     . "AGENT")
                                  ("Account.alias"           . "AGENT")
                                  ("Account.archivePassword" . "")
                                  ("Account.archivePIN"      . "")
                                  ("Account.archivePath"     . ,path))
                   wait-for-announcement?))
