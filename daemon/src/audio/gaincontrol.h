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
     * /param Input samples
     */
    void process(SFLDataFormat *, int samples);

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
	double g_a;

        /**
         * Attack ramp time (in ms)
         */
	double teta_a;

        /**
         * Average factor for release
	 */
        double g_r;

        /**
         * Release ramp time (in ms)
         */
        double teta_r;

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

    /**
     * Current audio level detection
     */
    RmsDetection detector;

    /**
     * First order mean filter
     */
    DetectionAverage averager;

    /**
     * Post processing compression
     */
    Limiter limiter;

    /**
     * Target audio level in dB
     */
    double targetLeveldB;

    /**
     * Target audio level in linear scale
     */
    double targetLevelLinear;

    /**
     * Current gain
     */
    double currentGain;

    /** 
     * Previou gain for smoothing
     */
    double previousGain;

    /**
     * Maximum incrementation stop of current gain
     */
    double maxIncreaseStep;

    /**
     * Maximum decrease step
     */ 
    double maxDecreaseStep;

};

#endif // GAINCONTROL_H
