#include <math.h>
#include <limits.h>
#include <fstream>
#include <iostream>

#include "global.h"
#include "gaincontrol.h"

#define SFL_GAIN_ATTACK_TIME 10
#define SFL_GAIN_RELEASE_TIME 300

#define SFL_GAIN_LIMITER_RATIO 0.1
#define SFL_GAIN_LIMITER_THRESHOLD 0.6

#define SFL_GAIN_LOGe10  2.30258509299404568402

#define DUMP_GAIN_CONTROL_SIGNAL

GainControl::GainControl(double sr, double target) : averager(sr, SFL_GAIN_ATTACK_TIME, SFL_GAIN_RELEASE_TIME)
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
	tmpOut.write(reinterpret_cast<char *>(&out), sizeof(double));
#endif

        buf[i] = (short)(out * (double)SHRT_MAX);
    }
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

    std::cout << "GainControl: g_attack: " << g_a << ", teta_attack: " << teta_a 
		<< ", g_release: " << g_r << ", teta_release: " << teta_r << std::endl;
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
    std::cout << "GainControl: limiter threshold: " << threshold << std::endl;
}

double GainControl::Limiter::limit(double in)
{
    double out;

    out = (in > threshold ? (ratio * (in - threshold)) + threshold :
    in < -threshold ? (ratio * (in + threshold)) - threshold : in);

    return out;
}
