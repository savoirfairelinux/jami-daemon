#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>
#include <assert.h>

#include "manager.h"
#include "global.h"
#include "user_cfg.h"

#include <stdio.h>

using std::cout;
using std::endl;

// Cppunit import

class ConfigurationTest : public CppUnit::TestCase {

    CPPUNIT_TEST_SUITE( ConfigurationTest );
        CPPUNIT_TEST( testDefaultValueAudio );
        CPPUNIT_TEST( testDefaultValuePreferences );
    CPPUNIT_TEST_SUITE_END();

    public:
        ConfigurationTest() : CppUnit::TestCase("Configuration Tests") {}
 
        void testDefaultValueAudio(){
            CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_CARD_ID_IN ) == ALSA_DFT_CARD) ;
            CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_CARD_ID_OUT ) == ALSA_DFT_CARD );
            CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_SAMPLE_RATE ) == DFT_SAMPLE_RATE);
            CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_FRAME_SIZE ) == DFT_FRAME_SIZE) ;
            CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_PLUGIN ) == PCM_DEFAULT );
            CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, VOLUME_SPKR ) == DFT_VOL_SPKR_STR);
            CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, VOLUME_MICRO ) == DFT_VOL_MICRO_STR);
        }

        void testDefaultValuePreferences(){
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

        void setUp(){
            Manager::instance().initConfigFile();
        }

        void tearDown(){
        }

};

CPPUNIT_TEST_SUITE_REGISTRATION( ConfigurationTest );
