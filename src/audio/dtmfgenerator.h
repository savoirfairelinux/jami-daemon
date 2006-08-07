/*
 * Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 * Author: Yan Morin <yan.morin@savoirfairelinux.com>
 * Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *
 * Portions (c) 2003 iptel.org
 *
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

#ifndef DTMFGENERATOR_H
#define DTMFGENERATOR_H

#include <exception>
#include <string.h>

//#include "tonegenerator.h"
#include "tone.h"

#define NUM_TONES 16

/*
 * DMTF Generator Exception
 */
class DTMFException : public std::exception
{
private:
	const char* reason;
public:
	DTMFException(const char* _reason) throw();
	virtual ~DTMFException() throw();
	virtual const char* what() const throw();
};


/*
 * DTMF Tone Generator
 */
class DTMFGenerator 
{
private:
	struct DTMFTone {
		unsigned char code; // Code of the tone
		int lower;          // Lower frequency
		int higher;         // Higher frequency
	};

/*
 * State of the DTMF generator
 */
	struct DTMFState {
		unsigned int offset;   // Offset in the sample currently being played
		int16* sample;         // Currently generated code
	};

	DTMFState state;
	static const DTMFTone tones[NUM_TONES];

	int16* samples[NUM_TONES];        // Generated samples

  /**
   * Sampling rate of generated dtmf
   */
  int _sampleRate;
  int _nbChannel;
  Tone tone;

public:
  /**
   * DTMF Generator contains frequency of each keys
   * and can build one DTMF.
   * @param sampleRate frequency of the sample (ex: 8000 hz)
   */
	DTMFGenerator(unsigned int sampleRate, unsigned int nbChannel);
	~DTMFGenerator();

/*
 * Get n samples of the signal of code code
 * @param buffer a int16 pointer to an allocated buffer
 * @param n      number of int16 to get, should be lower or equal to buffer size
 * @parma code   dtmf code to get sound
 */
	void getSamples(int16* buffer, size_t n, unsigned char code) throw (DTMFException);

/*
 * Get next n samples (continues where previous call to
 * genSample or genNextSamples stopped
 * @param buffer a int16 pointer to an allocated buffer 
 * @param n      number of int16 to get, should be lower or equal to buffer size
 */
	void getNextSamples(int16* buffer, size_t n) throw (DTMFException);

private:
	int16* generateSample(unsigned char code) throw (DTMFException);

};


#endif // DTMFGENERATOR_H
