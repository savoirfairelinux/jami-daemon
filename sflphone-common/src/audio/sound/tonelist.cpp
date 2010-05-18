/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "tonelist.h"

ToneList::ToneList() : _nbTone (TONE_NBTONE) ,
        _nbCountry (TONE_NBCOUNTRY),
        _defaultCountryId (ZID_NORTH_AMERICA)
{
    initToneDefinition();
}

ToneList::~ToneList()
{
}

void
ToneList::initToneDefinition()
{
    _toneZone[ZID_NORTH_AMERICA][Tone::TONE_DIALTONE] = "350+440";
    _toneZone[ZID_NORTH_AMERICA][Tone::TONE_BUSY] = "480+620/500,0/500";
    _toneZone[ZID_NORTH_AMERICA][Tone::TONE_RINGTONE] = "440+480/2000,0/4000";
    _toneZone[ZID_NORTH_AMERICA][Tone::TONE_CONGESTION] = "480+620/250,0/250";

    _toneZone[ZID_FRANCE][Tone::TONE_DIALTONE] = "440";
    _toneZone[ZID_FRANCE][Tone::TONE_BUSY] = "440/500,0/500";
    _toneZone[ZID_FRANCE][Tone::TONE_RINGTONE] = "440/1500,0/3500";
    _toneZone[ZID_FRANCE][Tone::TONE_CONGESTION] = "440/250,0/250";

    _toneZone[ZID_AUSTRALIA][Tone::TONE_DIALTONE] = "413+438";
    _toneZone[ZID_AUSTRALIA][Tone::TONE_BUSY] = "425/375,0/375";
    _toneZone[ZID_AUSTRALIA][Tone::TONE_RINGTONE] =
        "413+438/400,0/200,413+438/400,0/2000";
    _toneZone[ZID_AUSTRALIA][Tone::TONE_CONGESTION] = "425/375,0/375,420/375,8/375";

    _toneZone[ZID_UNITED_KINGDOM][Tone::TONE_DIALTONE] = "350+440";
    _toneZone[ZID_UNITED_KINGDOM][Tone::TONE_BUSY] = "400/375,0/375";
    _toneZone[ZID_UNITED_KINGDOM][Tone::TONE_RINGTONE] =
        "400+450/400,0/200,400+450/400,0/2000";
    _toneZone[ZID_UNITED_KINGDOM][Tone::TONE_CONGESTION] =
        "400/400,0/350,400/225,0/525";

    _toneZone[ZID_SPAIN][Tone::TONE_DIALTONE] = "425";
    _toneZone[ZID_SPAIN][Tone::TONE_BUSY] = "425/200,0/200";
    _toneZone[ZID_SPAIN][Tone::TONE_RINGTONE] = "425/1500,0/3000";
    _toneZone[ZID_SPAIN][Tone::TONE_CONGESTION] =
        "425/200,0/200,425/200,0/200,425/200,0/600";

    _toneZone[ZID_ITALY][Tone::TONE_DIALTONE] = "425/600,0/1000,425/200,0/200";
    _toneZone[ZID_ITALY][Tone::TONE_BUSY] = "425/500,0/500";
    _toneZone[ZID_ITALY][Tone::TONE_RINGTONE] = "425/1000,0/4000";
    _toneZone[ZID_ITALY][Tone::TONE_CONGESTION] = "425/200,0/200";

    _toneZone[ZID_JAPAN][Tone::TONE_DIALTONE] = "400";
    _toneZone[ZID_JAPAN][Tone::TONE_BUSY] = "400/500,0/500";
    _toneZone[ZID_JAPAN][Tone::TONE_RINGTONE] = "400+15/1000,0/2000";
    _toneZone[ZID_JAPAN][Tone::TONE_CONGESTION] = "400/500,0/500";
}

std::string
ToneList::getDefinition (COUNTRYID countryId, Tone::TONEID toneId)
{
    if (toneId == Tone::TONE_NULL) {
        return "";
    }

    return _toneZone[countryId][toneId];
}

ToneList::COUNTRYID
ToneList::getCountryId (const std::string& countryName)
{
    if (countryName.compare ("North America") == 0) {
        return ZID_NORTH_AMERICA;
    } else if (countryName.compare ("France") == 0) {
        return ZID_FRANCE;
    } else if (countryName.compare ("Australia") == 0) {
        return ZID_AUSTRALIA;
    } else if (countryName.compare ("United Kingdom") == 0) {
        return ZID_UNITED_KINGDOM;
    } else if (countryName.compare ("Spain") == 0) {
        return ZID_SPAIN;
    } else if (countryName.compare ("Italy") == 0) {
        return ZID_ITALY;
    } else if (countryName.compare ("Japan") == 0) {
        return ZID_JAPAN;
    } else {
        return _defaultCountryId; // default, we don't want segmentation fault
    }
}

TelephoneTone::TelephoneTone (const std::string& countryName, unsigned int sampleRate) :
        _currentTone (Tone::TONE_NULL),
        _toneList()
{
    ToneList::COUNTRYID countryId = _toneList.getCountryId (countryName);
    _tone[Tone::TONE_DIALTONE] = new Tone (_toneList.getDefinition (countryId, Tone::TONE_DIALTONE), sampleRate);
    _tone[Tone::TONE_BUSY] = new Tone (_toneList.getDefinition (countryId, Tone::TONE_BUSY), sampleRate);
    _tone[Tone::TONE_RINGTONE] = new Tone (_toneList.getDefinition (countryId, Tone::TONE_RINGTONE), sampleRate);
    _tone[Tone::TONE_CONGESTION] = new Tone (_toneList.getDefinition (countryId, Tone::TONE_CONGESTION), sampleRate);

}

TelephoneTone::~TelephoneTone()
{
    for (int i=0; i<_toneList.getNbTone(); i++) {
        delete _tone[i];
        _tone[i] = 0;
    }
}

void
TelephoneTone::setCurrentTone (Tone::TONEID toneId)
{
    if (toneId != Tone::TONE_NULL && _currentTone != toneId) {
        _tone[toneId]->reset();
    }

    _currentTone = toneId;
}

Tone*
TelephoneTone::getCurrentTone()
{
    if (_currentTone == Tone::TONE_NULL) {
        return 0;
    }

    return _tone[_currentTone];
}

bool
TelephoneTone::shouldPlay()
{
    return ( (_currentTone != Tone::TONE_NULL) ? true : false);
}


