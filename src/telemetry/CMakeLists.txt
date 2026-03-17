################################################################################
# Source group — telemetry
################################################################################

list(APPEND Source_Files__telemetry
    "${CMAKE_CURRENT_SOURCE_DIR}/telemetry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/remote_tracer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/telemetry.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/calls_trace.h"
)

set(Source_Files__telemetry ${Source_Files__telemetry} PARENT_SCOPE)
