/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
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

#pragma once

#include "tone.h"

#include <string>
#include <array>
#include <memory>

namespace jami {

class TelephoneTone
{
public:
    /** Countries */
    enum class CountryId {
        ZID_NORTH_AMERICA = 0,
        ZID_FRANCE,
        ZID_AUSTRALIA,
        ZID_UNITED_KINGDOM,
        ZID_SPAIN,
        ZID_ITALY,
        ZID_JAPAN,
        ZID_COUNTRIES,
    };

    TelephoneTone(const std::string& countryName, unsigned int sampleRate, AVSampleFormat sampleFormat);

    void setCurrentTone(Tone::ToneId toneId);
    void setSampleRate(unsigned int sampleRate, AVSampleFormat sampleFormat);
    std::shared_ptr<Tone> getCurrentTone();

private:
    NON_COPYABLE(TelephoneTone);

    static CountryId getCountryId(const std::string& countryName);

    void buildTones(unsigned int sampleRate, AVSampleFormat sampleFormat);

    CountryId countryId_;
    std::array<std::shared_ptr<Tone>, (size_t) Tone::ToneId::TONE_NULL> tones_ {};
    Tone::ToneId currentTone_;
};

} // namespace jami
