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
#include "sip/SdesNegotiator.h"

namespace sfl {

AudioRtpFactory::AudioRtpFactory(SIPCall *ca) : _rtpSession(NULL), remoteContext(NULL), localContext(NULL), ca_(ca)
{}

AudioRtpFactory::~AudioRtpFactory()
{
    delete _rtpSession;
}

void AudioRtpFactory::initAudioRtpConfig()
{
    if (_rtpSession != NULL)
        stop();

    std::string accountId(Manager::instance().getAccountFromCall(ca_->getCallId()));

    SIPAccount *account = dynamic_cast<SIPAccount *>(Manager::instance().getAccount(accountId));

    if (account) {
        _srtpEnabled = account->getSrtpEnabled();
        std::string key(account->getSrtpKeyExchange());

        if (key == "sdes")
            _keyExchangeProtocol = Sdes;
        else if (key == "zrtp")
            _keyExchangeProtocol = Zrtp;
        else
            _keyExchangeProtocol = Symmetric;

        _helloHashEnabled = account->getZrtpHelloHash();
    } else {
        _srtpEnabled = false;
        _keyExchangeProtocol = Symmetric;
        _helloHashEnabled = false;
    }
}

void AudioRtpFactory::initAudioSymmetricRtpSession()
{
    ost::MutexLock m(_audioRtpThreadMutex);

    if (_srtpEnabled) {
        std::string zidFilename(Manager::instance().voipPreferences.getZidFile());

        switch (_keyExchangeProtocol) {

            case Zrtp:
                _rtpSession = new AudioZrtpSession(ca_, zidFilename);

                if (_helloHashEnabled) {
                    // TODO: be careful with that. The hello hash is computed asynchronously. Maybe it's
                    // not even available at that point.
                    ca_->getLocalSDP()->setZrtpHash(static_cast<AudioZrtpSession *>(_rtpSession)->getHelloHash());
                }

                break;

            case Sdes:
                _rtpSession = new AudioSrtpSession(ca_);
                break;

            default:
                throw UnsupportedRtpSessionType("Unsupported Rtp Session Exception Type!");
        }
    } else {
        _rtpSession = new AudioSymmetricRtpSession(ca_);
    }
}

void AudioRtpFactory::start(AudioCodec* audiocodec)
{
    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException("AudioRtpFactory: Error: RTP session was null when trying to start audio thread");
    }

    if (_rtpSession->getAudioRtpType() == Sdes) {
        if (localContext && remoteContext) {
            static_cast<AudioSrtpSession *>(_rtpSession)->restoreCryptoContext(localContext, remoteContext);
        }
    }

    if (_rtpSession->startRtpThread(audiocodec) != 0) {
        throw AudioRtpFactoryException("AudioRtpFactory: Error: Failed to start AudioZrtpSession thread");
    }

}

void AudioRtpFactory::stop(void)
{
    ost::MutexLock mutex(_audioRtpThreadMutex);

    if (_rtpSession == NULL)
        return;

    if (_rtpSession->getAudioRtpType() == Sdes) {
        localContext = static_cast<AudioSrtpSession *>(_rtpSession)->_localCryptoCtx;
        remoteContext = static_cast<AudioSrtpSession *>(_rtpSession)->_remoteCryptoCtx;
    }

    delete _rtpSession;
    _rtpSession = NULL;
}

int AudioRtpFactory::getSessionMedia()
{
    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException("AudioRtpFactory: Error: RTP session was null when trying to get session media type");
    }

    return _rtpSession->getCodecPayloadType();
}

void AudioRtpFactory::updateSessionMedia(AudioCodec *audiocodec)
{
    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException("AudioRtpFactory: Error: _rtpSession was null when trying to update IP address");
    }

    _rtpSession->updateSessionMedia(audiocodec);
}

void AudioRtpFactory::updateDestinationIpAddress(void)
{
    if (_rtpSession)
        _rtpSession->updateDestinationIpAddress();
}

sfl::AudioZrtpSession * AudioRtpFactory::getAudioZrtpSession()
{
    if (_rtpSession->getAudioRtpType() == Zrtp) {
        return static_cast<AudioZrtpSession *>(_rtpSession);
    } else {
        throw AudioRtpFactoryException("RTP: Error: _rtpSession is NULL in getAudioZrtpSession");
    }
}

void sfl::AudioRtpFactory::initLocalCryptoInfo()
{
    if (_rtpSession && _rtpSession->getAudioRtpType() == Sdes) {
        static_cast<AudioSrtpSession *>(_rtpSession)->initLocalCryptoInfo();

        ca_->getLocalSDP()->setLocalSdpCrypto(static_cast<AudioSrtpSession *>(_rtpSession)->getLocalCryptoInfo());
    }
}

void AudioRtpFactory::setRemoteCryptoInfo(sfl::SdesNegotiator& nego)
{
    if (_rtpSession && _rtpSession->getAudioRtpType() == Sdes) {
        static_cast<AudioSrtpSession *>(_rtpSession)->setRemoteCryptoInfo(nego);
    } else {
        throw AudioRtpFactoryException("RTP: Error: _rtpSession is NULL in setRemoteCryptoInfo");
    }
}

void AudioRtpFactory::setDtmfPayloadType(unsigned int payloadType)
{
    if (_rtpSession)
        _rtpSession->setDtmfPayloadType(payloadType);
}

void AudioRtpFactory::sendDtmfDigit(int digit)
{
    _rtpSession->putDtmfEvent(digit);
}

}
