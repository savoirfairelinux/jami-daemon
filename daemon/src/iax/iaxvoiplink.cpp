/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "iaxvoiplink.h"
#include <unistd.h>
#include <cmath>
#include <algorithm>

#include "manager.h"
#include "iaxcall.h"
#include "iaxaccount.h"
#include "logger.h"
#include "hooks/urlhook.h"
#include "audio/audiolayer.h"
#include "audio/resampler.h"
#include "audio/ringbufferpool.h"
#include "array_size.h"
#include "map_utils.h"
#include "call_factory.h"
#include "ring_types.h"
#include "system_codec_container.h"
#include "intrin.h" // for UNUSED

namespace ring {

std::mutex IAXVoIPLink::mutexIAX = {};

IAXVoIPLink::IAXVoIPLink(IAXAccount& account) : account_(account), resampler_(new Resampler{44100})
{}

IAXVoIPLink::~IAXVoIPLink()
{
    terminate();
}

void
IAXVoIPLink::init(std::mt19937_64& rand_generator)
{
    if (initDone_)
        return;

    std::uniform_int_distribution<int> port_dist(1024, 65024);
    std::lock_guard<std::mutex> lock(mutexIAX);
    for (int port = IAX_DEFAULT_PORTNO, nbTry = 0; nbTry < 3 ; port = port_dist(rand_generator), nbTry++) {
        if (iax_init(port) >= 0) {
            Manager::instance().registerEventHandler((uintptr_t)this, std::bind(&IAXVoIPLink::handleEvents, this));
            initDone_ = true;
            break;
        }
    }
}

void
IAXVoIPLink::terminate()
{
    if (!initDone_)
        return;

    Manager::instance().unregisterEventHandler((uintptr_t)this);

    for (const auto& call : Manager::instance().callFactory.getAllCalls<IAXCall>()) {
        std::lock_guard<std::mutex> lock(mutexIAX);
        iax_hangup(call->session, const_cast<char*>("Dumped Call"));
        call->removeCall();
    }

    initDone_ = false;
}

static std::shared_ptr<IAXCall>
iaxGetCallFromSession(iax_session* session)
{
    for (auto call : Manager::instance().callFactory.getAllCalls<IAXCall>()) {
        if (call->session == session)
            return call;
    }
    return nullptr;
}

void
IAXVoIPLink::handleEvents()
{
    iax_event *event = NULL;

    {
        std::lock_guard<std::mutex> lock(mutexIAX);
        event = iax_get_event(0);
    }

    while (event != NULL) {

        // If we received an 'ACK', libiax2 tells apps to ignore them.
        if (event->etype == IAX_EVENT_NULL) {
            std::lock_guard<std::mutex> lock(mutexIAX);
            iax_event_free(event);
            event = iax_get_event(0);
            continue;
        }

        if (auto raw_call_ptr = iaxGetCallFromSession(event->session)) {
            iaxHandleCallEvent(event, *raw_call_ptr);
        } else if (event->session && account_.matchRegSession(event->session)) {
            // This is a registration session, deal with it
            iaxHandleRegReply(event);
        } else {
            // We've got an event before it's associated with any call
            iaxHandlePrecallEvent(event);
        }

        {
            std::lock_guard<std::mutex> lock(mutexIAX);
            iax_event_free(event);
            event = iax_get_event(0);
        }
    }

    account_.checkRegister();

    sendAudioFromMic();
}

void
IAXVoIPLink::sendAudioFromMic()
{
    for (const auto currentCall : Manager::instance().callFactory.getAllCalls<IAXCall>()) {
        if (currentCall->getState() != Call::ACTIVE)
            continue;

        int codecType = currentCall->getAudioCodecPayload();

        auto codec = account_.searchCodecByPayload(codecType, MEDIA_AUDIO);
        auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(codec);
        if (!accountAudioCodec)
            continue;

        Manager::instance().getRingBufferPool().setInternalSamplingRate(accountAudioCodec->audioformat.sample_rate);

        unsigned int mainBufferSampleRate = Manager::instance().getRingBufferPool().getInternalSamplingRate();

        // we have to get 20ms of data from the mic *20/1000 = /50
        // rate/50 shall be lower than IAX__20S_48KHZ_MAX
        size_t samples = mainBufferSampleRate * 20 / 1000;

        if (Manager::instance().getRingBufferPool().availableForGet(currentCall->getCallId()) < samples)
            continue;

        // Get bytes from micRingBuffer to data_from_mic
        rawBuffer_.resize(samples);
        samples = Manager::instance().getRingBufferPool().getData(rawBuffer_, currentCall->getCallId());

        int compSize = 0;
        unsigned int audioRate = accountAudioCodec->audioformat.sample_rate;
        int outSamples;
        UNUSED AudioBuffer *in;

        if (audioRate != mainBufferSampleRate) {
            rawBuffer_.setSampleRate(audioRate);
            resampledData_.setSampleRate(mainBufferSampleRate);
            resampler_->resample(rawBuffer_, resampledData_);
            in = &resampledData_;
            outSamples = 0;
        } else {
            outSamples = samples;
            in = &rawBuffer_;
        }

    /*
     * TODO ebail : *
     * IAX use old codec API (based on audiocodec wrapper)
     * It does not use libav API
     * We disable it for the moment
     */
#if 0
        compSize = audioCodec->encode(in->getData(), encodedData_, RAW_BUFFER_SIZE);
#endif

        if (currentCall->session and samples > 0) {
            std::lock_guard<std::mutex> lock(mutexIAX);

            if (iax_send_voice(currentCall->session, currentCall->format,
                               encodedData_, compSize, outSamples) == -1)
                RING_ERR("IAX: Error sending voice data.");
        }
    }
}

void
IAXVoIPLink::handleReject(IAXCall& call)
{
    call.setConnectionState(Call::CONNECTED);
    call.setState(Call::MERROR);
    Manager::instance().callFailure(call);
    call.removeCall();
}

void
IAXVoIPLink::handleAccept(iax_event* event, IAXCall& call)
{
    if (event->ies.format)
        call.format = event->ies.format;
}

void
IAXVoIPLink::handleAnswerTransfer(iax_event* event, IAXCall& call)
{
    if (call.getConnectionState() == Call::CONNECTED)
        return;

    call.setConnectionState(Call::CONNECTED);
    call.setState(Call::ACTIVE);

    if (event->ies.format)
        call.format = event->ies.format;

    Manager::instance().addStream(call);
    Manager::instance().peerAnsweredCall(call);
    Manager::instance().startAudioDriverStream();
    Manager::instance().getRingBufferPool().flushAllBuffers();
}

void
IAXVoIPLink::handleBusy(IAXCall& call)
{
    call.setConnectionState(Call::CONNECTED);
    call.setState(Call::BUSY);

    Manager::instance().callBusy(call);
    call.removeCall();
}

#if HAVE_INSTANT_MESSAGING
void
IAXVoIPLink::handleMessage(iax_event* event, IAXCall& call)
{
    Manager::instance().incomingMessage(call.getCallId(), call.getPeerNumber(),
                                        std::string((const char*) event->data));
}
#endif

void
IAXVoIPLink::handleRinging(IAXCall& call)
{
    call.setConnectionState(Call::RINGING);
    Manager::instance().peerRingingCall(call);
}

void
IAXVoIPLink::handleHangup(IAXCall& call)
{
    Manager::instance().peerHungupCall(call);
    call.removeCall();
}

void
IAXVoIPLink::iaxHandleCallEvent(iax_event* event, IAXCall& call)
{
    switch (event->etype) {
        case IAX_EVENT_HANGUP:
            handleHangup(call);
            break;

        case IAX_EVENT_REJECT:
            handleReject(call);
            break;

        case IAX_EVENT_ACCEPT:
            handleAccept(event, call);
            break;

        case IAX_EVENT_ANSWER:
        case IAX_EVENT_TRANSFER:
            handleAnswerTransfer(event, call);
            break;

        case IAX_EVENT_BUSY:
            handleBusy(call);
            break;

        case IAX_EVENT_VOICE:
            iaxHandleVoiceEvent(event, call);
            break;

        case IAX_EVENT_TEXT:
#if HAVE_INSTANT_MESSAGING
            handleMessage(event, call);
#endif
            break;

        case IAX_EVENT_RINGA:
            handleRinging(call);
            break;

        case IAX_IE_MSGCOUNT:
        case IAX_EVENT_TIMEOUT:
        case IAX_EVENT_PONG:
        default:
            break;

        case IAX_EVENT_URL:

            if (Manager::instance().hookPreference.getIax2Enabled())
                UrlHook::runAction(Manager::instance().hookPreference.getUrlCommand(), (char*) event->data);

            break;
    }
}

/* Handle audio event, VOICE packet received */
void
IAXVoIPLink::iaxHandleVoiceEvent(iax_event* event, IAXCall& call)
{
    // Skip this empty packet.
    if (!event->datalen)
        return;

    auto codec = account_.searchCodecByPayload(call.getAudioCodecPayload(), MEDIA_AUDIO);
    auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(codec);
    if (!accountAudioCodec)
        return;

    Manager::instance().getRingBufferPool().setInternalSamplingRate(accountAudioCodec->audioformat.sample_rate);
    unsigned int mainBufferSampleRate = Manager::instance().getRingBufferPool().getInternalSamplingRate();

    if (event->subclass)
        call.format = event->subclass;

    unsigned int size = event->datalen;
    unsigned int max = accountAudioCodec->audioformat.sample_rate * 20 / 1000;

    if (size > max)
        size = max;

    /*
     * TODO ebail : *
     * IAX use old codec API (based on audiocodec wrapper)
     * It does not use libav API
     * We disable it for the moment
     */
#if 0
    unsigned char *data = (unsigned char*) event->data;
    audioCodec->decode(rawBuffer_.getData(), data , size);
#endif
    AudioBuffer *out = &rawBuffer_;
    unsigned int audioRate = accountAudioCodec->audioformat.sample_rate;

    if (audioRate != mainBufferSampleRate) {
        rawBuffer_.setSampleRate(mainBufferSampleRate);
        resampledData_.setSampleRate(audioRate);
        resampler_->resample(rawBuffer_, resampledData_);
        out = &resampledData_;
    }

    call.putAudioData(*out);
}

/**
 * Handle the registration process
 */
void
IAXVoIPLink::iaxHandleRegReply(iax_event* event)
{
    if (event->etype != IAX_EVENT_REGREJ && event->etype != IAX_EVENT_REGACK)
        return;

    account_.destroyRegSession();
    account_.setRegistrationState((event->etype == IAX_EVENT_REGREJ) ? RegistrationState::ERROR_AUTH : RegistrationState::REGISTERED);

    if (event->etype == IAX_EVENT_REGACK)
        account_.setNextRefreshStamp(time(NULL) + (event->ies.refresh ? event->ies.refresh : 60));
}

void IAXVoIPLink::iaxHandlePrecallEvent(iax_event* event)
{
    const auto accountID = account_.getAccountID();
    std::shared_ptr<IAXCall> call;

    switch (event->etype) {
        case IAX_EVENT_CONNECT:
            call = account_.newIncomingCall<IAXCall>("");
            if (!call) {
                RING_ERR("failed to create an incoming IAXCall from account %s",
                      accountID.c_str());
                return;
            }

            call->session = event->session;
            call->setConnectionState(Call::PROGRESSING);

            if (event->ies.calling_number)
                call->setPeerNumber(event->ies.calling_number);

            if (event->ies.calling_name)
                call->setDisplayName(std::string(event->ies.calling_name));

            // if peerNumber exist append it to the name string
            if (event->ies.calling_number)
                call->initRecFilename(std::string(event->ies.calling_number));

            Manager::instance().incomingCall(*call, accountID);

            call->format = call->getFirstMatchingFormat(event->ies.format, accountID);
            if (!call->format)
                call->format = call->getFirstMatchingFormat(event->ies.capability, accountID);

            {
                std::lock_guard<std::mutex> lock(mutexIAX);
                iax_accept(event->session, call->format);
                iax_ring_announce(event->session);
            }

            break;

        case IAX_EVENT_HANGUP:
            if (auto raw_call_ptr = iaxGetCallFromSession(event->session)) {
                Manager::instance().peerHungupCall(*raw_call_ptr);
                raw_call_ptr->removeCall();
            }

            break;

        case IAX_EVENT_TIMEOUT: // timeout for an unknown session
        case IAX_IE_MSGCOUNT:
        case IAX_EVENT_REGACK:
        case IAX_EVENT_REGREJ:
        case IAX_EVENT_REGREQ:

            // Received when someone wants to register to us!?!
            // Asterisk receives and answers to that, not us, we're a phone.
        default:
            break;
    }
}

} // namespace ring
