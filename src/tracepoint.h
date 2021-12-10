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

#endif /* TRACEPOINT_H */

#include <lttng/tracepoint-event.h>

#else  /* ENABLE_TRACEPOINT */
#  define lttng_ust_tracepoint(...) static_assert(true)
#endif
