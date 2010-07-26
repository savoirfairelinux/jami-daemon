/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 *  Inspired by ringbuffer of Audacity Project
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */
#include "audiofile.h"
#include "audio/codecs/codecDescriptor.h"
#include <fstream>
#include <math.h>
#include <samplerate.h>
#include <cstring>

AudioFile::AudioFile()
        : AudioLoop(),
        _filename(),
        _codec (NULL),
        _start (false)

{
}

AudioFile::~AudioFile()
{
}

// load file in mono format
bool
AudioFile::loadFile (const std::string& filename, AudioCodec* codec , unsigned int sampleRate=8000)
{
    _codec = codec;

    // if the filename was already load, with the same samplerate
    // we do nothing

    if (strcmp (_filename.c_str(), filename.c_str()) == 0 && _sampleRate == (int) sampleRate) {
        return true;
    } else {
        // reset to 0
        delete [] _buffer;
        _buffer = 0;
        _size = 0;
        _pos = 0;
    }



    // no filename to load
    if (filename.empty()) {
        _debug ("Unable to open audio file: filename is empty");
        return false;
    }

    std::fstream file;

    file.open (filename.c_str(), std::fstream::in);

    if (!file.is_open()) {
        // unable to load the file
        _debug ("Unable to open audio file %s", filename.c_str());
        return false;
    }

    // get length of file:
    file.seekg (0, std::ios::end);

    int length = file.tellg();

    file.seekg (0, std::ios::beg);

    // allocate memory:
    char fileBuffer[length];

    // read data as a block:
    file.read (fileBuffer,length);

    file.close();


    // Decode file.ul
    // expandedsize is the number of bytes, not the number of int
    // expandedsize should be exactly two time more, else failed
    int16 monoBuffer[length];

    int expandedsize = (int) _codec->codecDecode (monoBuffer, (unsigned char *) fileBuffer, length);

    if (expandedsize != length*2) {
        _debug ("Audio file error on loading audio file!");
        return false;
    }

    unsigned int nbSampling = expandedsize/sizeof (int16);

    // we need to change the sample rating here:
    // case 1: we don't have to resample : only do splitting and convert

    if (sampleRate == 8000) {
        // just s
        _size   = nbSampling;
        _buffer = new SFLDataFormat[_size];
#ifdef DATAFORMAT_IS_FLOAT
        // src to dest
        src_short_to_float_array (monoBuffer, _buffer, nbSampling);
#else
        // dest to src
        memcpy (_buffer, monoBuffer, _size*sizeof (SFLDataFormat));
#endif

    } else {
        // case 2: we need to convert it and split it
        // convert here
        double factord = (double) sampleRate / 8000;
        float* floatBufferIn = new float[nbSampling];
        int    sizeOut  = (int) (ceil (factord*nbSampling));
        src_short_to_float_array (monoBuffer, floatBufferIn, nbSampling);
        SFLDataFormat* bufferTmp = new SFLDataFormat[sizeOut];

        SRC_DATA src_data;
        src_data.data_in = floatBufferIn;
        src_data.input_frames = nbSampling;
        src_data.output_frames = sizeOut;
        src_data.src_ratio = factord;

#ifdef DATAFORMAT_IS_FLOAT
        // case number 2.1: the output is float32 : convert directly in _bufferTmp
        src_data.data_out = bufferTmp;
        src_simple (&src_data, SRC_SINC_BEST_QUALITY, 1);
#else
        // case number 2.2: the output is int16 : convert and change to int16
        float* floatBufferOut = new float[sizeOut];
        src_data.data_out = floatBufferOut;

        src_simple (&src_data, SRC_SINC_BEST_QUALITY, 1);
        src_float_to_short_array (floatBufferOut, bufferTmp, src_data.output_frames_gen);

        delete [] floatBufferOut;
#endif
        delete [] floatBufferIn;
        nbSampling = src_data.output_frames_gen;

        // if we are in mono, we send the bufferTmp location and don't delete it
        // else we split the audio in 2 and put it into buffer
        _size = nbSampling;
        _buffer = bufferTmp;  // just send the buffer pointer;
        bufferTmp = 0;
    }

    return true;
}



WavFile::WavFile()
        : AudioLoop(),
        _filename(),
        _codec (NULL),
        _start (false)

{
}

WavFile::~WavFile()
{
}


bool WavFile::loadFile (const std::string& filename, AudioCodec* codec , unsigned int sampleRate) {
  
    std::fstream file;

    printf("WavFile: Open %s", filename.c_str());

    file.open(filename.c_str(), std::ios::in | std::ios::binary);

    char riff[4] = {};

    file.read(riff, 4);

    if ( strncmp("RIFF", riff, 4) != 0 ) {
        _error("WavFile: File is not of RIFF format");
        return false;
    }

    // Find the "fmt " chunk
    char fmt[4] = {}; 

    while ( strncmp("fmt ", fmt, 4) != 0 ) {
      file.read(fmt, 4);
      _debug("WavFile: Searching... %s", fmt);
    }

    SINT32 chunkSize;         // fmt chunk size
    unsigned short formatTag; // data compression tag

    file.read((char*)&chunkSize, 4); // Read fmt chunk size.
    file.read((char*)&formatTag, 2);

    _debug("Chunk size: %d\n", chunkSize);
    _debug("Format tag: %d\n", formatTag);


    if (formatTag != 1 ) { // PCM = 1, FLOAT = 3 {
        _error("WaveFile: File contains an unsupported data format type");
        return false;
    }

    // Get number of channels from the header.
    SINT16 chan;
    file.read((char*)&chan, 2);

    _channels = chan;

    _debug("WavFile: channel %d", _channels);


    // Get file sample rate from the header.
    SINT32 srate;
    file.read((char*)&srate, 4);

    _fileRate = (double)srate;

    printf("WavFile: srate %d", srate);
    
    SINT32 avgb;
    file.read((char*)&avgb, 4);

    _debug("WavFile: Average byte %i\n", avgb);

    SINT16 blockal;
    file.read((char*)&blockal, 2);

    _debug("WaveFile: block alignment %d", blockal);

    // Determine the data type
    _dataType = 0;

    SINT16 dt;
    file.read((char*)&dt, 2);

    _debug("WaveFile: dt %d", dt);

    if ( formatTag == 1 ) {
      if (dt == 8)
	_dataType = 1; // SINT8;
      else if (dt == 16)
	_dataType = 2; // SINT16;
      else if (dt == 32)
	_dataType = 3; // SINT32;
    }
    /*
    else if ( formatTag == 3 ) 
    {
      if (temp == 32)
        dataType_ = FLOAT32;
      else if (temp == 64)
        dataType_ = FLOAT64;
    }
    */
    else {
      _debug("WavFile: File's bits per sample is not supported");
      return false;
    }

    // Find the "data" chunk 
    char data[4] = {};

    while ( strncmp("data", data, 4) ) {
        file.read(data, 4);
	_debug("WavFile: Searching data");
    }

    // Get length of data from the header.
    SINT32 bytes;
    file.read((char*)&bytes, 4);

    _debug("WavFile: Data size in byte %ld", bytes);

    _fileSize = 8 * bytes / dt / _channels;  // sample frames

    _debug("WavFile: Data size in frames %ld", _fileSize);

    _debug("WavFile: File successfully opened");

    return true;

}
