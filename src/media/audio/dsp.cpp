/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
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
#include "dsp.h"
#include "audiobuffer.h"

namespace jami {

void
DSP::speexStateDeleter(SpeexPreprocessState *state)
{
    speex_preprocess_state_destroy(state);
}

DSP::DSP(int smplPerFrame, int channels, int samplingRate) :
    smplPerFrame_(smplPerFrame),
    dspStates_()
{
    for (int c = 0; c < channels; ++c)
        dspStates_.push_back(
                {speex_preprocess_state_init(smplPerFrame_, samplingRate),
                 speexStateDeleter});
}

void DSP::enableAGC()
{
    // automatic gain control, range [1-32768]
    for (const auto &state : dspStates_) {
        int enable = 1;
        speex_preprocess_ctl(state.get(), SPEEX_PREPROCESS_SET_AGC, &enable);
        int target = 16000;
        speex_preprocess_ctl(state.get(), SPEEX_PREPROCESS_SET_AGC_TARGET, &target);
    }
}

void DSP::disableAGC()
{
    for (const auto &state : dspStates_) {
        int enable = 0;
        speex_preprocess_ctl(state.get(), SPEEX_PREPROCESS_SET_AGC, &enable);
    }
}

void DSP::enableDenoise()
{
    for (const auto &state : dspStates_) {
        int enable = 1;
        speex_preprocess_ctl(state.get(), SPEEX_PREPROCESS_SET_DENOISE, &enable);
    }
}

void DSP::disableDenoise()
{
    for (const auto &state : dspStates_) {
        int enable = 0;
        speex_preprocess_ctl(state.get(), SPEEX_PREPROCESS_SET_DENOISE, &enable);
    }
}

void DSP::process(AudioBuffer& buff, int samples)
{
    if (samples != smplPerFrame_) {
        JAMI_WARN("Unexpected amount of samples");
        return;
    }

    auto &channelData = buff.getData();
    size_t index = 0;
    for (auto &c : channelData) {
        if (index < dspStates_.size() and dspStates_[index].get())
            speex_preprocess_run(dspStates_[index].get(), c.data());
        ++index;
    }
}

} // namespace jami
