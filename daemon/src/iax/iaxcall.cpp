/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#include <cstring>
#include <sys/socket.h>
#include <iax-client.h>
#include <frame.h>

#include "iaxcall.h"
#include "logger.h"
#include "manager.h"
#include "iaxaccount.h"
#include "iaxvoiplink.h"

#if HAVE_INSTANT_MESSAGING
#include "im/instant_messaging.h"
#endif

static int
codecToASTFormat(int c)
{
    switch (c) {
        case PAYLOAD_CODEC_ULAW:
            return AST_FORMAT_ULAW;
        case PAYLOAD_CODEC_GSM:
            return AST_FORMAT_GSM;
        case PAYLOAD_CODEC_ALAW:
            return AST_FORMAT_ALAW;
        case PAYLOAD_CODEC_ILBC_20:
            return AST_FORMAT_ILBC;
        case PAYLOAD_CODEC_SPEEX_8000:
            return AST_FORMAT_SPEEX;

        default:
            ERROR("Codec %d not supported!", c);
            return 0;
    }
}

IAXCall::IAXCall(const std::string& id, Call::CallType type,
                 IAXAccount& account, IAXVoIPLink* link) :
    Call(id, type, account), format(0), session(NULL), link_(link)
{}

int IAXCall::getSupportedFormat(const std::string &accountID) const
{
    using std::vector;
    const auto& account = Manager::instance().getAccount(accountID);

    int format_mask = 0;

    if (account) {
        vector<int> codecs(account->getActiveAudioCodecs());

        for (const auto &i : codecs)
            format_mask |= codecToASTFormat(i);
    } else
        ERROR("No IAx account could be found");

    return format_mask;
}

int IAXCall::getFirstMatchingFormat(int needles, const std::string &accountID) const
{
    using std::vector;
    const auto& account = Manager::instance().getAccount(accountID);

    if (account != NULL) {
        vector<int> codecs(account->getActiveAudioCodecs());

        for (const auto &i : codecs) {
            int format_mask = codecToASTFormat(i);

            // Return the first that matches
            if (format_mask & needles)
                return format_mask;
        }
    } else
        ERROR("No IAx account could be found");

    return 0;
}

int IAXCall::getAudioCodec() const
{
    switch (format) {
        case AST_FORMAT_ULAW:
            return PAYLOAD_CODEC_ULAW;
        case AST_FORMAT_GSM:
            return PAYLOAD_CODEC_GSM;
        case AST_FORMAT_ALAW:
            return PAYLOAD_CODEC_ALAW;
        case AST_FORMAT_ILBC:
            return PAYLOAD_CODEC_ILBC_20;
        case AST_FORMAT_SPEEX:
            return PAYLOAD_CODEC_SPEEX_8000;
        default:
            ERROR("IAX: Format %d not supported!", format);
            return -1;
    }
}

VoIPLink*
IAXCall::getVoIPLink() const
{ return link_; }

void
IAXCall::answer()
{
    Manager::instance().addStream(getCallId());

    {
        std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
        iax_answer(session);
    }

    setState(Call::ACTIVE);
    setConnectionState(Call::CONNECTED);

    Manager::instance().getMainBuffer().flushAllBuffers();
}

void
IAXCall::hangup(int reason UNUSED)
{
    Manager::instance().getMainBuffer().unBindAll(getCallId());

    std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
    iax_hangup(session, (char*) "Dumped Call");
    session = nullptr;

    link_->removeIaxCall(getCallId());
}

void
IAXCall::refuse()
{
    {
        std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
        iax_reject(session, (char*) "Call rejected manually.");
    }

    link_->removeIaxCall(getCallId());
}

void
IAXCall::transfer(const std::string& to)
{
    std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
    char callto[to.length() + 1];
    strcpy(callto, to.c_str());
    iax_transfer(session, callto);
}

bool
IAXCall::attendedTransfer(const std::string& /*targetID*/)
{
    return false; // TODO
}

void
IAXCall::onhold()
{
    Manager::instance().getMainBuffer().unBindAll(getCallId());

    {
        std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
        iax_quelch_moh(session, true);
    }

    setState(Call::HOLD);
}

void
IAXCall::offhold()
{
    Manager::instance().addStream(getCallId());

    {
        std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
        iax_unquelch(session);
    }

    setState(Call::ACTIVE);

    Manager::instance().startAudioDriverStream();
}

void
IAXCall::peerHungup()
{
    Manager::instance().getMainBuffer().unBindAll(getCallId());

    session = nullptr;
}

void
IAXCall::carryingDTMFdigits(char code)
{
    std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
    iax_send_dtmf(session, code);
}

#if HAVE_INSTANT_MESSAGING
void
IAXCall::sendTextMessage(const std::string& message, const std::string& /*from*/)
{
    std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
    sfl::InstantMessaging::send_iax_message(session, getCallId(), message.c_str());
}
#endif
