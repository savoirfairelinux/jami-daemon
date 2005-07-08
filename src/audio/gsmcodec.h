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

#ifndef __GSM_H__
#define __GSM_H__

#include "audiocodec.h"
extern "C" {
#include "gsm/gsm.h"
}

class Gsm : public AudioCodec {
public:
	Gsm (int payload, const string& codec);
	~Gsm (void);
	
	int	codecDecode	(short *, unsigned char *, unsigned int);
	int	codecEncode	(unsigned char *, short *, unsigned int);

private:
	string _codecName;
	int _payload;
	gsm _decode_gsmhandle;
	gsm _encode_gsmhandle;
	
};

#endif // __ULAW_H__
