/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author:  Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author:  Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __CODEC_AUDIO_H__
#define __CODEC_AUDIO_H__

#include <string>

/**
 * Abstract audio codec class.
 * A codec can be decode, encode
 * Each codec have a payload, clock rate and a codec name
 */
class AudioCodec {
public:
  AudioCodec(int payload, const std::string &codecName);
  virtual ~AudioCodec(void);	

  /**
   * @return the number of bytes decoded
   */
  virtual int codecDecode(short *, unsigned char *, unsigned int) = 0;
  virtual int codecEncode(unsigned char *, short *, unsigned int) = 0;

  /** Returns description for GUI usage */
  std::string getDescription() { return _description; }

  /** Value used for SDP negotiation */
  std::string getCodecName() { return _codecName; }
  int getPayload() { return _payload; }
  bool hasDynamicPayload() { return _hasDynamicPayload; }
  unsigned int getClockRate() { return _clockRate; }
  unsigned int getChannel() { return _channel; }
  bool isActive() { return _active; }
  void setActive(bool active) { _active = active; }

protected:
  /** Holds SDP-compliant codec name */
  std::string _codecName; // what we put inside sdp
  /** Holds the GUI-style codec description */
  std::string _description; // what we display to the user

  /**
   * Clock rate or sample rate of the codec, in Hz
   */
  unsigned int _clockRate;

  /**
   * Number of channel 1 = mono, 2 = stereo
   */
  unsigned int _channel;

private:
  bool _active;
  int _payload;
  bool _hasDynamicPayload;
};

#endif // __CODEC_AUDIO_H__
