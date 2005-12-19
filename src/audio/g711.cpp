/**
 * Copyright (C) 2005 Savoir-Faire Linux inc.
 * Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
    Implementation of ITU-T (formerly CCITT) Recomendation G711

    Copyright (C) 2004  J.D.Medhurst (a.k.a. Tixy)

 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 */
#include "common.h"
#include "g711.h"


/*
Members of class G711
*/


uint8 G711::ALawEncode(int16 pcm16)
	{
	int p = pcm16;
	uint a;  // A-law value we are forming
	if(p<0)
		{
		// -ve value
		// Note, ones compliment is here used here as this keeps encoding symetrical

		// and equal spaced around zero cross-over, (it also matches the standard).
		p = ~p;
		a = 0x00; // sign = 0
		}
	else
		{
		// +ve value
		a = 0x80; // sign = 1
		}

	// Calculate segment and interval numbers
	p >>= 4;
	if(p>=0x20)
		{
		if(p>=0x100)
			{
			p >>= 4;
			a += 0x40;
			}
		if(p>=0x40)
			{
			p >>= 2;
			a += 0x20;
			}
		if(p>=0x20)
			{
			p >>= 1;
			a += 0x10;
			}
		}
	// a&0x70 now holds segment value and 'p' the interval number

	a += p;  // a now equal to encoded A-law value

	return a^0x55;  // A-law has alternate bits inverted for transmission
	}


int G711::ALawDecode(uint8 alaw)
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


uint8 G711::ULawEncode(int16 pcm16)
	{
	int p = pcm16;
	uint u;  // u-law value we are forming

	if(p<0)
		{
		// -ve value
		// Note, ones compliment is here used here as this keeps encoding symetrical
		// and equal spaced around zero cross-over, (it also matches the standard).
		p = ~p;
		u = 0x80^0x10^0xff;  // Sign bit = 1 (^0x10 because this will get inverted later) ^0xff ^0xff to invert final u-Law code
		}
	else
		{
		// +ve value
		u = 0x00^0x10^0xff;  // Sign bit = 0 (-0x10 because this amount extra will get added later) ^0xff to invert final u-Law code
		}

	p += 0x84; // Add uLaw bias

	if(p>0x7f00)
		p = 0x7f00;  // Clip to 15 bits

	// Calculate segment and interval numbers
	p >>= 3;	// Shift down to 13bit
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
	// (u^0x10)&0x70 now equal to the segment value and 'p' the interval number (^0x10)

	u ^= p; // u now equal to encoded u-law value (with all bits inverted)

	return u;
	}


int G711::ULawDecode(uint8 ulaw)
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


uint8 G711::ALawToULaw(uint8 alaw)
	{
	uint8 sign=alaw&0x80;
	alaw ^= sign;
	alaw ^= 0x55;
	uint ulaw;
	if(alaw<45)
		{
		if(alaw<24)
			ulaw = (alaw<8) ? (alaw<<1)+1 : alaw+8;
		else
			ulaw = (alaw<32) ? (alaw>>1)+20 : alaw+4;
		}
	else
		{
		if(alaw<63)
			ulaw = (alaw<47) ? alaw+3 : alaw+2;
		else
			ulaw = (alaw<79) ? alaw+1 : alaw;
		}
	ulaw ^= sign;
	return ulaw^0x7f;
	}


uint8 G711::ULawToALaw(uint8 ulaw)
	{
	uint8 sign=ulaw&0x80;
	ulaw ^= sign;
	ulaw ^= 0x7f;
	uint alaw;
	if(ulaw<48)
		{
		if(ulaw<=32)
			alaw = (ulaw<=15) ? ulaw>>1 : ulaw-8;
		else
			alaw = (ulaw<=35) ? (ulaw<<1)-40 : ulaw-4;
		}
	else
		{
		if(ulaw<=63)
			alaw = (ulaw==48) ? ulaw-3 : ulaw-2;
		else
			alaw = (ulaw<=79) ? ulaw-1 : ulaw;
		}
	alaw ^= sign;
	return alaw^0x55;
	}


uint G711::ALawEncode(uint8* dst, int16* src, uint srcSize)
	{
	srcSize >>= 1;
	uint8* end = dst+srcSize;
	while(dst<end)
		*dst++ = ALawEncode(*src++);
	return srcSize;
	}


uint G711::ALawDecode(int16* dst, uint8* src, uint srcSize)
	{
	int16* end = dst+srcSize;
	while(dst<end)
		*dst++ = ALawDecode(*src++);
	return srcSize<<1;
	}


uint G711::ULawEncode(uint8* dst, int16* src, uint srcSize)
	{
	srcSize >>= 1;
	uint8* end = dst+srcSize;
	while(dst<end)
		*dst++ = ULawEncode(*src++);
	return srcSize;
	}


uint G711::ULawDecode(int16* dst, uint8* src, uint srcSize)
	{
	int16* end = dst+srcSize;
	while(dst<end)
		*dst++ = ULawDecode(*src++);
	return srcSize<<1;
	}


uint G711::ALawToULaw(uint8* dst, uint8* src, uint srcSize)
	{
	uint8* end = dst+srcSize;
	while(dst<end)
		*dst++ = ALawToULaw(*src++);
	return srcSize;
	}


uint G711::ULawToALaw(uint8* dst, uint8* src, uint srcSize)
	{
	uint8* end = dst+srcSize;
	while(dst<end)
		*dst++ = ULawToALaw(*src++);
	return srcSize;
	}
