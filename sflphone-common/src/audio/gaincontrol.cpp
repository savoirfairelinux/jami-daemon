#include <math.h>
#include <limits.h>
#include <fstream>
#include <iostream>

#include "global.h"
#include "gaincontrol.h"

#define SFL_GAIN_ATTACK_RELEASE_TIME 10

#define SFL_GAIN_LIMITER_RATIO 0.1
#define SFL_GAIN_LIMITER_THRESHOLD 0.6

GainControl::GainControl(double sr) : averager(sr, SFL_GAIN_ATTACK_RELEASE_TIME)
				    , limiter(SFL_GAIN_LIMITER_RATIO, SFL_GAIN_LIMITER_THRESHOLD) {}

GainControl::~GainControl() {}

std::fstream tmpRms("testrms.raw", std::fstream::out);

void GainControl::process(SFLDataFormat *inBuf, SFLDataFormat *outBuf, int bufLength) 
{
    double rms, rmsAvg, in, out;

    for(int i = 0; i < bufLength; i++) {
	in = (double)inBuf[i] / (double)SHRT_MAX;
        rms = detector.getRms(in);
        rmsAvg = sqrt(averager.getAverage(rms));
        
	tmpRms.write(reinterpret_cast<char *>(&rmsAvg), sizeof(double));

        out = limiter.limit(in);
        outBuf[i] = (short)(out * (double)SHRT_MAX);
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
