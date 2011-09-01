/*
 *  Copyright (C) 2008 2009 Savoir-Faire Linux inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <fstream>
#include <limits.h>

#include "speexechocancel.h"
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#include "manager.h"

// number of samples (20 ms)
#define EC_FRAME_SIZE 160
// number of sample to process, (800 à 4000 samples, 100 to 500 ms)
#define EC_FILTER_LENGTH 800


SpeexEchoCancel::SpeexEchoCancel()
{
    int samplingRate = 8000;

    int echoDelayMs = Manager::instance().getEchoCancelDelay();
    int echoTailLengthMs = Manager::instance().getEchoCancelTailLength();

    _echoDelay = echoDelayMs * samplingRate / 1000;
    _echoTailLength = echoTailLengthMs * samplingRate / 1000;

    // _echoState = speex_echo_state_init (EC_FRAME_SIZE, EC_FILTER_LENGTH);
    _echoState = speex_echo_state_init (EC_FRAME_SIZE, _echoTailLength);
    _preState = speex_preprocess_state_init (EC_FRAME_SIZE, samplingRate);

    _debug("EchoCancel: Initializing echo canceller with delay: %d, filter length: %d, frame size: %d and samplerate %d",
    											_echoDelay, _echoTailLength, EC_FRAME_SIZE, samplingRate);

    speex_echo_ctl (_echoState, SPEEX_ECHO_SET_SAMPLING_RATE, &samplingRate);
    speex_preprocess_ctl (_preState, SPEEX_PREPROCESS_SET_ECHO_STATE, _echoState);

    _micData = new RingBuffer (100000);
    _spkrData = new RingBuffer (100000);

    _micData->createReadPointer();
    _spkrData->createReadPointer();

#ifdef DUMP_ECHOCANCEL_INTERNAL_DATA
    micFile = new ofstream("test_mic_data.raw");
    spkrFile = new ofstream("test_spkr_data.raw");
    micProcessFile = new ofstream("test_mic_data_process.raw", std::ofstream::out);
    spkrProcessFile = new ofstream("test_spkr_data_process.raw", std::ofstream::out);
    echoFile = new ofstream("test_echo_data.raw");
#endif

    _spkrStopped = true;
}

SpeexEchoCancel::~SpeexEchoCancel()
{
    speex_echo_state_destroy (_echoState);
    speex_preprocess_state_destroy (_preState);
    delete _micData;
    delete _spkrData;
#ifdef DUMP_ECHOCANCEL_INTERNAL_DATA
    delete micFile;
    delete spkrFile;
    delete micProcessFile;
    delete spkrProcessFile;
    delete echoFile;
#endif

}

void SpeexEchoCancel::putData (SFLDataFormat *inputData, int nbBytes)
{
    if (_spkrStopped) {
        _micData->flushAll();
        _spkrData->flushAll();
        _spkrStopped = false;
    }

#ifdef DUMP_ECHOCANCEL_INTERNAL_DATA
    spkrFile->write(reinterpret_cast<char *>(inputData), nbBytes);
#endif

    _spkrData->Put (inputData, nbBytes);
}

int SpeexEchoCancel::process (SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes)
{
    if (_spkrStopped)
        return 0;

    const int byteSize = EC_FRAME_SIZE * sizeof(SFLDataFormat);

    // init temporary buffers
    memset (_tmpSpkr, 0, sizeof(_tmpSpkr));
    memset (_tmpMic, 0, sizeof(_tmpMic));
    memset (_tmpOut, 0, sizeof(_tmpOut));

#ifdef DUMP_ECHOCANCEL_INTERNAL_DATA
    micFile->write(reinterpret_cast<char *>(inputData), nbBytes);
#endif

    // Put mic data in ringbuffer
    _micData->Put (inputData, nbBytes);

    // Store data for synchronization
    int spkrAvail = _spkrData->AvailForGet();
    int micAvail = _micData->AvailForGet();

    // Init number of frame processed
    int nbFrame = 0;

    // Get data from mic and speaker
    // if ((spkrAvail >= (byteSize * 6)) && (micAvail >= byteSize)) {
    if ((spkrAvail >= (_echoDelay+byteSize)) && (micAvail >= byteSize)) {

    	int nbSamples = byteSize / sizeof(SFLDataFormat);

        // get synchronized data
        _spkrData->Get (_tmpSpkr, byteSize);
        _micData->Get (_tmpMic, byteSize);

#ifdef DUMP_ECHOCANCEL_INTERNAL_DATA
        micProcessFile->write(reinterpret_cast<char *>(_tmpMic), byteSize);
        spkrProcessFile->write(reinterpret_cast<char *>(_tmpSpkr), byteSize);
#endif

        int32_t tmp;
        for(int i = 0; i < nbSamples; i++) {
        	tmp = _tmpSpkr[i] * 3;
        	if(tmp > SHRT_MAX) {
        		tmp = SHRT_MAX;
        	}
        	_tmpSpkr[i] = (int16_t)tmp;

        	_tmpMic[i] /= 3;
        }


        // Processed echo cancellation
        speex_echo_cancellation (_echoState, _tmpMic, _tmpSpkr, _tmpOut);
        speex_preprocess_run(_preState, reinterpret_cast<short *>(_tmpOut));

#ifdef DUMP_ECHOCANCEL_INTERNAL_DATA
        echoFile->write(reinterpret_cast<char *>(_tmpOut), byteSize);
#endif

        for(int i = 0; i < nbSamples; i++) {
        	_tmpOut[i] *= 3;
        }

        memcpy (outputData, _tmpOut, byteSize);

        spkrAvail = _spkrData->AvailForGet();
        micAvail = _micData->AvailForGet();

        // increment nb of frame processed
        ++nbFrame;
    }
    else {
    	_debug("discarding");
    	_micData->Discard(byteSize);
    }

    return nbFrame * EC_FRAME_SIZE * sizeof(SFLDataFormat);
}
