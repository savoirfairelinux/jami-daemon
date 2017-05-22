/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include <iostream>
#include <iterator>
#include <algorithm>
#include <math.h>

#include "resamplertest.h"

ResamplerTest::ResamplerTest() :
    CppUnit::TestCase("Resampler module test"), inputBuffer(MAX_BUFFER_LENGTH, AudioFormat::MONO), outputBuffer(MAX_BUFFER_LENGTH, AudioFormat::MONO)
{}

void ResamplerTest::setUp()
{

}

void ResamplerTest::tearDown()
{

}

namespace {
    template <typename T>
    void print_buffer(T &buffer)
    {
#ifdef VERBOSE
        std::copy(buffer.begin(), buffer.end(),
                std::ostream_iterator<SFLAudioSample>(std::cout, ", "));
        std::cout << std::endl;
#endif
    }
}

void ResamplerTest::testUpsamplingRamp()
{
    // generate input samples and store them in inputBuffer
    generateRamp();

    std::cout << "Test Upsampling Ramp" << std::endl;
    Resampler resampler(44100);

    performUpsampling(resampler);

    AudioBuffer tmpInputBuffer(TMP_LOWSMPLR_BUFFER_LENGTH, AudioFormat::MONO);
    AudioBuffer tmpOutputBuffer(TMP_HIGHSMPLR_BUFFER_LENGTH, AudioFormat(16000, 1));

    tmpInputBuffer.copy(inputBuffer);
    std::cout << "Input Buffer" << std::endl;
    print_buffer(*tmpInputBuffer.getChannel(0));

    tmpOutputBuffer.copy(outputBuffer);
    std::cout << "Output Buffer" << std::endl;
    print_buffer(*tmpOutputBuffer.getChannel(0));
}

void ResamplerTest::testDownsamplingRamp()
{
    generateRamp();

    std::cout << "Test Downsampling Ramp" << std::endl;
    Resampler resampler(44100);

    performDownsampling(resampler);

    AudioBuffer tmpInputBuffer(TMP_HIGHSMPLR_BUFFER_LENGTH, AudioFormat(16000, 1));
    AudioBuffer tmpOutputBuffer(TMP_LOWSMPLR_BUFFER_LENGTH, AudioFormat::MONO);

    tmpInputBuffer.copy(inputBuffer);
    std::cout << "Input Buffer" << std::endl;
    print_buffer(*tmpInputBuffer.getChannel(0));

    tmpOutputBuffer.copy(outputBuffer);
    std::cout << "Output Buffer" << std::endl;
    print_buffer(*tmpOutputBuffer.getChannel(0));
}

void ResamplerTest::testUpsamplingTriangle()
{
    generateTriangularSignal();

    std::cout << "Test Upsampling Triangle" << std::endl;
    Resampler resampler(44100);

    performUpsampling(resampler);

    AudioBuffer tmpInputBuffer(TMP_LOWSMPLR_BUFFER_LENGTH, AudioFormat::MONO);
    AudioBuffer tmpOutputBuffer(TMP_HIGHSMPLR_BUFFER_LENGTH, AudioFormat(16000, 1));

    tmpInputBuffer.copy(inputBuffer);
    std::cout << "Input Buffer" << std::endl;
    print_buffer(*tmpInputBuffer.getChannel(0));

    tmpOutputBuffer.copy(outputBuffer);
    std::cout << "Output Buffer" << std::endl;
    print_buffer(*tmpOutputBuffer.getChannel(0));
}

void ResamplerTest::testDownsamplingTriangle()
{
    generateTriangularSignal();

    std::cout << "Test Downsampling Triangle" << std::endl;
    Resampler resampler(44100);

    performDownsampling(resampler);

    AudioBuffer tmpInputBuffer(TMP_HIGHSMPLR_BUFFER_LENGTH, AudioFormat(16000, 1));
    AudioBuffer tmpOutputBuffer(TMP_LOWSMPLR_BUFFER_LENGTH, AudioFormat::MONO);

    tmpInputBuffer.copy(inputBuffer);
    std::cout << "Input Buffer" << std::endl;
    print_buffer(*tmpInputBuffer.getChannel(0));

    tmpOutputBuffer.copy(outputBuffer);
    std::cout << "Output Buffer" << std::endl;
    print_buffer(*tmpOutputBuffer.getChannel(0));
}
void ResamplerTest::testUpsamplingSine()
{
    // generate input samples and store them in inputBuffer
    generateSineSignal();

    std::cout << "Test Upsampling Sine" << std::endl;
    Resampler resampler(44100);

    performUpsampling(resampler);

    AudioBuffer tmpInputBuffer(TMP_LOWSMPLR_BUFFER_LENGTH, AudioFormat::MONO);
    AudioBuffer tmpOutputBuffer(TMP_HIGHSMPLR_BUFFER_LENGTH, AudioFormat(16000, 1));

    tmpInputBuffer.copy(inputBuffer);
    std::cout << "Input Buffer" << std::endl;
    print_buffer(*tmpInputBuffer.getChannel(0));

    tmpOutputBuffer.copy(outputBuffer);
    std::cout << "Output Buffer" << std::endl;
    print_buffer(*tmpOutputBuffer.getChannel(0));
}

void ResamplerTest::testDownsamplingSine()
{
    // generate input samples and store them in inputBuffer
    generateSineSignal();

    std::cout << "Test Downsampling Sine" << std::endl;
    Resampler resampler(44100);

    performDownsampling(resampler);

    AudioBuffer tmpInputBuffer(TMP_HIGHSMPLR_BUFFER_LENGTH, AudioFormat(16000, 1));
    AudioBuffer tmpOutputBuffer(TMP_LOWSMPLR_BUFFER_LENGTH, AudioFormat::MONO);

    tmpInputBuffer.copy(inputBuffer);
    std::cout << "Input Buffer" << std::endl;
    print_buffer(*tmpInputBuffer.getChannel(0));

    tmpOutputBuffer.copy(outputBuffer);
    std::cout << "Output Buffer" << std::endl;
    print_buffer(*tmpOutputBuffer.getChannel(0));
}

void ResamplerTest::generateRamp()
{
    std::vector<SFLAudioSample>* buf = inputBuffer.getChannel(0);
    for (size_t i = 0; i < buf->size(); ++i)
        (*buf)[i] = i;
}

void ResamplerTest::generateTriangularSignal()
{
    std::vector<SFLAudioSample>* buf = inputBuffer.getChannel(0);
    for (size_t i = 0; i < buf->size(); ++i)
        (*buf)[i] = i * 10;
}

void ResamplerTest::generateSineSignal()
{
    std::vector<SFLAudioSample>* buf = inputBuffer.getChannel(0);
    for (size_t i = 0; i < buf->size(); ++i)
        (*buf)[i] = (SFLAudioSample) (1000.0 * sin(i));
}

void ResamplerTest::performUpsampling(Resampler &resampler)
{
    AudioBuffer tmpInputBuffer(TMP_LOWSMPLR_BUFFER_LENGTH, AudioFormat::MONO);
    AudioBuffer tmpOutputBuffer(TMP_HIGHSMPLR_BUFFER_LENGTH, AudioFormat(16000, 1));

    for (size_t i = 0, j = 0; i < (inputBuffer.frames() / 2); i += tmpInputBuffer.frames(), j += tmpOutputBuffer.frames()) {
        tmpInputBuffer.copy(inputBuffer, i);
        resampler.resample(tmpInputBuffer, tmpOutputBuffer);
        outputBuffer.copy(tmpOutputBuffer, -1, 0, j);
    }
}

void ResamplerTest::performDownsampling(Resampler &resampler)
{
    AudioBuffer tmpInputBuffer(TMP_HIGHSMPLR_BUFFER_LENGTH, AudioFormat(16000, 1));
    AudioBuffer tmpOutputBuffer(TMP_LOWSMPLR_BUFFER_LENGTH, AudioFormat::MONO);

    for (size_t i = 0, j = 0; i < inputBuffer.frames(); i += tmpInputBuffer.frames(), j += tmpOutputBuffer.frames()) {
        tmpInputBuffer.copy(inputBuffer, i);
        resampler.resample(tmpInputBuffer, tmpOutputBuffer);
        outputBuffer.copy(tmpOutputBuffer, -1, 0, j);
    }
}
