/*  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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
#include <cmath>
#include <samplerate.h>
#include <cstring>
#include <vector>
#include <climits>

#include "audiofile.h"
#include "audio/codecs/audiocodec.h"
#include "audio/samplerateconverter.h"
#include "logger.h"

RawFile::RawFile(const std::string& name, sfl::AudioCodec *codec, unsigned int sampleRate)
    : AudioFile(name, sampleRate), audioCodec_(codec)
{
    if (filepath_.empty())
        throw AudioFileException("Unable to open audio file: filename is empty");

    std::fstream file;
    file.open(filepath_.c_str(), std::fstream::in);

    if (!file.is_open())
        throw AudioFileException("Unable to open audio file");

    file.seekg(0, std::ios::end);
    size_t length = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> fileBuffer(length);
    file.read(&fileBuffer[0], length);
    file.close();

    const unsigned int frameSize = audioCodec_->getFrameSize();
    const unsigned int bitrate   = audioCodec_->getBitRate() * 1000 / 8;
    const unsigned int audioRate = audioCodec_->getClockRate();
    const unsigned int encFrameSize = frameSize * bitrate / audioRate;
    const unsigned int decodedSize = length * (frameSize / encFrameSize);

    SFLDataFormat *monoBuffer = new SFLDataFormat[decodedSize];
    SFLDataFormat *bufpos = monoBuffer;
    unsigned char *filepos = reinterpret_cast<unsigned char *>(&fileBuffer[0]);
    size_ = decodedSize;

    while (length >= encFrameSize) {
        bufpos += audioCodec_->decode(bufpos, filepos, encFrameSize);
        filepos += encFrameSize;
        length -= encFrameSize;
    }

    if (sampleRate == audioRate)
        buffer_ = monoBuffer;
    else {
        double factord = (double) sampleRate / audioRate;
        float* floatBufferIn = new float[size_];
        int    sizeOut  = ceil(factord * size_);
        src_short_to_float_array(monoBuffer, floatBufferIn, size_);
        delete [] monoBuffer;
        delete [] buffer_;
        buffer_ = new SFLDataFormat[sizeOut];

        SRC_DATA src_data;
        src_data.data_in = floatBufferIn;
        src_data.input_frames = size_;
        src_data.output_frames = sizeOut;
        src_data.src_ratio = factord;

        float* floatBufferOut = new float[sizeOut];
        src_data.data_out = floatBufferOut;

        src_simple(&src_data, SRC_SINC_BEST_QUALITY, 1);
        src_float_to_short_array(floatBufferOut, buffer_, src_data.output_frames_gen);

        delete [] floatBufferOut;
        delete [] floatBufferIn;
        size_ = src_data.output_frames_gen;
    }
}


WaveFile::WaveFile(const std::string &fileName, unsigned int sampleRate) : AudioFile(fileName, sampleRate)
{
    const std::fstream fs(fileName.c_str(), std::ios_base::in);

    if (!fs)
        throw AudioFileException("File " + fileName + " doesn't exist");

    std::fstream fileStream;
    fileStream.open(fileName.c_str(), std::ios::in | std::ios::binary);

    char riff[4] = { 0, 0, 0, 0 };
    fileStream.read(riff, sizeof riff / sizeof *riff);

    if (strncmp("RIFF", riff, sizeof riff / sizeof *riff) != 0)
        throw AudioFileException("File is not of RIFF format");

    char fmt[4] = { 0, 0, 0, 0 };
    int maxIteration = 10;

    while (maxIteration-- and strncmp("fmt ", fmt, sizeof fmt / sizeof *fmt))
        fileStream.read(fmt, sizeof fmt / sizeof *fmt);

    if (maxIteration == 0)
        throw AudioFileException("Could not find \"fmt \" chunk");

    SINT32 chunkSize; // fmt chunk size
    fileStream.read(reinterpret_cast<char *>(&chunkSize), sizeof chunkSize); // Read fmt chunk size.
    unsigned short formatTag; // data compression tag
    fileStream.read(reinterpret_cast<char *>(&formatTag), sizeof formatTag);

    if (formatTag != 1) // PCM = 1, FLOAT = 3
        throw AudioFileException("File contains an unsupported data format type");

    // Get number of channels from the header.
    SINT16 chan;
    fileStream.read(reinterpret_cast<char *>(&chan), sizeof chan);

    if (chan > 2)
        throw AudioFileException("WaveFile: unsupported number of channels");

    // Get file sample rate from the header.
    SINT32 fileRate;
    fileStream.read(reinterpret_cast<char *>(&fileRate), sizeof fileRate);

    SINT32 avgb;
    fileStream.read(reinterpret_cast<char *>(&avgb), sizeof avgb);

    SINT16 blockal;
    fileStream.read(reinterpret_cast<char *>(&blockal), sizeof blockal);

    // Determine the data type
    SINT16 dt;
    fileStream.read(reinterpret_cast<char *>(&dt), sizeof dt);

    if (dt != 8 && dt != 16 && dt != 32)
        throw AudioFileException("File's bits per sample with is not supported");

    // Find the "data" chunk
    char data[4] = { 0, 0, 0, 0 };
    maxIteration = 10;

    while (maxIteration-- && strncmp("data", data, sizeof data / sizeof *data))
        fileStream.read(data, sizeof data / sizeof *data);

    // Samplerate converter initialized with 88200 sample long
    const int rate = static_cast<SINT32>(sampleRate_);
    SamplerateConverter converter(std::max(fileRate, rate));

    // Get length of data from the header.
    SINT32 bytes;
    fileStream.read(reinterpret_cast<char *>(&bytes), sizeof bytes);
    
    // sample frames, should not be longer than a minute
    int nbSamples = std::min(60 * fileRate, 8 * bytes / dt / chan);

    DEBUG("Frame size %ld, data size %d align %d rate %d avgbyte %d "
          "chunk size %d dt %d", nbSamples, bytes, blockal, fileRate, avgb,
          chunkSize, dt);

    size_ = nbSamples;
    SFLDataFormat *tempBuffer = new SFLDataFormat[size_];

    fileStream.read(reinterpret_cast<char *>(tempBuffer),
                    nbSamples * sizeof(SFLDataFormat));

    // mix two channels together if stereo
    if (chan == 2) {
        for (int i = 0, j = 0; i < nbSamples - 1; i += 2, ++j)
            tempBuffer[j] = (tempBuffer[i] + tempBuffer[i + 1]) * 0.5;
        nbSamples *= 0.5;
    }

    if (fileRate != rate) {
        const float ratio = sampleRate_ / (float) fileRate;
        const int outSamples = ceil(nbSamples * ratio);
        size_ = outSamples;
        buffer_ = new SFLDataFormat[size_];
        converter.resample(tempBuffer, buffer_, size_, fileRate, sampleRate_, nbSamples);
        delete [] tempBuffer;
    } else
        buffer_ = tempBuffer;
}
