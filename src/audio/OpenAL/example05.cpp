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

#include <iostream>
#include <list>
#include <string>

#include "SFLAudio.hpp"
#include "Emitter.hpp"

using namespace SFLAudio;

int main(int, char* []) 
{
  AudioLayer *layer = SFLAudio::AudioManager::instance().currentLayer();
  Device *device = layer->openDevice();
  Context *context = device->createContext();
  Emitter *mic = layer->openCaptureDevice();

  mic->connect(context);
  mic->play();

  // Wait for user input.
  std::cout << "Press any key to quit the program." << std::endl;
  std::cin.get();
}
