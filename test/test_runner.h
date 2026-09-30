#include <iostream>
#include <stdexcept>
#include <string_view>

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestPath.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/CompilerOutputter.h>

#define CORE_TEST_RUNNER(suite_name) \
    int main(int argc, char** argv) \
    { \
        CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry(suite_name); \
        CppUnit::Test* suite = registry.makeTest(); \
        if (suite->countTestCases() == 0) { \
            std::cout << "No test cases specified for suite \"" << suite_name << "\"\n"; \
            return 1; \
        } \
        CppUnit::TextUi::TestRunner runner; \
        runner.addTest(suite); \
        try { \
            if (argc > 1) { \
                const std::string_view selected {argv[1]}; \
                auto findCase = [&](auto&& self, CppUnit::Test* test) -> CppUnit::Test* { \
                    const auto name = test->getName(); \
                    if (name == selected or (name.size() > selected.size() \
                                             and std::string_view(name).ends_with(selected))) \
                        return test; \
                    for (int i = 0; i < test->getChildTestCount(); ++i) { \
                        if (auto* found = self(self, test->getChildTestAt(i))) \
                            return found; \
                    } \
                    return nullptr; \
                }; \
                if (auto* test = findCase(findCase, suite)) { \
                    CppUnit::TestPath path; \
                    suite->findTestPath(test, path); \
                    return runner.run(path.toString()) ? 0 : 1; \
                } \
                std::cerr << "No test case matching " << selected << '\n'; \
                return 2; \
            } \
            return runner.run() ? 0 : 1; \
        } catch (const std::invalid_argument& e) { \
            std::cerr << e.what() << std::endl; \
            return 1; \
        } \
    }

// This version of the test runner is similar to CORE_TEST_RUNNER but
// can take multiple unit tests.
// It's practical to run a test for diffrent configs, for instance when
// running the same test for both Jami and SIP accounts.

// The test will abort if a test fails.
#define JAMI_TEST_RUNNER(...) \
    int main() \
    { \
        std::vector<std::string> suite_names {__VA_ARGS__}; \
        for (const std::string& name : suite_names) { \
            CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry(name); \
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
