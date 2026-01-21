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
add_library(guile-jami SHARED
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

# TODO: Add custom commands to compile Scheme modules if needed
# This would require configuring pre-inst-env and compile-scheme.scm
