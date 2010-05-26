/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#include "ZrtpSessionCallback.h"

#include "global.h"
#include "sip/sipcall.h"
#include "dbus/dbusmanager.h"
#include "dbus/callmanager.h"

#include <cstdlib>
#include <string>
#include <map>

using namespace GnuZrtpCodes;

using namespace ost;

using namespace std;

namespace sfl
{

ZrtpSessionCallback::ZrtpSessionCallback (SIPCall *sipcall) :
        _sipcall (sipcall)
{

    if (_mapInitialized) {
        return;
    }

    _info("Zrtp: Initialize callbacks");

    /**
     * Information Map
     */

    _infoMap.insert (pair<int32, std::string*> (InfoHelloReceived, new string ("Hello received, preparing a Commit")));
    _infoMap.insert (pair<int32, std::string*> (InfoCommitDHGenerated, new string ("Commit: Generated a public DH key")));
    _infoMap.insert (pair<int32, std::string*> (InfoRespCommitReceived, new string ("Responder: Commit received, preparing DHPart1")));
    _infoMap.insert (pair<int32, std::string*> (InfoDH1DHGenerated, new string ("DH1Part: Generated a public DH key")));
    _infoMap.insert (pair<int32, std::string*> (InfoInitDH1Received, new string ("Initiator: DHPart1 received, preparing DHPart2")));
    _infoMap.insert (pair<int32, std::string*> (InfoRespDH2Received, new string ("Responder: DHPart2 received, preparing Confirm1")));
    _infoMap.insert (pair<int32, std::string*> (InfoInitConf1Received, new string ("Initiator: Confirm1 received, preparing Confirm2")));
    _infoMap.insert (pair<int32, std::string*> (InfoRespConf2Received, new string ("Responder: Confirm2 received, preparing Conf2Ack")));
    _infoMap.insert (pair<int32, std::string*> (InfoRSMatchFound, new string ("At least one retained secrets matches - security OK")));
    _infoMap.insert (pair<int32, std::string*> (InfoSecureStateOn, new string ("Entered secure state")));
    _infoMap.insert (pair<int32, std::string*> (InfoSecureStateOff, new string ("No more security for this session")));

    /**
     * Warning Map
     */

    _warningMap.insert (pair<int32, std::string*> (WarningDHAESmismatch,
                        new string ("Commit contains an AES256 cipher but does not offer a Diffie-Helman 4096")));
    _warningMap.insert (pair<int32, std::string*> (WarningGoClearReceived, new string ("Received a GoClear message")));
    _warningMap.insert (pair<int32, std::string*> (WarningDHShort,
                        new string ("Hello offers an AES256 cipher but does not offer a Diffie-Helman 4096")));
    _warningMap.insert (pair<int32, std::string*> (WarningNoRSMatch, new string ("No retained secret matches - verify SAS")));
    _warningMap.insert (pair<int32, std::string*> (WarningCRCmismatch, new string ("Internal ZRTP packet checksum mismatch - packet dropped")));
    _warningMap.insert (pair<int32, std::string*> (WarningSRTPauthError, new string ("Dropping packet because SRTP authentication failed!")));
    _warningMap.insert (pair<int32, std::string*> (WarningSRTPreplayError, new string ("Dropping packet because SRTP replay check failed!")));

    _severeMap.insert (pair<int32, std::string*> (SevereHelloHMACFailed, new string ("Hash HMAC check of Hello failed!")));
    _severeMap.insert (pair<int32, std::string*> (SevereCommitHMACFailed, new string ("Hash HMAC check of Commit failed!")));
    _severeMap.insert (pair<int32, std::string*> (SevereDH1HMACFailed, new string ("Hash HMAC check of DHPart1 failed!")));
    _severeMap.insert (pair<int32, std::string*> (SevereDH2HMACFailed, new string ("Hash HMAC check of DHPart2 failed!")));
    _severeMap.insert (pair<int32, std::string*> (SevereCannotSend, new string ("Cannot send data - connection or peer down?")));
    _severeMap.insert (pair<int32, std::string*> (SevereProtocolError, new string ("Internal protocol error occured!")));
    _severeMap.insert (pair<int32, std::string*> (SevereNoTimer, new string ("Cannot start a timer - internal resources exhausted?")));
    _severeMap.insert (pair<int32, std::string*> (SevereTooMuchRetries,
                       new string ("Too much retries during ZRTP negotiation - connection or peer down?")));

    /**
     * Zrtp protocol related messages map
     */

    _zrtpMap.insert (pair<int32, std::string*> (MalformedPacket, new string ("Malformed packet (CRC OK, but wrong structure)")));
    _zrtpMap.insert (pair<int32, std::string*> (CriticalSWError, new string ("Critical software error")));
    _zrtpMap.insert (pair<int32, std::string*> (UnsuppZRTPVersion, new string ("Unsupported ZRTP version")));
    _zrtpMap.insert (pair<int32, std::string*> (HelloCompMismatch, new string ("Hello components mismatch")));
    _zrtpMap.insert (pair<int32, std::string*> (UnsuppHashType, new string ("Hash type not supported")));
    _zrtpMap.insert (pair<int32, std::string*> (UnsuppCiphertype, new string ("Cipher type not supported")));
    _zrtpMap.insert (pair<int32, std::string*> (UnsuppPKExchange, new string ("Public key exchange not supported")));
    _zrtpMap.insert (pair<int32, std::string*> (UnsuppSRTPAuthTag, new string ("SRTP auth. tag not supported")));
    _zrtpMap.insert (pair<int32, std::string*> (UnsuppSASScheme, new string ("SAS scheme not supported")));
    _zrtpMap.insert (pair<int32, std::string*> (NoSharedSecret, new string ("No shared secret available, DH mode required")));
    _zrtpMap.insert (pair<int32, std::string*> (DHErrorWrongPV, new string ("DH Error: bad pvi or pvr ( == 1, 0, or p-1)")));
    _zrtpMap.insert (pair<int32, std::string*> (DHErrorWrongHVI, new string ("DH Error: hvi != hashed data")));
    _zrtpMap.insert (pair<int32, std::string*> (SASuntrustedMiTM, new string ("Received relayed SAS from untrusted MiTM")));
    _zrtpMap.insert (pair<int32, std::string*> (ConfirmHMACWrong, new string ("Auth. Error: Bad Confirm pkt HMAC")));
    _zrtpMap.insert (pair<int32, std::string*> (NonceReused, new string ("Nonce reuse")));
    _zrtpMap.insert (pair<int32, std::string*> (EqualZIDHello, new string ("Equal ZIDs in Hello")));
    _zrtpMap.insert (pair<int32, std::string*> (GoCleatNotAllowed, new string ("GoClear packet received, but not allowed")));

    _mapInitialized = true;
}

void
ZrtpSessionCallback::secureOn (std::string cipher)
{
    _debug ("Zrtp: Secure mode is on with cipher %s", cipher.c_str());
    DBusManager::instance().getCallManager()->secureZrtpOn (_sipcall->getCallId(), cipher);
}

void
ZrtpSessionCallback::secureOff (void)
{
    _debug ("Zrtp: Secure mode is off");
    DBusManager::instance().getCallManager()->secureZrtpOff (_sipcall->getCallId());
}

void
ZrtpSessionCallback::showSAS (std::string sas, bool verified)
{
    _debug ("Zrtp: SAS is: %s", sas.c_str());
    DBusManager::instance().getCallManager()->showSAS (_sipcall->getCallId(), sas, verified);
}


void
ZrtpSessionCallback::zrtpNotSuppOther()
{
    _debug ("Zrtp: Callee does not support ZRTP");
    DBusManager::instance().getCallManager()->zrtpNotSuppOther (_sipcall->getCallId());
}


void
ZrtpSessionCallback::showMessage (GnuZrtpCodes::MessageSeverity sev, int32_t subCode)
{
    string* msg;

    if (sev == Info) {
        msg = _infoMap[subCode];

        if (msg != NULL) {
        }
    }

    if (sev == Warning) {
        msg = _warningMap[subCode];

        if (msg != NULL) {
        }
    }

    if (sev == Severe) {
        msg = _severeMap[subCode];

        if (msg != NULL) {
        }
    }



    if (sev == ZrtpError) {
        if (subCode < 0) {  // received an error packet from peer
            subCode *= -1;
            _debug ("Received an error packet from peer:");
        } else {
            _debug ("Sent error packet to peer:");
        }

        msg = _zrtpMap[subCode];

        if (msg != NULL) {

        }
    }
}

void
ZrtpSessionCallback::zrtpNegotiationFailed (MessageSeverity severity, int subCode)
{
    string* msg;

    if (severity == ZrtpError) {
        if (subCode < 0) {  // received an error packet from peer
            subCode *= -1;
            _debug ("Zrtp: Received error packet: ");
        } else {
            _debug ("Zrtp: Sent error packet: ");
        }

        msg = _zrtpMap[subCode];

        if (msg != NULL) {
            _debug ("%s", msg->c_str());
            DBusManager::instance().getCallManager()->zrtpNegotiationFailed (_sipcall->getCallId(), *msg, "ZRTP");
        }
    } else {
        msg = _severeMap[subCode];
        _debug ("%s", msg->c_str());
        DBusManager::instance().getCallManager()->zrtpNegotiationFailed (_sipcall->getCallId(), *msg, "severe");
    }
}

void
ZrtpSessionCallback::confirmGoClear()
{
    _debug ("Zrtp: Received go clear message. Until confirmation, ZRTP won't send any data");
    DBusManager::instance().getCallManager()->zrtpNotSuppOther (_sipcall->getCallId());
}

map<int32, std::string*>ZrtpSessionCallback::_infoMap;
map<int32, std::string*>ZrtpSessionCallback::_warningMap;
map<int32, std::string*>ZrtpSessionCallback::_severeMap;
map<int32, std::string*>ZrtpSessionCallback::_zrtpMap;

bool ZrtpSessionCallback::_mapInitialized = false;
}

