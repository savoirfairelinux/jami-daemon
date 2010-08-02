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

#include "speexechocancel.h"

#include <fstream>

// number of samples (20 ms)
#define FRAME_SIZE 160
// number of sample to process, (800 à 4000 samples, 100 to 500 ms)
#define FILTER_LENGTH 800


SpeexEchoCancel::SpeexEchoCancel()
{
    _debug ("EchoCancel: Instantiate echo canceller");

    int samplingRate = 8000;

    _echoState = speex_echo_state_init (FRAME_SIZE, FILTER_LENGTH);
    _preState = speex_preprocess_state_init (FRAME_SIZE, samplingRate);

    speex_echo_ctl (_echoState, SPEEX_ECHO_SET_SAMPLING_RATE, &samplingRate);
    speex_preprocess_ctl (_preState, SPEEX_PREPROCESS_SET_ECHO_STATE, _echoState);

    micFile = new ofstream ("micData", ofstream::binary);
    echoFile = new ofstream ("echoData", ofstream::binary);
    spkrFile = new ofstream ("spkrData", ofstream::binary);

    _micData = new RingBuffer (10000);
    _spkrData = new RingBuffer (10000);

    _micData->createReadPointer();
    _spkrData->createReadPointer();

    _spkrStoped = true;
}

SpeexEchoCancel::~SpeexEchoCancel()
{
    _debug ("EchoCancel: Delete echo canceller");

    speex_echo_state_destroy (_echoState);
    _echoState = NULL;

    speex_preprocess_state_destroy (_preState);
    _preState = NULL;

    delete _micData;
    _micData = NULL;

    delete _spkrData;
    _spkrData = NULL;

    micFile->close();
    spkrFile->close();
    echoFile->close();

    delete micFile;
    delete spkrFile;
    delete echoFile;
}

void SpeexEchoCancel::reset()
{

}

void SpeexEchoCancel::putData (SFLDataFormat *inputData, int nbBytes)
{
    // std::cout << "putData nbBytes: " << nbBytes << std::endl;

    if (_spkrStoped) {
        _micData->flushAll();
        _spkrData->flushAll();
        _spkrStoped = false;
    }

    // Put data in speaker ring buffer
    _spkrData->Put (inputData, nbBytes);

    // In case we use libspeex internal buffer
    // (require capture and playback stream to be synchronized)
    // speex_echo_playback(_echoState, inputData);
}

void SpeexEchoCancel::process (SFLDataFormat *data, int nbBytes) {}

int SpeexEchoCancel::process (SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes)
{

    if (_spkrStoped) {
        return 0;
    }

    int byteSize = FRAME_SIZE*2;

    // init temporary buffers
    memset (_tmpSpkr, 0, 5000);
    memset (_tmpMic, 0, 5000);
    memset (_tmpOut, 0, 5000);

    // Put mic data in ringbuffer
    _micData->Put (inputData, nbBytes);

    // Store data for synchronization
    int spkrAvail = _spkrData->AvailForGet();
    int micAvail = _micData->AvailForGet();

    // Init number of frame processed
    int nbFrame = 0;

    // Get data from mic and speaker
    while ( (spkrAvail > byteSize) && (micAvail > byteSize)) {

        // get synchronized data
        _spkrData->Get (_tmpSpkr, byteSize);
        _micData->Get (_tmpMic, byteSize);

        speex_preprocess_run (_preState, _tmpMic);

        micFile->write ( (const char *) _tmpMic, byteSize);
        spkrFile->write ( (const char *) _tmpSpkr, byteSize);

        // Processed echo cancellation
        speex_echo_cancellation (_echoState, _tmpMic, _tmpSpkr, _tmpOut);

        // speex_preprocess_run(_preState, _tmpOut);

        bcopy (_tmpOut, outputData+ (nbFrame*FRAME_SIZE), byteSize);

        echoFile->write ( (const char *) _tmpOut, byteSize);


        spkrAvail = _spkrData->AvailForGet();
        micAvail = _micData->AvailForGet();

        // increment nb of frame processed
        ++nbFrame;
    }

    return nbFrame * FRAME_SIZE;
}

void SpeexEchoCancel::process (SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes)
{


    // speex_echo_cancellation(_echoState, micData, spkrData, outputData);

}
