/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef _AUDIO_DEVICE_H
#define _AUDIO_DEVICE_H

#include <string>

#define AUDIODEVICERATE 8000

/**
 * @file audiodevice.h
 * @brief Container device for attribute storage
 * Have almost only get/set method
 */
class AudioDevice {
public:
  /**
   * Constructor
   * @param id Identifier
   * @param name Name
   */
  AudioDevice(int id, const std::string& name);
  
  /**
   * Destructor
   */
  ~AudioDevice();

  /** Default sample rate */
  const static double DEFAULT_RATE;

  /**
   * Read accessor to the ID
   * @return int	The ID of the audiodevice
   */
  int getId() { return _id; }
  
  /**
   * Read accessor to the name
   * @return std::string&  A string description
   */
  const std::string& getName() {return _name; }

  /**
   * Write accessor to the sample rate
   * @param rate  The sample rate
   */
  void setRate(double rate) { _rate = rate;}
  
  /**
   * Read accessor to the sample rate
   * @return double The sample rate
   */
  double getRate() { return _rate; }

private:

  /** Integer id of the device, can not be 0 */
  int _id;

  /** Name of the device */
  std::string _name;
  
  /** Default rate in Hz, like 8000.0, default is AudioDevice::DEFAULT_RATE */
  double _rate;
};

#endif // _AUDIO_DEVICE_H_

