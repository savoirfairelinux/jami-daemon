#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "tracepoint-def.h"
#pragma GCC diagnostic pop

#ifdef ENABLE_TRACEPOINTS

#  ifndef lttng_ust_tracepoint
#    define lttng_ust_tracepoint(...) tracepoint(__VA_ARGS__)
#  endif

#  ifndef lttng_ust_do_tracepoint
#    define lttng_ust_do_tracepoint(...) do_tracepoint(__VA_ARGS__)
#  endif

#  ifndef lttng_ust_tracepoint_enabled
#    define lttng_ust_tracepoint_enabled(...) tracepoint_enabled(__VA_ARGS__)
#  endif

#  define jami_tracepoint(tp_name, ...)					\
	lttng_ust_tracepoint(jami, tp_name __VA_OPT__(,) __VA_ARGS__)

#  define jami_do_tracepoint(tp_name, ...)				\
	lttng_ust_do_tracepoint(jami, tp_name __VA_OPT__(,) __VA_ARGS__)

#  define jami_tracepoint_enabled(tp_name)		\
	lttng_ust_tracepoint_enabled(jami, tp_name)

#else

#  define jami_tracepoint(...)         static_assert(true)
#  define jami_do_tracepoint(...)      static_assert(true)
#  define jami_tracepoint_enabled(...) false

#endif
