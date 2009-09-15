/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 */



#include "AudioRtpFactory.h"
#include "AudioZrtpSession.h"
#include "AudioSymmetricRtpSession.h"

#include "manager.h"
#include "account.h"
#include "sip/sipcall.h"

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

void AudioRtpFactory::initAudioRtpSession (SIPCall * ca)
{
    ost::MutexLock m (_audioRtpThreadMutex);

    assert (ca);

    if (_rtpSession != NULL) {
        _debugException ("An audio rtp thread was already created but not" \
                         "destroyed. Forcing it before continuing.\n");
        stop();
    }

    AccountID accountId = Manager::instance().getAccountFromCall (ca->getCallId());

    bool srtpEnabled = false;
    int keyExchangeProtocol = 1;
    bool helloHashEnabled = true;

    // Check if it is an IP-to-IP call

    if (accountId == AccountNULL) {
        srtpEnabled = Manager::instance().getConfigBool (IP2IP_PROFILE, SRTP_ENABLE);
        keyExchangeProtocol = Manager::instance().getConfigInt (IP2IP_PROFILE, SRTP_KEY_EXCHANGE);
        _debug ("Ip-to-ip profile selected with key exchange protocol number %d\n", keyExchangeProtocol);
        helloHashEnabled = Manager::instance().getConfigBool (IP2IP_PROFILE, ZRTP_HELLO_HASH);
    } else {
        srtpEnabled = Manager::instance().getConfigBool (accountId, SRTP_ENABLE);
        keyExchangeProtocol = Manager::instance().getConfigInt (accountId, SRTP_KEY_EXCHANGE);
        _debug ("Registered account %s profile selected with key exchange protocol number %d\n", accountId.c_str(), keyExchangeProtocol);
        helloHashEnabled = Manager::instance().getConfigBool (accountId, ZRTP_HELLO_HASH);
    }

    _debug ("Srtp enable: %d \n", srtpEnabled);

    if (srtpEnabled) {
        std::string zidFilename (Manager::instance().getConfigString (SIGNALISATION, ZRTP_ZIDFILE));

        switch (keyExchangeProtocol) {

            case Zrtp:
                _rtpSession = new AudioZrtpSession (&Manager::instance(), ca, zidFilename);
                _rtpSessionType = Zrtp;

                if (helloHashEnabled) {
                    // TODO: be careful with that. The hello hash is computed asynchronously. Maybe it's
                    // not even available at that point.
                    ca->getLocalSDP()->set_zrtp_hash (static_cast<AudioZrtpSession *> (_rtpSession)->getHelloHash());
                    _debug ("Zrtp hello hash fed to SDP\n");
                }

                break;

            case Sdes:

            default:
                throw UnsupportedRtpSessionType();
        }
    } else {
        _rtpSessionType = Symmetric;
        _rtpSession = new AudioSymmetricRtpSession (&Manager::instance(), ca);
        _debug ("Starting a symmetric unencrypted rtp session\n");
    }
}

void AudioRtpFactory::start (void)
{
    if (_rtpSession == NULL) {
        throw AudioRtpFactoryException ("_rtpSession was null when trying to start audio thread");
    }

    switch (_rtpSessionType) {

        case Sdes:

        case Symmetric:
            _debug ("Starting symmetric rtp thread\n");

            if (static_cast<AudioSymmetricRtpSession *> (_rtpSession)->startRtpThread() != 0) {
                throw AudioRtpFactoryException ("Failed to start AudioSymmetricRtpSession thread");
            }

            break;

        case Zrtp:

            if (static_cast<AudioZrtpSession *> (_rtpSession)->startRtpThread() != 0) {
                throw AudioRtpFactoryException ("Failed to start AudioZrtpSession thread");
            }

            break;
    }
}

void AudioRtpFactory::stop (void)
{
    ost::MutexLock mutex (_audioRtpThreadMutex);
    _debug ("Stopping audio rtp session\n");

    if (_rtpSession == NULL) {
        _debugException ("_rtpSession is null when trying to stop. Returning.");
        return;
    }

    try {
        switch (_rtpSessionType) {

            case Sdes:

            case Symmetric:
                delete static_cast<AudioSymmetricRtpSession *> (_rtpSession);
                break;

            case Zrtp:
                delete static_cast<AudioZrtpSession *> (_rtpSession);
                break;
        }

        _rtpSession = NULL;
    } catch (...) {
        _debugException ("Exception caught when stopping the audio rtp session\n");
        throw AudioRtpFactoryException();
    }
}

sfl::AudioZrtpSession * AudioRtpFactory::getAudioZrtpSession()
{
    if ( (_rtpSessionType == Zrtp) && (_rtpSessionType != NULL)) {
        return static_cast<AudioZrtpSession *> (_rtpSession);
    } else {
        throw AudioRtpFactoryException();
    }
}
}
