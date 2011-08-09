/*

	    if(_dbus)
		_dbus *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <fstream>
#include <math.h>
#include <samplerate.h>
#include <cstring>
#include <limits.h>

#include "audiofile.h"
#include "audio/codecs/audiocodecfactory.h"
#include "audio/codecs/audiocodec.h"
#include "audio/samplerateconverter.h"

#include "manager.h"

RawFile::RawFile() : audioCodec (NULL)
{
    AudioFile::_start = false;
}

RawFile::~RawFile()
{
}

// load file in mono format
void RawFile::loadFile (const std::string& name, sfl::AudioCodec* codec, unsigned int sampleRate = 8000) throw(AudioFileException)
{
    _debug("RawFile: Load new file %s", name.c_str());

    audioCodec = codec;

    // if the filename was already load, with the same samplerate
    // we do nothing

    if ((filepath == name) && (_sampleRate == (int)sampleRate)) {
	return;
    }

    filepath = name;

    // no filename to load
    if (filepath.empty()) {
        throw AudioFileException("Unable to open audio file: filename is empty");
    }

    std::fstream file;

    file.open (filepath.c_str(), std::fstream::in);
    if (!file.is_open()) {
        throw AudioFileException("Unable to open audio file");
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

    unsigned int expandedsize = audioCodec->decode (monoBuffer, reinterpret_cast<unsigned char *>(fileBuffer), length);
    if (expandedsize != length * sizeof(int16)) {
        throw AudioFileException("Audio file error on loading audio file!");
    }

    unsigned int nbSampling = expandedsize / sizeof(int16);

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
}




WaveFile::WaveFile () : byteCounter (0)
    , nbChannels (1)
    , fileLength (0)
    , dataOffset (0)
    , channels (0)
    , dataType (0)
    , fileRate (0)
{
    AudioFile::_start = false;
}


WaveFile::~WaveFile()
{
    _debug ("WaveFile: Destructor Called!");
}



void WaveFile::openFile (const std::string& fileName, int audioSamplingRate) throw(AudioFileException)
{
    try {

        if (isFileExist (fileName)) {
            openExistingWaveFile (fileName, audioSamplingRate);
        }
    }
    catch(AudioFileException &e) {
        throw;
    }
}


bool WaveFile::isFileExist (const std::string& fileName)
{
    std::fstream fs (fileName.c_str(), std::ios_base::in);

    if (!fs) {
        _debug ("WaveFile: file \"%s\" doesn't exist", fileName.c_str());
        return false;
    }

    _debug ("WaveFile: File \"%s\" exists", fileName.c_str());
    return true;
}


void WaveFile::openExistingWaveFile (const std::string& fileName, int audioSamplingRate) throw(AudioFileException)
{

    int maxIteration = 0;

    _debug ("WaveFile: Opening %s", fileName.c_str());
    filepath = fileName;

    fileStream.open (fileName.c_str(), std::ios::in | std::ios::binary);

    char riff[4] = {};
    fileStream.read (riff, 4);
    if (strncmp ("RIFF", riff, 4) != 0) {
        throw AudioFileException("File is not of RIFF format");
    }

    char fmt[4] = {};
    maxIteration = 10;
    while ((maxIteration > 0) && strncmp ("fmt ", fmt, 4)) {
        fileStream.read (fmt, 4);
        maxIteration--;
    }
    if(maxIteration == 0) {
        throw AudioFileException("Could not find \"fmt \" chunk");
    }

    SINT32 chunk_size; // fmt chunk size
    unsigned short formatTag; // data compression tag

    fileStream.read ( (char*) &chunk_size, 4); // Read fmt chunk size.
    fileStream.read ( (char*) &formatTag, 2);

    _debug ("WaveFile: Chunk size: %d", chunk_size);
    _debug ("WaveFile: Format tag: %d", formatTag);

    if (formatTag != 1) { // PCM = 1, FLOAT = 3
        throw AudioFileException("File contains an unsupported data format type");
    }

    // Get number of channels from the header.
    SINT16 chan;
    fileStream.read ( (char*) &chan, 2);
    channels = chan;
    _debug ("WaveFile: Channel %d", channels);


    // Get file sample rate from the header.
    SINT32 srate;
    fileStream.read ( (char*) &srate, 4);
    fileRate = (double) srate;
    _debug ("WaveFile: Sampling rate %d", srate);

    SINT32 avgb;
    fileStream.read ( (char*) &avgb, 4);
    _debug ("WaveFile: Average byte %d", avgb);\

    SINT16 blockal;
    fileStream.read ( (char*) &blockal, 2);
    _debug ("WaveFile: Block alignment %d", blockal);


    // Determine the data type
    dataType = 0;
    SINT16 dt;
    fileStream.read ( (char*) &dt, 2);
    _debug ("WaveFile: dt %d", dt);
    if (formatTag == 1) {
        if (dt == 8)
            dataType = 1; // SINT8;
        else if (dt == 16)
            dataType = 2; // SINT16;
        else if (dt == 32)
            dataType = 3; // SINT32;
    }
    else {
        throw AudioFileException("File's bits per sample with is not supported");
    }

    // Find the "data" chunk
    char data[4] = {};
    maxIteration = 10;
    while ((maxIteration > 0) && strncmp ("data", data, 4)) {
        fileStream.read (data, 4);
        maxIteration--;
    }


    // Sample rate converter initialized with 88200 sample long
    int converterSamples  = (srate > audioSamplingRate) ? srate : audioSamplingRate;
    SamplerateConverter _converter (converterSamples, 2000);

    int nbSampleMax = 512;

    // Get length of data from the header.
    SINT32 bytes;
    fileStream.read ( (char*) &bytes, 4);
    _debug ("WaveFile: data size in byte %d", bytes);

    fileLength = 8 * bytes / dt / channels;  // sample frames
    _debug ("WaveFile: data size in frame %ld", fileLength);

    // Should not be longer than a minute
    if (fileLength > (unsigned int) (60*srate))
        fileLength = 60*srate;

    SFLDataFormat *tempBuffer = new SFLDataFormat[fileLength];
    if (!tempBuffer) {
        throw AudioFileException("Could not allocate temporary buffer");
    }

    SFLDataFormat *tempBufferRsmpl = NULL;

    fileStream.read ( (char *) tempBuffer, fileLength*sizeof (SFLDataFormat));

    // mix two channels together if stereo
    if(channels == 2) {
    	int tmp = 0;
    	unsigned j = 0;
    	for(unsigned int i = 0; i < fileLength-1; i+=2) {
    		tmp = (tempBuffer[i] + tempBuffer[i+1]) / 2;
    		// saturate
    		if(tmp > SHRT_MAX) {
    			tmp = SHRT_MAX;
    		}
    		tempBuffer[j++] = (SFLDataFormat)tmp;
    	}

    	fileLength /= 2;
    }
    else if(channels > 2) {
	delete [] tempBuffer;
    	throw AudioFileException("WaveFile: unsupported number of channels");
    }

    // compute size of final buffer
    int nbSample;

    if (srate != audioSamplingRate) {
        nbSample = (int) ( (float) fileLength * ( (float) audioSamplingRate / (float) srate));
    } else {
        nbSample = fileLength;
    }

    int totalprocessed = 0;

    // require resampling
    if (srate != audioSamplingRate) {

        // initialize remaining samples to process
        int remainingSamples = fileLength;

        tempBufferRsmpl = new SFLDataFormat[nbSample];
        if (!tempBufferRsmpl) {
            throw AudioFileException("Could not allocate temporary buffer for ressampling");
        }

        SFLDataFormat *in = tempBuffer;
        SFLDataFormat *out = tempBufferRsmpl;

        while (remainingSamples > 0) {

            int toProcess = remainingSamples > nbSampleMax ? nbSampleMax : remainingSamples;

            if (srate < audioSamplingRate) {
                _converter.upsampleData (in, out, srate, audioSamplingRate, toProcess);
            } else if (srate > audioSamplingRate) {
                _converter.downsampleData (in, out, audioSamplingRate, srate, toProcess);
            }

            // nbSamplesConverted = nbSamplesConverted*2;

            in += toProcess;
            out += toProcess;
            remainingSamples -= toProcess;
            totalprocessed += toProcess;
        }
    }

    // Init audio loop buffer info
    _buffer = new SFLDataFormat[nbSample];
    if (_buffer == NULL) {
        throw AudioFileException("Could not allocate buffer for audio");
    }

    _size = nbSample;
    _sampleRate = (int) audioSamplingRate;

    // Copy audio into audioloopi
    if (srate != audioSamplingRate) {
        memcpy ( (void *) _buffer, (void *) tempBufferRsmpl, nbSample*sizeof (SFLDataFormat));
    }
    else {
        memcpy ( (void *) _buffer, (void *) tempBuffer, nbSample*sizeof (SFLDataFormat));
    }

    _debug ("WaveFile: file successfully opened");

    delete[] tempBuffer;

    if (tempBufferRsmpl) {
        delete[] tempBufferRsmpl;
    }
}


void WaveFile::loadFile (const std::string& name, sfl::AudioCodec * /*codec*/, unsigned int sampleRate) throw(AudioFileException)
{
    try { 
        openFile (name, sampleRate);
    }
    catch(AudioFileException &e) {
        throw;
    }
}
