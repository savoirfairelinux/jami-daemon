/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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



#include "AudioRtpFactory.h"
#include "AudioZrtpSession.h"
#include "AudioSrtpSession.h"
#include "AudioSymmetricRtpSession.h"
#include "manager.h"
#include "account.h"
#include "sip/sdp.h"
#include "sip/sipcall.h"
#include "sip/sipaccount.h"
#include "sip/SdesNegotiator.h"

#include <assert.h>

namespace sfl
{

AudioRtpFactory::AudioRtpFactory() : _rtpSession (NULL), remoteContext(NULL), localContext(NULL)
{

}

AudioRtpFactory::AudioRtpFactory (SIPCall *ca) : _rtpSession (NULL)
{
    assert (ca);

    try {
        initAudioRtpSession (ca);
    } catch (UnsupportedRtpSessionType& exception) {
        throw exception;
    }
}

AudioRtpFactory::~AudioRtpFactory()
{
    stop();
}

void AudioRtpFactory::initAudioRtpConfig (SIPCall *ca)
{
    assert (ca);

    if (_rtpSession != NULL) {
        _debugException ("An audio rtp thread was already created but not" \
                         "destroyed. Forcing it before continuing.");
        stop();
    }

    AccountID accountId = Manager::instance().getAccountFromCall (ca->getCallId());

    _debug ("AudioRtpFactory: Init rtp session for account %s", accountId.c_str());

    // Manager::instance().getAccountLink (accountId);
    Account *account = Manager::instance().getAccount (accountId);

    if (!account)
        _error ("AudioRtpFactory: Error no account found");

    if (account->getType() == "SIP") {
        SIPAccount *sipaccount = static_cast<SIPAccount *> (account);
        _srtpEnabled = sipaccount->getSrtpEnable();
        std::string tempkey = sipaccount->getSrtpKeyExchange();

        if (tempkey == "sdes")
            _keyExchangeProtocol = Sdes;
        else if (tempkey == "zrtp")
            _keyExchangeProtocol = Zrtp;
        else
            _keyExchangeProtocol = Symmetric;

        _debug ("AudioRtpFactory: Registered account %s profile selected with key exchange protocol number %d", accountId.c_str(), _keyExchangeProtocol);
        _helloHashEnabled = sipaccount->getZrtpHelloHash();
    } else {
        _srtpEnabled = false;
        _keyExchangeProtocol = Symmetric;
        _helloHashEnabled = false;
    }

}

void AudioRtpFactory::initAudioRtpSession (SIPCall * ca)
{
    ost::MutexLock m (_audioRtpThreadMutex);

    _debug ("AudioRtpFactory: Srtp enable: %d ", _srtpEnabled);

    if (_srtpEnabled) {
        std::string zidFilename (Manager::instance().voipPreferences.getZidFile());

        switch (_keyExchangeProtocol) {

            case Zrtp:
                _rtpSession = new AudioZrtpSession (&Manager::instance(), ca, zidFilename);
                _rtpSessionType = Zrtp;

                if (_helloHashEnabled) {
                    // TODO: be careful with that. The hello hash is computed asynchronously. Maybe it's
                    // not even available at that point.
                    ca->getLocalSDP()->setZrtpHash (static_cast<AudioZrtpSession *> (_rtpSession)->getHelloHash());
                    _debug ("AudioRtpFactory: Zrtp hello hash fed to SDP");
                }

                break;

            case Sdes:

                _rtpSession = new AudioSrtpSession (&Manager::instance(), ca);
                _rtpSessionType = Sdes;
                break;

            default:
                _debug ("AudioRtpFactory: Unsupported Rtp Session Exception Type!");
                throw UnsupportedRtpSessionType();
        }
    } else {
        _rtpSessionType = Symmetric;
        _rtpSession = new AudioSymmetricRtpSession (&Manager::instance(), ca);
        _debug ("AudioRtpFactory: Starting a symmetric unencrypted rtp session");
    }
}

void AudioRtpFactory::start (AudioCodec* audiocodec)
{
    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException ("AudioRtpFactory: Error: RTP session was null when trying to start audio thread");
    }

    switch (_rtpSessionType) {

        case Sdes:

        	if(localContext && remoteContext) {
        		static_cast<AudioSrtpSession *> (_rtpSession)->restoreCryptoContext(localContext, remoteContext);
        	}

            if (static_cast<AudioSrtpSession *> (_rtpSession)->startRtpThread (audiocodec) != 0) {
                throw AudioRtpFactoryException ("AudioRtpFactory: Error: Failed to start AudioSRtpSession thread");
            }

            break;

        case Symmetric:
            _debug ("Starting symmetric rtp thread");

            if (static_cast<AudioSymmetricRtpSession *> (_rtpSession)->startRtpThread (audiocodec) != 0) {
                throw AudioRtpFactoryException ("AudioRtpFactory: Error: Failed to start AudioSymmetricRtpSession thread");
            }

            break;

        case Zrtp:

            if (static_cast<AudioZrtpSession *> (_rtpSession)->startRtpThread (audiocodec) != 0) {
                throw AudioRtpFactoryException ("AudioRtpFactory: Error: Failed to start AudioZrtpSession thread");
            }

            break;
    }
}

void AudioRtpFactory::stop (void)
{
    ost::MutexLock mutex (_audioRtpThreadMutex);
    _info ("AudioRtpFactory: Stopping audio rtp session");

    if (_rtpSession == NULL) {
        _debugException ("AudioRtpFactory: Rtp session already deleted");
        return;
    }

    try {
        switch (_rtpSessionType) {

            case Sdes:
            	localContext = static_cast<AudioSrtpSession *> (_rtpSession)->_localCryptoCtx;
            	remoteContext = static_cast<AudioSrtpSession *> (_rtpSession)->_remoteCryptoCtx;
                static_cast<AudioSrtpSession *> (_rtpSession)->stopRtpThread();
                break;

            case Symmetric:
                static_cast<AudioRtpSession *> (_rtpSession)->stopRtpThread();
                break;

            case Zrtp:
                static_cast<AudioZrtpSession *> (_rtpSession)->stopRtpThread();
                break;
        }

        _rtpSession = NULL;
    } catch (...) {
        _debugException ("AudioRtpFactory: Error: Exception caught when stopping the audio rtp session");
        throw AudioRtpFactoryException ("AudioRtpFactory: Error: caught exception in AudioRtpFactory::stop");
    }
}

int AudioRtpFactory::getSessionMedia()
{
    _info ("AudioRtpFactory: Update session media");

    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException ("AudioRtpFactory: Error: RTP session was null when trying to get session media type");
    }

    int payloadType = 0;

    switch (_rtpSessionType) {
        case Sdes:
            payloadType = static_cast<AudioSrtpSession *> (_rtpSession)->getCodecPayloadType();
            break;
        case Symmetric:
            payloadType = static_cast<AudioSymmetricRtpSession *> (_rtpSession)->getCodecPayloadType();
            break;
        case Zrtp:
            payloadType = static_cast<AudioZrtpSession *> (_rtpSession)->getCodecPayloadType();
            break;
    }

    return payloadType;
}

void AudioRtpFactory::updateSessionMedia (AudioCodec *audiocodec)
{
    _info ("AudioRtpFactory: Updating session media");

    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException ("AudioRtpFactory: Error: _rtpSession was null when trying to update IP address");
    }

    switch (_rtpSessionType) {
    case Sdes:
    	static_cast<AudioSrtpSession *> (_rtpSession)->updateSessionMedia (audiocodec);
    	break;
    case Symmetric:
    	static_cast<AudioSymmetricRtpSession *> (_rtpSession)->updateSessionMedia (audiocodec);
    	break;
    case Zrtp:
    	static_cast<AudioZrtpSession *> (_rtpSession)->updateSessionMedia (audiocodec);
    	break;
    default:
    	_debug("AudioRtpFactory: Unknown session type");
    	break;
    }
}

void AudioRtpFactory::updateDestinationIpAddress (void)
{
    _info ("AudioRtpFactory: Updating IP address");

    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException ("AudioRtpFactory: Error: RtpSession was null when trying to update IP address");
    }

    switch (_rtpSessionType) {

        case Sdes:
            static_cast<AudioSrtpSession *> (_rtpSession)->updateDestinationIpAddress();
            break;

        case Symmetric:
            static_cast<AudioSymmetricRtpSession *> (_rtpSession)->updateDestinationIpAddress();
            break;

        case Zrtp:
            static_cast<AudioZrtpSession *> (_rtpSession)->updateDestinationIpAddress();
            break;
    }
}

sfl::AudioSymmetricRtpSession * AudioRtpFactory::getAudioSymetricRtpSession()
{
    if ( (_rtpSessionType == Symmetric) && (_rtpSessionType != NULL)) {
        return static_cast<AudioSymmetricRtpSession *> (_rtpSession);
    } else {
        throw AudioRtpFactoryException ("RTP: Error: _rtpSession is NULL in getAudioSymetricRtpSession");
    }
}

sfl::AudioZrtpSession * AudioRtpFactory::getAudioZrtpSession()
{
    if ( (_rtpSessionType == Zrtp) && (_rtpSessionType != NULL)) {
        return static_cast<AudioZrtpSession *> (_rtpSession);
    } else {
        throw AudioRtpFactoryException ("RTP: Error: _rtpSession is NULL in getAudioZrtpSession");
    }
}

void sfl::AudioRtpFactory::initLocalCryptoInfo (SIPCall * ca)
{
    if (_rtpSession && _rtpSessionType && (_rtpSessionType == Sdes)) {
        static_cast<AudioSrtpSession *> (_rtpSession)->initLocalCryptoInfo ();

        ca->getLocalSDP()->setLocalSdpCrypto (static_cast<AudioSrtpSession *> (_rtpSession)->getLocalCryptoInfo());
    }
}

void AudioRtpFactory::setRemoteCryptoInfo (sfl::SdesNegotiator& nego)
{
    if (_rtpSession && _rtpSessionType && (_rtpSessionType == Sdes)) {
        static_cast<AudioSrtpSession *> (_rtpSession)->setRemoteCryptoInfo (nego);
    } else {
        throw AudioRtpFactoryException ("RTP: Error: _rtpSession is NULL in setRemoteCryptoInfo");
    }
}

void AudioRtpFactory::sendDtmfDigit (int digit)
{

    switch (_rtpSessionType) {

        case Sdes:
            static_cast<AudioSrtpSession *> (_rtpSession)->putDtmfEvent (digit);
            break;

        case Symmetric:
            static_cast<AudioSymmetricRtpSession *> (_rtpSession)->putDtmfEvent (digit);
            break;

        case Zrtp:
            static_cast<AudioZrtpSession *> (_rtpSession)->putDtmfEvent (digit);
            break;
    }

}
}


