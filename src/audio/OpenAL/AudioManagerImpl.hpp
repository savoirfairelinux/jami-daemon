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

#ifndef __SFLAUDIO_AUDIO_MANAGER_IMPL_HPP__
#define __SFLAUDIO_AUDIO_MANAGER_IMPL_HPP__

#include <list>
#include <map>
#include <string>

namespace SFLAudio 
{
  class AudioLayer;

  class AudioManagerImpl
  {
  public:
    /**
     * We load all available layers.
     */
    AudioManagerImpl();
    ~AudioManagerImpl();

    /**
     * Return all loaded layers.
     */
    std::list< AudioLayer * > getLayers();

    /**
     * Return the layer specified. It will return NULL if
     * there's no corresponding layer.
     */
    AudioLayer *getLayer(const std::string &name);

    /**
     * Return the default layer.
     */
    AudioLayer *defaultLayer();

    /**
     * Return the current layer.
     */
    AudioLayer *currentLayer();

    /**
     * Set the current layer. It will do nothing if the 
     * layer is NULL.
     */
    void currentLayer(AudioLayer *layer);

    /**
     * Will register the layer. The manager will be 
     * the pointer's owner. You can use this if you 
     * have a layer that is isn't included in the main
     * library.
     */
    void registerLayer(AudioLayer *layer);

  private:
    typedef std::map< std::string, AudioLayer * > LayersType;
    LayersType mLayers;

    AudioLayer *mLayer;
  };
}

#endif

