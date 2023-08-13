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
#include "tonelist.h"

#include <ciso646> // fix windows compiler bug

namespace jami {

TelephoneTone::CountryId
TelephoneTone::getCountryId(const std::string& countryName)
{
    if (countryName == "North America")
        return CountryId::ZID_NORTH_AMERICA;
    else if (countryName == "France")
        return CountryId::ZID_FRANCE;
    else if (countryName == "Australia")
        return CountryId::ZID_AUSTRALIA;
    else if (countryName == "United Kingdom")
        return CountryId::ZID_UNITED_KINGDOM;
    else if (countryName == "Spain")
        return CountryId::ZID_SPAIN;
    else if (countryName == "Italy")
        return CountryId::ZID_ITALY;
    else if (countryName == "Japan")
        return CountryId::ZID_JAPAN;
    else
        return CountryId::ZID_NORTH_AMERICA; // default
}

TelephoneTone::TelephoneTone(const std::string& countryName, unsigned int sampleRate, AVSampleFormat sampleFormat)
    : countryId_(getCountryId(countryName))
    , currentTone_(Tone::ToneId::TONE_NULL)
{
    buildTones(sampleRate, sampleFormat);
}

void
TelephoneTone::setCurrentTone(Tone::ToneId toneId)
{
    if (toneId != Tone::ToneId::TONE_NULL && currentTone_ != toneId)
        tones_[(size_t) toneId]->reset();

    currentTone_ = toneId;
}

void
TelephoneTone::setSampleRate(unsigned int sampleRate, AVSampleFormat sampleFormat)
{
    buildTones(sampleRate, sampleFormat);
}

std::shared_ptr<Tone>
TelephoneTone::getCurrentTone()
{
    if (currentTone_ < Tone::ToneId::DIALTONE or currentTone_ >= Tone::ToneId::TONE_NULL)
        return nullptr;

    return tones_[(size_t) currentTone_];
}

void
TelephoneTone::buildTones(unsigned int sampleRate, AVSampleFormat sampleFormat)
{
    const char* toneZone[(size_t) TelephoneTone::CountryId::ZID_COUNTRIES]
                        [(size_t) Tone::ToneId::TONE_NULL]
        = {{
               // ZID_NORTH_AMERICA
               "350+440",             // Tone::TONE_DIALTONE
               "480+620/500,0/500",   // Tone::TONE_BUSY
               "440+480/2000,0/4000", // Tone::TONE_RINGTONE
               "480+620/250,0/250",   // Tone::TONE_CONGESTION
           },
           {
               // ZID_FRANCE
               "440",
               "440/500,0/500",
               "440/1500,0/3500",
               "440/250,0/250",
           },
           {
               // ZID_AUSTRALIA
               "413+438",
               "425/375,0/375",
               "413+438/400,0/200,413+438/400,0/2000",
               "425/375,0/375,420/375,8/375",
           },
           {
               // ZID_UNITED_KINGDOM
               "350+440",
               "400/375,0/375",
               "400+450/400,0/200,400+450/400,0/2000",
               "400/400,0/350,400/225,0/525",
           },
           {
               // ZID_SPAIN
               "425",
               "425/200,0/200",
               "425/1500,0/3000",
               "425/200,0/200,425/200,0/200,425/200,0/600",
           },
           {
               // ZID_ITALY
               "425/600,0/1000,425/200,0/200",
               "425/500,0/500",
               "425/1000,0/4000",
               "425/200,0/200",
           },
           {
               // ZID_JAPAN
               "400",
               "400/500,0/500",
               "400+15/1000,0/2000",
               "400/500,0/500",
           }};
    tones_[(size_t) Tone::ToneId::DIALTONE]
        = std::make_shared<Tone>(toneZone[(size_t) countryId_][(size_t) Tone::ToneId::DIALTONE],
                                 sampleRate, sampleFormat);
    tones_[(size_t) Tone::ToneId::BUSY]
        = std::make_shared<Tone>(toneZone[(size_t) countryId_][(size_t) Tone::ToneId::BUSY],
                                 sampleRate, sampleFormat);
    tones_[(size_t) Tone::ToneId::RINGTONE]
        = std::make_shared<Tone>(toneZone[(size_t) countryId_][(size_t) Tone::ToneId::RINGTONE],
                                 sampleRate, sampleFormat);
    tones_[(size_t) Tone::ToneId::CONGESTION]
        = std::make_shared<Tone>(toneZone[(size_t) countryId_][(size_t) Tone::ToneId::CONGESTION],
                                 sampleRate, sampleFormat);
}

} // namespace jami
