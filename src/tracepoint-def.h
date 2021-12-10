#include "config.h"

#ifdef ENABLE_TRACEPOINTS

#undef LTTNG_UST_TRACEPOINT_PROVIDER
#define LTTNG_UST_TRACEPOINT_PROVIDER jami

#undef LTTNG_UST_TRACEPOINT_INCLUDE
#define LTTNG_UST_TRACEPOINT_INCLUDE "src/tracepoint-def.h"

#if !defined(TRACEPOINT_DEF_H) || defined(LTTNG_UST_TRACEPOINT_HEADER_MULTI_READ)
#define TRACEPOINT_DEF_H

#include <lttng/tracepoint.h>


/*
 * Use LTTNG_UST_TRACEPOINT_EVENT(), LTTNG_UST_TRACEPOINT_EVENT_CLASS(),
 * LTTNG_UST_TRACEPOINT_EVENT_INSTANCE(), and LTTNG_UST_TRACEPOINT_LOGLEVEL()
 * here.
 */

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    scheduled_executor_task_begin,
    LTTNG_UST_TP_ARGS(
        const char *, executor_name,
        const char *, filename,
        uint32_t,     linum,
        uint64_t,     cookie
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(executor, executor_name)
        lttng_ust_field_string(source_filename, filename)
        lttng_ust_field_integer(uint32_t, source_line, linum)
        lttng_ust_field_integer(uint64_t, cookie, cookie)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    scheduled_executor_task_end,
    LTTNG_UST_TP_ARGS(uint64_t, cookie),
    LTTNG_UST_TP_FIELDS(lttng_ust_field_integer(uint64_t, cookie, cookie))
)

#endif /* TRACEPOINT_DEF_H */

#include <lttng/tracepoint-event.h>

#else  /* ENABLE_TRACEPOINTS */
#  define lttng_ust_tracepoint(...) static_assert(true)
#  define lttng_ust_do_tracepoint(...) static_assert(true)
#  define lttng_ust_tracepoint_enabled(...) false
#endif
