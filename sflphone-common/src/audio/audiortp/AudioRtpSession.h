/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
 */
#ifndef __SFL_AUDIO_RTP_SESSION_H__
#define __SFL_AUDIO_RTP_SESSION_H__

#include <iostream>
#include <exception>

#include "global.h"

#include "sip/sipcall.h"
#include "sip/sdp.h"
#include "audio/audiolayer.h"
#include "audio/codecs/audiocodec.h"
#include "audio/samplerateconverter.h"
#include "managerimpl.h"

#include <ccrtp/rtp.h>
#include <cc++/numbers.h> // ost::Time

namespace sfl {

    static const int schedulingTimeout = 100000;
    static const int expireTimeout = 1000000;
    
    class AudioRtpSessionException: public std::exception
    {
      virtual const char* what() const throw()
      {
        return "AudioRtpSessionException occured";
      }
    };
    
    template <typename D>
    class AudioRtpSession : public ost::Thread, public ost::TimerPort {
        public:
            /**
            * Constructor
            * @param sipcall The pointer on the SIP call
            */
            AudioRtpSession (ManagerImpl * manager, SIPCall* sipcall);

            ~AudioRtpSession();

            // Thread associated method
            virtual void run ();
            
            int startRtpThread();
    
        private:
        
            void initBuffers(void);
            
            void setSessionTimeouts(void);
            void setSessionMedia(void);
            void setDestinationIpAddress(void);
                
            int processDataEncode(void);
            void processDataDecode(unsigned char * spkrData, unsigned int size, int& countTime);
            
            inline float computeCodecFrameSize (int codecSamplePerFrame, int codecClockRate) {
                return ( (float) codecSamplePerFrame * 1000.0) / (float) codecClockRate;
            }          
            inline int computeNbByteAudioLayer (float codecFrameSize) {
                return (int) ( (float) _layerSampleRate * codecFrameSize * (float) sizeof (SFLDataFormat) / 1000.0);
            }
          
            void sendMicData(int timestamp);
            void receiveSpeakerData (int& countTime);
            
            ost::Time * _time;
   
            // This semaphore is not used 
            // but is needed in order to avoid 
            // ambiguous compiling problem.
            // It is set to 0, and since it is
            // optional in ost::thread, then 
            // it amounts to the same as doing
            // start() with no semaphore at all. 
            ost::Semaphore * _mainloopSemaphore;
                     
            AudioCodec * _audiocodec;
            
            AudioLayer * _audiolayer;
                                                 
            /** Mic-data related buffers */
            SFLDataFormat* _micData;
            SFLDataFormat* _micDataConverted;
            unsigned char* _micDataEncoded;

            /** Speaker-data related buffers */
            SFLDataFormat* _spkrDataDecoded;
            SFLDataFormat* _spkrDataConverted;

            /** Sample rate converter object */
            SamplerateConverter * _converter;

            /** Variables to process audio stream: sample rate for playing sound (typically 44100HZ) */
            int _layerSampleRate;  

            /** Sample rate of the codec we use to encode and decode (most of time 8000HZ) */
            int _codecSampleRate;

            /** Length of the sound frame we capture in ms (typically 20ms) */
            int _layerFrameSize; 

            /** Codecs frame size in samples (20 ms => 882 at 44.1kHz)
                The exact value is stored in the codec */
            int _codecFrameSize;

            /** Speaker buffer length in samples once the data are resampled
             *  (used for mixing and recording)
             */
            int _nSamplesSpkr; 

            /** Mic buffer length in samples once the data are resampled
             *  (used for mixing and recording)
             */
            int _nSamplesMic;
            
            /**
             * Maximum number of sample for audio buffers (mic and spkr)
             */
            int _nbSamplesMax; 
            
            /**
             * Manager instance. 
             */
             ManagerImpl * _manager;
            
        protected:
            SIPCall * _ca;
            
    };    
    
    template <typename D>
    AudioRtpSession<D>::AudioRtpSession(ManagerImpl * manager, SIPCall * sipcall) :
     _time (new ost::Time()), 
     _mainloopSemaphore(0),
     _audiocodec (NULL),
     _audiolayer (NULL),
     _ca (sipcall), 
     _micData (NULL), 
     _micDataConverted (NULL), 
     _micDataEncoded (NULL), 
     _spkrDataDecoded (NULL), 
     _spkrDataConverted (NULL),
     _converter (NULL),
     _layerSampleRate(0),
     _codecSampleRate(0), 
     _layerFrameSize(0),
     _manager(manager)
    {
        setCancel (cancelDefault);

        assert(_ca);
        
        _debug ("Local audio port %i will be used\n", _ca->getLocalAudioPort());

        //mic, we receive from soundcard in stereo, and we send encoded
        _audiolayer = _manager->getAudioDriver();
        
        if (_audiolayer == NULL) { throw AudioRtpSessionException(); }
        
        _layerFrameSize = _audiolayer->getFrameSize(); // in ms
        _layerSampleRate = _audiolayer->getSampleRate();

    }
    
    template <typename D>
    AudioRtpSession<D>::~AudioRtpSession()
    {
        _debug ("Delete AudioRtpSession instance\n");

        try {
            terminate();
        } catch (...) {
            _debugException ("Thread destructor didn't terminate correctly");
            throw;
        }

	_debug("Unbind audio RTP stream for call id %i\n", _ca->getCallId().c_str());
	_audiolayer->getMainBuffer()->unBindAll(_ca->getCallId());

        delete [] _micData;
        delete [] _micDataConverted;
        delete [] _micDataEncoded;
        delete [] _spkrDataDecoded;
        delete [] _spkrDataConverted;
        delete _time;
        delete _converter;
        _debug ("AudioRtpSession instance deleted\n");
    }
    
    template <typename D>
    void AudioRtpSession<D>::initBuffers() 
    {
        _converter = new SamplerateConverter (_layerSampleRate , _layerFrameSize);
        int nbSamplesMax = (int) (_layerSampleRate * _layerFrameSize /1000);
        _micData = new SFLDataFormat[nbSamplesMax];
        _micDataConverted = new SFLDataFormat[nbSamplesMax];
        _micDataEncoded = new unsigned char[nbSamplesMax];
        _spkrDataConverted = new SFLDataFormat[nbSamplesMax];
        _spkrDataDecoded = new SFLDataFormat[nbSamplesMax];

	_manager->addStream(_ca->getCallId());
    }
    
    template <typename D>
    void AudioRtpSession<D>::setSessionTimeouts(void) 
    {
        try {
            static_cast<D*>(this)->setSchedulingTimeout (schedulingTimeout);
            static_cast<D*>(this)->setExpireTimeout (expireTimeout);
        } catch (...) {
            _debugException ("Initialization failed while setting timeouts");
            throw AudioRtpSessionException();
        }
    }
    
    template <typename D>
    void AudioRtpSession<D>::setSessionMedia(void)
    {
        assert(_ca);

	AudioCodecType pl = (AudioCodecType)_ca->getLocalSDP()->get_session_media()->getPayload();
	_audiocodec = _manager->getCodecDescriptorMap().instantiateCodec(pl);

        if (_audiocodec == NULL) {
            _debug ("No audiocodec, can't init RTP media\n");
            throw AudioRtpSessionException();
        }

        _debug ("Init audio RTP session: codec payload %i\n", _audiocodec->getPayload());

        _codecSampleRate = _audiocodec->getClockRate();
        _codecFrameSize = _audiocodec->getFrameSize();

        //TODO: figure out why this is necessary.
        if (_audiocodec->getPayload() == 9) {
            _debug ("Setting payload format to G722\n");
            static_cast<D*>(this)->setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) _audiocodec->getPayload(), _audiocodec->getClockRate()));
        } else if (_audiocodec->hasDynamicPayload()) {
            _debug ("Setting a dynamic payload format\n");
            static_cast<D*>(this)->setPayloadFormat (ost::DynamicPayloadFormat ( (ost::PayloadType) _audiocodec->getPayload(), _audiocodec->getClockRate()));
        } else if (!_audiocodec->hasDynamicPayload() && _audiocodec->getPayload() != 9) {
            _debug ("Setting a static payload format\n");
            static_cast<D*>(this)->setPayloadFormat (ost::StaticPayloadFormat ( (ost::StaticPayloadType) _audiocodec->getPayload()));
        }
    }
    
    template <typename D>
    void AudioRtpSession<D>::setDestinationIpAddress(void)
    {
        if (_ca == NULL) {
            _debug ("Sipcall is gone.\n");
            throw AudioRtpSessionException();
        }
        
        _debug ("Setting IP address for the RTP session\n");
        
        ost::InetHostAddress remote_ip (_ca->getLocalSDP()->get_remote_ip().c_str());
        _debug ("Init audio RTP session: remote ip %s\n", _ca->getLocalSDP()->get_remote_ip().data());

        if (!remote_ip) {
            _debug ("Target IP address [%s] is not correct!\n", _ca->getLocalSDP()->get_remote_ip().data());
            return;
        }

        if (! static_cast<D*>(this)->addDestination (remote_ip, (unsigned short) _ca->getLocalSDP()->get_remote_audio_port())) {
            _debug ("Can't add destination to session!\n");
            return;
        }
    }
    
    template <typename D>
    int AudioRtpSession<D>::processDataEncode(void)
    {
        assert(_audiocodec);
        assert(_audiolayer);
        
        // compute codec framesize in ms
        float fixed_codec_framesize = computeCodecFrameSize (_audiocodec->getFrameSize(), _audiocodec->getClockRate());

        // compute nb of byte to get coresponding to 20 ms at audio layer frame size (44.1 khz)
        int maxBytesToGet = computeNbByteAudioLayer (fixed_codec_framesize);

        // available bytes inside ringbuffer
        int availBytesFromMic = _audiolayer->getMainBuffer()->availForGet(_ca->getCallId());

	_debug("AudioRtpSession::processDataEncode (%s): avail bytes from mic %i\n", _ca->getCallId().c_str(), availBytesFromMic);

        // set available byte to maxByteToGet
        int bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;

        if (bytesAvail == 0)
            return 0;

        // Get bytes from micRingBuffer to data_from_mic
        int nbSample = _audiolayer->getMainBuffer()->getData(_micData , bytesAvail, 100, _ca->getCallId()) / sizeof (SFLDataFormat);

        // nb bytes to be sent over RTP
        int compSize = 0;

        // test if resampling is required
        if (_audiocodec->getClockRate() != _layerSampleRate) {
            int nb_sample_up = nbSample;
            _nSamplesMic = nbSample;
            nbSample = _converter->downsampleData (_micData , _micDataConverted , _audiocodec->getClockRate(), _layerSampleRate , nb_sample_up);
            compSize = _audiocodec->codecEncode (_micDataEncoded, _micDataConverted, nbSample*sizeof (int16));
        } else {
            // no resampling required
            compSize = _audiocodec->codecEncode (_micDataEncoded, _micData, nbSample*sizeof (int16));
        }

        return compSize;
    }
    
    template <typename D>
    void AudioRtpSession<D>::processDataDecode(unsigned char * spkrData, unsigned int size, int& countTime) 
    {
        if (_audiocodec != NULL) {
            // Return the size of data in bytes
            int expandedSize = _audiocodec->codecDecode (_spkrDataDecoded , spkrData , size);

            // buffer _receiveDataDecoded ----> short int or int16, coded on 2 bytes
            int nbSample = expandedSize / sizeof (SFLDataFormat);

            // test if resampling is required
            if (_audiocodec->getClockRate() != _layerSampleRate) {

                // Do sample rate conversion
                int nb_sample_down = nbSample;
                nbSample = _converter->upsampleData (_spkrDataDecoded, _spkrDataConverted, _codecSampleRate, _layerSampleRate , nb_sample_down);
                // Store the number of samples for recording
                _nSamplesSpkr = nbSample;

                // put data in audio layer, size in byte
		_audiolayer->getMainBuffer()->putData (_spkrDataConverted, nbSample * sizeof (SFLDataFormat), 100, _ca->getCallId());

		_debug("AudioRtpSession::processDataDecode (%s): decode bytes: %i\n", _ca->getCallId().c_str(), nbSample);

            } else {
                // Store the number of samples for recording
                _nSamplesSpkr = nbSample;

		_debug("AudioRtpSession::processDataDecode (%s): decode bytes: %i\n", _ca->getCallId().c_str(), nbSample);

                // put data in audio layer, size in byte
                _audiolayer->getMainBuffer()->putData (_spkrDataConverted, nbSample * sizeof (SFLDataFormat), 100, _ca->getCallId());
            }

            // Notify (with a beep) an incoming call when there is already a call
            countTime += _time->getSecond();

            if (_manager->incomingCallWaiting() > 0) {
                countTime = countTime % 500; // more often...

                if (countTime == 0) {
                    _manager->notificationIncomingCall();
                }
            }

        } else {
            countTime += _time->getSecond();
        }
    }
    
    template <typename D>
    void AudioRtpSession<D>::sendMicData(int timestamp)
    {
        // STEP:
        //   1. get data from mic
        //   2. convert it to int16 - good sample, good rate
        //   3. encode it
        //   4. send it

        timestamp += _time->getSecond();

        if (!_audiolayer) {
            _debug ("No audiolayer available for MIC\n");
            return;
        }

        if (!_audiocodec) {
            _debug ("No audiocodec available for MIC\n");
            return;
        }

        int compSize = processDataEncode();

        // putData put the data on RTP queue, sendImmediate bypass this queue
        static_cast<D*>(this)->putData (timestamp, _micDataEncoded, compSize);
    }
    
    
    template <typename D>
    void AudioRtpSession<D>::receiveSpeakerData (int& countTime)
    {
        if (!_audiolayer) {
            _debug ("No audiolayer available for speaker\n");
            return;
        }

        if (!_audiocodec) {
            _debug ("No audiocodec available for speaker\n");
            return;
        }

        const ost::AppDataUnit* adu = NULL;

        adu = static_cast<D*>(this)->getData(static_cast<D*>(this)->getFirstTimestamp());

        if (adu == NULL) {
            // _debug("No RTP audio stream\n");
            return;
        }

        unsigned char* spkrData  = (unsigned char*) adu->getData(); // data in char

        unsigned int size = adu->getSize(); // size in char

        processDataDecode (spkrData, size, countTime);
    }
    
    template <typename D>
    int AudioRtpSession<D>::startRtpThread ()
    {
        _debug("Starting main thread\n");
        return start(_mainloopSemaphore);
    }
    
    template <typename D>
    void AudioRtpSession<D>::run ()
    {
        initBuffers();

        setSessionTimeouts();
        setDestinationIpAddress();
        setSessionMedia();

        int sessionWaiting;
        int timestep = _codecFrameSize;
        int timestamp = static_cast<D*>(this)->getCurrentTimestamp(); // for mic
        int countTime = 0; // for receive
        int threadSleep = 0;

        if (_codecSampleRate != 0)
            { threadSleep = (_codecFrameSize * 1000) / _codecSampleRate; }
        else
            { threadSleep = _layerFrameSize; }

        TimerPort::setTimer (threadSleep);
        
        if (_audiolayer == NULL) {
            _debug("For some unknown reason, audiolayer is null, just as \
            we were about to start the audio stream\n");
            throw AudioRtpSessionException();
        }

        _audiolayer->startStream();
        static_cast<D*>(this)->startRunning();

        _debug ("Entering RTP mainloop for callid %s\n",_ca->getCallId().c_str());

        while (!testCancel()) {
            // Send session
            sessionWaiting = static_cast<D*>(this)->isWaiting();

            sendMicData (timestamp);
            timestamp += timestep;

            // Recv session
            receiveSpeakerData (countTime);

            // Let's wait for the next transmit cycle
            if (sessionWaiting == 1) {
                // Record mic and speaker during conversation
                _ca->recAudio.recData (_spkrDataConverted, _micData, _nSamplesSpkr, _nSamplesMic);
            } else {
                // Record mic only while leaving a message
                _ca->recAudio.recData (_micData,_nSamplesMic);
            }

            // Let's wait for the next transmit cycle
            Thread::sleep (TimerPort::getTimer());

            // TimerPort::incTimer(20); // 'frameSize' ms
            TimerPort::incTimer (threadSleep);
        }
        
        _debug ("Left RTP main loop for callid %s\n",_ca->getCallId().c_str());
    }
    
}
#endif // __AUDIO_RTP_SESSION_H__

