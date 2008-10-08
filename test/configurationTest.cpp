#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>
#include <assert.h>

#include "manager.h"
#include "global.h"

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
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( AUDIO, ALSA_CARD_ID_IN ) == 0 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( AUDIO, ALSA_CARD_ID_IN ) == 0 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( AUDIO, ALSA_SAMPLE_RATE ) == 44100 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( AUDIO, ALSA_FRAME_SIZE ) == 20 );
            CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_PLUGIN ) == "default" );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( AUDIO, VOLUME_SPKR ) == 100 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( AUDIO, VOLUME_MICRO ) == 50 );
        }

        void testDefaultValuePreferences(){
            CPPUNIT_ASSERT( Manager::instance().getConfigString( PREFERENCES, ZONE_TONE ) == "North America" );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_ZEROCONF ) == 0 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_DIALPAD ) == 0 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_RINGTONE ) == 1 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_SEARCHBAR ) == 1 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_START ) == 0 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_POPUP ) == 1 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_NOTIFY ) == 1 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_MAIL_NOTIFY ) == 0 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_VOLUME ) == 0 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, REGISTRATION_EXPIRE ) == 180 );
            CPPUNIT_ASSERT( Manager::instance().getConfigInt( PREFERENCES, CONFIG_AUDIO ) == 0 );
   
        }

        void setUp(){
            Manager::instance().initConfigFile();
        }

        void tearDown(){
        }

};

CPPUNIT_TEST_SUITE_REGISTRATION( ConfigurationTest );
