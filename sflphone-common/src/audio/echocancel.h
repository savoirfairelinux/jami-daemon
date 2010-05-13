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

// maximum in segment size (segment are SEGMENT_LENGTH long)
#define MAX_DELAY 10

// Internal buffer size
#define BUFF_SIZE 10000

#define DEFAULT_SAMPLRATE 8000

#define DEFAULT_FRAME_LENGTH 20

class EchoCancel : public Algorithm {

 public:

    EchoCancel(int smplRate = DEFAULT_SAMPLRATE, int frameLength = DEFAULT_FRAME_LENGTH);

    ~EchoCancel();

    /**
     * Reset echocanceller internal state at runtime. Usefull when making a new call
     */ 
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

    /**
     * Set echo canceller internal sampling rate, reset if sampling rate changed
     */
    void setSamplingRate(int smplRate);

 private:

    /**
     * Actual method calld to supress echo from mic, micData and spkrData must be synchronized
     */
    void performEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData);

    /**
     * Update speaker level array for both micData and spkrData
     */
    void updateEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData);

    /**
     * Compute the average amplitude of the signal.
     * \param data must be of SEGMENT_LENGTH long.
     */ 
    int computeAmplitudeLevel(SFLDataFormat *data);

    /**
     * Return the max amplitude provided any of _avgSpkrLevelHist or _avgMicLevelHist
     */
    int getMaxAmplitude(int *data);

    /**
     * Apply gain factor on input buffer and copy result in output buffer.
     * Buffers must be of SEGMENT_LENGTH long. 
     * \param input buffer
     * \param output buffer
     */
    void amplifySignal(SFLDataFormat *micData, SFLDataFormat *outputData, float amplify);

    /**
     * Increase microphone gain by the provided factor. Sanity check are done internally.
     */
    void increaseFactor(float factor);

    /**
     * Decrease microphone gain.
     */
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
     * Internal buffer for audio processing
     */
    SFLDataFormat _tmpSpkr[BUFF_SIZE];
    SFLDataFormat _tmpMic[BUFF_SIZE];
    SFLDataFormat _tmpOut[BUFF_SIZE];

    /**
     * Audio stream sampling rate
     */
    int _samplingRate;

    /**
     * Audio frame size in ms
     */
    int _frameLength;

    /**
     * Number of sample per frame
     */
    int _smplPerFrame;

    /**
     * Number of samples per segment
     */
    int _smplPerSeg;
    
    /**
     * Number of segment per frame
     */
    int _nbSegmentPerFrame;

    /**
     * Number of segment considered in history 
     * Mainly used to compute signal level
     */
    int _historyLength;

    /**
     * Current playback level
     */ 
    int _spkrLevel;

    /**
     * Current capture level
     */
    int _micLevel;

    /**
     * Current index to store level in speaker history
     */
    int _spkrHistCnt;

    /**
     * Current index to store level in microphone history
     */
    int _micHistCnt;

    /**
     * Average speaker/microphone level history. Each value corespond to
     * the averaged amplitude value over a segment (SEGMENT_LENGTH long)
     */
    int _avgSpkrLevelHist[BUFF_SIZE];
    int _avgMicLevelHist[BUFF_SIZE];

    /**
     * Current linear gain factor to be applied on microphone 
     */
    float _amplFactor;

    /**
     * Stored linea gain factor for lowpass filtering
     */
    float _lastAmplFactor;

    /**
     * Linear gain factor buffer to adjust to system's latency 
     */
    float _delayedAmplify[MAX_DELAY];

    /**
     * read/write for mic gain delay 
     */
    int _amplIndexIn;
    int _amplIndexOut;

    /*
    ofstream *micFile;
    ofstream *spkrFile;
    ofstream *echoFile;
    */

    /**
     * Noise reduction processing state
     */
    SpeexPreprocessState *_noiseState;
    
};

#endif
