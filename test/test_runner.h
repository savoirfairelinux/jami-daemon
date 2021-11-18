#include <iostream>

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/CompilerOutputter.h>

#define RING_TEST_RUNNER(suite_name) \
    int main() \
    { \
        CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry( \
            suite_name); \
        CppUnit::Test* suite = registry.makeTest(); \
        if (suite->countTestCases() == 0) { \
            std::cout << "No test cases specified for suite \"" << suite_name << "\"\n"; \
            return 1; \
        } \
        CppUnit::TextUi::TestRunner runner; \
        runner.addTest(suite); \
        return runner.run() ? 0 : 1; \
    }

// Similar to RING_TEST_RUNNER but can take multiple unit tests.
// It's practical to run a test for diffrent configs, when running
// the same test for both Jami and SIP accounts.
// The test will abort if a test fails.
#define JAMI_TEST_RUNNER(...) \
    int main() \
    { \
        std::vector<std::string> suite_names {__VA_ARGS__}; \
        for (const std::string& name : suite_names) { \
            CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry( \
                name); \
            CppUnit::Test* suite = registry.makeTest(); \
            if (suite->countTestCases() == 0) { \
                std::cout << "No test cases specified for suite \"" << name << "\"\n"; \
                continue; \
            } \
            CppUnit::TextUi::TestRunner runner; \
            runner.addTest(suite); \
            if (not runner.run()) \
                return 1; \
        } \
        return 0; \
    }
