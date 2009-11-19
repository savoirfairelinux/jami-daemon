/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 */

#include <stdio.h>
#include <sstream>

#include "audiolayerTest.h"

#include <unistd.h>


using std::cout;
using std::endl;



void AudioLayerTest::setUp()
{

    // Instanciate the manager
    Manager::instance().init();
    Manager::instance().initConfigFile();

    // _audiodriver = Manager::instance().getAudioDriver();

    // std::string alsaPlugin;
    // AlsaLayer *alsalayer;

    // int numCardIn, numCardOut, sampleRate, frameSize;
    // layer = _audiodriver->getLayerType();

    /*
    alsaPlugin = Manager::instance().getConfigString( AUDIO , ALSA_PLUGIN );
    numCardIn  = Manager::instance().getConfigInt( AUDIO , ALSA_CARD_ID_IN );
    numCardOut = Manager::instance().getConfigInt( AUDIO , ALSA_CARD_ID_OUT );
    sampleRate = Manager::instance().getConfigInt( AUDIO , ALSA_SAMPLE_RATE );
    if (sampleRate <=0 || sampleRate > 48000) {
        sampleRate = 44100;
    }
    frameSize = Manager::instance().getConfigInt(AUDIO, ALSA_FRAME_SIZE );
    */

    // get a pointer to the audio layer
    // _audiodriver = Manager::instance().getAudioDriver();

}

void AudioLayerTest::testAudioLayerConfig()
{
    int sampling_rate = Manager::instance().getConfigInt (AUDIO, ALSA_SAMPLE_RATE);
    int frame_size = Manager::instance().getConfigInt (AUDIO, ALSA_FRAME_SIZE);
    frame_size = 0; // frame size in config not used anymore    

    int layer = Manager::instance().getAudioDriver()->getLayerType();

    if(layer != ALSA)
        Manager::instance().switchAudioManager();

    CPPUNIT_ASSERT ( (int) Manager::instance().getAudioDriver()->getSampleRate() == sampling_rate);
    CPPUNIT_ASSERT ( (int) Manager::instance().getAudioDriver()->getFrameSize() == frame_size);
}

void AudioLayerTest::testAudioLayerSwitch()
{

    _debug ("---------- AudioLayerTest::testAudioLayerSwitch ---------------------------");


    int previous_layer = Manager::instance().getAudioDriver()->getLayerType();

    for (int i = 0; i < 2; i++) {
        _debug ("---------- AudioLayerTest::testAudioLayerSwitch - %i -------------",i);
        Manager::instance().switchAudioManager();

        if (previous_layer == ALSA) {
            CPPUNIT_ASSERT (Manager::instance().getAudioDriver()->getLayerType() == PULSEAUDIO);
        } else {
            CPPUNIT_ASSERT (Manager::instance().getAudioDriver()->getLayerType() == ALSA);
        }

        previous_layer = Manager::instance().getAudioDriver()->getLayerType();

        usleep (100000);
    }
}



void AudioLayerTest::testPulseConnect()
{

    _debug ("---------- AudioLayerTest::testPulseConnect ---------------------------");

    ManagerImpl* manager;
    manager = &Manager::instance();

    // _pulselayer = new PulseLayer (manager);
    _pulselayer = (PulseLayer*)Manager::instance().getAudioDriver();

    CPPUNIT_ASSERT (_pulselayer->getLayerType() == PULSEAUDIO);

    std::string alsaPlugin;
    int numCardIn, numCardOut, sampleRate, frameSize;

    alsaPlugin = manager->getConfigString (AUDIO , ALSA_PLUGIN);
    numCardIn  = manager->getConfigInt (AUDIO , ALSA_CARD_ID_IN);
    numCardOut = manager->getConfigInt (AUDIO , ALSA_CARD_ID_OUT);
    sampleRate = manager->getConfigInt (AUDIO , ALSA_SAMPLE_RATE);
    frameSize = manager->getConfigInt (AUDIO, ALSA_FRAME_SIZE);

    CPPUNIT_ASSERT (_pulselayer->getPlaybackStream() == NULL);
    CPPUNIT_ASSERT (_pulselayer->getRecordStream() == NULL);

    _pulselayer->setErrorMessage (-1);

    try {
        CPPUNIT_ASSERT (_pulselayer->openDevice (numCardIn, numCardOut, sampleRate, frameSize, SFL_PCM_BOTH, alsaPlugin) == true);
    } catch (...) {
        _debug ("Exception occured wile opening device! ");
    }

    usleep (100000);

    CPPUNIT_ASSERT (_pulselayer->getPlaybackStream() == NULL);
    CPPUNIT_ASSERT (_pulselayer->getRecordStream() == NULL);

    // CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->pulseStream() != NULL);
    // CPPUNIT_ASSERT (_pulselayer->getRecordStream()->pulseStream() != NULL);

    // Must return Access failure "PA_ERR_ACCESS" == 2
    // CPPUNIT_ASSERT (_pulselayer->getPlaybackStream()->getStreamState() == 2);
    // CPPUNIT_ASSERT (_pulselayer->getRecordStream()->getStreamState() == 2);
    _debug("-------------------------- \n");
    _pulselayer->startStream ();

    // usleep(1000000);

 
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

    // usleep(1000000);
    CPPUNIT_ASSERT (_pulselayer->disconnectAudioStream() == true);
    
}


void AudioLayerTest::testAlsaConnect()
{

    _debug ("---------- AudioLayerTest::testAlsaConnect ---------------------------");

    int layer = Manager::instance().getAudioDriver()->getLayerType();

    std::string alsaPlugin;

    if (layer != ALSA) {
        Manager::instance().switchAudioManager();
        usleep (100000);
    }

    // _audiolayer = Manager::instance().getAudioDriver();

    // CPPUNIT_ASSERT(_audiolayer->closeLayer() == true);
    // usleep(100000);

    // delete _audiolayer; _audiolayer == NULL;

    Manager::instance().setConfig (PREFERENCES, CONFIG_AUDIO, ALSA);


    // _audiolayer->setErrorMessage(-1);
    // CPPUNIT_ASSERT(Manager::instance().initAudioDriver() == true);

    // _audiolayer = Manager::instance().getAudioDriver();

    // CPPUNIT_ASSERT(_audiolayer->getLayerType() == ALSA);

}

void AudioLayerTest::tearDown()
{
    // Delete the audio recorder module
    // delete _ar; _ar = NULL;
}
