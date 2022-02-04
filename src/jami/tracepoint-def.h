#include "config.h"

#ifdef ENABLE_TRACEPOINTS

#undef LTTNG_UST_TRACEPOINT_PROVIDER
#define LTTNG_UST_TRACEPOINT_PROVIDER jami

#undef LTTNG_UST_TRACEPOINT_INCLUDE
#define LTTNG_UST_TRACEPOINT_INCLUDE "src/jami/tracepoint-def.h"

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

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    ice_transport_context,
    LTTNG_UST_TP_ARGS(
            uint64_t, context
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(uint64_t, ice_context, context)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    ice_transport_send,
    LTTNG_UST_TP_ARGS(
            uint64_t, context,
            unsigned, component,
            size_t, len,
            const char*, remote_addr
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(uint64_t, ice_context, context)
            lttng_ust_field_integer(unsigned, component, component)
            lttng_ust_field_integer(size_t, packet_length, len)
            lttng_ust_field_string(remote_addr, remote_addr)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    ice_transport_send_status,
    LTTNG_UST_TP_ARGS(
            int, status
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(int, pj_status, status)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    ice_transport_recv,
    LTTNG_UST_TP_ARGS(
            uint64_t, context,
            unsigned, component,
            size_t, len,
            const char*, remote_addr
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(uint64_t, ice_context, context)
            lttng_ust_field_integer(unsigned, component, component)
            lttng_ust_field_integer(size_t, packet_length, len)
            lttng_ust_field_string(remote_addr, remote_addr)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    emit_signal,
    LTTNG_UST_TP_ARGS(
            const char*, signal_type
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_string(signal_type, signal_type)

    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    emit_signal_begin_callback,
    LTTNG_UST_TP_ARGS(
    const char*, filename,
            uint32_t, linum
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_string(source_filename, filename)
            lttng_ust_field_integer(uint32_t, source_line, linum)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    emit_signal_end_callback,
    LTTNG_UST_TP_ARGS(
    ),
    LTTNG_UST_TP_FIELDS(
    )
)

#endif /* TRACEPOINT_DEF_H */

#include <lttng/tracepoint-event.h>

#else  /* ENABLE_TRACEPOINTS */
#  define lttng_ust_tracepoint(...) static_assert(true)
#  define lttng_ust_do_tracepoint(...) static_assert(true)
#  define lttng_ust_tracepoint_enabled(...) false
#endif
