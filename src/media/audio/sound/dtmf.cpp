/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "dtmf.h"
#include "logger.h"
#include "tone.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <limits>

extern "C" {
#include <libavutil/mathematics.h>
}

namespace jami {
namespace {

struct DtmfTone
{
    char code;
    unsigned lower;
    unsigned higher;
};

constexpr DtmfTone TONES[] = {{'0', 941, 1336},
                              {'1', 697, 1209},
                              {'2', 697, 1336},
                              {'3', 697, 1477},
                              {'4', 770, 1209},
                              {'5', 770, 1336},
                              {'6', 770, 1477},
                              {'7', 852, 1209},
                              {'8', 852, 1336},
                              {'9', 852, 1477},
                              {'A', 697, 1633},
                              {'B', 770, 1633},
                              {'C', 852, 1633},
                              {'D', 941, 1633},
                              {'*', 941, 1209},
                              {'#', 941, 1477}};

} // namespace

std::shared_ptr<AudioFrame>
makeDtmfPulse(char code, unsigned sampleRate, std::chrono::milliseconds duration)
{
    const auto key = std::toupper(static_cast<unsigned char>(code));
    const auto tone = std::ranges::find(TONES, key, &DtmfTone::code);
    if (tone == std::end(TONES)) {
        JAMI_WARNING("Invalid DTMF key: {}", key);
        return {};
    }

    const auto samples = av_rescale_rnd(duration.count(), sampleRate, 1000, AV_ROUND_DOWN);
    if (samples <= 0 || samples > std::numeric_limits<int>::max()) {
        JAMI_WARNING("Invalid DTMF pulse duration: {} ms at {} Hz", duration.count(), sampleRate);
        return {};
    }

    auto pulse = std::make_shared<AudioFrame>(AudioFormat {sampleRate, 1}, samples);
    Tone::genSin(pulse->pointer(), 0, static_cast<unsigned>(samples), tone->lower, tone->higher);
    return pulse;
}

} // namespace jami
