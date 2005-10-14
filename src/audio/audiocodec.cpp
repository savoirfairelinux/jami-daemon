/**
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author:  Yan Morin <yan.morin@savoirfairelinux.com>
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

#include <iostream>
#include <string>

#include "audiocodec.h"

AudioCodec::AudioCodec (int payload, const std::string& codec) {
	_codecName = codec;
	_payload = payload;
}

AudioCodec::~AudioCodec (void) {
}

void
AudioCodec::setCodecName (const std::string& codec)
{
	_codecName = codec;
}

std::string
AudioCodec::getCodecName (void)
{
	return _codecName;
}


