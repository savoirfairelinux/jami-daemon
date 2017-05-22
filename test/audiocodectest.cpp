/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "audiocodectest.h"
#include "audio/codecs/audiocodecfactory.h"

#include "test_utils.h"
#include "sfl_types.h" // for SFLAudioSample

#include <cmath>
#include <climits>

/*
 * Detect the power of a signal for a given frequency.
 * Adapted from:
 * http://netwerkt.wordpress.com/2011/08/25/goertzel-filter/
 */
static double
goertzelFilter(SFLAudioSample *samples, double freq, unsigned N, double sample_rate)
{
    double s_prev = 0.0;
    double s_prev2 = 0.0;
    const double normalizedfreq = freq / sample_rate;
    double coeff = 2 * cos(M_2_PI * normalizedfreq);
    for (unsigned i = 0; i < N; i++) {
        double s = samples[i] + coeff * s_prev - s_prev2;
        s_prev2 = s_prev;
        s_prev = s;
    }

    return s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2;
}

void AudioCodecTest::testCodecs()
{
    TITLE();

    AudioCodecFactory factory;
    const auto payloadTypes = factory.getCodecList();

    std::vector<sfl::AudioCodec *> codecs;

    for (auto p : payloadTypes)
        codecs.push_back(factory.getCodec(p));

    std::vector<std::vector<SFLAudioSample>> sine = {};
    std::vector<std::vector<SFLAudioSample>> pcm;

    unsigned sampleRate = 0;
    double referencePower = 0.0;

    for (auto c : codecs) {

        // generate the sine tone if rate has changed
        if (sampleRate != c->getCurrentClockRate()) {
            sampleRate = c->getCurrentClockRate();
            const unsigned nbSamples = sampleRate * 0.02; // 20 ms worth of samples
            sine = {std::vector<SFLAudioSample>(nbSamples)};
            pcm = {std::vector<SFLAudioSample>(nbSamples)};

            const float theta = M_2_PI * frequency_ / sampleRate;

            for (unsigned i = 0; i < nbSamples; ++i) {
                sine[0][i] = SHRT_MAX * sin(theta * i);
                sine[0][i] >>= 3; /* attenuate it a bit */
            }

            /* Store the raw signal's power detected at 440 Hz, this is much cheaper
             * than an FFT */
            referencePower = goertzelFilter(sine[0].data(), frequency_, sine[0].size(), sampleRate);
        }

        std::vector<uint8_t> data(RAW_BUFFER_SIZE);

        const size_t encodedBytes = c->encode(sine, data.data(), sine[0].size());

        unsigned decoded = c->decode(pcm, data.data(), encodedBytes);
        CPPUNIT_ASSERT(decoded == sine[0].size());

        const auto decodedPower = goertzelFilter(pcm[0].data(), frequency_, pcm[0].size(), sampleRate);
        const auto decodedRatio = decodedPower / referencePower;
        CPPUNIT_ASSERT(decodedRatio > 0.0);
    }
}
