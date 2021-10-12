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
  #:use-module ((jami account) #:prefix account:)
  #:use-module ((jami call)    #:prefix call:)
  #:use-module ((jami signal)  #:prefix jami:))

(define-public account-id "afafafafafafafaf")
(define-public peer-id #f)

(define (wait-for-announcement-of% accounts)
  (let ((mtx (make-mutex))
	(cnd (make-condition-variable)))
    (map (lambda (this-account)
	   (jami:on-signal 'volatile-details-changed
			   (lambda (accountID details)
			     (cond
			      ((not (string= accountID this-account)) #f)
			      ((not (string= "true" (assoc-ref details "Account.deviceAnnounced"))) #f)
			      (else
			       (with-mutex mtx
				 (signal-condition-variable cnd))
			       #t))))
	   (with-mutex mtx
	     (wait-condition-variable cnd mtx)))
	 accounts)))

(define (wait-for-announcement-of accounts)
  (if (string? accounts)
      (wait-for-announcement-of% (list accounts))
      (wait-for-announcement-of% accounts)))

(define-public (ensure-account)

  (when (null? (account:get-details account-id))
    (account:add '(("Account.type"            . "RING")
		   ("Account.displayName"     . "AGENT")
		   ("Account.alias"           . "AGENT")
		   ("Account.archivePassword" . "")
		   ("Account.archivePIN"      . "")
		   ("Account.archivePath"     . ""))
		 account-id))

  (let ((result (wait-for-announcement-of account-id)))
    (display result)
    (newline)
    (unless (car result)
      (format #t "Timeout while waiting for account announcement~%")))


  (let ((details (account:get-details account-id)))
    (set! peer-id (assoc-ref details "Account.username"))))
