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

#include "AudioManagerImpl.hpp"
#include "NullLayer.hpp"
#include "OpenALLayer.hpp"
#include "PortAudioLayer.hpp"

SFLAudio::AudioManagerImpl::AudioManagerImpl() 
{
  mLayer = NULL;
  registerLayer(new SFLAudio::PortAudioLayer());
  registerLayer(new SFLAudio::OpenALLayer());
}

SFLAudio::AudioManagerImpl::~AudioManagerImpl() 
{
  for(LayersType::iterator layer = mLayers.begin();
      layer != mLayers.end();
      layer++) {
    delete layer->second;
  }
}

std::list< SFLAudio::AudioLayer * > 
SFLAudio::AudioManagerImpl::getLayers()
{
  std::list< AudioLayer * > layers;
  for(LayersType::iterator layer = mLayers.begin();
      layer != mLayers.end();
      layer++) {
    layers.push_back(layer->second);
  }
  
  return layers;
}

SFLAudio::AudioLayer * 
SFLAudio::AudioManagerImpl::getLayer(const std::string &name)
{
  AudioLayer *layer = NULL;
  LayersType::iterator it = mLayers.find(name);
  if(it != mLayers.end()) {
    layer = it->second;
  }
  else {
    layer = new NullLayer();
  }
  
  return layer;
}

SFLAudio::AudioLayer * 
SFLAudio::AudioManagerImpl::defaultLayer()
{
  return getLayer("openal");
}

SFLAudio::AudioLayer * 
SFLAudio::AudioManagerImpl::currentLayer()
{
  if(mLayer == NULL) {
    mLayer = defaultLayer();
  }

  return mLayer;
}

void
SFLAudio::AudioManagerImpl::currentLayer(AudioLayer *layer)
{
  if(layer != NULL) {
    mLayer = layer;
  }
}

void
SFLAudio::AudioManagerImpl::registerLayer(AudioLayer *layer)
{
  mLayers.insert(std::make_pair(layer->getName(), layer));
}
