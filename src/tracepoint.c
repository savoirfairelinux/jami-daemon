#include "config.h"

#ifdef ENABLE_TRACEPOINT
#  define LTTNG_UST_TRACEPOINT_CREATE_PROBES
#  define LTTNG_UST_TRACEPOINT_DEFINE
#  include "./tracepoint.h"
#endif
