/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <stdio.h>
#include <sstream>

#include "audiolayertest.h"
#include "audio/pulseaudio/audiostream.h"

#include <unistd.h>

using std::cout;
using std::endl;

void AudioLayerTest::testAudioLayerConfig()
{
    _debug ("-------------------- AudioLayerTest::testAudioLayerConfig --------------------\n");

    CPPUNIT_ASSERT( Manager::instance().audioPreference.getSmplrate() == 44100);

    // alsa preferences
    CPPUNIT_ASSERT( Manager::instance().audioPreference.getCardin() == 0);
    CPPUNIT_ASSERT( Manager::instance().audioPreference.getCardout() == 0);
    CPPUNIT_ASSERT( Manager::instance().audioPreference.getCardring() == 0);
    CPPUNIT_ASSERT( Manager::instance().audioPreference.getPlugin() == "default");

    // pulseaudio preferences
    CPPUNIT_ASSERT( Manager::instance().audioPreference.getDevicePlayback() == "alsa_output.pci-0000_00_1b.0.analog-stereo");
    CPPUNIT_ASSERT( Manager::instance().audioPreference.getDeviceRecord() == "alsa_input.pci-0000_00_1b.0.analog-stereo");
    CPPUNIT_ASSERT( Manager::instance().audioPreference.getDeviceRingtone() == "alsa_output.pci-0000_00_1b.0.analog-stereo");

    CPPUNIT_ASSERT( Manager::instance().audioPreference.getVolumemic() == 100);
    CPPUNIT_ASSERT( Manager::instance().audioPreference.getVolumespkr() == 100);

    // TODO: Fix tests
    //CPPUNIT_ASSERT ( (int) Manager::instance().getAudioDriver()->getSampleRate() == sampling_rate);
}

void AudioLayerTest::testAudioLayerSwitch()
{
    _debug ("-------------------- AudioLayerTest::testAudioLayerSwitch --------------------\n");

    bool wasAlsa = dynamic_cast<AlsaLayer*>(Manager::instance().getAudioDriver()) != 0;

    for (int i = 0; i < 2; i++) {
        _debug ("iter - %i", i);
        Manager::instance().switchAudioManager();

        if (wasAlsa)
            CPPUNIT_ASSERT (dynamic_cast<PulseLayer*>(Manager::instance().getAudioDriver()));
        else
            CPPUNIT_ASSERT (dynamic_cast<AlsaLayer*>(Manager::instance().getAudioDriver()));

        wasAlsa = dynamic_cast<AlsaLayer*>(Manager::instance().getAudioDriver()) != 0;
        usleep (100000);
    }
}

void AudioLayerTest::testPulseConnect()
{
    _debug ("-------------------- AudioLayerTest::testPulseConnect --------------------\n");

    if (dynamic_cast<AlsaLayer*>(Manager::instance().getAudioDriver())) {
        Manager::instance().switchAudioManager();
    	usleep (100000);
    }

    _pulselayer = dynamic_cast<PulseLayer*>(Manager::instance().getAudioDriver());

    CPPUNIT_ASSERT (_pulselayer);
}
