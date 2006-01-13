/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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

#ifndef __SFLAUDIO_AUDIO_LAYER_HPP__
#define __SFLAUDIO_AUDIO_LAYER_HPP__

#include <list>
#include <string>

namespace SFLAudio 
{
  class Device;
  class Emitter;

  class AudioLayer 
  {
  public:
    /**
     * This will initialize this audio system. The name 
     * given in argument is the name of the system.
     *
     */
    AudioLayer(const std::string &name);

    /**
     * This function returns all the devices availables
     * on the system.
     */
    virtual std::list< std::string > getDevicesNames() = 0;

    /**
     * Will return the name of the audio system. OpenAL or
     * PortAudio might be an answer.
     */
    std::string getName();

    /**
     * Open the default device.
     */
    virtual Device *openDevice() = 0;

    /**
     * Open the default capture device.
     */
    virtual Emitter *openCaptureDevice() = 0;
    
    /**
     * Open the specified device. If the device don't 
     * exists, the default will be opened.
     */
    virtual Device *openDevice(const std::string &name) = 0;

  private:
    std::string mName;
  };
}

#endif 


