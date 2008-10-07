#include "../manager.h"
#include "../global.h"

// Cppunit import
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/CompilerOutputter.h>

class ConfigurationTest : public CppUnit::TestFixture {

    CPPUNIT_TEST_SUITE( ConfigurationTest );
    CPPUNIT_TEST( testDefaultValueAudio );
    CPPUNIT_TEST_SUITE_END();

    public:
    void setUp(){
        Manager::instance().initConfigFile();
    }

    void tearDown(){

    }

    void testDefaultValueAudio(){
        CPPUNIT_ASSERT( Manager::instance().getConfigString( AUDIO, ALSA_PLUGIN ) == "default" );
    }

};

CPPUNIT_TEST_SUITE_REGISTRATION( ConfigurationTest );

int main(){

    CppUnit::TextUi::TestRunner runner;
    CppUnit::TestFactoryRegistry &registry =
        CppUnit::TestFactoryRegistry::getRegistry() ;
    runner.addTest( registry.makeTest() ) ;
    runner.setOutputter( CppUnit::CompilerOutputter::defaultOutputter(
                &runner.result(),
                std::cerr ) );
    bool wasSuccessful = runner.run( "", false ) ;
    return wasSuccessful ? 0 : 1;



    return 0;


}
