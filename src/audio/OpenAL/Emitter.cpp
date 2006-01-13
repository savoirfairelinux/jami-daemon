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

#include "Context.hpp"
#include "Emitter.hpp"


SFLAudio::Emitter::Emitter() 
  : mSource(0)
  , mFormat(0)
  , mFreq(0)
{}


SFLAudio::Emitter::Emitter(int format, int freq) 
  : mSource(0)
  , mFormat(format)
  , mFreq(freq)
{}


int 
SFLAudio::Emitter::getFrequency()
{return mFreq;}

int 
SFLAudio::Emitter::getFormat()
{return mFormat;}

void 
SFLAudio::Emitter::setFrequency(int freq)
{mFreq = freq;}

void 
SFLAudio::Emitter::setFormat(int format)
{mFormat = format;}

void
SFLAudio::Emitter::connect(Source *source)
{mSource = source;}

void
SFLAudio::Emitter::connect(Context *context)
{mSource = context->createSource(this);}

SFLAudio::Source *
SFLAudio::Emitter::getSource()
{return mSource;}

bool
SFLAudio::Emitter::isNull()
{return false;}
