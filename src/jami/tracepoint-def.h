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
    emit_signal_end,
    LTTNG_UST_TP_ARGS(
    ),
    LTTNG_UST_TP_FIELDS(
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

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    audio_input_read_from_device_end,
    LTTNG_UST_TP_ARGS(
        const char*, id
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(uint64_t, id, strtoull(id, NULL, 16))
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    audio_layer_put_recorded_end,
    LTTNG_UST_TP_ARGS(
    ),
    LTTNG_UST_TP_FIELDS(
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    audio_layer_get_to_play_end,
    LTTNG_UST_TP_ARGS(
    ),
    LTTNG_UST_TP_FIELDS(
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    call_start,
    LTTNG_UST_TP_ARGS(
            const char*, id
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(uint64_t, id, strtoull(id, NULL, 16))
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    call_end,
    LTTNG_UST_TP_ARGS(
            const char*, id
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(uint64_t, id, strtoull(id, NULL, 16))
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    conference_begin,
    LTTNG_UST_TP_ARGS(
            const char*, id
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(uint64_t, id, strtoull(id, NULL, 16))
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    conference_end,
    LTTNG_UST_TP_ARGS(
            const char*, id
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(uint64_t, id, strtoull(id, NULL, 16))
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    jami,
    conference_add_participant,
    LTTNG_UST_TP_ARGS(
            const char*, conference_id,
            const char*, participant_id
    ),
    LTTNG_UST_TP_FIELDS(
            lttng_ust_field_integer(uint64_t, id, strtoull(conference_id, NULL, 16))
            lttng_ust_field_integer(uint64_t, participant_id, strtoull(participant_id, NULL, 16))
    )
)

#endif /* TRACEPOINT_DEF_H */

#include <lttng/tracepoint-event.h>

#endif
