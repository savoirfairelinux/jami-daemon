/**
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author:  Yan Morin <yan.morin@savoirfairelinux.com>
 * Author:  Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *                                                                              
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/



#include "../common.h"
#include "audiocodec.h"

class Ulaw : public AudioCodec {
public:
    // 0 PCMU A 8000 1 [RFC3551]
	Ulaw(int payload=0)
 	: AudioCodec(payload, "PCMU")
	{
  		_clockRate = 8000;
  		_channel   = 1;
	}

	virtual int codecDecode (short *dst, unsigned char *src, unsigned int size) {
		int16* end = dst+size;
        	while(dst<end)
                	*dst++ = ULawDecode(*src++);
        	return size<<1;
	}

	virtual int codecEncode (unsigned char *dst, short *src, unsigned int size) {
  		size >>= 1;
        	uint8* end = dst+size;
        	while(dst<end)
                	*dst++ = ULawEncode(*src++);
        	return size;
	}

	int ULawDecode(uint8 ulaw)
	{
		ulaw ^= 0xff;  // u-law has all bits inverted for transmission
		int linear = ulaw&0x0f;
        	linear <<= 3;
        	linear |= 0x84;  // Set MSB (0x80) and a 'half' bit (0x04) to place PCM value in middle of range

        	uint shift = ulaw>>4;
        	shift &= 7;
        	linear <<= shift;
		linear -= 0x84; // Subract uLaw bias

        	if(ulaw&0x80)
                	return -linear;
       		else
                	return linear;
	}

	uint8 ULawEncode(int16 pcm16)
	{
		int p = pcm16;
        	uint u;  // u-law value we are forming

        	if(p<0)
                {
			p = ~p;
                	u = 0x80^0x10^0xff;  // Sign bit = 1 (^0x10 because this will get inverted later) ^0xff ^0xff to invert final u-Law code
                }
		else{
			u = 0x00^0x10^0xff;  // Sign bit = 0 (-0x10 because this amount extra will get added later) ^0xff to invert final u-Law code
                }

        	p += 0x84; // Add uLaw bias

        	if(p>0x7f00)
                	p = 0x7f00;  // Clip to 15 bits
		// Calculate segment and interval numbers
		p >>= 3;        // Shift down to 13bit
		if(p>=0x100)
                {
                	p >>= 4;
                	u ^= 0x40;
                }
        	if(p>=0x40)
                {
                	p >>= 2;
                	u ^= 0x20;
                }
        	if(p>=0x20)
                {
                	p >>= 1;
                	u ^= 0x10;
                }
		u ^= p; // u now equal to encoded u-law value (with all bits inverted)

        	return u;
	}
};

// the class factories
extern "C" AudioCodec* create() {
    return new Ulaw(0);
}

extern "C" void destroy(AudioCodec* a) {
    delete a;
}
