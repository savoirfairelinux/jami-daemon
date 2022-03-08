#include <iostream>

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/CompilerOutputter.h>

#include "jami.h"

#define JAMI_TEST_RUNNER_WITH_DAEMON(...) \
    int main() \
    { \
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG)); \
        DRing::start("jami-sample.yml"); \
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
            if (not runner.run()) { \
                DRing::fini(); \
                return 1; \
            } \
        } \
        DRing::fini(); \
        return 0; \
    }

#define JAMI_TEST_RUNNER_WITHOUT_DAEMON(...) \
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
