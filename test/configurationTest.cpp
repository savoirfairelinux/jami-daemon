/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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

#include "configurationTest.h"

using std::cout;
using std::endl;

void ConfigurationTest::setUp(){
    // Load the default configuration
    Manager::instance().initConfigFile(false);
}

void ConfigurationTest::testDefaultValueAudio(){
    CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_CARD_ID_IN ) == ALSA_DFT_CARD) ;
    CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_CARD_ID_OUT ) == ALSA_DFT_CARD );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_SAMPLE_RATE ) == DFT_SAMPLE_RATE);
    CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_FRAME_SIZE ) == DFT_FRAME_SIZE) ;
    CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_PLUGIN ) == PCM_DEFAULT );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, VOLUME_SPKR ) == DFT_VOL_SPKR_STR);
    CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, VOLUME_MICRO ) == DFT_VOL_MICRO_STR);
}

void ConfigurationTest::testDefaultValuePreferences(){
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, ZONE_TONE ) == DFT_ZONE );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_ZEROCONF ) == CONFIG_ZEROCONF_DEFAULT_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_DIALPAD ) == YES_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_RINGTONE ) == YES_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_SEARCHBAR ) == YES_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_START ) == NO_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_POPUP ) == YES_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_NOTIFY ) == YES_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_MAIL_NOTIFY ) == NO_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_VOLUME ) == YES_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, REGISTRATION_EXPIRE ) == DFT_EXPIRE_VALUE );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, CONFIG_AUDIO ) == DFT_AUDIO_MANAGER );

}

void ConfigurationTest::testDefaultValueSignalisation(){
    CPPUNIT_ASSERT( Manager::instance().getConfigString( SIGNALISATION , SYMMETRIC ) == YES_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( SIGNALISATION , PLAY_DTMF ) == YES_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( SIGNALISATION , PLAY_TONES ) == YES_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( SIGNALISATION , PULSE_LENGTH ) == DFT_PULSE_LENGTH_STR );
    CPPUNIT_ASSERT( Manager::instance().getConfigString( SIGNALISATION , SEND_DTMF_AS ) == SIP_INFO_STR );
}

void ConfigurationTest::testLoadAccountMap(){
    Manager::instance().loadAccountMap();  
}

