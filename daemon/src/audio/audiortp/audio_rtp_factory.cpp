/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#include "audio_rtp_factory.h"
#if HAVE_ZRTP
#include "audio_zrtp_session.h"
#endif
#include "audio_symmetric_rtp_session.h"
#include "manager.h"
#include "sip/sdp.h"
#include "sip/sipcall.h"
#include "sip/sipaccount.h"
#include "sip/sdes_negotiator.h"
#include "logger.h"
#include "config.h"

namespace sfl {

AudioRtpFactory::AudioRtpFactory(SIPCall *ca) : rtpSession_(NULL),
    audioRtpThreadMutex_(), srtpEnabled_(false), helloHashEnabled_(false),
    cachedLocalMasterKey_(MAX_MASTER_KEY_LENGTH),
    cachedLocalMasterSalt_(MAX_MASTER_SALT_LENGTH),
    cachedRemoteMasterKey_(MAX_MASTER_KEY_LENGTH),
    cachedRemoteMasterSalt_(MAX_MASTER_SALT_LENGTH),
    remoteOfferIsSet_(false), ca_(ca),
    keyExchangeProtocol_(NONE)
{}

AudioRtpFactory::~AudioRtpFactory()
{
    delete rtpSession_;
}

void AudioRtpFactory::initConfig()
{
    DEBUG("AudioRtpFactory: init config");
    if (rtpSession_ != NULL)
        stop();

    std::string accountId(Manager::instance().getAccountFromCall(ca_->getCallId()));

    SIPAccount *account = Manager::instance().getSipAccount(accountId);

    if (account) {
        srtpEnabled_ = account->getSrtpEnabled();
        std::string key(account->getSrtpKeyExchange());
        if (srtpEnabled_) {
#if HAVE_ZRTP
            if (key == "sdes")
                keyExchangeProtocol_ = SDES;
            else if (key == "zrtp")
                keyExchangeProtocol_ = ZRTP;
#else
                keyExchangeProtocol_ = SDES;
#endif
        } else {
            keyExchangeProtocol_ = NONE;
        }
        helloHashEnabled_ = account->getZrtpHelloHash();
    } else {
        srtpEnabled_ = false;
        keyExchangeProtocol_ = NONE;
        helloHashEnabled_ = false;
    }
}

void AudioRtpFactory::initSession()
{
    DEBUG("AudioRtpFactory: init session2");
    ost::MutexLock m(audioRtpThreadMutex_);

    if (srtpEnabled_) {
        const std::string zidFilename(Manager::instance().voipPreferences.getZidFile());

        switch (keyExchangeProtocol_) {
#if HAVE_ZRTP
            case ZRTP:
                rtpSession_ = new AudioZrtpSession(*ca_, zidFilename);
                // TODO: be careful with that. The hello hash is computed asynchronously. Maybe it's
                // not even available at that point.
                if (helloHashEnabled_)
                    ca_->getLocalSDP()->setZrtpHash(static_cast<AudioZrtpSession *>(rtpSession_)->getHelloHash());
                break;
#endif
            case SDES:
                rtpSession_ = new AudioSrtpSession(*ca_);
                break;

            default:
                throw UnsupportedRtpSessionType("Unsupported Rtp Session Exception Type!");
        }
    } else
        rtpSession_ = new AudioSymmetricRtpSession(*ca_);
}

void sfl::AudioRtpFactory::start(const std::vector<AudioCodec*> &audioCodecs)
{
    if (rtpSession_ == NULL)
        throw AudioRtpFactoryException("RTP session was null when trying to start audio thread");

    if (rtpSession_->startRtpThread(audioCodecs) != 0)
        throw AudioRtpFactoryException("Failed to start AudioRtpSession thread");
}

void AudioRtpFactory::stop()
{
    ost::MutexLock mutex(audioRtpThreadMutex_);

    delete rtpSession_;
    rtpSession_ = NULL;
}

int AudioRtpFactory::getSessionMedia()
{
    if (rtpSession_ == NULL)
        throw AudioRtpFactoryException("RTP session was null when trying to get session media type");

    return rtpSession_->getEncoderPayloadType();
}

void AudioRtpFactory::updateSessionMedia(const std::vector<AudioCodec*> &audioCodecs)
{
    if (rtpSession_ == NULL)
        throw AudioRtpFactoryException("rtpSession_ was NULL when trying to update IP address");

    rtpSession_->updateSessionMedia(audioCodecs);
}

void AudioRtpFactory::updateDestinationIpAddress()
{
    if (rtpSession_)
        rtpSession_->updateDestinationIpAddress();
}

#if HAVE_ZRTP
sfl::AudioZrtpSession * AudioRtpFactory::getAudioZrtpSession()
{
    if (keyExchangeProtocol_ == ZRTP)
        return static_cast<AudioZrtpSession *>(rtpSession_);
    else
        throw AudioRtpFactoryException("rtpSession_ is NULL in getAudioZrtpSession");
}
#endif

void sfl::AudioRtpFactory::initLocalCryptoInfo()
{
    DEBUG("AudioRtpFactory: Init local crypto info");
    if (rtpSession_ && keyExchangeProtocol_ == SDES) {
        AudioSrtpSession *srtp = static_cast<AudioSrtpSession*>(rtpSession_);
        // the context is invalidated and deleted by the call to initLocalCryptoInfo
        srtp->initLocalCryptoInfo();
        ca_->getLocalSDP()->setLocalSdpCrypto(srtp->getLocalCryptoInfo());
    }
}

void sfl::AudioRtpFactory::initLocalCryptoInfoOnOffHold()
{
    DEBUG("AudioRtpFactory: Init local crypto info");
    if (rtpSession_ && keyExchangeProtocol_ == SDES) {
        AudioSrtpSession *srtp = static_cast<AudioSrtpSession*>(rtpSession_);
        // the context is invalidated and deleted by the call to initLocalCryptoInfo
        srtp->initLocalCryptoInfoOnOffhold();
        ca_->getLocalSDP()->setLocalSdpCrypto(srtp->getLocalCryptoInfo());
    }
}


void AudioRtpFactory::setRemoteCryptoInfo(sfl::SdesNegotiator& nego)
{
    if (rtpSession_ ) {
        if (keyExchangeProtocol_ == SDES) {
            AudioSrtpSession *srtp = static_cast<AudioSrtpSession *>(rtpSession_);
            srtp->setRemoteCryptoInfo(nego);
        } else {
            ERROR("Should not store remote crypto info for non-SDES sessions");
        }
    } else
        throw AudioRtpFactoryException("rtpSession_ is NULL in setRemoteCryptoInfo");
}

void AudioRtpFactory::setDtmfPayloadType(unsigned int payloadType)
{
    if (rtpSession_)
        rtpSession_->setDtmfPayloadType(payloadType);
}

void AudioRtpFactory::sendDtmfDigit(int digit)
{
    if (rtpSession_)
        rtpSession_->putDtmfEvent(digit);
}

void sfl::AudioRtpFactory::saveLocalContext()
{
    if (rtpSession_ and keyExchangeProtocol_ == SDES) {
        cachedLocalMasterKey_ = rtpSession_->getLocalMasterKey();
        cachedLocalMasterSalt_ = rtpSession_->getLocalMasterSalt();
    }
}

void sfl::AudioRtpFactory::restoreLocalContext()
{
    if (rtpSession_ and keyExchangeProtocol_ == SDES) {
        rtpSession_->setLocalMasterKey(cachedLocalMasterKey_);
        rtpSession_->setLocalMasterSalt(cachedLocalMasterSalt_);
    }
}
}
