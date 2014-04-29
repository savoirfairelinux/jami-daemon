/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include "logger.h"
#include "sip/sipcall.h"
#include "client/client.h"
#include "client/callmanager.h"
#include "manager.h"

#include <cstdlib>
#include <map>

using namespace GnuZrtpCodes;

using namespace ost;

namespace sfl {

ZrtpSessionCallback::ZrtpSessionCallback(SIPCall &call) : call_(call)
{
    // we've already initialized the maps, we only need to check one
    if (not infoMap_.empty())
        return;

    // Information Map
    infoMap_[InfoHelloReceived] = "Hello received, preparing a Commit";
    infoMap_[InfoCommitDHGenerated] =  "Commit: Generated a public DH key";
    infoMap_[InfoRespCommitReceived] = "Responder: Commit received, preparing DHPart1";
    infoMap_[InfoDH1DHGenerated] = "DH1Part: Generated a public DH key";
    infoMap_[InfoInitDH1Received] = "Initiator: DHPart1 received, preparing DHPart2";
    infoMap_[InfoRespDH2Received] = "Responder: DHPart2 received, preparing Confirm1";
    infoMap_[InfoInitConf1Received] = "Initiator: Confirm1 received, preparing Confirm2";
    infoMap_[InfoRespConf2Received] = "Responder: Confirm2 received, preparing Conf2Ack";
    infoMap_[InfoRSMatchFound] = "At least one retained secrets matches - security OK";
    infoMap_[InfoSecureStateOn] = "Entered secure state";
    infoMap_[InfoSecureStateOff] = "No more security for this session";

    // Warning Map
    warningMap_[WarningDHAESmismatch] = "Commit contains an AES256 cipher but does not offer a Diffie-Helman 4096";
    warningMap_[WarningGoClearReceived] = "Received a GoClear message";
    warningMap_[WarningDHShort] = "Hello offers an AES256 cipher but does not offer a Diffie-Helman 4096";
    warningMap_[WarningNoRSMatch] = "No retained secret matches - verify SAS";
    warningMap_[WarningCRCmismatch] = "Internal ZRTP packet checksum mismatch - packet dropped";
    warningMap_[WarningSRTPauthError] = "Dropping packet because SRTP authentication failed!";
    warningMap_[WarningSRTPreplayError] = "Dropping packet because SRTP replay check failed!";

    severeMap_[SevereHelloHMACFailed] = "Hash HMAC check of Hello failed!";
    severeMap_[SevereCommitHMACFailed] = "Hash HMAC check of Commit failed!";
    severeMap_[SevereDH1HMACFailed] = "Hash HMAC check of DHPart1 failed!";
    severeMap_[SevereDH2HMACFailed] = "Hash HMAC check of DHPart2 failed!";
    severeMap_[SevereCannotSend] = "Cannot send data - connection or peer down?";
    severeMap_[SevereProtocolError] = "Internal protocol error occured!";
    severeMap_[SevereNoTimer] = "Cannot start a timer - internal resources exhausted?";
    severeMap_[SevereTooMuchRetries] = "Too much retries during ZRTP negotiation - connection or peer down?";

    // Zrtp protocol related messages map
    zrtpMap_[MalformedPacket] = "Malformed packet (CRC OK, but wrong structure)";
    zrtpMap_[CriticalSWError] = "Critical software error";
    zrtpMap_[UnsuppZRTPVersion] = "Unsupported ZRTP version";
    zrtpMap_[HelloCompMismatch] = "Hello components mismatch";
    zrtpMap_[UnsuppHashType] = "Hash type not supported";
    zrtpMap_[UnsuppCiphertype] = "Cipher type not supported";
    zrtpMap_[UnsuppPKExchange] = "Public key exchange not supported";
    zrtpMap_[UnsuppSRTPAuthTag] = "SRTP auth. tag not supported";
    zrtpMap_[UnsuppSASScheme] = "SAS scheme not supported";
    zrtpMap_[NoSharedSecret] = "No shared secret available, DH mode required";
    zrtpMap_[DHErrorWrongPV] = "DH Error: bad pvi or pvr ( == 1, 0, or p-1)";
    zrtpMap_[DHErrorWrongHVI] = "DH Error: hvi != hashed data";
    zrtpMap_[SASuntrustedMiTM] = "Received relayed SAS from untrusted MiTM";
    zrtpMap_[ConfirmHMACWrong] = "Auth. Error: Bad Confirm pkt HMAC";
    zrtpMap_[NonceReused] = "Nonce reuse";
    zrtpMap_[EqualZIDHello] = "Equal ZIDs in Hello";
    zrtpMap_[GoCleatNotAllowed] = "GoClear packet received, but not allowed";
}

void
ZrtpSessionCallback::secureOn(std::string cipher)
{
    DEBUG("Secure mode is on with cipher %s", cipher.c_str());
    Manager::instance().getClient()->getCallManager()->secureZrtpOn(call_.getCallId(), cipher);
}

void
ZrtpSessionCallback::secureOff()
{
    DEBUG("Secure mode is off");
    Manager::instance().getClient()->getCallManager()->secureZrtpOff(call_.getCallId());
}

void
ZrtpSessionCallback::showSAS(std::string sas, bool verified)
{
    DEBUG("SAS is: %s", sas.c_str());
    Manager::instance().getClient()->getCallManager()->showSAS(call_.getCallId(), sas, verified);
}

void
ZrtpSessionCallback::zrtpNotSuppOther()
{
    DEBUG("Callee does not support ZRTP");
    Manager::instance().getClient()->getCallManager()->zrtpNotSuppOther(call_.getCallId());
}

void
ZrtpSessionCallback::showMessage(GnuZrtpCodes::MessageSeverity sev, int32_t subCode)
{
    if (sev == ZrtpError) {
        if (subCode < 0) {  // received an error packet from peer
            DEBUG("Received an error packet from peer:");
        } else
            DEBUG("Sent error packet to peer:");
    }
}

void
ZrtpSessionCallback::zrtpNegotiationFailed(MessageSeverity severity, int subCode)
{
    if (severity == ZrtpError) {
        if (subCode < 0) {  // received an error packet from peer
            subCode *= -1;
            DEBUG("Received error packet: ");
        } else
            DEBUG("Sent error packet: ");

        std::map<int32, std::string>::const_iterator iter = zrtpMap_.find(subCode);

        if (iter != zrtpMap_.end()) {
            DEBUG("%s", iter->second.c_str());
            Manager::instance().getClient()->getCallManager()->zrtpNegotiationFailed(call_.getCallId(), iter->second, "ZRTP");
        }
    } else {
        std::map<int32, std::string>::const_iterator iter = severeMap_.find(subCode);

        if (iter != severeMap_.end()) {
            DEBUG("%s", iter->second.c_str());
            Manager::instance().getClient()->getCallManager()->zrtpNegotiationFailed(call_.getCallId(), iter->second, "severe");
        }
    }
}

void
ZrtpSessionCallback::confirmGoClear()
{
    DEBUG("Received go clear message. Until confirmation, ZRTP won't send any data");
    Manager::instance().getClient()->getCallManager()->zrtpNotSuppOther(call_.getCallId());
}

std::map<int32, std::string> ZrtpSessionCallback::infoMap_;
std::map<int32, std::string> ZrtpSessionCallback::warningMap_;
std::map<int32, std::string> ZrtpSessionCallback::severeMap_;
std::map<int32, std::string> ZrtpSessionCallback::zrtpMap_;
}

