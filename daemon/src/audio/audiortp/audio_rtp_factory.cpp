/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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



#include "audio_rtp_factory.h"
#include "audio_zrtp_session.h"
#include "audio_srtp_session.h"
#include "audio_symmetric_rtp_session.h"
#include "manager.h"
#include "sip/sdp.h"
#include "sip/sipcall.h"
#include "sip/sipaccount.h"
#include "sip/sdes_negotiator.h"

namespace sfl {

AudioRtpFactory::AudioRtpFactory(SIPCall *ca) : rtpSession_(NULL),
    audioRtpThreadMutex_(), srtpEnabled_(false),
    keyExchangeProtocol_(Symmetric), helloHashEnabled_(false),
    remoteContext_(NULL), localContext_(NULL), ca_(ca)
{}

AudioRtpFactory::~AudioRtpFactory()
{
    delete rtpSession_;
}

void AudioRtpFactory::initAudioRtpConfig()
{
    if (rtpSession_ != NULL)
        stop();

    std::string accountId(Manager::instance().getAccountFromCall(ca_->getCallId()));

    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(accountId));

    if (account) {
        srtpEnabled_ = account->getSrtpEnabled();
        std::string key(account->getSrtpKeyExchange());

        if (key == "sdes")
            keyExchangeProtocol_ = Sdes;
        else if (key == "zrtp")
            keyExchangeProtocol_ = Zrtp;
        else
            keyExchangeProtocol_ = Symmetric;

        helloHashEnabled_ = account->getZrtpHelloHash();
    } else {
        srtpEnabled_ = false;
        keyExchangeProtocol_ = Symmetric;
        helloHashEnabled_ = false;
    }
}

void AudioRtpFactory::initAudioSymmetricRtpSession()
{
    ost::MutexLock m(audioRtpThreadMutex_);

    if (srtpEnabled_) {
        std::string zidFilename(Manager::instance().voipPreferences.getZidFile());

        switch (keyExchangeProtocol_) {

            case Zrtp:
                rtpSession_ = new AudioZrtpSession(ca_, zidFilename);
                // TODO: be careful with that. The hello hash is computed asynchronously. Maybe it's
                // not even available at that point.
                if (helloHashEnabled_)
                    ca_->getLocalSDP()->setZrtpHash(static_cast<AudioZrtpSession *>(rtpSession_)->getHelloHash());
                break;

            case Sdes:
                rtpSession_ = new AudioSrtpSession(ca_);
                break;

            default:
                throw UnsupportedRtpSessionType("Unsupported Rtp Session Exception Type!");
        }
    } else
        rtpSession_ = new AudioSymmetricRtpSession(ca_);
}

void AudioRtpFactory::start(AudioCodec* audiocodec)
{
    if (rtpSession_ == NULL)
        throw AudioRtpFactoryException("AudioRtpFactory: Error: RTP session was null when trying to start audio thread");

    if (rtpSession_->getAudioRtpType() == Sdes)
        if (localContext_ and remoteContext_)
            static_cast<AudioSrtpSession *>(rtpSession_)->restoreCryptoContext(localContext_, remoteContext_);

    if (rtpSession_->startRtpThread(audiocodec) != 0)
        throw AudioRtpFactoryException("AudioRtpFactory: Error: Failed to start AudioZrtpSession thread");
}

void AudioRtpFactory::stop()
{
    ost::MutexLock mutex(audioRtpThreadMutex_);

    if (rtpSession_ == NULL)
        return;

    if (rtpSession_->getAudioRtpType() == Sdes) {
        localContext_ = static_cast<AudioSrtpSession*>(rtpSession_)->localCryptoCtx_;
        remoteContext_ = static_cast<AudioSrtpSession*>(rtpSession_)->remoteCryptoCtx_;
    }

    delete rtpSession_;
    rtpSession_ = NULL;
}

int AudioRtpFactory::getSessionMedia()
{
    if (rtpSession_ == NULL)
        throw AudioRtpFactoryException("AudioRtpFactory: Error: RTP session was null when trying to get session media type");

    return rtpSession_->getCodecPayloadType();
}

void AudioRtpFactory::updateSessionMedia(AudioCodec *audiocodec)
{
    if (rtpSession_ == NULL)
        throw AudioRtpFactoryException("AudioRtpFactory: Error: rtpSession_ was null when trying to update IP address");

    rtpSession_->updateSessionMedia(audiocodec);
}

void AudioRtpFactory::updateDestinationIpAddress()
{
    if (rtpSession_)
        rtpSession_->updateDestinationIpAddress();
}

sfl::AudioZrtpSession * AudioRtpFactory::getAudioZrtpSession()
{
    if (rtpSession_->getAudioRtpType() == Zrtp)
        return static_cast<AudioZrtpSession *>(rtpSession_);
    else
        throw AudioRtpFactoryException("RTP: Error: rtpSession_ is NULL in getAudioZrtpSession");
}

void sfl::AudioRtpFactory::initLocalCryptoInfo()
{
    if (rtpSession_ && rtpSession_->getAudioRtpType() == Sdes) {
        static_cast<AudioSrtpSession *>(rtpSession_)->initLocalCryptoInfo();
        ca_->getLocalSDP()->setLocalSdpCrypto(static_cast<AudioSrtpSession *>(rtpSession_)->getLocalCryptoInfo());
    }
}

void AudioRtpFactory::setRemoteCryptoInfo(sfl::SdesNegotiator& nego)
{
    if (rtpSession_ && rtpSession_->getAudioRtpType() == Sdes)
        static_cast<AudioSrtpSession *>(rtpSession_)->setRemoteCryptoInfo(nego);
    else
        throw AudioRtpFactoryException("RTP: Error: rtpSession_ is NULL in setRemoteCryptoInfo");
}

void AudioRtpFactory::setDtmfPayloadType(unsigned int payloadType)
{
    if (rtpSession_)
        rtpSession_->setDtmfPayloadType(payloadType);
}

void AudioRtpFactory::sendDtmfDigit(int digit)
{
    rtpSession_->putDtmfEvent(digit);
}

}
