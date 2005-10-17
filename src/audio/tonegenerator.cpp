/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
 
#include <iostream>
#include <fstream>
#include <math.h> 
#include <stdlib.h>
 
#include "audiolayer.h"
#include "codecDescriptor.h"
#include "ringbuffer.h"
#include "tonegenerator.h"
#include "../global.h"
#include "../manager.h"
#include "../user_cfg.h"

int AMPLITUDE = 32767;


///////////////////////////////////////////////////////////////////////////////
// ToneThread implementation
///////////////////////////////////////////////////////////////////////////////
ToneThread::ToneThread (int16 *buf, int size) : ost::Thread () {
  this->buffer = buf;
  this->size = size;
  // channels is 2 (global.h)
  this->buf_ctrl_vol = new int16[size*CHANNELS];
}

ToneThread::~ToneThread (void) {
  try {
    terminate();
  } catch (...) {
    _debug("ToneThread: try to terminate, but catch an exception...\n");
  }
  delete[] buf_ctrl_vol; buf_ctrl_vol=NULL;
}

void
ToneThread::run (void) {
	int k;
	//int spkrVolume;
	bool started = false;

	// How long do 'size' samples play ?
	unsigned int play_time = (size * 1000) / SAMPLING_RATE - 10;

  ManagerImpl& manager = Manager::instance();
  manager.getAudioDriver()->flushMain();

  // this loop can be outside the stream, since we put the volume inside the ringbuffer
  for (int j = 0; j < size; j++) {
		k = j<<1; // channels is 2 (global.h)
              // split in two
		buf_ctrl_vol[k] = buf_ctrl_vol[k+1] = buffer[j];
    // * spkrVolume/100;
	}

  // Create a new stereo buffer with the volume adjusted
  // spkrVolume = manager.getSpkrVolume();
  // Push the tone to the audio FIFO

  // size = number of int16 * 2 (two channels) * 
  // int16 are the buf_ctrl_vol 
  //  unsigned char are the sample_ptr inside ringbuffer

  int size_in_char = size * 2 * (sizeof(int16)/sizeof(unsigned char));
  _debug(" size : %d\t size_in_char : %d\n", size, size_in_char);

 	while (!testCancel()) {
    manager.getAudioDriver()->putMain(buf_ctrl_vol, size_in_char);
		// The first iteration will start the audio stream if not already.
    if (!started) {
	  	started = true;
		  manager.getAudioDriver()->startStream();
	  }
		
		// next iteration later, sound is playing.
		this->sleep(play_time); // this is not a pause, this is the sound that play
	}
}

///////////////////////////////////////////////////////////////////////////////
// ToneGenerator implementation
///////////////////////////////////////////////////////////////////////////////

ToneGenerator::ToneGenerator () {	
	this->initTone();
	tonethread = NULL;
	_dst = NULL;
	_src = NULL;
	_ulaw = new Ulaw (PAYLOAD_CODEC_ULAW, "G711u");

  _currentTone = ZT_TONE_NULL;
  _currentZone = 0;
}

ToneGenerator::~ToneGenerator (void) {
	delete tonethread; tonethread = 0;
  delete [] _dst;    _dst = 0;
  delete [] _src;    _src = 0;
  delete _ulaw;      _ulaw = 0;
}

/**
 * Initialisation of ring tone for supported zone
 * http://nemesis.lonestar.org/reference/telecom/signaling/busy.html
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
ToneGenerator::generateSin (int lowerfreq, int higherfreq, int16* ptr, int len) const {
	double var1, var2;
													
	var1 = (double)2 * (double)M_PI * (double)higherfreq / (double)SAMPLING_RATE; 
	var2 = (double)2 * (double)M_PI * (double)lowerfreq / (double)SAMPLING_RATE;

  double amp = (double)(AMPLITUDE >> 2);
	
	for(int t = 0; t < len; t++) {
		ptr[t] = (int16)(amp * ((sin(var1 * t) + sin(var2 * t))));
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
ToneGenerator::buildTone (unsigned int idCountry, unsigned int idTones, int16* temp) {
	std::string s;
	int count = 0;
	int byte = 0;
  int byte_max = 0;
	int nbcomma = 0;
	int16 *buffer = new int16[SIZEBUF]; //1kb
	int pos;

	std::string str(toneZone[idCountry][idTones]);
	nbcomma = contains(toneZone[idCountry][idTones], ',');

	// Number of format sections 
	int byte_temp = 0;
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
		
		// If there is time or if it's unlimited
		if (time > 0) {
			byte = (SAMPLING_RATE * time) / 1000;
		} else {
			byte = SAMPLING_RATE;
		}
		// Generate SAMPLING_RATE samples of sinus, buffer is the result
		generateSin(freq1, freq2, buffer, byte);
		
    //_debug("freq1: %d, freq2: %d, time: %d, byte: %d, byte_temp: %d\n", freq1, freq2, time, byte, byte_temp);
		// To concatenate the different buffers for each section.
		count = 0;
    byte_max = byte + byte_temp;
		for (int j = byte_temp; j < byte_max; j++) {
			temp[j] = buffer[count++]; // copy each int16 data...
		}
		byte_temp += byte;
		
		str = str.substr(str.find(',') + 1, str.length());
	}

  totalbytes = byte_temp;
/*
	// Total number in final buffer
	if (byte != SAMPLING_RATE) {
		totalbytes = byte + (byte_temp);
	} else {
		totalbytes = byte;
	}
*/
	delete[] buffer; buffer=NULL;
}

/**
 * Returns id selected zone for tone choice
 * 
 * @param	name of the zone
 * @return	id of the zone
 */
int
ToneGenerator::idZoneName (const std::string& name) {
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
		return ID_NORTH_AMERICA; // default, we don't want segmentation fault
	}
}

/**
 * Handle the required tone 
 *  
 * @param	idr: specified tone
 * @param	var: indicates begin/end of the tone
 */
void
ToneGenerator::toneHandle (unsigned int idr, const std::string& zone) {
  if (idr == ZT_TONE_NULL) return;
  unsigned int idz = idZoneName(zone);
  // if the tonethread run
  if ( tonethread != NULL ) {
    // if it's the good tone and good zone, do nothing
    if (idr == _currentTone && idz == _currentZone ) {
      // do nothing
      return;
    } else {

      stopTone();
      _currentTone = idr;
      _currentZone = idz;
    }
  } else {
    _currentTone = idr;
    _currentZone = idz;
  }
  buildTone(idz, idr, _buf);
  tonethread = new ToneThread(_buf, totalbytes);
  _debug("Thread: start tonethread\n");
  tonethread->start();
}

void
ToneGenerator::stopTone() {
  _currentTone = ZT_TONE_NULL;

  _debug("Thread: delete tonethread\n");
  delete tonethread; tonethread = NULL;
  // we end the last thread
  _debug("Thread: tonethread deleted\n");
}

/**
 * @return 1 if everything is ok
 */
int
ToneGenerator::playRingtone (const char *fileName) {
  if (tonethread != NULL) {
    stopTone();
  }
  delete [] _dst; _dst = NULL;
  delete [] _src; _src = NULL;

	int expandedsize, length;

	if (fileName == NULL) {
		return 0;
	}
	
	std::fstream file;
	file.open(fileName, std::fstream::in);
	if (!file.is_open()) {
		return 0;
  	}

  // get length of file:
  file.seekg (0, std::ios::end);
  length = file.tellg();
  file.seekg (0, std::ios::beg);

    // allocate memory:
  _src = new char [length];
  _dst = new short[length*2];

  // read data as a block:
  file.read (_src,length);
  file.close();

  // Decode file.ul
  expandedsize = _ulaw->codecDecode (_dst, (unsigned char *)_src, length);

  _debug("length (pre-ulaw) : %d\n", length);
  _debug("expandedsize (post-ulaw) : %d\n", expandedsize);

  if (tonethread == NULL) {
    _debug("Thread: start tonethread\n");
    tonethread = new ToneThread ((int16*)_dst, expandedsize);
    tonethread->start();
  }

  return 1;
}

int
ToneGenerator::contains (const std::string& str, char c)
{
	static int nb = 0;
	
	unsigned int pos = str.find(c);
	if (pos != std::string::npos) {
		nb = nb + 1;
		return contains(str.substr(pos + 1, str.length()), c);
	} else {
		return nb;
	}

}
