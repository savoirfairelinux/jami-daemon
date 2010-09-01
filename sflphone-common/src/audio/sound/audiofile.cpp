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
#include "audio/samplerateconverter.h"
#include <fstream>
#include <math.h>
#include <samplerate.h>
#include <cstring>



RawFile::RawFile() : _filename()
        , _codec (NULL)
{
    AudioFile::_start = false;
}

RawFile::~RawFile()
{
}

// load file in mono format
bool
RawFile::loadFile (const std::string& filename, AudioCodec* codec , unsigned int sampleRate=8000)
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




WaveFile::WaveFile () : _byte_counter (0)
        , _nb_channels (1)
        , _file_size (0)
        , _data_offset (0)
        , _channels (0)
        , _data_type (0)
        , _file_rate (0)
{
    AudioFile::_start = false;
}


WaveFile::~WaveFile()
{
    _debug ("WaveFile: Destructor Called!");
}



bool WaveFile::openFile (const std::string& fileName, int audioSamplingRate)
{
    if (isFileExist (fileName)) {
        openExistingWaveFile (fileName, audioSamplingRate);
    }

    return true;
}



bool WaveFile::closeFile()
{

    _file_stream.close();

    return true;

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


bool WaveFile::isFileOpened()
{

    if (_file_stream.is_open()) {
        _debug ("WaveFile: file is openened");
        return true;
    } else {
        _debug ("WaveFile: file is not openend");
        return false;
    }
}


bool WaveFile::openExistingWaveFile (const std::string& fileName, int audioSamplingRate)
{

    _debug ("WaveFile: Opening %s", fileName.c_str());

    _file_stream.open (fileName.c_str(), std::ios::in | std::ios::binary);

    char riff[4] = {};

    _file_stream.read (riff, 4);

    if (strncmp ("RIFF", riff, 4) != 0) {
        _debug ("WaveFile: File is not of RIFF format");
        return false;
    }

    // Find the "fmt " chunk
    char fmt[4] = {};

    while (strncmp ("fmt ", fmt, 4) != 0) {
        _file_stream.read (fmt, 4);
        _debug ("Searching... \"fmt \"");
    }

    SINT32 chunk_size;         // fmt chunk size
    unsigned short format_tag; // data compression tag

    _file_stream.read ( (char*) &chunk_size, 4); // Read fmt chunk size.
    _file_stream.read ( (char*) &format_tag, 2);

    _debug ("WaveFile: Chunk size: %d", chunk_size);
    _debug ("WaveFile: Format tag: %d", format_tag);


    if (format_tag != 1) { // PCM = 1, FLOAT = 3
        _debug ("WaveFile: File contains an unsupported data format type");
        return false;
    }



    // Get number of channels from the header.
    SINT16 chan;
    _file_stream.read ( (char*) &chan, 2);

    _channels = chan;

    _debug ("WaveFile: Channel %d", _channels);


    // Get file sample rate from the header.
    SINT32 srate;
    _file_stream.read ( (char*) &srate, 4);

    _file_rate = (double) srate;

    _debug ("WaveFile: Sampling rate %d", srate);

    SINT32 avgb;
    _file_stream.read ( (char*) &avgb, 4);

    _debug ("WaveFile: Average byte %d", avgb);

    SINT16 blockal;
    _file_stream.read ( (char*) &blockal, 2);

    _debug ("WaveFile: Block alignment %d", blockal);


    // Determine the data type
    _data_type = 0;

    SINT16 dt;
    _file_stream.read ( (char*) &dt, 2);

    _debug ("WaveFile: dt %d", dt);


    if (format_tag == 1) {
        if (dt == 8)
            _data_type = 1; // SINT8;
        else if (dt == 16)
            _data_type = 2; // SINT16;
        else if (dt == 32)
            _data_type = 3; // SINT32;
    }
    /*
      else if ( format_tag == 3 )
      {
        if (temp == 32)
          dataType_ = FLOAT32;
        else if (temp == 64)
          dataType_ = FLOAT64;
      }
    */
    else {
        _debug ("WaveFile: File's bits per sample with is not supported");
        return false;
    }


    // Find the "data" chunk
    char data[4] = {};

    while (strncmp ("data", data, 4)) {
        _file_stream.read (data, 4);
        _debug ("Searching... data");
    }


    // Sample rate converter initialized with 88200 sample long
    int converterSamples  = (srate > audioSamplingRate) ? srate : audioSamplingRate;
    SamplerateConverter _converter (converterSamples, 2000);

    int nbSampleMax = 512;

    // Get length of data from the header.
    SINT32 bytes;
    _file_stream.read ( (char*) &bytes, 4);

    _debug ("WaveFile: data size in byte %d", bytes);

    _file_size = 8 * bytes / dt / _channels;  // sample frames

    _debug ("WaveFile: data size in frame %ld", _file_size);

    // Should not be longer than a minute
    if (_file_size > (unsigned int) (60*srate))
        _file_size = 60*srate;

    SFLDataFormat *tempBuffer = new SFLDataFormat[_file_size];

    if (!tempBuffer)
        return false;

    SFLDataFormat *tempBufferRsmpl = NULL;

    _file_stream.read ( (char *) tempBuffer, _file_size*sizeof (SFLDataFormat));

    // compute size of final buffer
    int nbSample;

    if (srate != audioSamplingRate) {
        nbSample = (int) ( (float) _file_size * ( (float) audioSamplingRate / (float) srate));
    } else
        nbSample = _file_size;

    int totalprocessed = 0;

    // require resampling
    if (srate != audioSamplingRate) {

        // initialize remaining samples to process
        int remainingSamples = _file_size;

        tempBufferRsmpl = new SFLDataFormat[nbSample];

        if (!tempBufferRsmpl)
            return false;

        SFLDataFormat *in = tempBuffer;
        SFLDataFormat *out = tempBufferRsmpl;

        while (remainingSamples > 0) {

            int toProcess = remainingSamples > nbSampleMax ? nbSampleMax : remainingSamples;
            int nbSamplesConverted = 0;

            if (srate < audioSamplingRate) {
                nbSamplesConverted = _converter.upsampleData (in, out, srate, audioSamplingRate, toProcess);
            } else if (srate > audioSamplingRate) {
                nbSamplesConverted = _converter.downsampleData (in, out, audioSamplingRate, srate, toProcess);
            }

            // nbSamplesConverted = nbSamplesConverted*2;

            in += toProcess;
            out += nbSamplesConverted;
            remainingSamples -= toProcess;
            totalprocessed += nbSamplesConverted;
        }
    }

    // Init audio loop buffer info
    _buffer = new SFLDataFormat[nbSample];

    if (!_buffer)
        return false;

    _size = nbSample;
    _sampleRate = (int) audioSamplingRate;

    // Copy audio into audioloop
    if (srate != audioSamplingRate)
        memcpy ( (void *) _buffer, (void *) tempBufferRsmpl, nbSample*sizeof (SFLDataFormat));
    else
        memcpy ( (void *) _buffer, (void *) tempBuffer, nbSample*sizeof (SFLDataFormat));


    _debug ("WaveFile: file successfully opened");

    delete[] tempBuffer;

    if (tempBufferRsmpl)
        delete[] tempBufferRsmpl;

    return true;

}


bool WaveFile::loadFile (const std::string& filename, AudioCodec *codec , unsigned int sampleRate)
{

    openFile (filename, sampleRate);

    return true;
}
