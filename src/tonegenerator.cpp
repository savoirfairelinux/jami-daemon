/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
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

#include <qapplication.h>
#include <qstring.h>

#include "audiocodec.h"
#include "configuration.h"
#include "global.h"
#include "manager.h"
#include "tonegenerator.h"

using namespace std;


///////////////////////////////////////////////////////////////////////////////
// ToneThread implementation
///////////////////////////////////////////////////////////////////////////////
ToneThread::ToneThread (Manager *mngr, short *buf) {
	this->mngr = mngr;
	this->buffer = buf;
}

ToneThread::~ToneThread (void) {
	this->terminate();
}

void
ToneThread::run (void) {
	while (mngr->tonezone) {
		mngr->audiodriver->audio_buf.setData(buffer, mngr->getSpkrVolume());
		mngr->audiodriver->writeBuffer();
	} 
}

///////////////////////////////////////////////////////////////////////////////
// ToneGenerator implementation
///////////////////////////////////////////////////////////////////////////////

ToneGenerator::ToneGenerator (Manager *mngr) {	
	this->initTone();
	this->manager = mngr;
	buf = new short[SIZEBUF];	
	tonethread = NULL;
}

ToneGenerator::ToneGenerator () {	
	this->initTone();	
	tonethread = NULL;
}

ToneGenerator::~ToneGenerator (void) {
	delete[] buf;
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
 * @param	amplitude
 * @param	samplingRate
 * @param	ptr for result buffer
 */
void
ToneGenerator::generateSin (int lowerfreq, int higherfreq, int amplitude, 
												int samplingRate, short*ptr) {
	double var1, var2;
													
	var1 = (double)2 * (double)M_PI * (double)higherfreq / (double)samplingRate; 
	var2 = (double)2 * (double)M_PI * (double)lowerfreq / (double)samplingRate;
	
	for(int i = 0; i < samplingRate; i++) {
		ptr[i] = (short)((double)(amplitude >> 2) * sin(var1 * i) + 
					(double)(amplitude >> 2) * sin(var2 * i));
	}
}

/**
 * Build tone according to the id-zone, with initialisation of ring tone.
 * Generate sinus with frequencies alternatively by time
 *
 * @param	idCountry		
 * @param	idTones			
 * @param 	samplingRate	
 * @param	amplitude		to calculate sinus
 * @param	ns				section number of format tone
 */
void
ToneGenerator::buildTone (int idCountry, int idTones, int samplingRate, 
							int amplitude, short* temp) {
	QString s;
	int count = 0;
	int	byte = 0,
		byte_temp = 0,
		nbcomma;
	short *buffer = new short[1024*1024];

	nbcomma = (toneZone[idCountry][idTones]).contains(',');
	// Number of format sections 
	for (int i = 0; i < nbcomma+1; i++) {
		s = (toneZone[idCountry][idTones]).section(',', i, i);
		freq1 = ((s.section('/', 0, 0)).section('+', 0, 0)).toInt();
		freq2 = ((s.section('/', 0, 0)).section('+', 1, 1)).toInt();
		time = (s.section('/', 1, 1)).toInt();	
		// Generate sinus, buffer is the result
		generateSin(freq1, freq2, amplitude, samplingRate, buffer);
		
		// If there is time or if it's unlimited
		if (time) {
			byte = (samplingRate*2*time)/1000;
		} else {
			byte = samplingRate;
		}
		
		// To concatenate the different buffers for each section.
		for (int j = byte_temp * i; j < byte + (byte_temp * i); j++) {
			temp[j] = buffer[count++];
		}		
		byte_temp = byte;
		count = 0;
	}
	// Total number in final buffer
	totalbytes = byte + (byte_temp * (nbcomma+1));
	
	delete[] buffer;
}

/**
 * Returns id selected zone for tone choice
 * 
 * @param	name of the zone
 * @return	id of the zone
 */
int
ToneGenerator::idZoneName (const QString &name) {
	if (name == "North America") {
		return ID_NORTH_AMERICA;
	} else if (name == "France") {
		return ID_FRANCE;
	} else if (name == "Australia") {
		return ID_AUSTRALIA;
	} else if (name == "United Kingdom") {
		return ID_UNITED_KINGDOM;
	} else if (name == "Spain") {
		return ID_SPAIN;
	} else if (name == "Italy") {
		return ID_ITALY;
	} else if (name == "Japan") {
		return ID_JAPAN;
	} else {
		qWarning("Zone no supported");
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
	int idz = idZoneName(Config::gets("Preferences", "Options.zoneToneChoice"));
	
	if (idz != -1) {
		buildTone (idz, idr, SAMPLING_RATE, AMPLITUDE, buf);

		// New thread for the tone
		if (tonethread == NULL) {
			tonethread = new ToneThread (manager, buf);
			manager->audiodriver->audio_buf.resize(totalbytes);	
			tonethread->start();
		}

		if (!manager->tonezone) {
			manager->audiodriver->resetDevice();
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
	expandedsize = AudioCodec::codecDecode (
				PAYLOAD_CODEC_ULAW,
				dst,
				(unsigned char *)src,
				length);
	
	// Start tone thread
	if (tonethread == NULL) {
		tonethread = new ToneThread (manager, dst);
		manager->audiodriver->audio_buf.resize(expandedsize);
		tonethread->start();
	}
	if (!manager->tonezone) {
		manager->audiodriver->resetDevice();
		if (tonethread != NULL) {	
			delete tonethread;
			tonethread = NULL;
			delete[] dst;
			delete[] src;
		}
	}
	file.close();
	return 1;
}


