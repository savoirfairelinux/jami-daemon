/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of 
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
 */
#ifndef __TONELIST_H__
#define __TONELIST_H__

#include "tone.h"

/**
 * @author Yan Morin <yan.morin@savoirfairelinux.com>
 */
class ToneList {
public:
  ToneList();
  ~ToneList();

  enum COUNTRYID {
    ZID_NORTH_AMERICA = 0,
    ZID_FRANCE,
    ZID_AUSTRALIA,
    ZID_UNITED_KINGDOM,
    ZID_SPAIN,
    ZID_ITALY,
    ZID_JAPAN
  };

  /**
    * get the string definition of a tone
    * return the default country or default tone if id are invalid
    * @param country the country Id, see ToneList constructor for the list
    * @param toneId  toneId
    * @return a string definition of the tone
    */
  std::string getDefinition(COUNTRYID countryId, Tone::TONEID toneId);
  /**
   * get the country id associate to a country name
   * return the default country id if not found
   * The default tone/country are set inside the ToneList constructor
   * @param countryName countryName, see the ToneList constructor list
   * @return Country Id or default Id
   */
  COUNTRYID getCountryId(const std::string& countryName);
  int getNbTone() { return _nbTone; }
private:
  void initToneDefinition();
  std::string _toneZone[TONE_NBCOUNTRY][TONE_NBTONE];
  int _nbTone;
  int _nbCountry;
  COUNTRYID _defaultCountryId;
};

/**
 * @author Yan Morin <yan.morin@savoirfairelinux.com>
 */
class TelephoneTone {
public:
  /** Initialize the toneList and set the current tone to null */
  TelephoneTone(const std::string& countryName, unsigned int sampleRate, unsigned int nbChannel);
  ~TelephoneTone();

  /** send TONE::ZT_TONE_NULL to stop the playing */
  void setCurrentTone(Tone::TONEID toneId);

  /** 
    * @return the currentTone after setting it with setCurrentTone 
    *         0 if the current tone is null
    */
  Tone* getCurrentTone();

  /** @return true if you should play the tone (CurrentTone is not NULL) */
  bool shouldPlay();

private:
  Tone* _tone[TONE_NBTONE];
  Tone::TONEID _currentTone;
  ToneList _toneList;
};

#endif
