#include <math.h>
#include <limits.h>
#include <fstream>
#include <iostream>

#include "global.h"
#include "gaincontrol.h"

#define SFL_GAIN_ATTACK_RELEASE_TIME 10

#define SFL_GAIN_LIMITER_RATIO 0.1
#define SFL_GAIN_LIMITER_THRESHOLD 0.6

#define SFL_GAIN_LOGe10  2.30258509299404568402

GainControl::GainControl(double sr, double target) : averager(sr, SFL_GAIN_ATTACK_RELEASE_TIME)
				    , limiter(SFL_GAIN_LIMITER_RATIO, SFL_GAIN_LIMITER_THRESHOLD)
				    , targetGaindB(target)
				    , targetGainLinear(0.0)
{
    targetGainLinear = exp(targetGaindB * 0.05 * SFL_GAIN_LOGe10);

    _debug("GainControl: Target gain %d dB (%d linear)", targetGaindB, targetGainLinear);
}

GainControl::~GainControl() {}

#ifdef DUMP_GAIN_CONTROL_SIGNAL
std::fstream tmpRms("gaintestrms.raw", std::fstream::out);
std::fstream tmpIn("gaintestin.raw", std::fstream::out);
std::fstream tmpOut("gaintestout.raw", std::fstream::out);
#endif

void GainControl::process(SFLDataFormat *buf, int bufLength) 
{
    double rms, rmsAvg, in, out;

    for(int i = 0; i < bufLength; i++) {
	in = (double)buf[i] / (double)SHRT_MAX;
        rms = detector.getRms(in);
        rmsAvg = sqrt(averager.getAverage(rms));
       
#ifdef DUMP_GAIN_CONTROL_SIGNAL 
	tmpRms.write(reinterpret_cast<char *>(&rmsAvg), sizeof(double));
        tmpIn.write(reinterpret_cast<char *>(&in), sizeof(double));
#endif

        out = limiter.limit(in);

#ifdef DUMP_GAIN_CONTROL_SIGNAL
	tmpRms.write(retinterpret_cast<char *>(&out), sizeof(double));
#endif

        buf[i] = (short)(out * (double)SHRT_MAX);
    }
}

GainControl::RmsDetection::RmsDetection() {}

double GainControl::RmsDetection::getRms(double in) 
{
    return in * in;
}

GainControl::DetectionAverage::DetectionAverage(double sr, double t) : 
			g(0.0), teta(t), samplingRate(sr), previous_y(0.0) 
{
    g = exp(-1.0 / (samplingRate * (teta / 1000.0)));        

    std::cout << "GainControl: g: " << g << ", teta: " << teta << std::endl;
}

double GainControl::DetectionAverage::getAverage(double in)
{
    previous_y = ((1.0 - g) * in) + (g * previous_y);

    return previous_y;
}

GainControl::Limiter::Limiter(double r, double thresh) : ratio(r), threshold(thresh)
{
    std::cout << "GainControl: limiter threshold: " << threshold << std::endl;
}

double GainControl::Limiter::limit(double in)
{
    double out;

    out = (in > threshold ? (ratio * (in - threshold)) + threshold :
    in < -threshold ? (ratio * (in + threshold)) - threshold : in);

    return out;
}
