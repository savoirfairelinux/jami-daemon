/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#ifndef ECHOCANCEL_H
#define ECHOCANCEL_H

#include <cc++/thread.h>
#include <speex/speex_preprocess.h>

#include "audioprocessing.h"
#include "ringbuffer.h"
#include "delaydetection.h"

// Number of ms in sec
#define MS_PER_SEC 1000

// Length of audio segment in ms
#define SEGMENT_LENGTH 10

// Length of the echo tail in ms
#define ECHO_LENGTH 1000
#define SPKR_LENGTH 80
#define MIC_LENGTH 80

// Voice level threashold 
#define MIN_SIG_LEVEL 250

// Delay between mic and speaker
// #define DELAY_AMPLIFY 60

// maximum in segment size (segment are SEGMENT_LENGTH long)
#define MAX_DELAY_LINE_AMPL 100 // 1 sec

// Internal buffer size
#define BUFF_SIZE 10000

#define DEFAULT_SAMPLRATE 8000

#define DEFAULT_FRAME_LENGTH 20

#define MIC_ADAPT_SIZE 100 // 1 sec
#define SPKR_ADAPT_SIZE 20 // 200 ms

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
     * Get data ready to be played by speakers
     */
    virtual int getData(SFLDataFormat *outputData);

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

    /**
     * Set echo canceller state to active/deactive
     */ 
    void setEchoCancelState(bool state) { _echoActive = state; }

    /**
     * Return the echo canceller state
     */
    bool getEchoCancelState(void) { return _echoActive; }

    /**
     * Set the noise suppression state to active/deactive
     */
    void setNoiseSuppressState(bool state) { _noiseActive = state; }

    /**
     * Return the noise suppression state
     */
    bool getNoiseSuppressState(void) { return _noiseActive; }

 private:

    /**
     * Actual method calld to supress echo from mic, micData and spkrData must be synchronized
     */
    void performEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData, SFLDataFormat *outputData);

    /**
     * This is the fall back method in case there is no spkr data available
     */
    void performEchoCancelNoSpkr(SFLDataFormat *micData, SFLDataFormat *outputData);

    /**
     * Compute current instantaneous microphone signal power and store it in internal array 
     */
    void updateMicLevel(SFLDataFormat *micData);

    /**
     * Compute current instantaneous spkeaker signal power and store uit in internal array
     */
    void updateSpkrLevel(SFLDataFormat *spkrData);

    /**
     * Update speaker level array for both micData and spkrData
     */
    void updateEchoCancel(SFLDataFormat *micData, SFLDataFormat *spkrData);

    /**
     * Compute the average amplitude of the signal.
     * \param data must be of SEGMENT_LENGTH long.
     */ 
    int computeAmplitudeLevel(SFLDataFormat *data, int size);

    /**
     * Compute amplitude signal
     */
    SFLDataFormat estimatePower(SFLDataFormat *data, SFLDataFormat *ampl, int size, SFLDataFormat mem);

    /**
     * Return the max amplitude provided any of _avgSpkrLevelHist or _avgMicLevelHist
     */
    int getMaxAmplitude(int *data, int size);

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
     * Perform simple correlation between data1 and data2
     */
    int performCorrelation(int *data1, int *data2, int size);

    /**
     * Return maximum in data index
     */
    int getMaximumIndex(int *data, int size);

    /**
     * Internal buffer for mic data synchronization
     */
    RingBuffer *_micData;

    /**
     * Internal buffer for speaker data synchronization
     */
    RingBuffer *_spkrData;

    RingBuffer *_spkrDataOut;

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
    int _micHistoryLength;

    int _spkrHistoryLength;

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
    float _delayLineAmplify[MAX_DELAY_LINE_AMPL];

    /**
     * read/write for mic gain delay 
     */
    int _amplDelayIndexIn;
    int _amplDelayIndexOut;

    /**
     * State variable to determine if adaptation must be performed
     */
    bool _adaptDone;

    /**
     * State variable to specify if adaptation is started
     */
    bool _adaptStarted;

    /**
     * Adaptation index
     */
    int _adaptCnt;

    /**
     * Factor for power estimation
     */
    float _alpha;

    /**
     * Termporary spkr level memories
     */
    SFLDataFormat _spkrLevelMem;
    SFLDataFormat _micLevelMem;

    int _spkrAdaptCnt;

    int _micAdaptCnt;
    
    int _spkrAdaptSize;

    int _micAdaptSize;

    int _spkrAdaptArray[BUFF_SIZE];

    int _micAdaptArray[BUFF_SIZE];

    int _correlationSize;

    int _correlationArray[BUFF_SIZE];

    int _processedByte;

    ofstream *micFile;
    ofstream *spkrFile;
    ofstream *echoFile;

    ofstream *micLevelData;
    ofstream *spkrLevelData;

    // #ifdef HAVE_SPEEXDSP_LIB
    /**
     * Noise reduction processing state
     */
    SpeexPreprocessState *_noiseState;
    // #endif

    /**
     * true if noise suppressor is active, false elsewhere
     */
    bool _echoActive;

    /**
     * true if noise suppressor is active, false elsewhere
     */
    bool _noiseActive;

    DelayDetection _delayDetector;

};

#endif
