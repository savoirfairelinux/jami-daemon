#ifndef GAINCONTROL_H
#define GAINCONTROL_H

#include "global.h"

#define SFL_GAIN_BUFFER_LENGTH 160

class GainControl {

public:
    GainControl(double);
    ~GainControl(void);

    void process(SFLDataFormat *, SFLDataFormat *, int);

private:

    class RmsDetection {
    public:
	/**
	 * Constructor for this class
         */
	RmsDetection(void);

        /**
	 * Get rms value
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
    	Limiter(double, double);

        double limit(double);

    private:
	double ratio;
	double threshold;
    };

    RmsDetection detector;

    DetectionAverage averager;

    Limiter limiter;
};

#endif // GAINCONTROL_H
