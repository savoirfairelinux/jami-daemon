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
#include <climits>

#include "speexechocancel.h"
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#include "manager.h"

// number of samples (20 ms)
#define EC_FRAME_SIZE 160
// number of sample to process, (800 à 4000 samples, 100 to 500 ms)
#define EC_FILTER_LENGTH 800

namespace {
const int SPEEX_SAMPLE_RATE = 8000;
const int RINGBUFFER_SIZE = 100000;
}

SpeexEchoCancel::SpeexEchoCancel() :
    echoDelay_(Manager::instance().getEchoCancelDelay() * SPEEX_SAMPLE_RATE / 1000),
    echoTailLength_(Manager::instance().getEchoCancelTailLength() * SPEEX_SAMPLE_RATE / 1000),
    echoState_(speex_echo_state_init(EC_FRAME_SIZE, echoTailLength_)),
    preState_(speex_preprocess_state_init(EC_FRAME_SIZE, SPEEX_SAMPLE_RATE)),
    micData_(new RingBuffer(RINGBUFFER_SIZE)),
    spkrData_(new RingBuffer(RINGBUFFER_SIZE)),
    spkrStopped_(true)
{
    DEBUG("EchoCancel: Initializing echo canceller with delay: %d, filter length: %d, frame size: %d and samplerate %d",
          echoDelay_, echoTailLength_, EC_FRAME_SIZE, SPEEX_SAMPLE_RATE);

    int rate = SPEEX_SAMPLE_RATE;
    speex_echo_ctl(echoState_, SPEEX_ECHO_SET_SAMPLING_RATE, &rate);
    speex_preprocess_ctl(preState_, SPEEX_PREPROCESS_SET_ECHO_STATE, echoState_);

    micData_->createReadPointer();
    spkrData_->createReadPointer();
}

SpeexEchoCancel::~SpeexEchoCancel()
{
    speex_echo_state_destroy(echoState_);
    speex_preprocess_state_destroy(preState_);
    delete spkrData_;
    delete micData_;
}

void SpeexEchoCancel::putData(SFLDataFormat *inputData, int samples)
{
    if (spkrStopped_) {
        micData_->flushAll();
        spkrData_->flushAll();
        spkrStopped_ = false;
    }

    spkrData_->Put(inputData, samples * sizeof(SFLDataFormat));
}

int SpeexEchoCancel::process(SFLDataFormat *inputData, SFLDataFormat *outputData, int samples)
{
    if (spkrStopped_)
        return 0;

    const int byteSize = EC_FRAME_SIZE * sizeof(SFLDataFormat);

    // init temporary buffers
    memset(tmpSpkr_, 0, sizeof(tmpSpkr_));
    memset(tmpMic_, 0, sizeof(tmpMic_));
    memset(tmpOut_, 0, sizeof(tmpOut_));

    // Put mic data in ringbuffer
    micData_->Put(inputData, samples * sizeof(SFLDataFormat));

    // Store data for synchronization
    int spkrAvail = spkrData_->AvailForGet();
    int micAvail = micData_->AvailForGet();

    if (spkrAvail < (echoDelay_+byteSize) || micAvail < byteSize) {
        micData_->Discard(byteSize);
        return 0;
    }

    spkrData_->Get(tmpSpkr_, byteSize);
    micData_->Get(tmpMic_, byteSize);

    for (int i = 0; i < EC_FRAME_SIZE; i++) {
        int32_t tmp = tmpSpkr_[i] * 3;

        if (tmp > SHRT_MAX)
            tmp = SHRT_MAX;

        tmpSpkr_[i] = (int16_t)tmp;

        tmpMic_[i] /= 3;
    }

    speex_echo_cancellation(echoState_, tmpMic_, tmpSpkr_, tmpOut_);
    speex_preprocess_run(preState_, reinterpret_cast<short *>(tmpOut_));

    for (int i = 0; i < EC_FRAME_SIZE; i++)
        tmpOut_[i] *= 3;

    memcpy(outputData, tmpOut_, byteSize);

    spkrAvail = spkrData_->AvailForGet();
    micAvail = micData_->AvailForGet();

    return EC_FRAME_SIZE;
}
