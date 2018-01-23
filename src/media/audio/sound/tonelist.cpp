/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
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

namespace ring {

static const char *toneZone[TelephoneTone::ZID_COUNTRIES][Tone::TONE_NULL] = {
    {
        // ZID_NORTH_AMERICA
        "350+440",                // Tone::TONE_DIALTONE
        "480+620/500,0/500",      // Tone::TONE_BUSY
        "440+480/2000,0/4000",    // Tone::TONE_RINGTONE
        "480+620/250,0/250",      // Tone::TONE_CONGESTION
    },
    {
        //ZID_FRANCE
        "440",
        "440/500,0/500",
        "440/1500,0/3500",
        "440/250,0/250",
    },
    {
        //ZID_AUSTRALIA
        "413+438",
        "425/375,0/375",
        "413+438/400,0/200,413+438/400,0/2000",
        "425/375,0/375,420/375,8/375",
    },
    {
        //ZID_UNITED_KINGDOM
        "350+440",
        "400/375,0/375",
        "400+450/400,0/200,400+450/400,0/2000",
        "400/400,0/350,400/225,0/525",
    },
    {
        //ZID_SPAIN
        "425",
        "425/200,0/200",
        "425/1500,0/3000",
        "425/200,0/200,425/200,0/200,425/200,0/600",
    },
    {
        //ZID_ITALY
        "425/600,0/1000,425/200,0/200",
        "425/500,0/500",
        "425/1000,0/4000",
        "425/200,0/200",
    },
    {
        //ZID_JAPAN
        "400",
        "400/500,0/500",
        "400+15/1000,0/2000",
        "400/500,0/500",
    }
};


TelephoneTone::COUNTRYID
TelephoneTone::getCountryId(const std::string& countryName)
{
    if (countryName == "North America")         return ZID_NORTH_AMERICA;
    else if (countryName == "France")           return ZID_FRANCE;
    else if (countryName == "Australia")        return ZID_AUSTRALIA;
    else if (countryName == "United Kingdom")   return ZID_UNITED_KINGDOM;
    else if (countryName == "Spain")            return ZID_SPAIN;
    else if (countryName == "Italy")            return ZID_ITALY;
    else if (countryName == "Japan")            return ZID_JAPAN;
    else                                        return ZID_NORTH_AMERICA; // default
}

TelephoneTone::TelephoneTone(const std::string& countryName, unsigned int sampleRate)
    : countryId_(getCountryId(countryName))
    , currentTone_(Tone::TONE_NULL)
{
    buildTones(sampleRate);
}

void
TelephoneTone::setCurrentTone(Tone::TONEID toneId)
{
    if (toneId != Tone::TONE_NULL && currentTone_ != toneId)
        tones_[toneId].reset();

    currentTone_ = toneId;
}

void
TelephoneTone::setSampleRate(unsigned int sampleRate)
{
    buildTones(sampleRate);
}

Tone*
TelephoneTone::getCurrentTone()
{
    if (currentTone_ < Tone::TONE_DIALTONE or currentTone_ >= Tone::TONE_NULL)
        return nullptr;

    return &tones_[currentTone_];
}

void
TelephoneTone::buildTones(unsigned int sampleRate)
{
    tones_[Tone::TONE_DIALTONE] = Tone(toneZone[countryId_][Tone::TONE_DIALTONE], sampleRate);
    tones_[Tone::TONE_BUSY] = Tone(toneZone[countryId_][Tone::TONE_BUSY], sampleRate);
    tones_[Tone::TONE_RINGTONE] = Tone(toneZone[countryId_][Tone::TONE_RINGTONE], sampleRate);
    tones_[Tone::TONE_CONGESTION] = Tone(toneZone[countryId_][Tone::TONE_CONGESTION], sampleRate);
}

} // namespace ring
