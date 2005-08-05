/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
 
#include <math.h> 
#include <iostream>
#include <fstream>
#include <stdlib.h>
 
#include "audiolayer.h"
#include "audiortp.h"
#include "codecDescriptor.h"
#include "ringbuffer.h"
#include "ulaw.h"
#include "tonegenerator.h"
#include "../configuration.h" 
#include "../global.h"
#include "../manager.h"
#include "../user_cfg.h"

using namespace std;

int AMPLITUDE = 8192;


///////////////////////////////////////////////////////////////////////////////
// ToneThread implementation
///////////////////////////////////////////////////////////////////////////////
ToneThread::ToneThread (int16 *buf, int size) : Thread () {
	this->buffer = buf;  
	this->size = size;
	this->buf_ctrl_vol = new int16[size*CHANNELS];
}

ToneThread::~ToneThread (void) {
	delete[] buf_ctrl_vol;
}

void
ToneThread::run (void) {
	int k;
	int spkrVolume;
	bool started = false;

	// How long do 'size' samples play ?
	unsigned int play_time = (size * 1000) / SAMPLING_RATE;

	while (Manager::instance().getZonetone()) {
		// Create a new stereo buffer with the volume adjusted
		spkrVolume = Manager::instance().getSpkrVolume();
		for (int j = 0; j < size; j++) {
			k = j*2;
			buf_ctrl_vol[k] = buf_ctrl_vol[k+1] = buffer[j] * spkrVolume/100;
		}

		// Push the tone to the audio FIFO
		Manager::instance().getAudioDriver()->mainSndRingBuffer().Put(buf_ctrl_vol, 
			SAMPLES_SIZE(size));

		// The first iteration will start the audio stream if not already.
		if (!started) {
			started = true;
			Manager::instance().getAudioDriver()->startStream();
		}
		
		// next iteration later, sound is playing.
		this->sleep (play_time);
	}
}

///////////////////////////////////////////////////////////////////////////////
// ToneGenerator implementation
///////////////////////////////////////////////////////////////////////////////

ToneGenerator::ToneGenerator () {	
	this->initTone();
	tonethread = NULL;
}

ToneGenerator::~ToneGenerator (void) {
	delete tonethread;
}

/**
 * Initialisation of ring tone for supported zone
 */
void
ToneGenerator::initTone (void) {
	toneZone[ID_NORTH_AMERICA][ZT_TONE_DIALTONE] = "350+440";
	toneZone[ID_NORTH_AMERICA][ZT_TONE_BUSY] = "480+620/500,0/500";
	toneZone[ID_NORTH_AMERICA][ZT_TONE_RINGTONE] = "440+480/2000,0/4000";
	toneZone[ID_NORTH_AMERICA][ZT_TONE_CONGESTION] = "480+620/250,0/250"; 

	toneZone[ID_FRANCE][ZT_TONE_DIALTONE] = "440";
	toneZone[ID_FRANCE][ZT_TONE_BUSY] = "440/500,0/500";
	toneZone[ID_FRANCE][ZT_TONE_RINGTONE] = "440/1500,0/3500";
	toneZone[ID_FRANCE][ZT_TONE_CONGESTION] = "440/250,0/250";
	
	toneZone[ID_AUSTRALIA][ZT_TONE_DIALTONE] = "413+438";
	toneZone[ID_AUSTRALIA][ZT_TONE_BUSY] = "425/375,0/375";
	toneZone[ID_AUSTRALIA][ZT_TONE_RINGTONE] = 
		"413+438/400,0/200,413+438/400,0/2000";
	toneZone[ID_AUSTRALIA][ZT_TONE_CONGESTION] = "425/375,0/375,420/375,8/375"; 
	
	toneZone[ID_UNITED_KINGDOM][ZT_TONE_DIALTONE] = "350+440";
	toneZone[ID_UNITED_KINGDOM][ZT_TONE_BUSY] = "400/375,0/375";
	toneZone[ID_UNITED_KINGDOM][ZT_TONE_RINGTONE] = 
		"400+450/400,0/200,400+450/400,0/2000";
	toneZone[ID_UNITED_KINGDOM][ZT_TONE_CONGESTION] = 
		"400/400,0/350,400/225,0/525";
	
	toneZone[ID_SPAIN][ZT_TONE_DIALTONE] = "425";
	toneZone[ID_SPAIN][ZT_TONE_BUSY] = "425/200,0/200";
	toneZone[ID_SPAIN][ZT_TONE_RINGTONE] = "425/1500,0/3000";
	toneZone[ID_SPAIN][ZT_TONE_CONGESTION] = 
		"425/200,0/200,425/200,0/200,425/200,0/600";
	
	toneZone[ID_ITALY][ZT_TONE_DIALTONE] = "425/600,0/1000,425/200,0/200";
	toneZone[ID_ITALY][ZT_TONE_BUSY] = "425/500,0/500";
	toneZone[ID_ITALY][ZT_TONE_RINGTONE] = "425/1000,0/4000";
	toneZone[ID_ITALY][ZT_TONE_CONGESTION] = "425/200,0/200";
	
	toneZone[ID_JAPAN][ZT_TONE_DIALTONE] = "400";
	toneZone[ID_JAPAN][ZT_TONE_BUSY] = "400/500,0/500";
	toneZone[ID_JAPAN][ZT_TONE_RINGTONE] = "400+15/1000,0/2000";
	toneZone[ID_JAPAN][ZT_TONE_CONGESTION] = "400/500,0/500";
}

/**
 * Calculate superposition of 2 sinus 
 *
 * @param	lower frequency
 * @param	higher frequency
 * @param	ptr for result buffer
 */
void
ToneGenerator::generateSin (int lowerfreq, int higherfreq, int16* ptr) {
	double var1, var2;
													
	var1 = (double)2 * (double)M_PI * (double)higherfreq / (double)SAMPLING_RATE; 
	var2 = (double)2 * (double)M_PI * (double)lowerfreq / (double)SAMPLING_RATE;
	
	for(int t = 0; t < SAMPLING_RATE; t++) {
		ptr[t] = (int16)((double)(AMPLITUDE >> 2) * sin(var1 * t) +
                    (double)(AMPLITUDE >> 2) * sin(var2 * t));
	}
}

/**
 * Build tone according to the id-zone, with initialisation of ring tone.
 * Generate sinus with frequencies alternatively by time
 *
 * @param	idCountry		
 * @param	idTones			
 * @param	ns				section number of format tone
 */
void
ToneGenerator::buildTone (int idCountry, int idTones, int16* temp) {
	string s;
	int count = 0;
	int	byte = 0,
		byte_temp = 0;
	static int	nbcomma = 0;
	int16 *buffer = new int16[SIZEBUF]; //1kb
	int pos;

	string str(toneZone[idCountry][idTones]);
	nbcomma = contains(toneZone[idCountry][idTones], ',');

	// Number of format sections 
	for (int i = 0; i < nbcomma + 1; i++) {
		// Sample string: "350+440" or "350+440/2000,244+655/2000"
		pos = str.find(',');
		if (pos < 0) { // no comma found
			pos = str.length();
		}

		s = str.substr(0, pos); // 

		// The 1st frequency is before the first +
		int pos_freq2;	
		pos_freq2 = pos = s.find('+');
		if (pos < 0) {
			pos = s.length(); // no + found
		}
		freq1 = atoi((s.substr(0, pos)).data());

		int pos2 = s.find('/');
		if (pos2 < 0) {
			pos2 = s.length();
		}
		if (pos_freq2 < 0) {
			// freq2 was not found
			freq2 = 0;
		} else {
			// freq2 was found and is after the +
			freq2 = atoi( s.substr(pos + 1, pos2).data() );
		}

		pos = s.find('/');
		if (pos < 0) {
			time = 0; // No time specified, tone will last forever.
		} else {
			time = atoi((s.substr(pos + 1, s.length())).data());
		}
		
		// Generate SAMPLING_RATE samples of sinus, buffer is the result
		generateSin(freq1, freq2, buffer);
		
		// If there is time or if it's unlimited
		if (time > 0) {
			byte = (SAMPLING_RATE * 2 * time) / 1000;
		} else {
			byte = SAMPLING_RATE;
		}
		
		// To concatenate the different buffers for each section.
		count = 0;
		for (int j = byte_temp * i; j < byte + (byte_temp * i); j++) {
			temp[j] = buffer[count++];
		}		
		byte_temp = byte;
		
		str = str.substr(str.find(',') + 1, str.length());
	}

	// Total number in final buffer
	if (byte != SAMPLING_RATE) {
		totalbytes = byte + (byte_temp * (nbcomma+1));
	} else {
		totalbytes = byte;
	}
	delete[] buffer;
}

/**
 * Returns id selected zone for tone choice
 * 
 * @param	name of the zone
 * @return	id of the zone
 */
int
ToneGenerator::idZoneName (const string& name) {
	if (name.compare("North America") == 0) {
		return ID_NORTH_AMERICA;
	} else if (name.compare("France") == 0) {
		return ID_FRANCE;
	} else if (name.compare("Australia") == 0) {
		return ID_AUSTRALIA;
	} else if (name.compare("United Kingdom") == 0) {
		return ID_UNITED_KINGDOM;
	} else if (name.compare("Spain") == 0) {
		return ID_SPAIN;
	} else if (name.compare("Italy") == 0) {
		return ID_ITALY;
	} else if (name.compare("Japan") == 0) {
		return ID_JAPAN;
	} else {
		_debug("Zone no supported\n");
		return -1;
	}
}

/**
 * Handle the required tone 
 *  
 * @param	idr: specified tone
 * @param	var: indicates begin/end of the tone
 */
void
ToneGenerator::toneHandle (int idr) {
	int idz = idZoneName(get_config_fields_str(PREFERENCES, ZONE_TONE));
	
	if (idz != -1) {
		buildTone (idz, idr, buf);

		// New thread for the tone
		if (tonethread == NULL) {
			tonethread = new ToneThread (buf, totalbytes);
			tonethread->start();
		}

		if (!Manager::instance().getZonetone()) {
			Manager::instance().getAudioDriver()->stopStream();
			Manager::instance().getAudioDriver()->mainSndRingBuffer().flush();
			if (tonethread != NULL) {	
				delete tonethread;
				tonethread = NULL;
			}
		}
	}
}


int
ToneGenerator::playRingtone (const char *fileName) {
	short* dst = NULL;
	char* src = NULL;
	int expandedsize, length;
	Ulaw* ulaw = new Ulaw (PAYLOAD_CODEC_ULAW, "G711u");

	if (fileName == NULL) {
		return 0;
	}
	
	fstream file;
	file.open(fileName, fstream::in);
	if (!file.is_open()) {
		return 0;
  	}

	// get length of file:
  	file.seekg (0, ios::end);
  	length = file.tellg();
  	file.seekg (0, ios::beg);

  	// allocate memory:
  	src = new char [length];
	dst = new short[length*2];
	
  	// read data as a block:
  	file.read (src,length);
	
	// Decode file.ul
	expandedsize = ulaw->codecDecode (dst, (unsigned char *)src, length);

	if (tonethread == NULL) {
		tonethread = new ToneThread ((int16*)dst, expandedsize);
		tonethread->start();
	}
	if (!Manager::instance().getZonetone()) {
		Manager::instance().getAudioDriver()->stopStream();
		Manager::instance().getAudioDriver()->mainSndRingBuffer().flush();
		if (tonethread != NULL) {	
			delete tonethread;
			tonethread = NULL;
			delete[] dst;
			delete[] src;
			delete ulaw;
		}
	}
	
	file.close();
	return 1;
}

int
ToneGenerator::contains (const string& str, char c)
{
	static int nb = 0;
	
	unsigned int pos = str.find(c);
	if (pos != string::npos) {
		nb = nb + 1;
		return contains(str.substr(pos + 1, str.length()), c);
	} else {
		return nb;
	}

}
