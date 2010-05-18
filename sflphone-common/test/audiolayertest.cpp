/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <unistd.h>

using std::cout;
using std::endl;

void AudioLayerTest::testAudioLayerConfig() {
	_debug ("-------------------- AudioLayerTest::testAudioLayerConfig --------------------\n");

	int sampling_rate = Manager::instance().getConfigInt(AUDIO,
			AUDIO_SAMPLE_RATE);
	int frame_size = Manager::instance().getConfigInt(AUDIO, ALSA_FRAME_SIZE);

	int layer = Manager::instance().getAudioDriver()->getLayerType();

	// if (layer != ALSA)
	// 	Manager::instance().switchAudioManager();

	// TODO: Fix tests
	//CPPUNIT_ASSERT ( (int) Manager::instance().getAudioDriver()->getSampleRate() == sampling_rate);

	//CPPPUNIT_ASSERT ( (int) Manager::instance().getAudioDriver()->getFrameSize() == frame_size);
}

void AudioLayerTest::testAudioLayerSwitch() {
	_debug ("-------------------- AudioLayerTest::testAudioLayerSwitch --------------------\n");

	int previous_layer = Manager::instance().getAudioDriver()->getLayerType();

	for (int i = 0; i < 2; i++) {
		_debug ("iter - %i",i);
		Manager::instance().switchAudioManager();

		if (previous_layer == ALSA) {
			CPPUNIT_ASSERT (Manager::instance().getAudioDriver()->getLayerType() == PULSEAUDIO);
		} else {
			CPPUNIT_ASSERT (Manager::instance().getAudioDriver()->getLayerType() == ALSA);
		}

		previous_layer = Manager::instance().getAudioDriver()->getLayerType();

		usleep(100000);
	}
}

void AudioLayerTest::testPulseConnect() {
	_debug ("-------------------- AudioLayerTest::testPulseConnect --------------------\n");

	if (Manager::instance().getAudioDriver()->getLayerType() == ALSA)
		return;

	ManagerImpl* manager;
	manager = &Manager::instance();

	_pulselayer = (PulseLayer*) Manager::instance().getAudioDriver();

	CPPUNIT_ASSERT (_pulselayer->getLayerType() == PULSEAUDIO);

	std::string alsaPlugin;
	int numCardIn, numCardOut, sampleRate, frameSize;

	alsaPlugin = manager->getConfigString(AUDIO, ALSA_PLUGIN);
	numCardIn = manager->getConfigInt(AUDIO, ALSA_CARD_ID_IN);
	numCardOut = manager->getConfigInt(AUDIO, ALSA_CARD_ID_OUT);
	sampleRate = manager->getConfigInt(AUDIO, AUDIO_SAMPLE_RATE);
	frameSize = manager->getConfigInt(AUDIO, ALSA_FRAME_SIZE);

	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream() == NULL);
	CPPUNIT_ASSERT (_pulselayer->getRecordStream() == NULL);

	_pulselayer->setErrorMessage(-1);

	try {
		CPPUNIT_ASSERT (_pulselayer->openDevice (numCardIn, numCardOut, sampleRate, frameSize, SFL_PCM_BOTH, alsaPlugin) == true);
	} catch (...) {
		_debug ("Exception occured wile opening device! ");
	}

	usleep(100000);

	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream() == NULL);
	CPPUNIT_ASSERT (_pulselayer->getRecordStream() == NULL);

	_debug ("-------------------------- \n");
	_pulselayer->startStream();

	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->pulseStream() != NULL);
	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->pulseStream() != NULL);

	// Must return No error "PA_OK" == 1
	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->getStreamState() == 1);
	CPPUNIT_ASSERT (_pulselayer->getRecordStream()->getStreamState() == 1);

	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->disconnectStream() == true);
	CPPUNIT_ASSERT (_pulselayer->getRecordStream()->disconnectStream() == true);

	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->connectStream() == true);
	CPPUNIT_ASSERT (_pulselayer->getRecordStream()->connectStream() == true);

	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->getStreamState() == 1);
	CPPUNIT_ASSERT (_pulselayer->getRecordStream()->getStreamState() == 1);

	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->connectStream() == true);
	CPPUNIT_ASSERT (_pulselayer->getRecordStream()->connectStream() == true);

	CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->getStreamState() == 1);
	CPPUNIT_ASSERT (_pulselayer->getRecordStream()->getStreamState() == 1);

	CPPUNIT_ASSERT (_pulselayer->disconnectAudioStream() == true);
}
