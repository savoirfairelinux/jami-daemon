#ifndef COMPLEXNUMBERTEST_H
#define COMPLEXNUMBERTEST_H

#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include "Complex.h"

class ComplexNumberTest : public CppUnit::TestCase {
public:
  ComplexNumberTest( std::string name );

void runTest();

};
#endif
