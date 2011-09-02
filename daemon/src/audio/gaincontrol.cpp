#include <math.h>
#include <limits.h>
#include <fstream>
#include <iostream>

#include "global.h"
#include "gaincontrol.h"

#define SFL_GAIN_ATTACK_TIME 10
#define SFL_GAIN_RELEASE_TIME 300

#define SFL_GAIN_LIMITER_RATIO 0.1
#define SFL_GAIN_LIMITER_THRESHOLD 0.8

#define SFL_GAIN_LOGe10  2.30258509299404568402

// #define DUMP_GAIN_CONTROL_SIGNAL

GainControl::GainControl(double sr, double target) : averager(sr, SFL_GAIN_ATTACK_TIME, SFL_GAIN_RELEASE_TIME)
				    , limiter(SFL_GAIN_LIMITER_RATIO, SFL_GAIN_LIMITER_THRESHOLD)
				    , targetLeveldB(target)
				    , targetLevelLinear(0.0)
				    , currentGain(1.0)
				    , previousGain(0.0)
				    , maxIncreaseStep(0.0)
				    , maxDecreaseStep(0.0)
{
    targetLevelLinear = exp(targetLeveldB * 0.05 * SFL_GAIN_LOGe10);

    maxIncreaseStep = exp(0.11513 * 12. * 160 / 8000); // Computed on 12 frames (240 ms)
    maxDecreaseStep = exp(-0.11513 * 40. * 160 / 8000); // Computed on 40 frames (800 ms)

    _debug("GainControl: Target gain %f dB (%f linear)", targetLeveldB, targetLevelLinear);

}

GainControl::~GainControl() {}

#ifdef DUMP_GAIN_CONTROL_SIGNAL
std::fstream tmpRms("gaintestrms.raw", std::fstream::out);
std::fstream tmpIn("gaintestin.raw", std::fstream::out);
std::fstream tmpOut("gaintestout.raw", std::fstream::out);
#endif

void GainControl::process(SFLDataFormat *buf, int samples)
{
    double rms, rmsAvgLevel, in, out, diffRms, maxRms;

    maxRms = 0.0;
    for(int i = 0; i < samples; i++) {
        // linear conversion
	in = (double)buf[i] / (double)SHRT_MAX;
	
        out = currentGain * in;

	rms = detector.getRms(out);
	rmsAvgLevel = sqrt(averager.getAverage(rms));

#ifdef DUMP_GAIN_CONTROL_SIGNAL 
	tmpRms.write(reinterpret_cast<char *>(&rmsAvgLevel), sizeof(double));
        tmpIn.write(reinterpret_cast<char *>(&in), sizeof(double));
#endif

	if(rmsAvgLevel > maxRms) {
	    maxRms = rmsAvgLevel;
        }

        out = limiter.limit(out);

#ifdef DUMP_GAIN_CONTROL_SIGNAL
	tmpOut.write(reinterpret_cast<char *>(&out), sizeof(double));
#endif

        buf[i] = (short)(out * (double)SHRT_MAX);
    }

    diffRms = maxRms - targetLevelLinear;
    
    if((diffRms > 0.0) && (maxRms > 0.1)) {
	currentGain *= maxDecreaseStep;
    }
    else if((diffRms <= 0.0) && (maxRms > 0.1)) { 
	currentGain *= maxIncreaseStep;
    }
    else if(maxRms <= 0.1) {
	currentGain = 1.0;
    }

    currentGain = 0.5 * (currentGain + previousGain);

    previousGain = currentGain;

    // _debug("GainControl: current gain: %f, target gain: %f, rmsAvgLevel %f, target level %f", 
    //     currentGain, gainTargetLevel, rmsAvgLevel, targetLevelLinear);
}

GainControl::RmsDetection::RmsDetection() {}

double GainControl::RmsDetection::getRms(double in) 
{
    return in * in;
}

GainControl::DetectionAverage::DetectionAverage(double sr, double ta, double tr) : 
			g_a(0.0), teta_a(ta), g_r(0.0), teta_r(tr), samplingRate(sr), previous_y(0.0) 
{
    g_a = exp(-1.0 / (samplingRate * (teta_a / 1000.0)));        
    g_r = exp(-1.0 / (samplingRate * (teta_r / 1000.0)));
}

double GainControl::DetectionAverage::getAverage(double in)
{
    if(in > previous_y) {
        previous_y = ((1.0 - g_a) * in) + (g_a * previous_y);
    }
    else {
	previous_y = ((1.0 - g_r) * in) + (g_r * previous_y);
    }

    return previous_y;
}

GainControl::Limiter::Limiter(double r, double thresh) : ratio(r), threshold(thresh)
{
}

double GainControl::Limiter::limit(double in)
{
    double out;

    out = (in > threshold ? (ratio * (in - threshold)) + threshold :
    in < -threshold ? (ratio * (in + threshold)) - threshold : in);

    return out;
}
