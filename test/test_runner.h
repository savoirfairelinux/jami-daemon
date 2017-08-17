#include <iostream>

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/CompilerOutputter.h>

#define RING_TEST_RUNNER(suite_name) \
int main() \
{ \
    CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry(suite_name); \
    CppUnit::Test *suite = registry.makeTest(); \
    if(suite->countTestCases() == 0) { \
        std::cout << "No test cases specified for suite \"" << suite_name << "\"\n"; \
        return 1; \
    } \
    CppUnit::TextUi::TestRunner runner; \
    runner.addTest(suite); \
    return runner.run() ? 0 : 1; \
}
