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
#include "iaxvoiplink.h"
#include <unistd.h>
#include <cmath>
#include <algorithm>

#include "iaxcall.h"
#include "im/instant_messaging.h"
#include "iaxaccount.h"
#include "logger.h"
#include "manager.h"
#include "hooks/urlhook.h"
#include "audio/audiolayer.h"
#include "audio/resampler.h"
#include "array_size.h"
#include "map_utils.h"

AccountMap IAXVoIPLink::iaxAccountMap_;
IAXCallMap IAXVoIPLink::iaxCallMap_;
std::mutex IAXVoIPLink::iaxCallMapMutex_;

IAXVoIPLink::IAXVoIPLink(const std::string& accountID) :
    regSession_(NULL)
    , nextRefreshStamp_(0)
    , mutexIAX_()
    , rawBuffer_(RAW_BUFFER_SIZE, AudioFormat::MONO)
    , resampledData_(RAW_BUFFER_SIZE * 4, AudioFormat::MONO)
    , encodedData_()
    , resampler_(44100)
    , initDone_(false)
    , accountID_(accountID)
{
    srand(time(NULL));    // to get random number for RANDOM_PORT
}


IAXVoIPLink::~IAXVoIPLink()
{
    regSession_ = NULL; // shall not delete it // XXX: but why?
    terminate();

    // This is our last account
    if (iaxAccountMap_.size() == 1)
        clearIaxCallMap();
}

void
IAXVoIPLink::init()
{
    if (initDone_)
        return;

    for (int port = IAX_DEFAULT_PORTNO, nbTry = 0; nbTry < 3 ; port = rand() % 64000 + 1024, nbTry++) {
        if (iax_init(port) >= 0) {
            handlingEvents_ = true;
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

    std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
    handlingEvents_ = false;

    for (auto & item : iaxCallMap_) {
        auto& call = item.second;
        if (call) {
            std::lock_guard<std::mutex> lock(mutexIAX_);
            iax_hangup(call->session, const_cast<char*>("Dumped Call"));
            call.reset();
        }
    }

    iaxCallMap_.clear();

    initDone_ = false;
}

bool
IAXVoIPLink::handleEvents()
{
    iax_event *event = NULL;

    {
        std::lock_guard<std::mutex> lock(mutexIAX_);
        event = iax_get_event(0);
    }

    while (event != NULL) {

        // If we received an 'ACK', libiax2 tells apps to ignore them.
        if (event->etype == IAX_EVENT_NULL) {
            std::lock_guard<std::mutex> lock(mutexIAX_);
            iax_event_free(event);
            event = iax_get_event(0);
            continue;
        }

        const std::string id(iaxFindCallIDBySession(event->session));

        if (not id.empty()) {
            iaxHandleCallEvent(event, id);
        } else if (event->session && event->session == regSession_) {
            // This is a registration session, deal with it
            iaxHandleRegReply(event);
        } else {
            // We've got an event before it's associated with any call
            iaxHandlePrecallEvent(event);
        }

        {
            std::lock_guard<std::mutex> lock(mutexIAX_);
            iax_event_free(event);
            event = iax_get_event(0);
        }
    }

    if (nextRefreshStamp_ and nextRefreshStamp_ < time(NULL)) {
        auto account = Manager::instance().getIaxAccount(accountID_);
        if (account)
            sendRegister(*account);
    }

    sendAudioFromMic();

    // thread wait 3 millisecond
    usleep(3000);
    return handlingEvents_;
}

std::vector<std::string>
IAXVoIPLink::getCallIDs()
{
    std::vector<std::string> v;
    std::lock_guard<std::mutex> lock(iaxCallMapMutex_);

    map_utils::vectorFromMapKeys(iaxCallMap_, v);
    return v;
}

std::vector<std::shared_ptr<Call> >
IAXVoIPLink::getCalls(const std::string &account_id) const
{
    std::vector<std::shared_ptr<Call> > calls;
    for (const auto & item : iaxCallMap_) {
        if (item.second->getAccountId() == account_id)
            calls.push_back(item.second);
    }
    return calls;
}

void
IAXVoIPLink::sendAudioFromMic()
{
    for (const auto & item : iaxCallMap_) {
        auto& currentCall = item.second;

        if (!currentCall or currentCall->getState() != Call::ACTIVE)
            continue;

        int codecType = currentCall->getAudioCodec();
        sfl::AudioCodec *audioCodec = static_cast<sfl::AudioCodec *>(Manager::instance().audioCodecFactory.getCodec(codecType));

        if (!audioCodec)
            continue;

        Manager::instance().getMainBuffer().setInternalSamplingRate(audioCodec->getClockRate());

        unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();

        // we have to get 20ms of data from the mic *20/1000 = /50
        // rate/50 shall be lower than IAX__20S_48KHZ_MAX
        size_t samples = mainBufferSampleRate * 20 / 1000;

        if (Manager::instance().getMainBuffer().availableForGet(currentCall->getCallId()) < samples)
            continue;

        // Get bytes from micRingBuffer to data_from_mic
        rawBuffer_.resize(samples);
        samples = Manager::instance().getMainBuffer().getData(rawBuffer_, currentCall->getCallId());

        int compSize;
        unsigned int audioRate = audioCodec->getClockRate();
        int outSamples;
        AudioBuffer *in;

        if (audioRate != mainBufferSampleRate) {
            rawBuffer_.setSampleRate(audioRate);
            resampledData_.setSampleRate(mainBufferSampleRate);
            resampler_.resample(rawBuffer_, resampledData_);
            in = &resampledData_;
            outSamples = 0;
        } else {
            outSamples = samples;
            in = &rawBuffer_;
        }

        compSize = audioCodec->encode(in->getData(), encodedData_, RAW_BUFFER_SIZE);

        if (currentCall->session and samples > 0) {
            std::lock_guard<std::mutex> lock(mutexIAX_);

            if (iax_send_voice(currentCall->session, currentCall->format, encodedData_, compSize, outSamples) == -1)
                ERROR("IAX: Error sending voice data.");
        }
    }
}

std::shared_ptr<IAXCall>
IAXVoIPLink::getIAXCall(const std::string& id)
{
    return getIaxCall(id);
}

void
IAXVoIPLink::sendRegister(Account& a)
{
    IAXAccount& account = static_cast<IAXAccount&>(a);
    if (not account.isEnabled()) {
        WARN("Account must be enabled to register, ignoring");
        return;
    }

    if (account.getHostname().empty())
        throw VoipLinkException("Account hostname is empty");

    if (account.getUsername().empty())
        throw VoipLinkException("Account username is empty");

    std::lock_guard<std::mutex> lock(mutexIAX_);

    if (regSession_)
        iax_destroy(regSession_);

    regSession_ = iax_session_new();

    if (regSession_) {
        iax_register(regSession_, account.getHostname().data(), account.getUsername().data(), account.getPassword().data(), 120);
        nextRefreshStamp_ = time(NULL) + 10;
        account.setRegistrationState(RegistrationState::TRYING);
    }
}

void
IAXVoIPLink::sendUnregister(Account& a, std::function<void(bool)> cb)
{
    if (regSession_) {
        std::lock_guard<std::mutex> lock(mutexIAX_);
        iax_destroy(regSession_);
        regSession_ = NULL;
    }

    nextRefreshStamp_ = 0;

    static_cast<IAXAccount&>(a).setRegistrationState(RegistrationState::UNREGISTERED);

    if (cb)
        cb(true);
}

std::shared_ptr<Call>
IAXVoIPLink::newOutgoingCall(const std::string& id, const std::string& toUrl, const std::string &account_id)
{
    std::shared_ptr<IAXCall> call(new IAXCall(id, Call::OUTGOING, account_id));

    call->setPeerNumber(toUrl);
    call->initRecFilename(toUrl);

    iaxOutgoingInvite(call.get());
    call->setConnectionState(Call::PROGRESSING);
    call->setState(Call::ACTIVE);
    addIaxCall(call);

    return call;
}


void
IAXVoIPLink::answer(Call *call)
{
    Manager::instance().addStream(call->getCallId());

    {
        std::lock_guard<std::mutex> lock(mutexIAX_);
        call->answer();
    }

    call->setState(Call::ACTIVE);
    call->setConnectionState(Call::CONNECTED);

    Manager::instance().getMainBuffer().flushAllBuffers();
}

void
IAXVoIPLink::hangup(const std::string& id, int reason UNUSED)
{
    Manager::instance().getMainBuffer().unBindAll(id);

    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            throw VoipLinkException("Could not find call");

        {
            std::lock_guard<std::mutex> lock(mutexIAX_);
            iax_hangup(call->session, (char*) "Dumped Call");
        }
        call->session = NULL;
    }

    removeIaxCall(id);
}


void
IAXVoIPLink::peerHungup(const std::string& id)
{
    Manager::instance().getMainBuffer().unBindAll(id);

    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            throw VoipLinkException("Could not find call");

        call->session = NULL;
    }

    removeIaxCall(id);
}



void
IAXVoIPLink::onhold(const std::string& id)
{
    Manager::instance().getMainBuffer().unBindAll(id);

    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            throw VoipLinkException("Could not find call");
        {
            std::lock_guard<std::mutex> lock(mutexIAX_);
            iax_quelch_moh(call->session, true);
        }
        call->setState(Call::HOLD);
    }

}

void
IAXVoIPLink::offhold(const std::string& id)
{
    Manager::instance().addStream(id);

    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            throw VoipLinkException("Could not find call");

        {
            std::lock_guard<std::mutex> lock(mutexIAX_);
            iax_unquelch(call->session);
        }

        call->setState(Call::ACTIVE);
    }

    Manager::instance().startAudioDriverStream();
}

void
IAXVoIPLink::transfer(const std::string& id, const std::string& to)
{
    char callto[to.length() + 1];
    strcpy(callto, to.c_str());

    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            return;
        {
            std::lock_guard<std::mutex> lock(mutexIAX_);
            iax_transfer(call->session, callto);
        }
    }
}

bool
IAXVoIPLink::attendedTransfer(const std::string& /*transferID*/, const std::string& /*targetID*/)
{
    return false; // TODO
}

void
IAXVoIPLink::refuse(const std::string& id)
{
    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            return;
        {
            std::lock_guard<std::mutex> lock(mutexIAX_);
            iax_reject(call->session, (char*) "Call rejected manually.");
        }
    }
    removeIaxCall(id);
}


void
IAXVoIPLink::carryingDTMFdigits(const std::string& id, char code)
{
    std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
    auto call = getIAXCall(id);
    if (!call)
        return;

    {
        std::lock_guard<std::mutex> lock(mutexIAX_);
        iax_send_dtmf(call->session, code);
    }
}

#if HAVE_INSTANT_MESSAGING
void
IAXVoIPLink::sendTextMessage(const std::string& callID,
                             const std::string& message,
                             const std::string& /*from*/)
{
    std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
    auto call = getIAXCall(callID);
    if (!call)
        return;

    {
        std::lock_guard<std::mutex> lock(mutexIAX_);
        sfl::InstantMessaging::send_iax_message(call->session, callID, message.c_str());
    }
}
#endif

void
IAXVoIPLink::clearIaxCallMap()
{
    std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
    iaxCallMap_.clear();
}

void
IAXVoIPLink::addIaxCall(std::shared_ptr<IAXCall>& call)
{
    if (call == nullptr)
        return;

    std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
    if (!getIaxCall(call->getCallId()))
        iaxCallMap_[call->getCallId()] = std::shared_ptr<IAXCall>(call);
}

void
IAXVoIPLink::removeIaxCall(const std::string& id)
{
    std::lock_guard<std::mutex> lock(iaxCallMapMutex_);

    DEBUG("Removing call %s from list", id.c_str());
    iaxCallMap_.erase(id);
}


std::shared_ptr<IAXCall>
IAXVoIPLink::getIaxCall(const std::string& id)
{
    IAXCallMap::iterator iter = iaxCallMap_.find(id);
    if (iter != iaxCallMap_.end())
        return iter->second;
    else
        return nullptr;
}

void
IAXVoIPLink::iaxOutgoingInvite(IAXCall* call)
{
    std::lock_guard<std::mutex> lock(mutexIAX_);

    call->session = iax_session_new();

    IAXAccount *account = Manager::instance().getIaxAccount(accountID_);
    std::string username(account->getUsername());
    std::string strNum(username + ":" + account->getPassword() + "@" + account->getHostname() + "/" + call->getPeerNumber());

    /** @todo Make preference dynamic, and configurable */
    int audio_format_preferred = call->getFirstMatchingFormat(call->getSupportedFormat(accountID_), accountID_);
    int audio_format_capability = call->getSupportedFormat(accountID_);

    iax_call(call->session, username.c_str(), username.c_str(), strNum.c_str(),
             NULL, 0, audio_format_preferred, audio_format_capability);
}


std::string
IAXVoIPLink::iaxFindCallIDBySession(iax_session* session)
{
    std::lock_guard<std::mutex> lock(iaxCallMapMutex_);

    for (const auto & item : iaxCallMap_) {
        auto& call = item.second;

        if (call and call->session == session)
            return call->getCallId();
    }

    return "";
}

void
IAXVoIPLink::handleReject(const std::string &id)
{
    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        if (auto call = getIAXCall(id)) {
            call->setConnectionState(Call::CONNECTED);
            call->setState(Call::ERROR);
        }
    }
    Manager::instance().callFailure(id);
    removeIaxCall(id);
}

void
IAXVoIPLink::handleAccept(iax_event* event, const std::string &id)
{
    if (event->ies.format) {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        if (auto call = getIAXCall(id))
            call->format = event->ies.format;
    }
}

void
IAXVoIPLink::handleAnswerTransfer(iax_event* event, const std::string &id)
{
    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call or call->getConnectionState() == Call::CONNECTED)
            return;

        call->setConnectionState(Call::CONNECTED);
        call->setState(Call::ACTIVE);

        if (event->ies.format)
            call->format = event->ies.format;
    }

    Manager::instance().addStream(id);
    Manager::instance().peerAnsweredCall(id);
    Manager::instance().startAudioDriverStream();
    Manager::instance().getMainBuffer().flushAllBuffers();
}


void
IAXVoIPLink::handleBusy(const std::string &id)
{
    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            return;
        call->setConnectionState(Call::CONNECTED);
        call->setState(Call::BUSY);
    }
    Manager::instance().callBusy(id);
    removeIaxCall(id);
}

void
IAXVoIPLink::handleMessage(iax_event* event, const std::string &id)
{
    std::string peerNumber;
    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            return;
        peerNumber = call->getPeerNumber();
    }

    Manager::instance().incomingMessage(id, peerNumber, std::string((const char*) event->data));
}

void
IAXVoIPLink::handleRinging(const std::string &id)
{
    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            return;
        call->setConnectionState(Call::RINGING);
    }
    Manager::instance().peerRingingCall(id);
}

void
IAXVoIPLink::iaxHandleCallEvent(iax_event* event, const std::string &id)
{
    switch (event->etype) {
        case IAX_EVENT_HANGUP:
            Manager::instance().peerHungupCall(id);
            removeIaxCall(id);
            break;

        case IAX_EVENT_REJECT:
            handleReject(id);
            break;

        case IAX_EVENT_ACCEPT:
            handleAccept(event, id);
            break;

        case IAX_EVENT_ANSWER:
        case IAX_EVENT_TRANSFER:
            handleAnswerTransfer(event, id);
            break;

        case IAX_EVENT_BUSY:
            handleBusy(id);
            break;

        case IAX_EVENT_VOICE:
            iaxHandleVoiceEvent(event, id);
            break;

        case IAX_EVENT_TEXT:
#if HAVE_INSTANT_MESSAGING
            handleMessage(event, id);
#endif
            break;

        case IAX_EVENT_RINGA:
            handleRinging(id);
            break;

        case IAX_IE_MSGCOUNT:
        case IAX_EVENT_TIMEOUT:
        case IAX_EVENT_PONG:
        default:
            break;

        case IAX_EVENT_URL:

            if (Manager::instance().getConfigString("Hooks", "Hooks.iax2_enabled") == "1")
                UrlHook::runAction(Manager::instance().getConfigString("Hooks", "Hooks.url_command"), (char*) event->data);

            break;
    }
}


/* Handle audio event, VOICE packet received */
void IAXVoIPLink::iaxHandleVoiceEvent(iax_event* event, const std::string &id)
{
    // Skip this empty packet.
    if (!event->datalen)
        return;

    AudioBuffer *out;

    {
        std::lock_guard<std::mutex> lock(iaxCallMapMutex_);
        auto call = getIAXCall(id);
        if (!call)
            throw VoipLinkException("Could not find call");

        sfl::AudioCodec *audioCodec = static_cast<sfl::AudioCodec *>(Manager::instance().audioCodecFactory.getCodec(call->getAudioCodec()));

        if (!audioCodec)
            return;

        Manager::instance().getMainBuffer().setInternalSamplingRate(audioCodec->getClockRate());
        unsigned int mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();

        if (event->subclass)
            call->format = event->subclass;

        unsigned char *data = (unsigned char*) event->data;
        unsigned int size = event->datalen;

        unsigned int max = audioCodec->getClockRate() * 20 / 1000;

        if (size > max)
            size = max;

        audioCodec->decode(rawBuffer_.getData(), data , size);
        out = &rawBuffer_;
        unsigned int audioRate = audioCodec->getClockRate();

        if (audioRate != mainBufferSampleRate) {
            rawBuffer_.setSampleRate(mainBufferSampleRate);
            resampledData_.setSampleRate(audioRate);
            resampler_.resample(rawBuffer_, resampledData_);
            out = &resampledData_;
        }
    }

    Manager::instance().getMainBuffer().putData(*out, id);
}

/**
 * Handle the registration process
 */
void IAXVoIPLink::iaxHandleRegReply(iax_event* event)
{
    IAXAccount *account = Manager::instance().getIaxAccount(accountID_);

    if (event->etype != IAX_EVENT_REGREJ && event->etype != IAX_EVENT_REGACK)
        return;

    {
        std::lock_guard<std::mutex> lock(mutexIAX_);
        iax_destroy(regSession_);
        regSession_ = NULL;
    }

    account->setRegistrationState((event->etype == IAX_EVENT_REGREJ) ? RegistrationState::ERROR_AUTH : RegistrationState::REGISTERED);

    if (event->etype == IAX_EVENT_REGACK)
        nextRefreshStamp_ = time(NULL) + (event->ies.refresh ? event->ies.refresh : 60);
}

void IAXVoIPLink::iaxHandlePrecallEvent(iax_event* event)
{
    std::shared_ptr<IAXCall> call;
    std::string id;
    int format;

    switch (event->etype) {
        case IAX_EVENT_CONNECT:
            id = Manager::instance().getNewCallID();

            call = std::make_shared<IAXCall>(id, Call::INCOMING, accountID_);

            call->session = event->session;
            call->setConnectionState(Call::PROGRESSING);

            if (event->ies.calling_number)
                call->setPeerNumber(event->ies.calling_number);

            if (event->ies.calling_name)
                call->setDisplayName(std::string(event->ies.calling_name));

            // if peerNumber exist append it to the name string
            if (event->ies.calling_number)
                call->initRecFilename(std::string(event->ies.calling_number));

            Manager::instance().incomingCall(*call, accountID_);

            format = call->getFirstMatchingFormat(event->ies.format, accountID_);

            if (!format)
                format = call->getFirstMatchingFormat(event->ies.capability, accountID_);

            iax_accept(event->session, format);
            iax_ring_announce(event->session);
            addIaxCall(call);
            call->format = format;

            break;

        case IAX_EVENT_HANGUP:
            id = iaxFindCallIDBySession(event->session);

            if (not id.empty()) {
                Manager::instance().peerHungupCall(id);
                removeIaxCall(id);
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

void
IAXVoIPLink::unloadAccountMap()
{
    for (auto &a : iaxAccountMap_)
        unloadAccount(a);
    iaxAccountMap_.clear();
}
