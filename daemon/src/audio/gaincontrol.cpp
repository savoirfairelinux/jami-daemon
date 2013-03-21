/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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
    DEBUG("Target gain %f dB (%f linear)", targetLeveldB_, targetLevelLinear_);
}

void GainControl::process(AudioBuffer& buf)
{
    process(buf.getChannel()->data(), buf.samples());
}

void GainControl::process(SFLAudioSample *buf, int samples)
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
