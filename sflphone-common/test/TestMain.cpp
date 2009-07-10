#include <cppunit/TextTestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

int main (int argc, const char* argv[])
{
    CppUnit::TextTestRunner runner;
    runner.addTest (CppUnit::TestFactoryRegistry::getRegistry().makeTest());

    return runner.run();

}

