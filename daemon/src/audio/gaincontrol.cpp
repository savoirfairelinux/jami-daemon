#include <cmath>
#include <climits>
#include <fstream>

#include "sfl_types.h"
#include "logger.h"
#include "gaincontrol.h"

#define SFL_GAIN_ATTACK_TIME 10
#define SFL_GAIN_RELEASE_TIME 300

#define SFL_GAIN_LIMITER_RATIO 0.1
#define SFL_GAIN_LIMITER_THRESHOLD 0.8

#define SFL_GAIN_LOGe10  2.30258509299404568402

GainControl::GainControl(double sr, double target) : averager_(sr, SFL_GAIN_ATTACK_TIME, SFL_GAIN_RELEASE_TIME)
    , limiter_(SFL_GAIN_LIMITER_RATIO, SFL_GAIN_LIMITER_THRESHOLD)
    , targetLeveldB_(target)
    , targetLevelLinear_(exp(targetLeveldB_ * 0.05 * SFL_GAIN_LOGe10))
    , currentGain_(1.0)
    , previousGain_(0.0)
    , maxIncreaseStep_(exp(0.11513 * 12. * 160 / 8000)) // Computed on 12 frames (240 ms)
    , maxDecreaseStep_(exp(-0.11513 * 40. * 160 / 8000))// Computed on 40 frames (800 ms)
{
    DEBUG("GainControl: Target gain %f dB (%f linear)", targetLeveldB_, targetLevelLinear_);
}

void GainControl::process(SFLDataFormat *buf, int samples)
{
    double rms, rmsAvgLevel, in, out, diffRms, maxRms;

    maxRms = 0.0;

    for (int i = 0; i < samples; i++) {
        // linear conversion
        in = (double)buf[i] / (double)SHRT_MAX;

        out = currentGain_ * in;

        rms = out * out;
        rmsAvgLevel = sqrt(averager_.getAverage(rms));

        if (rmsAvgLevel > maxRms)
            maxRms = rmsAvgLevel;

        out = limiter_.limit(out);

        buf[i] = (short)(out * (double)SHRT_MAX);
    }

    diffRms = maxRms - targetLevelLinear_;

    if ((diffRms > 0.0) && (maxRms > 0.1))
        currentGain_ *= maxDecreaseStep_;
    else if ((diffRms <= 0.0) && (maxRms > 0.1))
        currentGain_ *= maxIncreaseStep_;
    else if (maxRms <= 0.1)
        currentGain_ = 1.0;

    currentGain_ = 0.5 * (currentGain_ + previousGain_);

    previousGain_ = currentGain_;
}

GainControl::DetectionAverage::DetectionAverage(double sr, double ta, double tr) :
    g_a_(0.0), teta_a_(ta), g_r_(0.0), teta_r_(tr), samplingRate_(sr), previous_y_(0.0)
{
    g_a_ = exp(-1.0 / (samplingRate_ * (teta_a_ / 1000.0)));
    g_r_ = exp(-1.0 / (samplingRate_ * (teta_r_ / 1000.0)));
}

double GainControl::DetectionAverage::getAverage(double in)
{
    if (in > previous_y_)
        previous_y_ = ((1.0 - g_a_) * in) + (g_a_ * previous_y_);
    else
        previous_y_ = ((1.0 - g_r_) * in) + (g_r_ * previous_y_);
    return previous_y_;
}

GainControl::Limiter::Limiter(double r, double thresh) : ratio_(r), threshold_(thresh)
{}

double GainControl::Limiter::limit(double in) const
{
    double out = (in > threshold_ ? (ratio_ * (in - threshold_)) + threshold_ :
           in < -threshold_ ? (ratio_ * (in + threshold_)) - threshold_ : in);

    return out;
}
