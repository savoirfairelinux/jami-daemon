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
#include <sndfile.hh>

#include "audiofile.h"
#include "audio/codecs/audiocodec.h"
#include "audio/samplerateconverter.h"
#include "client/callmanager.h"
#include "manager.h"

#include "logger.h"

void
AudioFile::onBufferFinish()
{
    // We want to send values in milisecond
    const int divisor = buffer_->getSampleRate() / 1000;
    if (divisor == 0) {
        ERROR("Error cannot update playback slider, sampling rate is 0");
        return;
    }

    if ((updatePlaybackScale_ % 5) == 0) {
        CallManager *cm = Manager::instance().getClient()->getCallManager();
        cm->updatePlaybackScale(filepath_, pos_ / divisor, buffer_->samples() / divisor);
    }

    updatePlaybackScale_++;
}

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

    if (sampleRate == audioRate) {
        delete buffer_;
        buffer_ = buffer;
    } else {
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
        buffer->deinterleave(scratch, samplesOut, channels);
        delete buffer_;
        buffer_ = buffer;

        delete [] floatBufferOut;
        delete [] floatBufferIn;
        delete [] scratch;
    }
}


WaveFile::WaveFile(const std::string &fileName, unsigned int sampleRate) : AudioFile(fileName, sampleRate)
{
    SndfileHandle fileHandle(fileName.c_str(), SFM_READ);

    if (!fileHandle)
        throw AudioFileException("File " + fileName + " doesn't exist");

    if (fileHandle.channels() > 2)
        throw AudioFileException("Unsupported number of channels");

    const sf_count_t nbFrames = fileHandle.frames();

    SFLAudioSample * tempBuffer = new SFLAudioSample[nbFrames * fileHandle.channels()];

    fileHandle.read(tempBuffer, nbFrames);

    AudioBuffer * buffer = new AudioBuffer(nbFrames, fileHandle.channels(), fileHandle.samplerate());
    buffer->deinterleave(tempBuffer, nbFrames, fileHandle.channels());

    const int rate = static_cast<int32_t>(sampleRate);
    if (fileHandle.samplerate() != rate) {
        SamplerateConverter converter(std::max(fileHandle.samplerate(), rate), fileHandle.channels());
        AudioBuffer * resampled = new AudioBuffer(nbFrames, fileHandle.channels(), rate);
        converter.resample(*buffer, *resampled);
        delete buffer;
        delete buffer_;
        buffer_ = resampled;
    } else {
        delete buffer_;
        buffer_ = buffer;
    }

    delete [] tempBuffer;
}
