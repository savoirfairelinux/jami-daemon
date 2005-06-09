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
#include <portaudio.h>

#include "pa_converters.h"
#include "pa_dither.h"

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

void 
AudioCodec::float32ToInt16 (float32* src, int16* dst, int size) {
	PaUtilConverter* myconverter;
	struct  PaUtilTriangularDitherGenerator tdg;
	PaUtil_InitializeTriangularDitherState (&tdg);

	myconverter = PaUtil_SelectConverter (paFloat32, paInt16, paNoFlag);
	if (myconverter != NULL) {
		myconverter(dst, 1, src, 1, size, &tdg);
	} else {
		_debug("Format conversion is not supported\n");
	}
}

void 
AudioCodec::int16ToFloat32 (int16* src, float32* dst, int size) {
	PaUtilConverter* myconverter;

	myconverter = PaUtil_SelectConverter (paInt16, paFloat32, paNoFlag);
	if (myconverter != NULL) {
		myconverter(dst, 1, src, 1, size, NULL);
	} else {
		_debug("Format conversion is not supported\n");
	}
}

