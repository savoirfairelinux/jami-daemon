/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef GAINCONTROL_H
#define GAINCONTROL_H

#include "global.h"
#include "audiobuffer.h"

class GainControl {

    public:
        /**
         * Constructor for the gain controller
         * /param Sampling rate
         * /param Target gain in dB
         */
        GainControl(double, double);

        /**
         * Apply addaptive gain factor on input signal
         * /param Input audio buffer
         * /param Input samples
         */
        void process(SFLAudioSample *, int samples);
        void process(AudioBuffer& buf);

    private:
        class DetectionAverage {
            public:
                /**
                 * Constructor for this class
                 * /param Sampling rate
                 * /param Attack ramping time
                 * /param Release ramping time
                 */
                DetectionAverage(double, double, double);

                /**
                	 * Process average
                	 */
                double getAverage(double);

            private:
                /**
                 * Average factor for attack
                 */
                double g_a_;

                /**
                 * Attack ramp time (in ms)
                 */
                double teta_a_;

                /**
                 * Average factor for release
                	 */
                double g_r_;

                /**
                 * Release ramp time (in ms)
                 */
                double teta_r_;

                /**
                	 * Samplig rate
                	 */
                double samplingRate_;

                /**
                	 * Previous gain (first order memory)
                 */
                double previous_y_;
        };

        class Limiter {
            public:
                /**
                 * Limiter
                	 * /param Threshold
                	 * /param Ratio
                 */
                Limiter(double, double);

                /**
                 * Perform compression on input signal
                 */
                double limit(double) const;

            private:
                double ratio_;
                double threshold_;
        };

        /**
         * First order mean filter
         */
        DetectionAverage averager_;

        /**
         * Post processing compression
         */
        Limiter limiter_;

        /**
         * Target audio level in dB
         */
        double targetLeveldB_;

        /**
         * Target audio level in linear scale
         */
        double targetLevelLinear_;

        /**
         * Current gain
         */
        double currentGain_;

        /**
         * Previou gain for smoothing
         */
        double previousGain_;

        /**
         * Maximum incrementation stop of current gain
         */
        double maxIncreaseStep_;

        /**
         * Maximum decrease step
         */
        double maxDecreaseStep_;

};

#endif // GAINCONTROL_H
