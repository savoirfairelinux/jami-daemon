/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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
#include "sip/sipcall.h"
#include "sip/SdesNegotiator.h"

#include <assert.h>

namespace sfl
{

AudioRtpFactory::AudioRtpFactory() : _rtpSession (NULL)
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

void AudioRtpFactory::initAudioRtpConfig(SIPCall *ca)
{
    assert (ca);

    if (_rtpSession != NULL) {
        _debugException ("An audio rtp thread was already created but not" \
                         "destroyed. Forcing it before continuing.");
        stop();
    }

    AccountID accountId = Manager::instance().getAccountFromCall (ca->getCallId());

    // Check if it is an IP-to-IP call
    if (accountId == AccountNULL) {
        _srtpEnabled = Manager::instance().getConfigBool (IP2IP_PROFILE, SRTP_ENABLE);
        _keyExchangeProtocol = Manager::instance().getConfigInt (IP2IP_PROFILE, SRTP_KEY_EXCHANGE);
        _debug ("Ip-to-ip profile selected with key exchange protocol number %d", _keyExchangeProtocol);
        _helloHashEnabled = Manager::instance().getConfigBool (IP2IP_PROFILE, ZRTP_HELLO_HASH);
    } else {
        _srtpEnabled = Manager::instance().getConfigBool (accountId, SRTP_ENABLE);
        _keyExchangeProtocol = Manager::instance().getConfigInt (accountId, SRTP_KEY_EXCHANGE);
        _debug ("Registered account %s profile selected with key exchange protocol number %d", accountId.c_str(), _keyExchangeProtocol);
        _helloHashEnabled = Manager::instance().getConfigBool (accountId, ZRTP_HELLO_HASH);
    }
}

void AudioRtpFactory::initAudioRtpSession (SIPCall * ca)
{
    ost::MutexLock m (_audioRtpThreadMutex);

    _debug ("Srtp enable: %d ", _srtpEnabled);
    if (_srtpEnabled) {
        std::string zidFilename (Manager::instance().getConfigString (SIGNALISATION, ZRTP_ZIDFILE));

        switch (_keyExchangeProtocol) {

            case Zrtp:
                _rtpSession = new AudioZrtpSession (&Manager::instance(), ca, zidFilename);
                _rtpSessionType = Zrtp;

                if (_helloHashEnabled) {
                    // TODO: be careful with that. The hello hash is computed asynchronously. Maybe it's
                    // not even available at that point.
                    ca->getLocalSDP()->set_zrtp_hash (static_cast<AudioZrtpSession *> (_rtpSession)->getHelloHash());
                    _debug ("Zrtp hello hash fed to SDP");
                }

                break;

            case Sdes:

	        _rtpSession = new AudioSrtpSession (&Manager::instance(), ca);
                _rtpSessionType = Sdes;

		ca->getLocalSDP()->set_srtp_crypto(static_cast<AudioSrtpSession *> (_rtpSession)->getLocalCryptoInfo());
		break;

            default:
	        _debug("Unsupported Rtp Session Exception Type!");
                throw UnsupportedRtpSessionType();
        }
    } else {
        _rtpSessionType = Symmetric;
        _rtpSession = new AudioSymmetricRtpSession (&Manager::instance(), ca);
        _debug ("Starting a symmetric unencrypted rtp session");
    }
}

void AudioRtpFactory::start (AudioCodec* audiocodec)
{
    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException ("RTP: Error: _rtpSession was null when trying to start audio thread");
    }

    switch (_rtpSessionType) {

        case Sdes:
	    if (static_cast<AudioSrtpSession *> (_rtpSession)->startRtpThread(audiocodec) != 0) {
                throw AudioRtpFactoryException ("RTP: Error: Failed to start AudioSRtpSession thread");
            }
	    break;

        case Symmetric:
            _debug ("Starting symmetric rtp thread");

            if (static_cast<AudioSymmetricRtpSession *> (_rtpSession)->startRtpThread(audiocodec) != 0) {
                throw AudioRtpFactoryException ("RTP: Error: Failed to start AudioSymmetricRtpSession thread");
            }

            break;

        case Zrtp:

            if (static_cast<AudioZrtpSession *> (_rtpSession)->startRtpThread(audiocodec) != 0) {
                throw AudioRtpFactoryException ("RTP: Error: Failed to start AudioZrtpSession thread");
            }
            break;
    }
}

void AudioRtpFactory::stop (void)
{
    ost::MutexLock mutex (_audioRtpThreadMutex);
    _info("RTP: Stopping audio rtp session");

    if (_rtpSession == NULL) {
        _debugException ("RTP: Error: _rtpSession is null when trying to stop. Returning.");
        return;
    }

    try {
        switch (_rtpSessionType) {

            case Sdes:
            	delete static_cast<AudioSrtpSession *> (_rtpSession);
            	break;

            case Symmetric:
                delete static_cast<AudioSymmetricRtpSession *> (_rtpSession);
                break;

            case Zrtp:
                delete static_cast<AudioZrtpSession *> (_rtpSession);
                break;
        }

        _rtpSession = NULL;
    } catch (...) {
        _debugException ("RTP: Error: Exception caught when stopping the audio rtp session");
        throw AudioRtpFactoryException("RTP: Error: caught exception in AudioRtpFactory::stop");
    }
}

void AudioRtpFactory::updateDestinationIpAddress (void)
{
    _info ("RTP: Updating IP address");
    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException ("RTP: Error: _rtpSession was null when trying to update IP address");
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
	        throw AudioRtpFactoryException("RTP: Error: _rtpSession is NULL in getAudioSymetricRtpSession");
	    }
}

sfl::AudioZrtpSession * AudioRtpFactory::getAudioZrtpSession()
{
    if ( (_rtpSessionType == Zrtp) && (_rtpSessionType != NULL)) {
        return static_cast<AudioZrtpSession *> (_rtpSession);
    } else {
        throw AudioRtpFactoryException("RTP: Error: _rtpSession is NULL in getAudioZrtpSession");
    }
}

void AudioRtpFactory::setRemoteCryptoInfo(sfl::SdesNegotiator& nego)
{
    if ( _rtpSession && _rtpSessionType && (_rtpSessionType == Sdes)) {
        static_cast<AudioSrtpSession *> (_rtpSession)->setRemoteCryptoInfo(nego);
    }
    else {
        throw AudioRtpFactoryException("RTP: Error: _rtpSession is NULL in setRemoteCryptoInfo");
    }
}

void AudioRtpFactory::sendDtmfDigit(int digit) {
	switch(_rtpSessionType) {

	case Sdes:
		static_cast<AudioSrtpSession *> (_rtpSession)->putDtmfEvent(digit);
		break;

	case Symmetric:
		static_cast<AudioSymmetricRtpSession *> (_rtpSession)->putDtmfEvent(digit);
		break;

	case Zrtp:
		static_cast<AudioZrtpSession *> (_rtpSession)->putDtmfEvent(digit);
		break;
	}
}
}


