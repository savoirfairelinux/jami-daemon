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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

    AudioBuffer * buffer = new AudioBuffer(decodedSize);
    unsigned bufpos = 0;
    unsigned char *filepos = reinterpret_cast<unsigned char *>(&fileBuffer[0]);

    while (length >= encFrameSize) {
        bufpos += audioCodec_->decode(buffer->getData(), filepos, encFrameSize, bufpos);
        filepos += encFrameSize;
        length -= encFrameSize;
    }

    if (sampleRate == audioRate)
        buffer_ = buffer;
    else {
        double factord = (double) sampleRate / audioRate;

        const size_t channels = buffer->channels();

        // FIXME: it looks like buffer and buffer_ are leaked in this case
        if (channels > 2)
            throw AudioFileException("WaveFile: unsupported number of channels");

        size_t samples = buffer->samples();
        size_t size = channels * samples;
        float* floatBufferIn = new float[size];
        buffer->interleaveFloat(floatBufferIn);

        int samplesOut = ceil(factord * samples);
        int sizeOut = samplesOut * channels;

        delete buffer_;

        SRC_DATA src_data;
        src_data.data_in = floatBufferIn;
        src_data.input_frames = samples;
        src_data.output_frames = samplesOut;
        src_data.src_ratio = factord;

        float* floatBufferOut = new float[sizeOut];
        src_data.data_out = floatBufferOut;

        src_simple(&src_data, SRC_SINC_BEST_QUALITY, channels);
        samplesOut = src_data.output_frames_gen;
        sizeOut = samplesOut * channels;

        SFLAudioSample *scratch = new SFLAudioSample[sizeOut];
        src_float_to_short_array(floatBufferOut, scratch, src_data.output_frames_gen);
        buffer->fromInterleaved(scratch, samplesOut, channels);
        buffer_ = buffer;

        delete [] floatBufferOut;
        delete [] floatBufferIn;
        delete [] scratch;
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
    fileStream.read(riff, sizeof riff / sizeof * riff);

    if (strncmp("RIFF", riff, sizeof riff / sizeof * riff) != 0)
        throw AudioFileException("File is not of RIFF format");

    char fmt[4] = { 0, 0, 0, 0 };
    int maxIteration = 10;

    while (maxIteration-- and strncmp("fmt ", fmt, sizeof fmt / sizeof * fmt))
        fileStream.read(fmt, sizeof fmt / sizeof * fmt);

    if (maxIteration == 0)
        throw AudioFileException("Could not find \"fmt \" chunk");

    int32_t chunkSize; // fmt chunk size
    fileStream.read(reinterpret_cast<char *>(&chunkSize), sizeof chunkSize); // Read fmt chunk size.
    unsigned short formatTag; // data compression tag
    fileStream.read(reinterpret_cast<char *>(&formatTag), sizeof formatTag);

    if (formatTag != 1) // PCM = 1, FLOAT = 3
        throw AudioFileException("File contains an unsupported data format type");

    // Get number of channels from the header.
    int16_t chan;
    fileStream.read(reinterpret_cast<char *>(&chan), sizeof chan);

    if (chan > 2)
        throw AudioFileException("WaveFile: unsupported number of channels");

    // Get file sample rate from the header.
    int32_t fileRate;
    fileStream.read(reinterpret_cast<char *>(&fileRate), sizeof fileRate);

    int32_t avgb;
    fileStream.read(reinterpret_cast<char *>(&avgb), sizeof avgb);

    int16_t blockal;
    fileStream.read(reinterpret_cast<char *>(&blockal), sizeof blockal);

    // Determine the data type
    int16_t dt;
    fileStream.read(reinterpret_cast<char *>(&dt), sizeof dt);

    if (dt != 8 && dt != 16 && dt != 32)
        throw AudioFileException("File's bits per sample with is not supported");

    // Find the "data" chunk
    char data[4] = { 0, 0, 0, 0 };
    maxIteration = 10;

    while (maxIteration-- && strncmp("data", data, sizeof data / sizeof * data))
        fileStream.read(data, sizeof data / sizeof * data);

    // Samplerate converter initialized with 88200 sample long
    const int rate = static_cast<int32_t>(sampleRate);
    SamplerateConverter converter(std::max(fileRate, rate), chan);

    // Get length of data from the header.
    int32_t bytes;
    fileStream.read(reinterpret_cast<char *>(&bytes), sizeof bytes);

    // sample frames, should not be longer than a minute
    int nbSamples = std::min(60 * fileRate, 8 * bytes / dt / chan);

    DEBUG("Frame size %ld, data size %d align %d rate %d avgbyte %d "
          "chunk size %d dt %d", nbSamples, bytes, blockal, fileRate, avgb,
          chunkSize, dt);

    SFLAudioSample * tempBuffer = new SFLAudioSample[nbSamples * chan];

    fileStream.read(reinterpret_cast<char *>(tempBuffer),
                    nbSamples * chan * sizeof(SFLAudioSample));

    AudioBuffer * buffer = new AudioBuffer(nbSamples, chan, fileRate);
    buffer->fromInterleaved(tempBuffer, nbSamples, chan);

    if (fileRate != rate) {
        AudioBuffer * resampled = new AudioBuffer(nbSamples, chan, rate);
        converter.resample(*buffer, *resampled);
        delete buffer;
        buffer_ = resampled;
    } else
        buffer_ = buffer;

    delete [] tempBuffer;
}
