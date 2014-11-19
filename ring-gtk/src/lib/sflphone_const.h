/****************************************************************************
 *   Copyright (C) 2009-2014 by Savoir-Faire Linux                          *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

/* THIS FILE IS DEPRECATED DO NOT ADD NEW GLOBAL CONSTANTS */

#ifndef SFLPHONE_CONST_H
#define SFLPHONE_CONST_H


#define ICON_INCOMING                     ":/images/icons/ring.svg"
#define ICON_RINGING                      ":/images/icons/ring.svg"
#define ICON_CURRENT                      ":/images/icons/current.svg"
#define ICON_DIALING                      ":/images/icons/dial.svg"
#define ICON_HOLD                         ":/images/icons/hold.svg"
#define ICON_FAILURE                      ":/images/icons/fail.svg"
#define ICON_BUSY                         ":/images/icons/busy.svg"
#define ICON_TRANSFER                     ":/images/icons/transfert.svg"
#define ICON_TRANSF_HOLD                  ":/images/icons/transfert.svg"
#define ICON_CONFERENCE                   ":/images/icons/user-group-properties.svg"
#define ICON_CALL                         ":/images/icons/call.svg"
#define ICON_HANGUP                       ":/images/icons/hang_up.svg"
#define ICON_UNHOLD                       ":/images/icons/unhold.svg"
#define ICON_ACCEPT                       ":/images/icons/accept.svg"
#define ICON_REFUSE                       ":/images/icons/refuse.svg"
#define ICON_EXEC_TRANSF                  ":/images/icons/call.svg"
#define ICON_REC_DEL_OFF                  ":/images/icons/record_disabled.svg"
#define ICON_REC_DEL_ON                   ":/images/icons/record.svg"
#define ICON_MAILBOX                      ":/images/icons/mailbox.svg"

#define ICON_DISPLAY_VOLUME_CONSTROLS     ":/images/icons/icon_volume_off.svg"
#define ICON_DISPLAY_DIALPAD              ":/images/icons/icon_dialpad.svg"
#define ICON_HISTORY_INCOMING             ":/images/icons/incoming.svg"
#define ICON_HISTORY_OUTGOING             ":/images/icons/outgoing.svg"
#define ICON_HISTORY_MISSED               ":/images/icons/missed.svg"
#define ICON_HISTORY_MISSED_OUT           ":/images/icons/missed_out.svg"
#define ICON_SFLPHONE                     ":/images/icons/sflphone.svg"

// #define ACCOUNT_TYPES_TAB                 {QString(Account::ProtocolName::SIP), QString(Account::ProtocolName::IAX)}
/*********************/

/** MIME API */
#define MIME_CALLID           "text/sflphone.call.id"
#define MIME_CONTACT          "text/sflphone.contact"
#define MIME_HISTORYID        "text/sflphone.history.id"
#define MIME_PHONENUMBER      "text/sflphone.phone.number"
#define MIME_PLAIN_TEXT       "text/plain"
#define MIME_HTML_TEXT        "text/html"
#define MIME_ACCOUNT          "text/sflphone.account.id"
#define MIME_PROFILE          "text/sflphone.profile.id"
#endif
