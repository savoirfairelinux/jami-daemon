=================================
Adding static tracepoints to Jami
=================================

:Author: Olivier Dion <olivier.dion@savoirfairelinux.com>

This documentation explains the scope usage of static tracepoints in Jami and
how to add new tracepoints.

Scope
-----

Tracepoints work kind of like a logging system.  It's however much faster, more
tolerant to faults and can incorporate additional contextual information only
available in the kernel.

For example, tracepoints are buffered in a ring buffer that is shared with a
consumer daemon.  Thus, Jami is not slow down by I/O operations like with
regular text logging.  This makes some scenario much more reproducible.  Also,
since the traces are in shared memory, even if Jami crashes the last traces are
still available to be consumed by the daemon

For the contextual information, these can come from performance counters such
as the number of cache misses, branch misses, cpu cycles etc.  It can also be
coming from the kernel itself such as the number of cpu migrations for a thread,
context switches, page faults, etc.

These contextual information are **not** statically decided at compile time
when the tracepoint is defined.  They are rather determined at trace time in a
tracing session (see doc/trace/tracepoint-analysis.rst).

Enabling tracepoints
--------------------

To enable tracepoints in Jami, you should configure the project using the
``--enable-tracepoints`` feature.  You also need ``lttng-ust >= 2.13.0`` and
``liburcu >= 0.13.0``.

How to define a tracepoint
--------------------------

To define a new tracepoints, you need to add the definition in src/jami/tracepoint-def.h

It's recommended to use the ``LTTNG_UST_TRACEPOINT_EVENT`` macro and avoid using
the others except if you know what you're doing.

The ``LTTNG_UST_TRACEPOINT_EVENT`` is composed of 4 parts:

  1. The provider name.  This is always ``jami``.
  2. The tracepoint name.  A tracepoint name must be unique in a provider.
  3. The arguments passed to the tracepoints.  Arguments are only evaluated if
     the tracepoint is enabled at runtime.
  4. The fields of the tracepoint.  Fields can use the tracepoint's arguments.

NOTE!  As the documentation of LTTng says, the concatenation of provider name +
tracepoint name must **not exceed 254 characters** or you will get bite.

For example, here's the definition of a tracepoint for the scheduled executor in
src/jami/tracepoint-def.h::

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

We can see that:

  1. The provider name is ``jami``.
  2. The tracepoint name is ``scheduled_executor_task_begin``.
  3. The tracepoint takes 4 arguments of types ``const char *``, ``const char *``,
     ``uint32_t``, ``uint64_t`` respectively named ``executor_name``, ``filename``,
     ``linum``, ``cookie``.
  4. The tracepoint generates 4 fields of CTF type ``string``, ``string``,
     ``integer``, ``integer`` respectively named ``executor``, ``source_filename``,
     ``source_line``, ``cookie``.

NOTE!  In this example, the expressions of the fields are the arguments of the
tracepoint but it could be different.  For example, you could replace
``lttng_ust_field_integer(uint64_t, cookie, cookie)`` with
``lttng_ust_field_integer(uint64_t, cookie, linum + cookie * cookie)``.  At the
end, any C expression can be used in the value of a field.  The expression can
refer to all arguments of the tracepoints.  Thus, if one of your argument to the
tracepoint is a pointer to a structure, you can deference the structure to read
a member.  Just be careful for side effects.

How to use a tracepoint
-----------------------

Now that you have defined a tracepoint, you perhaps want to use it in Jami or
reuse an existing one.  The first thing to do is to import src/jami/tracepoint.h in
your compilation unit.  Then you need to use the ``jami_tracepoint()``
macro.  It takes the tracepoint name followed by a variable number of
arguments defined by the tracepoint.

For example, here's how the tracepoint ``secheduled_executor_task_begin`` is
used in src/scheduled_executor.h::

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-parameter"
    void run(const char* executor_name)
    {
        if (job_.fn) {
            jami_tracepoint(scheduled_executor_task_begin,
                            executor_name,
                            job_.filename, job_.linum,
                            cookie_);
            job_.fn();
            jami_tracepoint(scheduled_executor_task_end,
                            cookie_);
        }
    }
  #pragma GCC pop

NOTE!  The ``jami_tracepoint(...)`` macro expands to
``static_assert(true)`` if tracepoints are not enabled in Jami.  Thus, never do
side effects in tracepoint!  This is also why we use the GCC diagnostic pragma
here to avoid the warnings about unused parameter when tracepoints are disabled.


Further reading
---------------

`https://lttng.org/docs/v2.13/`_
