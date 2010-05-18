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

#ifndef __TONELIST_H__
#define __TONELIST_H__

#include "tone.h"

/**
 * @file tonelist.h
 * @brief Manages the different kind of tones according to the country
 */
class ToneList {
public:
  /**
   * Constructor
   */
  ToneList();

  /**
   * Destructor
   */
  ~ToneList();


  /** Countries */ 
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
    * Get the string definition of a tone
    * return the default country or default tone if id are invalid
    * @param countryId	The country Id, see ToneList constructor for the list
    * @param toneId  The toneId
    * @return std::string A string definition of the tone
    */
  std::string getDefinition(COUNTRYID countryId, Tone::TONEID toneId);

  /**
   * Get the country id associate to a country name
   * return the default country id if not found
   * The default tone/country are set inside the ToneList constructor
   * @param countryName countryName, see the ToneList constructor list
   * @return COUNTRYID	Country Id or default Id
   */
  COUNTRYID getCountryId(const std::string& countryName);

  /** @return int The number of tones */
  int getNbTone() { return _nbTone; }

private:

  // Copy Constructor
  ToneList(const ToneList& rh);

  // Assignment Operator
  ToneList& operator=( const ToneList& rh);

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
  TelephoneTone(const std::string& countryName, unsigned int sampleRate);
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
  // Copy Constructor
  TelephoneTone(const TelephoneTone& rh);

  // Assignment Operator
  TelephoneTone& operator=( const TelephoneTone& rh);
  
  Tone* _tone[TONE_NBTONE];
  Tone::TONEID _currentTone;
  ToneList _toneList;
};

#endif
