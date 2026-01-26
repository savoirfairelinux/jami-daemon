cmake_minimum_required(VERSION 3.16)
# Check for Guile (required for agent)
pkg_search_module(Guile REQUIRED IMPORTED_TARGET guile-3.0>=3.0.7)
# Guile wrapper executable
# This wrapper is necessary because Guile is not compiled with ASAN.
# If we're using the agent with -fsanitize=address, and just run guile,
# everything will go haywire. Thus, compile a minimal wrapper with the
# same flags passed while configuring so that we can overcome this.
add_executable(guile-wrapper
    build-aux/guile-wrapper.cpp
)
target_link_libraries(guile-wrapper PRIVATE PkgConfig::Guile)
# Guile-Jami library
# Build as MODULE since it needs to be a loadable library for Guile
add_library(guile-jami MODULE
    src/main.cpp
    src/utils.h
    src/bindings/jami.h
    src/bindings/bindings.cpp
    src/bindings/bindings.h
    src/bindings/account.h
    src/bindings/call.h
    src/bindings/conversation.h
    src/bindings/signal.cpp
    src/bindings/signal.h
)
# Ensure Position Independent Code for shared library linking
set_target_properties(guile-jami PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX "lib"  # Ensure it's named libguile-jami.so
)
target_include_directories(guile-jami PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(guile-jami PRIVATE
    ${PROJECT_NAME}
    PkgConfig::Guile
)
# Install the library
install(TARGETS guile-jami
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
# Scheme modules to compile
set(MODULES
    agent.scm
    examples/active-agent.scm
    examples/passive-agent.scm
    jami.scm
    jami/account.scm
    jami/call.scm
    jami/conversation.scm
    jami/logger.scm
    jami/signal.scm
    scenarios/bulk-calls/scenario.scm
    scenarios/peer-monitor/scenario.scm
)
set(abs_top_srcdir "${CMAKE_SOURCE_DIR}")
set(abs_top_builddir "${CMAKE_BINARY_DIR}")
set(AGENT_BUILD_DIR "${CMAKE_BINARY_DIR}/test/agent")

file(MAKE_DIRECTORY "${AGENT_BUILD_DIR}")
file(MAKE_DIRECTORY "${AGENT_BUILD_DIR}/scenarios/bulk-calls")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/build-aux/pre-inst-env.in"
    "${AGENT_BUILD_DIR}/pre-inst-env"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/bulk-calls/run-scenario.in"
    "${AGENT_BUILD_DIR}/scenarios/bulk-calls/run-scenario"
    @ONLY
)
