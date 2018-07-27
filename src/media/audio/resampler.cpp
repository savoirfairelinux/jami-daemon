/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 */

#include "logger.h"
#include "media_filter.h"
#include "resampler.h"
#include "ring_types.h"

namespace ring {

Resampler::Resampler(AudioFormat format, bool quality)
    : format_(format)
{
    setFormat(format, quality);
}

Resampler::Resampler(unsigned sample_rate, unsigned channels, bool quality)
    : format_(sample_rate, channels)
{
    setFormat(format_, quality);
}

Resampler::~Resampler() = default;

void
Resampler::setFormat(AudioFormat format, bool quality)
{
    format_ = format;
}

void
Resampler::resample(const AudioBuffer &dataIn, AudioBuffer &dataOut)
{
    filter_.reset(new MediaFilter());

    std::stringstream aformat;
    aformat << "aformat=sample_fmts=s16:channel_layout=stereo|mono:sample_rates="
        << dataOut.getSampleRate();

    if (filter_->initialize(aformat.str()) < 0) {
        RING_ERR() << "Failed to initialize resampler";
        return;
    }

    if (filter_->feedInput(dataIn.toAVFrame()) < 0) {
        RING_ERR() << "Failed to resample audio";
        return;
    }

    auto frame = filter_->readOutput();
    if (!frame) {
        RING_ERR() << "Failed to resample audio";
        return;
    }

    // TODO put frame in dataOut
}

} // namespace ring
