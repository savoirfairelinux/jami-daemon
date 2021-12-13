===================================
Analyzing Jami's static tracepoints
===================================

:Author: Olivier Dion <olivier.dion@savoirfairelinux.com>

This documentation explains how to create a tracing session, enabling Jami's
tracepoints for the session, adding contextual information and analyzing the
result.

For this entire document, we will focus our attention on tracing the scheduled
executor.  We will use ``lttng-tools`` for controlling our session and
``babeltrace2`` for analyzing our trace.

Sessions management
-------------------

The first thing to do is to create a session with ``lttng create SESSION`` where
``SESSION`` is the name of your session.  If left empty, the session named is
``auto``.  Session's traces can be found under
``~/lttng-traces/SESSION-TIMESTAMP``.

At any time, you can invoke ``lttng list`` to see the list of sessions.  You can
switch to a session using ``lttng set-session SESSION``

For now, let's create our session like so ``lttng create jami``.  If you then do
``lttng status`` you should have something like::

  Recording session jami: [inactive]
    Trace output: /home/old/lttng-traces/jami-20211213-112959

Now if we create a new session like ``lttng create temp``.  You will see that if
you do ``lttng status`` again, you're now on this session::

  Recording session temp: [inactive]
    Trace output: /home/old/lttng-traces/temp-20211213-113342

But your ``jami`` session is still there.  Try ``lttng list`` to confirm::

  Available recording sessions:
  1) temp [inactive]
    Trace output: /home/old/lttng-traces/temp-20211213-113342

  2) jami [inactive]
    Trace output: /home/old/lttng-traces/jami-20211213-112959


  Use lttng list <session_name> for more details

We can now do ``lttng set-session jami && lttng destroy temp`` to set the
current session to ``jami`` and destroy the ``temp`` session.  You should now be
using the ``jami`` session for the rest of this document.


Enabling event
--------------

You now want to add tracing events to your session before starting it.  We will
add all events from the provider ``jami`` to our session with ``lttng enable-event --userspace 'jami:*'``
::
   ust event jami:* created in channel channel0

Once this is done, try ``lttng status`` to see what happen::

  Recording session jami: [inactive]
    Trace output: /home/old/lttng-traces/jami-20211213-112959

  === Domain: User space ===

  Buffering scheme: per-user

  Tracked process attributes
    Virtual process IDs:  all
    Virtual user IDs:     all
    Virtual group IDs:    all

  Channels:
  -------------
  - channel0: [enabled]

    Attributes:
      Event-loss mode:  discard
      Sub-buffer size:  524288 bytes
      Sub-buffer count: 4
      Switch timer:     inactive
      Read timer:       inactive
      Monitor timer:    1000000 µs
      Blocking timeout: 0 µs
      Trace file count: 1 per stream
      Trace file size:  unlimited
      Output mode:      mmap

    Statistics:
      Discarded events: 0

    Recording event rules:
      jami:* (type: tracepoint) [enabled]

There's a lots of informations here.  We won't go into the details of all.  The
most important are the session name, and the channel's recording event rules.
We can see that we're indeed in our session and that there's a recording rule
``jami:*`` for the ``channel0``.  Our rule is globing all events produced by the
``jami`` provider.  Other informations are documented in the LTTng'
documentation.

NOTE!  Do not confuse the ``jami`` session with the ``jami`` provider.  Even
though they have the same name in this document, you could have a session named
``foo`` instead of ``jami`` and everything in this document should work the same.

Adding context
--------------

We will now show on to add contextual information to your session.  You see a
list of available contextual information using ``lttng add-context --list``.
Some of them require root privileges.  For our example, we will add the
``perf:thread:cpu-cycles`` and ``perf:thread:cpu-migrations`` contexts.

To do, run ``lttng add-context --userspace --type=perf:thread:cycles --type=perf:thread:cpu-migrations``::

  ust context perf:thread:cycles added to all channels
  ust context perf:thread:cpu-migrations added to all channels

Running the session
-------------------

You can now start the session with ``lttng start``::

  Tracing started for session jami

and run Jami ``./bin/jamid``.

Let it run a few seconds and stop the Jami's daemon.  Then you can run ``lttng
destroy`` (or ``lttng stop`` if you do not want to destroy) to stop and destroy
your session::

  Destroying session jami...
  Session jami destroyed

Analysis
--------

We can now analyze our trace.  For this example, we will only show the trace
with ``babeltrace``.  Run ``babeltrace2 ~/lttng-traces/TRACE`` where ``TRACE``
is your trace folder.  You should now have something like this::

  [12:05:05.899862574] (+?.?????????) laura jami:scheduled_executor_task_begin: { cpu_id = 13 }, { perf_thread_cycles = 49068, perf_thread_cpu_migrations = 0 }, { executor = "natpmp", source_filename = "upnp/protocol/natpmp/nat_pmp.cpp", source_line = 233, cookie = 0 }
  [12:05:05.916075225] (+0.016212651) laura jami:scheduled_executor_task_end: { cpu_id = 13 }, { perf_thread_cycles = 1697757, perf_thread_cpu_migrations = 0 }, { cookie = 0 }
  [12:05:08.307655201] (+2.391579976) laura jami:scheduled_executor_task_begin: { cpu_id = 14 }, { perf_thread_cycles = 8044, perf_thread_cpu_migrations = 0 }, { executor = "manager", source_filename = "upnp/protocol/pupnp/pupnp.cpp", source_line = 397, cookie = 1 }
  [12:05:08.307760459] (+0.000105258) laura jami:scheduled_executor_task_end: { cpu_id = 14 }, { perf_thread_cycles = 137657, perf_thread_cpu_migrations = 0 }, { cookie = 1 }
  [12:05:25.916502877] (+17.608742418) laura jami:scheduled_executor_task_begin: { cpu_id = 13 }, { perf_thread_cycles = 1858657, perf_thread_cpu_migrations = 0 }, { executor = "natpmp", source_filename = "upnp/protocol/natpmp/nat_pmp.cpp", source_line = 233, cookie = 2 }
  [12:05:25.918499760] (+0.001996883) laura jami:scheduled_executor_task_end: { cpu_id = 13 }, { perf_thread_cycles = 2877865, perf_thread_cpu_migrations = 0 }, { cookie = 2 }

We can see from that trace that the scheduled executor ``natpmp`` has scheduled
a task that consumed 0.016 second.  The task has consumed 1648689 cycles without
cpu migration, thus the task has run on cpu 13 for its entire time.

This is not very useful on its own.  We need a better way of doing trace
analysis.  Fortunately, Babeltrace has Python3 bindings.  It's therefore
trivial to do statistical analysis and graph using Python.

For example, let's says we want to analyze the packets sent and received at the
ICE transport level.  For this, we can use the tools
``./tools/trace/ice-transport/stats`` like so::

  ./tools/trace/ice-transports/stats --log-scale ~/lttng-traces/TRACE [PID]

where TRACE would be the name of your trace and PID the optional process
identifier of the process executing Jami at the time of the trace.  You should
get something along like this::

 Direction                 Count            Average size (bytes)
    Send                    129                     211
    Recv                    201                     545

 ================================================================================
          Send         |  count   |                distribution                 |
 --------------------------------------------------------------------------------
         0 -> 1        |    0     |                                             |
         1 -> 2        |    0     |                                             |
         2 -> 4        |    0     |                                             |
         4 -> 8        |    2     |                                             |
         8 -> 16       |    0     |                                             |
        16 -> 32       |    3     |*                                            |
        32 -> 64       |    87    |******************************               |
        64 -> 128      |    2     |                                             |
       128 -> 256      |    4     |*                                            |
       256 -> 512      |    23    |********                                     |
       512 -> 1024     |    5     |*                                            |
      1024 -> 2048     |    0     |                                             |
      2048 -> 4096     |    1     |                                             |
      4096 -> 8192     |    2     |                                             |
 ================================================================================
 ================================================================================
          Recv         |  count   |                distribution                 |
 --------------------------------------------------------------------------------
         0 -> 1        |    0     |                                             |
         1 -> 2        |    0     |                                             |
         2 -> 4        |    0     |                                             |
         4 -> 8        |    2     |                                             |
         8 -> 16       |    0     |                                             |
        16 -> 32       |    3     |                                             |
        32 -> 64       |    79    |*****************                            |
        64 -> 128      |    7     |*                                            |
       128 -> 256      |    6     |*                                            |
       256 -> 512      |    29    |******                                       |
       512 -> 1024     |    10    |**                                           |
      1024 -> 2048     |    62    |*************                                |
      2048 -> 4096     |    1     |                                             |
      4096 -> 8192     |    2     |                                             |
 ================================================================================

Another example of a trace tools is for timeline of execution.  For example, you
might be interested to see the defered execution of Jami's executors.  For this,
you can use ``./tools/trace/timeline/executor TRACE`` to get something like this::


	Executor: natpmp
		scheduled   duration (µs)  source
		14:59:50    2713.66        upnp/protocol/natpmp/nat_pmp.cpp:233
		15:00:10    2234.57        upnp/protocol/natpmp/nat_pmp.cpp:233
		15:00:40    2430.03        upnp/protocol/natpmp/nat_pmp.cpp:233

	Executor: manager
		scheduled   duration (µs)  source
		14:59:52    23.66          upnp/protocol/pupnp/pupnp.cpp:397
		15:00:15    24.11          upnp/protocol/pupnp/pupnp.cpp:397
		15:00:47    27.56          upnp/protocol/pupnp/pupnp.cpp:397
