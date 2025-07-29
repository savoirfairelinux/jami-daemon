# Copyright (C) 2025 Savoir-faire Linux Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.

# Coverage analysis support for CMake build
# Ported from autotools Makefile.am coverage targets

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "Coverage analysis is typically done in Debug builds. Current build type is ${CMAKE_BUILD_TYPE}")
endif()

# Find required tools
find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)
find_program(GCOVR_PATH gcovr)

# Check if coverage tools are available
set(COVERAGE_TOOLS_AVAILABLE FALSE)
if(LCOV_PATH AND GENHTML_PATH)
    set(COVERAGE_TOOLS_AVAILABLE TRUE)
    message(STATUS "Found lcov: ${LCOV_PATH}")
    message(STATUS "Found genhtml: ${GENHTML_PATH}")
endif()

if(GCOVR_PATH)
    message(STATUS "Found gcovr: ${GCOVR_PATH}")
endif()

# Function to setup coverage for a target
function(setup_target_for_coverage target_name)
    if(NOT COVERAGE_TOOLS_AVAILABLE AND NOT GCOVR_PATH)
        message(WARNING "Coverage tools not found. Install lcov and genhtml or gcovr for coverage analysis.")
        return()
    endif()

    # Add coverage flags to the target
    target_compile_options(${target_name} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fprofile-arcs -ftest-coverage>
        $<$<COMPILE_LANGUAGE:C>:-fprofile-arcs -ftest-coverage>
    )

    target_link_libraries(${target_name} PRIVATE gcov)
endfunction()

# Create coverage targets
if(COVERAGE_TOOLS_AVAILABLE OR GCOVR_PATH)
    add_custom_target(coverage-clean
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning coverage data..."
        COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --zerocounters || true
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/*.info
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/html-coverage-output
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/xml-coverage-output
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/json-coverage-output
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Cleaning coverage data"
    )

    add_custom_target(coverage-cleaner
        DEPENDS coverage-clean
        COMMAND find ${CMAKE_BINARY_DIR} -name '*.gcda' -delete -o -name '*.gcno' -delete || true
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Deep cleaning coverage data (removing .gcda and .gcno files)"
    )

    if(COVERAGE_TOOLS_AVAILABLE)
        # coverage-html target (using lcov/genhtml)
        add_custom_target(coverage-html
            # Initialize coverage data
            COMMAND ${LCOV_PATH} --capture --initial --directory ${CMAKE_BINARY_DIR} --output-file jami-coverage-base.info
            # Capture test execution data
            COMMAND ${LCOV_PATH} --capture --directory ${CMAKE_BINARY_DIR} --output-file jami-coverage-tests.info
            # Combine base and test coverage
            COMMAND ${LCOV_PATH} --add-tracefile jami-coverage-base.info --add-tracefile jami-coverage-tests.info --output-file jami-coverage.info
            # Remove unwanted files
            COMMAND ${LCOV_PATH} --remove jami-coverage.info 
                "*/contrib/*"
                "*/bin/dbus/*"
                "*/_deps/*"
                "*/3rdparty/*"
                "*/test/*"
                "/usr/*"
                --output-file jami-coverage-filtered.info
            COMMAND ${CMAKE_COMMAND} -E make_directory html-coverage-output
            COMMAND ${GENHTML_PATH} -o html-coverage-output jami-coverage-filtered.info
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating HTML coverage report"
        )
    endif()

    if(GCOVR_PATH)
        # coverage-xml target (using gcovr)
        add_custom_target(coverage-xml
            COMMAND ${CMAKE_COMMAND} -E echo "Generating XML coverage report..."
            COMMAND ${CMAKE_COMMAND} -E make_directory xml-coverage-output
            COMMAND ${GCOVR_PATH} 
                --filter ${CMAKE_CURRENT_SOURCE_DIR}/src 
                --filter ${CMAKE_CURRENT_SOURCE_DIR}/test
                --xml-pretty --xml xml-coverage-output/coverage.xml
                --root ${CMAKE_CURRENT_SOURCE_DIR}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating XML coverage report"
        )

        # coverage-json target (using gcovr)
        add_custom_target(coverage-json
            COMMAND ${CMAKE_COMMAND} -E echo "Generating JSON coverage report..."
            COMMAND ${CMAKE_COMMAND} -E make_directory json-coverage-output
            COMMAND ${GCOVR_PATH} 
                --filter ${CMAKE_CURRENT_SOURCE_DIR}/src 
                --filter ${CMAKE_CURRENT_SOURCE_DIR}/test
                --json-pretty --json json-coverage-output/coverage.json
                --root ${CMAKE_CURRENT_SOURCE_DIR}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating JSON coverage report"
        )

        # coverage-text target (using gcovr for console output)
        add_custom_target(coverage-text
            COMMAND ${CMAKE_COMMAND} -E echo "Generating text coverage report..."
            COMMAND ${GCOVR_PATH} 
                --filter ${CMAKE_CURRENT_SOURCE_DIR}/src 
                --filter ${CMAKE_CURRENT_SOURCE_DIR}/test
                --txt-pretty --txt text-coverage-output.txt
                --root ${CMAKE_CURRENT_SOURCE_DIR}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating text coverage report"
        )
    endif()

    # all-coverage target (combines all available coverage formats)
    set(ALL_COVERAGE_TARGETS coverage-clean)
    if(COVERAGE_TOOLS_AVAILABLE)
        list(APPEND ALL_COVERAGE_TARGETS coverage-html)
    endif()
    if(GCOVR_PATH)
        list(APPEND ALL_COVERAGE_TARGETS coverage-xml coverage-json)
    endif()

    add_custom_target(all-coverage
        DEPENDS ${ALL_COVERAGE_TARGETS}
        COMMENT "Generating all available coverage reports"
    )

    # Print information about available coverage targets
    message(STATUS "Coverage targets available:")
    message(STATUS "  coverage-clean     - Clean coverage data")
    message(STATUS "  coverage-cleaner   - Deep clean coverage data")
    if(COVERAGE_TOOLS_AVAILABLE)
        message(STATUS "  coverage-html      - Generate HTML coverage report")
    endif()
    if(GCOVR_PATH)
        message(STATUS "  coverage-xml       - Generate XML coverage report")
        message(STATUS "  coverage-json      - Generate JSON coverage report")
        message(STATUS "  coverage-text      - Generate text coverage report")
    endif()
    message(STATUS "  all-coverage       - Generate all available coverage reports")

else()
    message(WARNING "No coverage tools found. Install lcov/genhtml and/or gcovr for coverage analysis.")
endif()
