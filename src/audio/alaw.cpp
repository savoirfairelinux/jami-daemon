/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author:  Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author:  Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include "common.h"
#include "audiocodec.h"

class Alaw : public AudioCodec {
public:

// 8 PCMA A 8000 1 [RFC3551]
   Alaw(int payload=0)
 : AudioCodec(payload, "PCMA")
{
  //_description = "G711a";

  _clockRate = 8000;
  _channel   = 1;
}


virtual int codecDecode (short *dst, unsigned char *src, unsigned int size) 
{
	int16* end = dst+size;
        while(dst<end)
                *dst++ = ALawDecode(*src++);
        return size<<1;
}

int ALawDecode(uint8 alaw)
{
	alaw ^= 0x55;  // A-law has alternate bits inverted for transmission

        uint sign = alaw&0x80;

        int linear = alaw&0x1f;
        linear <<= 4;
	linear += 8;  // Add a 'half' bit (0x08) to place PCM value in middle of range

        alaw &= 0x7f;
        if(alaw>=0x20)
        {
        	linear |= 0x100;  // Put in MSB
		uint shift = (alaw>>4)-1;
                linear <<= shift;
        }

        if(!sign)
                return -linear;
        else
                return linear;
}

virtual int codecEncode (unsigned char *dst, short *src, unsigned int size) 
{	
	size >>= 1;
        uint8* end = dst+size;
        while(dst<end)
                *dst++ = ALawEncode(*src++);
        return size;
}


uint8 ALawEncode (int16 pcm16)
{
	int p = pcm16;
        uint u;  // u-law value we are forming

        if(p<0)
        {
	p = ~p;
                u = 0x80^0x10^0xff;  // Sign bit = 1 (^0x10 because this will get inverted later) ^0xff ^0xff to invert final u-Law code
                }
        else
                {
	//+ve value
	u = 0x00^0x10^0xff;	
 }

        p += 0x84; // Add uLaw bias

        if(p>0x7f00)
                p = 0x7f00;  // Clip to 15 bits
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
    return new Alaw(8);
}

extern "C" void destroy(AudioCodec* a) {
    delete a;
}

