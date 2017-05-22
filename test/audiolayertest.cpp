/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include "audiolayertest.h"

#include "logger.h"
#include "manager.h"
#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"
#include "test_utils.h"
#include <unistd.h>

AudioLayerTest::AudioLayerTest() : manager_(0), pulselayer_(0), layer_(0)
{}

void AudioLayerTest::testAudioLayerConfig()
{
    TITLE();

    CPPUNIT_ASSERT(Manager::instance().audioPreference.getAlsaSmplrate() == 44100);

    // alsa preferences
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getAlsaCardin() == 0);
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getAlsaCardout() == 0);
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getAlsaCardring() == 0);
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getAlsaPlugin() == "default");

    // pulseaudio preferences
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getPulseDevicePlayback() == "alsa_output.pci-0000_00_1b.0.analog-stereo");
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getPulseDeviceRecord() == "alsa_input.pci-0000_00_1b.0.analog-stereo");
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getPulseDeviceRingtone() == "alsa_output.pci-0000_00_1b.0.analog-stereo");

    CPPUNIT_ASSERT(Manager::instance().audioPreference.getVolumemic() == 1.0);
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getVolumespkr() == 1.0);

    // TODO: Fix tests
    //CPPUNIT_ASSERT ( (int) Manager::instance().getAudioDriver()->getSampleRate() == sampling_rate);
}

void AudioLayerTest::testAudioLayerSwitch()
{
    TITLE();

    bool wasAlsa = dynamic_cast<AlsaLayer*>(Manager::instance().getAudioDriver()) != 0;

    for (int i = 0; i < 2; i++) {
        DEBUG("iter - %i", i);
        if (wasAlsa)
            Manager::instance().setAudioManager(PULSEAUDIO_API_STR);
        else
            Manager::instance().setAudioManager(ALSA_API_STR);

        if (wasAlsa)
            CPPUNIT_ASSERT(dynamic_cast<PulseLayer*>(Manager::instance().getAudioDriver()));
        else
            CPPUNIT_ASSERT(dynamic_cast<AlsaLayer*>(Manager::instance().getAudioDriver()));

        wasAlsa = dynamic_cast<AlsaLayer*>(Manager::instance().getAudioDriver()) != 0;
        const struct timespec req = {0, 100000000};
        nanosleep(&req, 0);
    }
}

void AudioLayerTest::testPulseConnect()
{
    TITLE();

    if (dynamic_cast<AlsaLayer*>(Manager::instance().getAudioDriver())) {
        Manager::instance().setAudioManager(PULSEAUDIO_API_STR);
        const struct timespec req = {0, 100000000};
        nanosleep(&req, 0);
    }

    pulselayer_ = dynamic_cast<PulseLayer*>(Manager::instance().getAudioDriver());

    CPPUNIT_ASSERT(pulselayer_);
}
