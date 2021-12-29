#include "config.h"

#ifdef ENABLE_TRACEPOINTS

#undef LTTNG_UST_TRACEPOINT_PROVIDER
#define LTTNG_UST_TRACEPOINT_PROVIDER jami

#undef LTTNG_UST_TRACEPOINT_INCLUDE
#define LTTNG_UST_TRACEPOINT_INCLUDE "src/tracepoint-def.h"

#if !defined(TRACEPOINT_H) || defined(LTTNG_UST_TRACEPOINT_HEADER_MULTI_READ)
#define TRACEPOINT_H

#include <lttng/tracepoint.h>

/*
 * Use LTTNG_UST_TRACEPOINT_EVENT(), LTTNG_UST_TRACEPOINT_EVENT_CLASS(),
 * LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(), and LTTNG_UST_TRACEPOINT_LOGLEVEL()
 * here.
 */

#endif /* TRACEPOINT_H */

#include <lttng/tracepoint-event.h>

#else  /* ENABLE_TRACEPOINTS */
#  define lttng_ust_tracepoint(...) static_assert(true)
#  define lttng_ust_do_tracepoint(...) static_assert(true)
#  define lttng_ust_tracepeoint_enabled(...) false
#endif
