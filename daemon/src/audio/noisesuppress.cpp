/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "logger.h"
#include "noisesuppress.h"

NoiseSuppress::NoiseSuppress(int smplPerFrame, int channels, int samplingRate) :
    smplPerFrame_(smplPerFrame),
    noiseStates_(channels, nullptr)
{
    for (auto &state : noiseStates_) {
        state = speex_preprocess_state_init(smplPerFrame_, samplingRate);

        int i = 1;
        speex_preprocess_ctl(state, SPEEX_PREPROCESS_SET_DENOISE, &i);
    }
}


NoiseSuppress::~NoiseSuppress()
{
    for (auto state : noiseStates_)
        speex_preprocess_state_destroy(state);
}

void NoiseSuppress::process(AudioBuffer& buff, int samples)
{
    if (samples != smplPerFrame_) {
        WARN("Unexpected amount of samples");
        return;
    }

    auto &channelData = buff.getData();
    size_t index = 0;
    for (auto &c : channelData) {
        if (index < noiseStates_.size() and noiseStates_[index])
            speex_preprocess_run(noiseStates_[index], c.data());
        ++index;
    }
}
