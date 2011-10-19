#ifndef GAINCONTROL_H
#define GAINCONTROL_H

#include "global.h"

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
        void process(SFLDataFormat *, int samples);

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
                double limit(double);

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
