#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestCase.h>
#include <cppunit/TestSuite.h>
#include <assert.h>

#include "manager.h"
#include "global.h"

// Cppunit import

class ConfigurationTest : public CppUnit::TestCase {

    CPPUNIT_TEST_SUITE( ConfigurationTest );
        CPPUNIT_TEST( testDefaultValueAudio );
        CPPUNIT_TEST( testTheTest );
    CPPUNIT_TEST_SUITE_END();

    public:
        ConfigurationTest() : CppUnit::TestCase("Configuration Tests") {}
 
        void testDefaultValueAudio(){
                CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_PLUGIN ) == "default" );
        }

        void testTheTest(){
                CPPUNIT_ASSERT( 3 == 2 ); 
        }

        void setUp(){
        }

        void tearDown(){
        }

};

CPPUNIT_TEST_SUITE_REGISTRATION( ConfigurationTest );
