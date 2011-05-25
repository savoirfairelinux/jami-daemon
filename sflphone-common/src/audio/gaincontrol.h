#ifndef GAINCONTROL_H
#define GAINCONTROL_H

#include "global.h"

#define SFL_GAIN_BUFFER_LENGTH 160

class GainControl {

public:
    /**
     * Constructor for the gain controller
     * /param Sampling rate
     * /param Target gain in dB
     */
    GainControl(double, double);

    /**
     * Destructor for this class
     */
    ~GainControl(void);

    /**
     * Apply addaptive gain factor on input signal
     * /param Input audio buffer
     * /param Input buffer length
     */
    void process(SFLDataFormat *, int);

private:

    /**
     * Rms detector
     */
    class RmsDetection {
    public:
	/**
	 * Constructor for this class
         */
	RmsDetection(void);

        /**
	 * Get rms value
         * /param Audio sample
	 */
        double getRms(double);
    
    };

    class DetectionAverage {
    public:
        /**
         * Constructor for this class
         */
	DetectionAverage(double, double);
        
        /**
	 * Process average
	 */
        double getAverage(double);

    private:
        /**
         * Average factor
         */
	double g;

        /**
         * Attack and release ramp time (in ms)
         */
	double teta;

        /**
	 * Samplig rate
	 */
        double samplingRate;

        /**
	 * Previous gain (first order memory)
         */
        double previous_y;
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
	double ratio;
	double threshold;
    };

    RmsDetection detector;

    DetectionAverage averager;

    Limiter limiter;

    double targetGaindB;

    double targetGainLinear;
};

#endif // GAINCONTROL_H
