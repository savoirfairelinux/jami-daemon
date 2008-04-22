/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
 */ 
 
#include <fstream>
#include <math.h> 
#include <stdlib.h>
 
#include "tonegenerator.h"
#include "../global.h"

int AMPLITUDE = 32767;

///////////////////////////////////////////////////////////////////////////////
// ToneGenerator implementation
///////////////////////////////////////////////////////////////////////////////

ToneGenerator::ToneGenerator (unsigned int sampleRate) {	
  _sampleRate = sampleRate;
}

ToneGenerator::~ToneGenerator (void) {
}

/**
 * Calculate superposition of 2 sinus 
 *
 */
void
ToneGenerator::generateSin (int lowerfreq, int higherfreq, int16* ptr, int len) const {
  double var1, var2;
													
  var1 = (double)2 * (double)M_PI * (double)higherfreq / (double)_sampleRate; 
  var2 = (double)2 * (double)M_PI * (double)lowerfreq / (double)_sampleRate;

  double amp = (double)(AMPLITUDE >> 2);
 	
  for(int t = 0; t < len; t++) {
    ptr[t] = (int16)(amp * ((sin(var1 * t) + sin(var2 * t))));
  }
}

