/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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



#include "delaydetection.h"
#include "math.h"
#include <string.h>
#include <samplerate.h>

namespace {
// decimation filter coefficient
const float decimationCoefs[] = {-0.09870257, 0.07473655, 0.05616626, 0.04448337, 0.03630817, 0.02944626,
                           0.02244098, 0.01463477, 0.00610982, -0.00266367, -0.01120109, -0.01873722,
                           -0.02373243, -0.02602213, -0.02437806, -0.01869834, -0.00875287, 0.00500204,
                           0.02183252, 0.04065763, 0.06015944, 0.0788299, 0.09518543, 0.10799179,
                           0.1160644,  0.12889288, 0.1160644, 0.10799179, 0.09518543, 0.0788299,
                           0.06015944, 0.04065763, 0.02183252, 0.00500204, -0.00875287, -0.01869834,
                           -0.02437806, -0.02602213, -0.02373243, -0.01873722, -0.01120109, -0.00266367,
                           0.00610982, 0.01463477, 0.02244098, 0.02944626, 0.03630817, 0.04448337,
                           0.05616626,  0.07473655, -0.09870257
                          };
std::vector<double> ird(decimationCoefs, decimationCoefs + sizeof(decimationCoefs) /sizeof(float));


// decimation filter coefficient
const float bandpassCoefs[] = {0.06278034, -0.0758545, -0.02274943, -0.0084497, 0.0702427, 0.05986113,
                         0.06436469, -0.02412049, -0.03433526, -0.07568665, -0.03214543, -0.07236507,
                         -0.06979052, -0.12446371, -0.05530828, 0.00947243, 0.15294699, 0.17735563,
                         0.15294699, 0.00947243, -0.05530828, -0.12446371, -0.06979052, -0.07236507,
                         -0.03214543, -0.07568665, -0.03433526, -0.02412049,  0.06436469, 0.05986113,
                         0.0702427, -0.0084497, -0.02274943, -0.0758545, 0.06278034
                        };
std::vector<double> irb(bandpassCoefs, bandpassCoefs + sizeof(bandpassCoefs) / sizeof(float));
} // end anonymous namespace


FirFilter::FirFilter(const std::vector<double> &ir) : length_(ir.size()),
    impulseResponse_(ir),
    counter_(0)
{
    memset(taps_, 0, sizeof(double) * MAXFILTERSIZE);
}

float FirFilter::getOutputSample(float inputSample)
{
    taps_[counter_] = inputSample;
    double result = 0.0;
    int index = counter_;

    for (int i = 0; i < length_; ++i) {
        result = result + impulseResponse_[i] * taps_[index--];

        if (index < 0)
            index = length_ - 1;
    }

    counter_++;

    if (counter_ >= length_)
        counter_ = 0;

    return result;
}

void FirFilter::reset()
{
    for (int i = 0; i < length_; ++i)
        impulseResponse_[i] = 0.0;
}


DelayDetection::DelayDetection() :
    internalState_(WaitForSpeaker), decimationFilter_(ird),
    bandpassFilter_(irb), segmentSize_(DELAY_BUFF_SIZE),
    downsamplingFactor_(8),
    spkrDownSize_(DELAY_BUFF_SIZE / downsamplingFactor_),
    micDownSize_(WINDOW_SIZE / downsamplingFactor_),
    nbMicSampleStored_(0),
    nbSpkrSampleStored_(0)
{
    memset(spkrReference_, 0, sizeof(float) *WINDOW_SIZE*2);
    memset(capturedData_, 0, sizeof(float) *DELAY_BUFF_SIZE*2);
    memset(spkrReferenceDown_, 0, sizeof(float) *WINDOW_SIZE*2);
    memset(captureDataDown_, 0, sizeof(float) *DELAY_BUFF_SIZE*2);
    memset(spkrReferenceFilter_, 0, sizeof(float) *WINDOW_SIZE*2);
    memset(captureDataFilter_, 0, sizeof(float) *DELAY_BUFF_SIZE*2);
    memset(correlationResult_, 0, sizeof(float) *DELAY_BUFF_SIZE*2);

}

void DelayDetection::putData(SFLDataFormat *inputData, int nbSamples)
{
    // Machine may already got a spkr and is waiting for mic or computing correlation
    if (nbSpkrSampleStored_ == WINDOW_SIZE)
        return;

    if ((nbSpkrSampleStored_ + nbSamples) > WINDOW_SIZE)
        nbSamples = WINDOW_SIZE - nbSpkrSampleStored_;

    if (nbSamples) {

        float tmp[nbSamples];
        float down[nbSamples];

        convertInt16ToFloat32(inputData, tmp, nbSamples);
        memcpy(spkrReference_ + nbSpkrSampleStored_, tmp, nbSamples * sizeof(float));

        downsampleData(tmp, down, nbSamples, downsamplingFactor_);
        bandpassFilter(down, nbSamples / downsamplingFactor_);
        memcpy(spkrReferenceDown_+ (nbSpkrSampleStored_ / downsamplingFactor_), down, (nbSamples / downsamplingFactor_) * sizeof(float));

        nbSpkrSampleStored_ += nbSamples;
    }

    // Update the state
    internalState_ = WaitForMic;
}

void DelayDetection::process(SFLDataFormat *inputData, int nbSamples)
{

    if (internalState_ != WaitForMic)
        return;

    if ((nbMicSampleStored_ + nbSamples) > DELAY_BUFF_SIZE)
        nbSamples = DELAY_BUFF_SIZE - nbMicSampleStored_;

    if (nbSamples) {
        float tmp[nbSamples];
        float down[nbSamples];

        convertInt16ToFloat32(inputData, tmp, nbSamples);
        memcpy(capturedData_ + nbMicSampleStored_, tmp, nbSamples);

        downsampleData(tmp, down, nbSamples, downsamplingFactor_);

        memcpy(captureDataDown_ + (nbMicSampleStored_ / downsamplingFactor_), down, (nbSamples / downsamplingFactor_) * sizeof(float));

        nbMicSampleStored_ += nbSamples;
    }

    if (nbMicSampleStored_ == DELAY_BUFF_SIZE)
        internalState_ = ComputeCorrelation;
    else
        return;

    crossCorrelate(spkrReferenceDown_, captureDataDown_, correlationResult_, micDownSize_, spkrDownSize_);
}

void DelayDetection::crossCorrelate(float *ref, float *seg, float *res, int refSize, int segSize)
{
    // Output has same size as the
    int rsize = refSize;
    int ssize = segSize;
    int tmpsize = segSize - refSize + 1;

    // perform autocorrelation on reference signal
    float acref = correlate(ref, ref, rsize);

    // perform crossrelation on signal
    float acseg = 0.0;
    float r;

    while (--tmpsize) {
        --ssize;
        acseg = correlate(seg+tmpsize, seg+tmpsize, rsize);
        res[ssize] = correlate(ref, seg+tmpsize, rsize);
        r = sqrt(acref*acseg);

        if (r < 0.0000001)
            res[ssize] = 0.0;
        else
            res[ssize] = res[ssize] / r;
    }

    // perform crosscorrelation on zerro padded region
    int i = 0;

    while (rsize) {
        acseg = correlate(seg, seg, rsize);
        res[ssize - 1] = correlate(ref + i, seg, rsize);
        r = sqrt(acref * acseg);

        if (r < 0.0001)
            res[ssize - 1] = 0.0;
        else
            res[ssize - 1] = res[ssize-1] / r;

        --rsize;
        --ssize;
        ++i;
    }
}

double DelayDetection::correlate(float *sig1, float *sig2, short size)
{
    short s = size;

    double ac = 0.0;

    while (s--)
        ac += sig1[s] * sig2[s];

    return ac;
}


void DelayDetection::convertInt16ToFloat32(SFLDataFormat *input, float *output, int nbSamples)
{
    static const float S2F_FACTOR = .000030517578125f;
    int len = nbSamples;

    while (len) {
        len--;
        output[len] = (float) input[len] * S2F_FACTOR;
    }
}


void DelayDetection::downsampleData(float *input, float *output, int nbSamples, int factor)
{
    int src_err;
    SRC_STATE *src_state  = src_new(SRC_LINEAR, 1, &src_err);

    double downfactor = 1.0 / (double) factor;

    if (downfactor != 1.0) {
        SRC_DATA src_data;
        src_data.data_in = input;
        src_data.data_out = output;
        src_data.input_frames = nbSamples;
        src_data.output_frames = nbSamples / factor;
        src_data.src_ratio = downfactor;
        src_data.end_of_input = 0; // More data will come

        src_process(src_state, &src_data);
    }
}


void DelayDetection::bandpassFilter(float *input, int nbSamples)
{
    for (int i = 0; i < nbSamples; ++i)
        input[i] = bandpassFilter_.getOutputSample(input[i]);
}


int DelayDetection::getMaxIndex(float *data, int size)
{
    float max = 0.0;
    int k = 0;

    for (int i = 0; i < size; i++)
        if (data[i] >= max) {
            max = data[i];
            k = i;
        }

    return k;
}
