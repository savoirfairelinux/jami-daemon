/*
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#if defined(__APPLE__)
# include <machine/endian.h>
#else
# include <endian.h>
#endif
#include <string.h>
#include <iostream>
#include <string>

#include "portaudio.h"

#include "../global.h"

#include "audiocodec.h"
#include "../configuration.h"

using namespace std;


AudioCodec::AudioCodec (int payload, const string& codec) {
	_codecName = codec;
	_payload = payload;
}

AudioCodec::~AudioCodec (void) {
}

void
AudioCodec::setCodecName (const string& codec)
{
	_codecName = codec;
}

string
AudioCodec::getCodecName (void)
{
	return _codecName;
}


