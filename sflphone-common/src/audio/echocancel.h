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

#ifndef ECHOCANCEL_H
#define ECHOCANCEL_H

#include "audioprocessing.h"
#include <speex/speex_preprocess.h>

#include "ringbuffer.h"

// Number of ms in sec
#define MS_PER_SEC 1000

// Length of audio segment in ms
#define SEGMENT_LENGTH 10

// Length of the echo tail in ms
#define ECHO_LENGTH 100

// Voice level threashold 
#define MIN_SIG_LEVEL 100

// Delay between mic and speaker
#define DELAY_AMPLIFY 60

class EchoCancel : public Algorithm {

 public:

    EchoCancel();

    ~EchoCancel();

    virtual void reset(void);

    /**
     * Add speaker data into internal buffer
     * \param inputData containing far-end voice data to be sent to speakers 
     */
    virtual void putData(SFLDataFormat *inputData, int nbBytes);

    /**
     * Unused
     */
    virtual void process(SFLDataFormat *data, int nbBytes);

    /**
     * Perform echo cancellation using internal buffers
     * \param inputData containing mixed echo and voice data
     * \param outputData containing 
     */
    virtual int process(SFLDataFormat *inputData, SFLDataFormat *outputData, int nbBytes);

    /**
     * Perform echo cancellation, application must provide its own buffer
     * \param micData containing mixed echo and voice data
     * \param spkrData containing far-end voice data to be sent to speakers
     * \param outputData containing the processed data
     */
    virtual void process(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData, int nbBytes);

 private:

    /**
     * Actual method calld to supress echo from mic, micData and spkrData must be synchronized
     */
    void performEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData);

    /**
     * Update speaker level array for both micData and spkrData
     */
    void updateEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData);

    int computeAmplitudeLevel(SFLDataFormat *data);


    int getMaxAmplitude(int *data);

    void amplifySignal(SFLDataFormat *micData, SFLDataFormat *outputData);

    void increaseFactor(float factor);

    void decreaseFactor();

    /**
     * Internal buffer for mic data synchronization
     */
    RingBuffer *_micData;

    /**
     * Internal buffer for speaker data synchronization
     */
    RingBuffer *_spkrData;

    /**
     * Boolean value 
     */
    bool _spkrStoped;

    /**
     * Temp buffer
     */
    SFLDataFormat _tmpSpkr[5000];
    SFLDataFormat _tmpMic[5000];
    SFLDataFormat _tmpOut[5000];


    int _samplingRate;
    int _smplPerFrame;
    int _smplPerSeg;
    int _nbSegment;
    int _historyLength;

    int _spkrLevel;
    int _micLevel;

    int _spkrHistCnt;
    int _micHistCnt;

    int _avgSpkrLevelHist[5000];
    int _avgMicLevelHist[5000];

    float _amplFactor;
    float _amplify;
    float _delayedAmplify[10];
    float _lastAmplFactor;

    int _amplIndexIn;
    int _amplIndexOut;

    ofstream *micFile;
    ofstream *spkrFile;
    ofstream *echoFile;

    SpeexPreprocessState *noiseState;
    
};

#endif
