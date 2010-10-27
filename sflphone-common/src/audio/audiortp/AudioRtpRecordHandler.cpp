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

#include "audio/audiolayer.h"
#include "manager.h"

namespace sfl
{

static const SFLDataFormat initFadeinFactor = 32000;

AudioRtpRecord::AudioRtpRecord(ManagerImpl *manager) : _audioCodec(NULL)
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
	// _audioLayerFrameSize = manager->getAudioDriver()->getFrameSize(); // in ms
	// _audioLayerSampleRate = manager->getAudioDriver()->getSampleRate();

	_audioLayerFrameSize = Manager::instance().getAudioDriver()->getFrameSize(); // in ms
	_audioLayerSampleRate = Manager::instance().getAudioDriver()->getSampleRate();

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

AudioCodec *AudioRtpRecord::getAudioCodec() const
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

EventQueue *AudioRtpRecord::getEventQueue()
{
    return &_eventQueue;
}

int AudioRtpRecord::getEventQueueSize() const
{
	return _eventQueue.size();
}

SFLDataFormat *AudioRtpRecord::getMicData()
{
	return _micData;
}

SFLDataFormat *AudioRtpRecord::getMicDataConverted()
{
	return _micDataConverted;
}

unsigned char *AudioRtpRecord::getMicDataEncoded()
{
	return _micDataEncoded;
}

SFLDataFormat *AudioRtpRecord::getMicAmplFactor()
{
    return &_micAmplFactor;
}

bool AudioRtpRecord::getMicFadeInComplete() const
{
    return _micFadeInComplete;
}

SFLDataFormat *AudioRtpRecord::getSpkrAmplFactor()
{
    return &_spkrAmplFactor;
}

SFLDataFormat *AudioRtpRecord::getSpkrDataConverted() const
{
    return _spkrDataConverted;
}

SFLDataFormat *AudioRtpRecord::getSpkrDataDecoded() const
{
    return _spkrDataDecoded;
}

bool AudioRtpRecord::getSpkrFadeInComplete() const
{
    return _spkrFadeInComplete;
}

AudioProcessing *AudioRtpRecord::getNoiseReductionProcess() const
{
	return _audioProcess;
}

void AudioRtpRecord::setAudioCodec(AudioCodec *audiocodec)
{
    this->_audioCodec = audiocodec;
}

void AudioRtpRecord::setCodecPayloadType(int codecPayloadType)
{
    this->_codecPayloadType = codecPayloadType;
}

void AudioRtpRecord::setHasDynamicPayload(bool hasDynamicPayload)
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

void AudioRtpRecord::setMicData(SFLDataFormat * micData)
{
	this->_micData = micData;
}

void AudioRtpRecord::setMicDataConverted(SFLDataFormat *micDataConverted)
{
	this->_micDataConverted = micDataConverted;
}

void AudioRtpRecord::setMicDataEncoded(unsigned char *micDataEncoded)
{
	this->_micDataEncoded = micDataEncoded;
}

void AudioRtpRecord::setMicAmplFactor(SFLDataFormat _micAmplFactor)
{
    this->_micAmplFactor = _micAmplFactor;
}

void AudioRtpRecord::setMicFadeInComplete(bool _micFadeInComplete)
{
    this->_micFadeInComplete = _micFadeInComplete;
}

void AudioRtpRecord::setSpkrAmplFactor(SFLDataFormat _spkrAmplFactor)
{
    this->_spkrAmplFactor = _spkrAmplFactor;
}

void AudioRtpRecord::setSpkrDataConverted(SFLDataFormat *_spkrDataConverted)
{
    this->_spkrDataConverted = _spkrDataConverted;
}

void AudioRtpRecord::setSpkrDataDecoded(SFLDataFormat *_spkrDataDecoded)
{
    this->_spkrDataDecoded = _spkrDataDecoded;
}

void AudioRtpRecord::setSpkrFadeInComplete(bool _spkrFadeInComplete)
{
    this->_spkrFadeInComplete = _spkrFadeInComplete;
}

void AudioRtpRecord::setAudioProcessing(AudioProcessing *audioProcess)
{
	this->_audioProcess = audioProcess;
}

void AudioRtpRecord::setNoiseSuppress(NoiseSuppress *noiseSuppress)
{
	this->_noiseSuppress = noiseSuppress;
}

AudioRtpRecordHandler::AudioRtpRecordHandler(ManagerImpl *manager, SIPCall *ca) : _audioRtpRecord(manager), _ca(ca) {
	// TODO Auto-generated constructor stub

}


AudioRtpRecordHandler::~AudioRtpRecordHandler() {
	// TODO Auto-generated destructor stub
}

void AudioRtpRecordHandler::setRtpMedia(AudioCodec* audioCodec) {

	// Set varios codec info to reduce indirection
	_audioRtpRecord.setAudioCodec(audioCodec);
	_audioRtpRecord.setCodecPayloadType(audioCodec->getPayload());
    _audioRtpRecord.setCodecSampleRate(audioCodec->getClockRate());
    _audioRtpRecord.setCodecFrameSize(audioCodec->getFrameSize());
    _audioRtpRecord.setHasDynamicPayload(audioCodec->hasDynamicPayload());
}


void AudioRtpRecordHandler::init()
{
    // init noise reduction process
    // _noiseSuppress = new NoiseSuppress (getCodecFrameSize(), getCodecSampleRate());
    // _audioProcess = new AudioProcessing (_noiseSuppress);

	// _audioRtpRecord._noiseSuppress
	// _audioRtpRecord.
}

void AudioRtpRecordHandler::initBuffers()
{
    // Set sampling rate, main buffer choose the highest one
    Manager::instance().getAudioDriver()->getMainBuffer()->setInternalSamplingRate (getCodecSampleRate());

    // may be different than one already set
    // _converterSamplingRate = Manager::instance().getAudioDriver()->getMainBuffer()->getInternalSamplingRate();

    // initialize SampleRate converter using AudioLayer's sampling rate
    // (internal buffers initialized with maximal sampling rate and frame size)
    _audioRtpRecord.setConverter(new SamplerateConverter (getAudioLayerSampleRate(), getAudioLayerFrameSize()));

    int nbSamplesMax = (int) ((getCodecSampleRate() * getAudioLayerFrameSize() / 1000)) * 2;
    _audioRtpRecord.setMicData(new SFLDataFormat[nbSamplesMax]);
    _audioRtpRecord.setMicDataConverted(new SFLDataFormat[nbSamplesMax]);
    _audioRtpRecord.setMicDataEncoded(new unsigned char[nbSamplesMax * 2]);
    _audioRtpRecord.setSpkrDataConverted(new SFLDataFormat[nbSamplesMax]);
    _audioRtpRecord.setSpkrDataDecoded(new SFLDataFormat[nbSamplesMax]);
    // memset (_micData, 0, nbSamplesMax*sizeof (SFLDataFormat));
    // memset (_micDataConverted, 0, nbSamplesMax*sizeof (SFLDataFormat));
    // memset (_micDataEncoded, 0, nbSamplesMax*2);
    // memset (_spkrDataConverted, 0, nbSamplesMax*sizeof (SFLDataFormat));
    // memset (_spkrDataDecoded, 0, nbSamplesMax*sizeof (SFLDataFormat));
    Manager::instance().addStream(_ca->getCallId());
}

void AudioRtpRecordHandler::initNoiseSuppress()
{
	NoiseSuppress *noiseSuppress = new NoiseSuppress (getCodecFrameSize(), getCodecSampleRate());
	AudioProcessing *processing = new AudioProcessing (noiseSuppress);

	_audioRtpRecord.setNoiseSuppress(noiseSuppress);
	_audioRtpRecord.setAudioProcessing(processing);
}

void AudioRtpRecordHandler::putDtmfEvent(int digit)
{
	sfl::DtmfEvent *dtmf = new sfl::DtmfEvent();
	dtmf->payload.event = digit;
	dtmf->payload.ebit = false; // end of event bit
	dtmf->payload.rbit = false; // reserved bit
	dtmf->payload.duration = 1; // duration for this event
	dtmf->newevent = true;
	dtmf->length = 1000;
	getEventQueue()->push_back(dtmf);
	_debug("AudioRtpSession: Put Dtmf Event %d", getEventQueue()->size());
}

int AudioRtpRecordHandler::processDataEncode(void)
{
	_debug("AudioProcessEncode");

	AudioCodec *audioCodec = getAudioCodec();
	AudioLayer *audioLayer = Manager::instance().getAudioDriver();

	SFLDataFormat *micData = _audioRtpRecord.getMicData();
	unsigned char *micDataEncoded = _audioRtpRecord.getMicDataEncoded();
	SFLDataFormat *micDataConverted = _audioRtpRecord.getMicDataConverted();

	assert(audioCodec);
    assert(audioLayer);

    int mainBufferSampleRate = audioLayer->getMainBuffer()->getInternalSamplingRate();
    // compute codec framesize in ms
    float fixedCodecFramesize = computeCodecFrameSize(getCodecFrameSize(), getCodecSampleRate());
    // compute nb of byte to get coresponding to 20 ms at audio layer frame size (44.1 khz)
    int maxBytesToGet = computeNbByteAudioLayer(fixedCodecFramesize);
    // available bytes inside ringbuffer
    int availBytesFromMic = audioLayer->getMainBuffer()->availForGet(_ca->getCallId());
    // set available byte to maxByteToGet
    int bytesAvail = (availBytesFromMic < maxBytesToGet) ? availBytesFromMic : maxBytesToGet;
    if(bytesAvail == 0){
    	memset(micDataEncoded, 0, sizeof (SFLDataFormat));
    	return getCodecFrameSize();
    }

    // Get bytes from micRingBuffer to data_from_mic
    int nbSample = audioLayer->getMainBuffer()->getData(micData, bytesAvail, 100, _ca->getCallId()) / sizeof (SFLDataFormat);
    if(!_audioRtpRecord.getMicFadeInComplete())
    	_audioRtpRecord.setMicFadeInComplete(fadeIn(micData, nbSample, _audioRtpRecord.getMicAmplFactor()));

    if(nbSample == 0)
    	return nbSample;

    // nb bytes to be sent over RTP
    int compSize = 0;

    // test if resampling is required
    if(_audioRtpRecord.getCodecSampleRate() != mainBufferSampleRate){

    	int nbSampleUp = nbSample;

    	nbSample = _audioRtpRecord.getConverter()->downsampleData(micData, micDataConverted, _audioRtpRecord.getCodecSampleRate(), mainBufferSampleRate, nbSampleUp);
    	if(Manager::instance().audioPreference.getNoiseReduce())
    		_audioRtpRecord.getNoiseReductionProcess()->processAudio(micDataConverted, nbSample * sizeof (SFLDataFormat));

    	compSize = audioCodec->codecEncode(micDataEncoded, micDataConverted, nbSample * sizeof (SFLDataFormat));
    }else{
    	if(Manager::instance().audioPreference.getNoiseReduce())
    		_audioRtpRecord.getNoiseReductionProcess()->processAudio(micData, nbSample * sizeof (SFLDataFormat));

    	// no resampling required
    	compSize = audioCodec->codecEncode(micDataEncoded, micData, nbSample * sizeof (SFLDataFormat));
    }
    return compSize;
}

void AudioRtpRecordHandler::processDataDecode(unsigned char *spkrData, unsigned int size)
{

	_debug("AudioProcessDecode");

	AudioCodec *audioCodec = getAudioCodec();
	AudioLayer *audioLayer = Manager::instance().getAudioDriver();

	if (!audioCodec)
		return;

    SFLDataFormat *spkrDataDecoded = _audioRtpRecord.getSpkrDataConverted();
    SFLDataFormat *spkrDataConverted = _audioRtpRecord.getSpkrDataDecoded();

	int mainBufferSampleRate = Manager::instance().getAudioDriver()->getMainBuffer()->getInternalSamplingRate();

	// Return the size of data in bytes
	int expandedSize = audioCodec->codecDecode (spkrDataDecoded , spkrData , size);

	// buffer _receiveDataDecoded ----> short int or int16, coded on 2 bytes
	int nbSample = expandedSize / sizeof (SFLDataFormat);

	if (!_audioRtpRecord.getSpkrFadeInComplete())
		_audioRtpRecord.setSpkrFadeInComplete(fadeIn(spkrDataDecoded, nbSample, _audioRtpRecord.getMicAmplFactor()));

	// test if resampling is required
	if (audioCodec->getClockRate() != mainBufferSampleRate) {

		// Do sample rate conversion
		int nbSampleDown = nbSample;

		nbSample = _audioRtpRecord.getConverter()->upsampleData (spkrDataDecoded, spkrDataConverted, _audioRtpRecord.getCodecSampleRate(), mainBufferSampleRate, nbSampleDown);

		// put data in audio layer, size in byte
		Manager::instance().getAudioDriver()->getMainBuffer()->putData (spkrDataConverted, nbSample * sizeof (SFLDataFormat), 100, _ca->getCallId());


	} else {
		// put data in audio layer, size in byte
		Manager::instance().getAudioDriver()->getMainBuffer()->putData (spkrDataDecoded, expandedSize, 100, _ca->getCallId());
	}
}

bool AudioRtpRecordHandler::fadeIn(SFLDataFormat *audio, int size, SFLDataFormat *factor)
{
	// apply amplitude factor;
	while(size){
		size--;
		audio[size] /= *factor;
	}
	// decrease factor
	*factor /= FADEIN_STEP_SIZE;

	// if factor reach 0, thsi function should no be called anymore
	if(*factor == 0)
		return true;

	return false;
}

}

