#include "config.h"

#ifdef ENABLE_TRACEPOINT

#undef LTTNG_UST_TRACEPOINT_PROVIDER
#define LTTNG_UST_TRACEPOINT_PROVIDER jami

#undef LTTNG_UST_TRACEPOINT_INCLUDE
#define LTTNG_UST_TRACEPOINT_INCLUDE "src/tracepoint.h"

#if !defined(TRACEPOINT_H) || defined(LTTNG_UST_TRACEPOINT_HEADER_MULTI_READ)
#define TRACEPOINT_H

#include <lttng/tracepoint.h>

/* Include tracepoints here. */

/*
 * Or use LTTNG_UST_TRACEPOINT_EVENT(), LTTNG_UST_TRACEPOINT_EVENT_CLASS(),
 * LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(), and LTTNG_UST_TRACEPOINT_LOGLEVEL()
 * here.
 */

#endif /* TRACEPOINT_H */

#include <lttng/tracepoint-event.h>

#else  /* ENABLE_TRACEPOINT */
#  define lttng_ust_tracepoint(...) static_assert(true)
#endif
