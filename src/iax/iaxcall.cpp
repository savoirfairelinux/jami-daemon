/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
#include <iax/iax-client.h>
#include <iax/frame.h>

#include "intrin.h"
#include "iaxcall.h"
#include "logger.h"
#include "manager.h"
#include "iaxaccount.h"
#include "iaxvoiplink.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"

#if HAVE_INSTANT_MESSAGING
#include "im/instant_messaging.h"
#endif

namespace ring {

const char* const IAXCall::LINK_TYPE = IAXAccount::ACCOUNT_TYPE;

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
            RING_ERR("Codec %d not supported!", c);
            return 0;
    }
}

IAXCall::IAXCall(IAXAccount& account, const std::string& id, Call::CallType type)
    : Call(account, id, type),
      format(0),
      session(NULL)
{
    ringbuffer_ = Manager::instance().getRingBufferPool().createRingBuffer(getCallId());
}

int
IAXCall::getSupportedFormat(const std::string &accountID) const
{
    const auto account = Manager::instance().getAccount(accountID);

    int format_mask = 0;

    if (account) {
        std::vector<unsigned> codecs{account->getActiveAccountCodecInfoIdList(MEDIA_AUDIO)};

        for (const auto &i : codecs)
            format_mask |= codecToASTFormat(i);
    } else
        RING_ERR("No IAx account could be found");

    return format_mask;
}

int
IAXCall::getFirstMatchingFormat(int needles, const std::string &accountID) const
{
    const auto account = Manager::instance().getAccount(accountID);

    if (account != NULL) {
        std::vector<unsigned> codecs{account->getActiveAccountCodecInfoIdList(MEDIA_AUDIO)};

        for (const auto &i : codecs) {
            int format_mask = codecToASTFormat(i);

            // Return the first that matches
            if (format_mask & needles)
                return format_mask;
        }
    } else
        RING_ERR("No IAx account could be found");

    return 0;
}

int
IAXCall::getAudioCodecPayload() const
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
            RING_ERR("IAX: Format %d not supported!", format);
            return -1;
    }
}

void
IAXCall::answer()
{
    Manager::instance().addStream(*this);

    {
        std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
        iax_answer(session);
    }

    setState(Call::CallState::ACTIVE, Call::ConnectionState::CONNECTED);

    Manager::instance().getRingBufferPool().flushAllBuffers();
}

void
IAXCall::hangup(int reason UNUSED)
{
    Manager::instance().getRingBufferPool().unBindAll(getCallId());

    std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
    iax_hangup(session, (char*) "Dumped Call");
    session = nullptr;

    removeCall();
}

void
IAXCall::refuse()
{
    {
        std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
        iax_reject(session, (char*) "Call rejected manually.");
    }

    removeCall();
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

bool
IAXCall::onhold()
{
    Manager::instance().getRingBufferPool().unBindAll(getCallId());

    {
        std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
        iax_quelch_moh(session, true);
    }

    return setState(Call::CallState::HOLD);
}

bool
IAXCall::offhold()
{
    Manager::instance().addStream(*this);

    {
        std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
        iax_unquelch(session);
    }

    if (setState(Call::CallState::ACTIVE)) {
        Manager::instance().startAudioDriverStream();
        return true;
    }

    return false;
}

void
IAXCall::peerHungup()
{
    Manager::instance().getRingBufferPool().unBindAll(getCallId());
    session = nullptr;
    Call::peerHungup();
}

void
IAXCall::carryingDTMFdigits(char code)
{
    std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
    iax_send_dtmf(session, code);
}

#if HAVE_INSTANT_MESSAGING
void
IAXCall::sendTextMessage(const std::map<std::string, std::string>& messages,
                         const std::string& /*from*/)
{
    std::lock_guard<std::mutex> lock(IAXVoIPLink::mutexIAX);
    const auto& msgs = InstantMessaging::appendMimePayloads(messages);
    InstantMessaging::sendIaxMessage(session, getCallId(), msgs);
}
#endif

void
IAXCall::putAudioData(AudioBuffer& buf)
{
    ringbuffer_->put(buf);
}

} // namespace ring
