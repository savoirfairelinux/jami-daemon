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

