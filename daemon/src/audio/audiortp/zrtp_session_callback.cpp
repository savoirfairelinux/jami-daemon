/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */
#include "zrtp_session_callback.h"

#include "global.h"
#include "sip/sipcall.h"
#include "dbus/dbusmanager.h"
#include "dbus/callmanager.h"
#include "manager.h"

#include <cstdlib>
#include <string>
#include <map>
#include <utility> // for std::pair

using namespace GnuZrtpCodes;

using namespace ost;

namespace sfl {

ZrtpSessionCallback::ZrtpSessionCallback(SIPCall *sipcall) :
    sipcall_(sipcall)
{
    using std::pair;
    using std::string;

    if (mapInitialized_)
        return;

    _info("Zrtp: Initialize callbacks");

    // Information Map
    infoMap_.insert(pair<int32, string*> (InfoHelloReceived, new string("Hello received, preparing a Commit")));
    infoMap_.insert(pair<int32, string*> (InfoCommitDHGenerated, new string("Commit: Generated a public DH key")));
    infoMap_.insert(pair<int32, string*> (InfoRespCommitReceived, new string("Responder: Commit received, preparing DHPart1")));
    infoMap_.insert(pair<int32, string*> (InfoDH1DHGenerated, new string("DH1Part: Generated a public DH key")));
    infoMap_.insert(pair<int32, string*> (InfoInitDH1Received, new string("Initiator: DHPart1 received, preparing DHPart2")));
    infoMap_.insert(pair<int32, string*> (InfoRespDH2Received, new string("Responder: DHPart2 received, preparing Confirm1")));
    infoMap_.insert(pair<int32, string*> (InfoInitConf1Received, new string("Initiator: Confirm1 received, preparing Confirm2")));
    infoMap_.insert(pair<int32, string*> (InfoRespConf2Received, new string("Responder: Confirm2 received, preparing Conf2Ack")));
    infoMap_.insert(pair<int32, string*> (InfoRSMatchFound, new string("At least one retained secrets matches - security OK")));
    infoMap_.insert(pair<int32, string*> (InfoSecureStateOn, new string("Entered secure state")));
    infoMap_.insert(pair<int32, string*> (InfoSecureStateOff, new string("No more security for this session")));

    // Warning Map
    warningMap_.insert(pair<int32, string*> (WarningDHAESmismatch,
                       new string("Commit contains an AES256 cipher but does not offer a Diffie-Helman 4096")));
    warningMap_.insert(pair<int32, string*> (WarningGoClearReceived, new string("Received a GoClear message")));
    warningMap_.insert(pair<int32, string*> (WarningDHShort,
                       new string("Hello offers an AES256 cipher but does not offer a Diffie-Helman 4096")));
    warningMap_.insert(pair<int32, string*> (WarningNoRSMatch, new string("No retained secret matches - verify SAS")));
    warningMap_.insert(pair<int32, string*> (WarningCRCmismatch, new string("Internal ZRTP packet checksum mismatch - packet dropped")));
    warningMap_.insert(pair<int32, string*> (WarningSRTPauthError, new string("Dropping packet because SRTP authentication failed!")));
    warningMap_.insert(pair<int32, string*> (WarningSRTPreplayError, new string("Dropping packet because SRTP replay check failed!")));

    severeMap_.insert(pair<int32, string*> (SevereHelloHMACFailed, new string("Hash HMAC check of Hello failed!")));
    severeMap_.insert(pair<int32, string*> (SevereCommitHMACFailed, new string("Hash HMAC check of Commit failed!")));
    severeMap_.insert(pair<int32, string*> (SevereDH1HMACFailed, new string("Hash HMAC check of DHPart1 failed!")));
    severeMap_.insert(pair<int32, string*> (SevereDH2HMACFailed, new string("Hash HMAC check of DHPart2 failed!")));
    severeMap_.insert(pair<int32, string*> (SevereCannotSend, new string("Cannot send data - connection or peer down?")));
    severeMap_.insert(pair<int32, string*> (SevereProtocolError, new string("Internal protocol error occured!")));
    severeMap_.insert(pair<int32, string*> (SevereNoTimer, new string("Cannot start a timer - internal resources exhausted?")));
    severeMap_.insert(pair<int32, string*> (SevereTooMuchRetries,
                                            new string("Too much retries during ZRTP negotiation - connection or peer down?")));

    /**
     * Zrtp protocol related messages map
     */

    zrtpMap_.insert(pair<int32, string*> (MalformedPacket, new string("Malformed packet (CRC OK, but wrong structure)")));
    zrtpMap_.insert(pair<int32, string*> (CriticalSWError, new string("Critical software error")));
    zrtpMap_.insert(pair<int32, string*> (UnsuppZRTPVersion, new string("Unsupported ZRTP version")));
    zrtpMap_.insert(pair<int32, string*> (HelloCompMismatch, new string("Hello components mismatch")));
    zrtpMap_.insert(pair<int32, string*> (UnsuppHashType, new string("Hash type not supported")));
    zrtpMap_.insert(pair<int32, string*> (UnsuppCiphertype, new string("Cipher type not supported")));
    zrtpMap_.insert(pair<int32, string*> (UnsuppPKExchange, new string("Public key exchange not supported")));
    zrtpMap_.insert(pair<int32, string*> (UnsuppSRTPAuthTag, new string("SRTP auth. tag not supported")));
    zrtpMap_.insert(pair<int32, string*> (UnsuppSASScheme, new string("SAS scheme not supported")));
    zrtpMap_.insert(pair<int32, string*> (NoSharedSecret, new string("No shared secret available, DH mode required")));
    zrtpMap_.insert(pair<int32, string*> (DHErrorWrongPV, new string("DH Error: bad pvi or pvr ( == 1, 0, or p-1)")));
    zrtpMap_.insert(pair<int32, string*> (DHErrorWrongHVI, new string("DH Error: hvi != hashed data")));
    zrtpMap_.insert(pair<int32, string*> (SASuntrustedMiTM, new string("Received relayed SAS from untrusted MiTM")));
    zrtpMap_.insert(pair<int32, string*> (ConfirmHMACWrong, new string("Auth. Error: Bad Confirm pkt HMAC")));
    zrtpMap_.insert(pair<int32, string*> (NonceReused, new string("Nonce reuse")));
    zrtpMap_.insert(pair<int32, string*> (EqualZIDHello, new string("Equal ZIDs in Hello")));
    zrtpMap_.insert(pair<int32, string*> (GoCleatNotAllowed, new string("GoClear packet received, but not allowed")));

    mapInitialized_ = true;
}

void
ZrtpSessionCallback::secureOn(std::string cipher)
{
    _debug("Zrtp: Secure mode is on with cipher %s", cipher.c_str());
    Manager::instance().getDbusManager()->getCallManager()->secureZrtpOn(sipcall_->getCallId(), cipher);
}

void
ZrtpSessionCallback::secureOff()
{
    _debug("Zrtp: Secure mode is off");
    Manager::instance().getDbusManager()->getCallManager()->secureZrtpOff(sipcall_->getCallId());
}

void
ZrtpSessionCallback::showSAS(std::string sas, bool verified)
{
    _debug("Zrtp: SAS is: %s", sas.c_str());
    Manager::instance().getDbusManager()->getCallManager()->showSAS(sipcall_->getCallId(), sas, verified);
}


void
ZrtpSessionCallback::zrtpNotSuppOther()
{
    _debug("Zrtp: Callee does not support ZRTP");
    Manager::instance().getDbusManager()->getCallManager()->zrtpNotSuppOther(sipcall_->getCallId());
}


void
ZrtpSessionCallback::showMessage(GnuZrtpCodes::MessageSeverity sev, int32_t subCode)
{
    if (sev == ZrtpError) {
        if (subCode < 0) {  // received an error packet from peer
            subCode *= -1;
            _debug("Received an error packet from peer:");
        } else
            _debug("Sent error packet to peer:");
    }
}

void
ZrtpSessionCallback::zrtpNegotiationFailed(MessageSeverity severity, int subCode)
{
    std::string* msg;

    if (severity == ZrtpError) {
        if (subCode < 0) {  // received an error packet from peer
            subCode *= -1;
            _debug("Zrtp: Received error packet: ");
        } else
            _debug("Zrtp: Sent error packet: ");

        msg = zrtpMap_[subCode];

        if (msg != NULL) {
            _debug("%s", msg->c_str());
            Manager::instance().getDbusManager()->getCallManager()->zrtpNegotiationFailed(sipcall_->getCallId(), *msg, "ZRTP");
        }
    } else {
        msg = severeMap_[subCode];
        _debug("%s", msg->c_str());
        Manager::instance().getDbusManager()->getCallManager()->zrtpNegotiationFailed(sipcall_->getCallId(), *msg, "severe");
    }
}

void
ZrtpSessionCallback::confirmGoClear()
{
    _debug("Zrtp: Received go clear message. Until confirmation, ZRTP won't send any data");
    Manager::instance().getDbusManager()->getCallManager()->zrtpNotSuppOther(sipcall_->getCallId());
}

std::map<int32, std::string*>ZrtpSessionCallback::infoMap_;
std::map<int32, std::string*>ZrtpSessionCallback::warningMap_;
std::map<int32, std::string*>ZrtpSessionCallback::severeMap_;
std::map<int32, std::string*>ZrtpSessionCallback::zrtpMap_;

bool ZrtpSessionCallback::mapInitialized_ = false;
}

