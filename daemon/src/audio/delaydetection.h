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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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


#ifndef DELAYDETECTION_H
#define DELAYDETECTION_H

#include "sfl_types.h"
#include <vector>

// Template size in samples for correlation
#define WINDOW_SIZE 256

// Segment length in ms for correlation
#define MAX_DELAY 150

// Size of internal buffers in samples
#define  DELAY_BUFF_SIZE MAX_DELAY * 8000 / 1000

#define MAXFILTERSIZE 100

class FirFilter {

    public:
        FirFilter(const std::vector<double> &ir);
        /**
         * Perform filtering on one sample
         */
        float getOutputSample(float inputSample);

        void reset();

    private:

        /**
         * Length of the filter
         */
        int length_;

        /**
         * Coefficient of the filter
         */
        std::vector<double> impulseResponse_;

        /**
         * Circular buffer
         */
        double taps_[MAXFILTERSIZE];
        int counter_;
};


class DelayDetection {
    public:

        DelayDetection();

        ~DelayDetection();

        void putData(SFLAudioSample *inputData, int samples);

        void process(SFLAudioSample *inputData, int samples);

    private:

        enum State {
            WaitForSpeaker,
            WaitForMic,
            ComputeCorrelation
        };


        /**
         * Perform a normalized crosscorrelation between template and segment
         */
        void crossCorrelate(float *ref, float *seg, float *res, int refSize, int segSize);

        /**
         * Perform a correlation on specified signals (mac)
         */
        double correlate(float *sig1, float *sig2, short size);

        void convertInt16ToFloat32(SFLAudioSample *input, float *ouput, int nbSamples);

        void downsampleData(float *input, float *output, int nbSamples, int factor);

        void bandpassFilter(float *input, int nbSamples);

        static int getMaxIndex(float *data, int size);

        State internalState_;

        FirFilter decimationFilter_;

        FirFilter bandpassFilter_;

        /**
         * Segment size in samples for correlation
         */
        short segmentSize_;

        int downsamplingFactor_;

        float spkrReference_[WINDOW_SIZE*2];

        float capturedData_[DELAY_BUFF_SIZE*2];

        float spkrReferenceDown_[WINDOW_SIZE*2];

        float captureDataDown_[DELAY_BUFF_SIZE*2];

        float spkrReferenceFilter_[WINDOW_SIZE*2];

        float captureDataFilter_[DELAY_BUFF_SIZE*2];

        float correlationResult_[DELAY_BUFF_SIZE*2];

        int spkrDownSize_;

        int micDownSize_;

        int nbMicSampleStored_;

        int nbSpkrSampleStored_;

    public:

        friend class DelayDetectionTest;
};

#endif
