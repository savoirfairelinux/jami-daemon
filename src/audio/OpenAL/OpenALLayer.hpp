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

#ifndef __SFLAUDIO_OPENAL_LAYER_HPP__
#define __SFLAUDIO_OPENAL_LAYER_HPP__

#include "AudioLayer.hpp"

namespace SFLAudio 
{
  class OpenALLayer : public AudioLayer
  {
  public:
    OpenALLayer();

    virtual std::list< std::string > getDevicesNames();
    virtual std::list< std::string > getCaptureDevicesNames();
    virtual Device *openDevice();
    virtual Device *openDevice(const std::string &name);
  };
}

#endif 


