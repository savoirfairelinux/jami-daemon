/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include "AudioRtpRecordHandler.h"

namespace sfl
{

static const SFLDataFormat initFadeinFactor = 32000;

AudioRtpRecord::AudioRtpRecord() : _audioCodec(NULL)
								 , _hasDynamicPayloadType(false)
								 , _micData(NULL)
								 , _micDataConverted(NULL)
								 , _micDataEncoded(NULL)
								 , _spkrDataDecoded(NULL)
								 , _spkrDataConverted(NULL)
								 , _converter(NULL)
								 , _audioLayerSampleRate(0)
								 , _codecSampleRate(0)
								 , _audioLayerFrameSize(0)
								 , _codecFrameSize(0)
								 , _converterSamplingRate(0)
								 , _micFadeInComplete(false)
								 , _spkrFadeInComplete(false)
								 , _micAmplFactor(initFadeinFactor)
								 , _spkrAmplFactor(initFadeinFactor)
								 , _audioProcess(NULL)
								 , _noiseSuppress(NULL)
{
	_audioLayerFrameSize = _manager->getAudioDriver()->getFrameSize(); // in ms
	_audioLayerSampleRate = _manager->getAudioDriver()->getSampleRate();
}


AudioRtpRecord::~AudioRtpRecord()
{
	if (_micData)
		delete [] _micData;
	_micData = NULL;

	if (_micDataConverted)
		delete [] _micDataConverted;
	_micDataConverted = NULL;

	if (_micDataEncoded)
		delete [] _micDataEncoded;
	_micDataEncoded = NULL;

	if (_spkrDataDecoded)
		delete [] _spkrDataDecoded;
	_spkrDataDecoded = NULL;

	if (_spkrDataConverted)
		delete [] _spkrDataConverted;
	_spkrDataConverted = NULL;


	if(_converter)
		delete _converter;
	_converter = NULL;

	if (_audioCodec) {
		delete _audioCodec;
		_audioCodec = NULL;
	}

	if (_audioProcess) {
		delete _audioProcess;
		_audioProcess = NULL;
	}

	if (_noiseSuppress) {
		delete _noiseSuppress;
		_noiseSuppress = NULL;
	}
}

AudioCodec *AudioRtpRecord::getAudiocodec() const
{
    return _audioCodec;
}

int AudioRtpRecord::getCodecPayloadType() const
{
    return _codecPayloadType;
}

bool AudioRtpRecord::getHasDynamicPayload() const
{
	return _hasDynamicPayloadType;
}

int AudioRtpRecord::getAudioLayerFrameSize() const
{
    return _audioLayerFrameSize;
}

int AudioRtpRecord::getAudioLayerSampleRate() const
{
    return _audioLayerSampleRate;
}

int AudioRtpRecord::getCodecFrameSize() const
{
    return _codecFrameSize;
}

int AudioRtpRecord::getCodecSampleRate() const
{
    return _codecSampleRate;
}

SamplerateConverter *AudioRtpRecord::getConverter() const
{
    return _converter;
}

int AudioRtpRecord::getConverterSamplingRate() const
{
    return _converterSamplingRate;
}

EventQueue *AudioRtpRecord::getEventQueue() const
{
    return &_eventQueue;
}

int AudioRtpRecord::getEventQueueSize() const
{
	return _eventQueue.size();
}

unsigned char *getEncodedData() const
{
	return _micDataEncoded;
}

void AudioRtpRecord::setAudiocodec(AudioCodec *audiocodec)
{
    this->_audiocodec = audiocodec;
}

void AudioRtpRecord::setCodecPayloadType(int codecPayloadType)
{
    this->codecPayloadType = codecPayloadType;
}

bool AudioRtpRecord::setHasDynamicPayload(bool hasDynamicPayload)
{
	_hasDynamicPayloadType = hasDynamicPayload;
}

void AudioRtpRecord::setAudioLayerFrameSize(int _audioLayerFrameSize)
{
    this->_audioLayerFrameSize = _audioLayerFrameSize;
}

void AudioRtpRecord::setAudioLayerSampleRate(int _audioLayerSampleRate)
{
    this->_audioLayerSampleRate = _audioLayerSampleRate;
}

void AudioRtpRecord::setCodecFrameSize(int _codecFrameSize)
{
    this->_codecFrameSize = _codecFrameSize;
}

void AudioRtpRecord::setCodecSampleRate(int _codecSampleRate)
{
    this->_codecSampleRate = _codecSampleRate;
}

void AudioRtpRecord::setConverter(SamplerateConverter *_converter)
{
    this->_converter = _converter;
}

void AudioRtpRecord::setConverterSamplingRate(int _converterSamplingRate)
{
    this->_converterSamplingRate = _converterSamplingRate;
}

void AudioRtpRecord::setEventQueue(EventQueue _eventQueue)
{
    this->_eventQueue = _eventQueue;
}


AudioRtpRecordHandler::AudioRtpRecordHandler() {
	// TODO Auto-generated constructor stub

}


AudioRtpRecordHandler::~AudioRtpRecordHandler() {
	// TODO Auto-generated destructor stub
}

void AudioRtpRecord::SetRtpMedia(AudioCodec* audioCodec) {

	// Set varios codec info to reduce indirection
	_audioRtpRecord.setAudioCodec(audioCodec);
	_audioRtpRecord.setCodecPayloadType(audioCodec->getPayloadType());
    _audioRtpRecord.setCodecSampleRate(audioCodec->getClockRate());
    _audioRtpRecord.setCodecFrameSize(audioCodec->getFrameSize());
    _audioRtpRecord.setHasDynamicPayload(audioCodec->hasDynamicPayload());
}


void AudioRtpRecord::init()
{
    // init noise reduction process
    // _noiseSuppress = new NoiseSuppress (getCodecFrameSize(), getCodecSampleRate());
    // _audioProcess = new AudioProcessing (_noiseSuppress);

	// _audioRtpRecord._noiseSuppress
	// _audioRtpRecord.
}

void AudioRtpRecord::initBuffers()
{
    // Set sampling rate, main buffer choose the highest one
    _manager->getAudioDriver()->getMainBuffer()->setInternalSamplingRate (_codecSampleRate);

    // may be different than one already set
    _converterSamplingRate = _manager->getAudioDriver()->getMainBuffer()->getInternalSamplingRate();

    // initialize SampleRate converter using AudioLayer's sampling rate
    // (internal buffers initialized with maximal sampling rate and frame size)
    _converter = new SamplerateConverter (_layerSampleRate, _layerFrameSize);

    int nbSamplesMax = (int) (_codecSampleRate * _layerFrameSize /1000) *2;
    _micData = new SFLDataFormat[nbSamplesMax];
    _micDataConverted = new SFLDataFormat[nbSamplesMax];
    _micDataEncoded = new unsigned char[nbSamplesMax*2];
    _spkrDataConverted = new SFLDataFormat[nbSamplesMax];
    _spkrDataDecoded = new SFLDataFormat[nbSamplesMax];


    memset (_micData, 0, nbSamplesMax*sizeof (SFLDataFormat));
    memset (_micDataConverted, 0, nbSamplesMax*sizeof (SFLDataFormat));
    memset (_micDataEncoded, 0, nbSamplesMax*2);
    memset (_spkrDataConverted, 0, nbSamplesMax*sizeof (SFLDataFormat));
    memset (_spkrDataDecoded, 0, nbSamplesMax*sizeof (SFLDataFormat));

    _manager->addStream (_ca->getCallId());
}


void AudioRtpRecordHandler::putDtmfEvent (int digit)
{

    sfl::DtmfEvent *dtmf = new sfl::DtmfEvent();

    dtmf->payload.event = digit;
    dtmf->payload.ebit = false; 			// end of event bit
    dtmf->payload.rbit = false;  		// reserved bit
    dtmf->payload.duration = 1; 	        // duration for this event
    dtmf->newevent = true;
    dtmf->length = 1000;

    _eventQueue.push_back (dtmf);

    _debug ("AudioRtpSession: Put Dtmf Event %d", _eventQueue.size());
}

int AudioRtpRecordHandler::processDataEncode (void)
{
    assert (_audiocodec);
    assert (_audiolayer);

    int _mainBufferSampleRate = _manager->getAudioDriver()->getMainBuffer()->getInternalSamplingRate();

    // compute codec framesize in ms
    float fixed_codec_framesize = computeCodecFrameSize (_audiocodec->getFrameSize(), _audiocodec->getClockRate());

    // compute nb of byte to get coresponding to 20 ms at audio layer frame size (44.1 khz)
    int maxBytesToGet = computeNbByteAudioLayer (fixed_codec_framesize);

    // available bytes inside ringbuffer
    int availBytesFromMic = _manager->getAudioDriver()->getMainBuffer()->availForGet (_ca->getCallId());

    // set available byte to maxByteToGet
    int bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;

    if (bytesAvail == 0) {
        memset (_micDataEncoded, 0, sizeof (SFLDataFormat));
        return _audiocodec->getFrameSize();
    }

    // Get bytes from micRingBuffer to data_from_mic
    int nbSample = _manager->getAudioDriver()->getMainBuffer()->getData (_micData , bytesAvail, 100, _ca->getCallId()) / sizeof (SFLDataFormat);

    if (!_micFadeInComplete)
        _micFadeInComplete = fadeIn (_micData, nbSample, &_micAmplFactor);

    if (nbSample == 0)
        return nbSample;

    // nb bytes to be sent over RTP
    int compSize = 0;

    // test if resampling is required
    if (_audiocodec->getClockRate() != _mainBufferSampleRate) {
        int nb_sample_up = nbSample;

        _nSamplesMic = nbSample;

        nbSample = _converter->downsampleData (_micData , _micDataConverted , _audiocodec->getClockRate(), _mainBufferSampleRate, nb_sample_up);

        if (_manager->audioPreference.getNoiseReduce())
            _audioProcess->processAudio (_micDataConverted, nbSample*sizeof (SFLDataFormat));

        compSize = _audiocodec->codecEncode (_micDataEncoded, _micDataConverted, nbSample*sizeof (SFLDataFormat));

    } else {

        _nSamplesMic = nbSample;

        if (_manager->audioPreference.getNoiseReduce())
            _audioProcess->processAudio (_micData, nbSample*sizeof (SFLDataFormat));

        // no resampling required
        compSize = _audiocodec->codecEncode (_micDataEncoded, _micData, nbSample*sizeof (SFLDataFormat));

    }

    return compSize;
}

void AudioRtpRecordHandler::processDataDecode (unsigned char * spkrData, unsigned int size)
{
    if (_audiocodec != NULL) {


        int _mainBufferSampleRate = _manager->getAudioDriver()->getMainBuffer()->getInternalSamplingRate();

        // Return the size of data in bytes
        int expandedSize = _audiocodec->codecDecode (_spkrDataDecoded , spkrData , size);

        // buffer _receiveDataDecoded ----> short int or int16, coded on 2 bytes
        int nbSample = expandedSize / sizeof (SFLDataFormat);

        if (!_spkrFadeInComplete)
            _spkrFadeInComplete = fadeIn (_spkrDataDecoded, nbSample, &_spkrAmplFactor);

        // test if resampling is required
        if (_audiocodec->getClockRate() != _mainBufferSampleRate) {

            // Do sample rate conversion
            int nb_sample_down = nbSample;

            nbSample = _converter->upsampleData (_spkrDataDecoded, _spkrDataConverted, _codecSampleRate, _mainBufferSampleRate, nb_sample_down);

            // Store the number of samples for recording
            _nSamplesSpkr = nbSample;

            // put data in audio layer, size in byte
            _manager->getAudioDriver()->getMainBuffer()->putData (_spkrDataConverted, nbSample * sizeof (SFLDataFormat), 100, _ca->getCallId());


        } else {
            // Store the number of samples for recording
            _nSamplesSpkr = nbSample;

            // put data in audio layer, size in byte
            _manager->getAudioDriver()->getMainBuffer()->putData (_spkrDataDecoded, expandedSize, 100, _ca->getCallId());
        }

        // Notify (with a beep) an incoming call when there is already a call
        if (_manager->incomingCallWaiting() > 0) {
            _countNotificationTime += _time->getSecond();
            int countTimeModulo = _countNotificationTime % 5000;

            // _debug("countNotificationTime: %d\n", countNotificationTime);
            // _debug("countTimeModulo: %d\n", countTimeModulo);
            if ( (countTimeModulo - _countNotificationTime) < 0) {
                _manager->notificationIncomingCall();
            }

            _countNotificationTime = countTimeModulo;
        }

    }
}


bool AudioRtpRecordHandler::fadeIn (SFLDataFormat *audio, int size, SFLDataFormat *factor)
{

    // apply amplitude factor;
    while (size) {
        size--;
        audio[size] /= *factor;
    }

    // decrease factor
    *factor /= FADEIN_STEP_SIZE;

    // if factor reach 0, thsi function should no be called anymore
    if (*factor == 0)
        return true;

    return false;
}



}

