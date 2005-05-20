/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author:  Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include "g711.h"
#include "ulaw.h"

Ulaw::Ulaw(int payload, const string& codec) : AudioCodec(payload, codec)
{
	_codecName = codec;
	_payload = payload;
}

Ulaw::~Ulaw (void)
{
}

int
Ulaw::codecDecode (short *dst, unsigned char *src, unsigned int size) 
{
	return G711::ULawDecode (dst, src, size);
}

int
Ulaw::codecEncode (unsigned char *dst, short *src, unsigned int size) 
{	
	return G711::ULawEncode (dst, src, size);
}

