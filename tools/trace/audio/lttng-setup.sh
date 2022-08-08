#!/bin/sh

# this script sets up an lttng session and enables triggers for call start and conference end
# ideally, this script would be more configurable, right now it's hardcoded and hacky

if lttng status | grep -q "Recording session"; then
    echo "lttng session already active, exiting"
    exit 1
fi

SESSION="jami"

# Prepare a capture session

lttng create "$SESSION"

# Enable kernel and user channels

# lttng enable-channel -k --subbuf-size 4096K kernel
lttng enable-channel -u --subbuf-size 1024K user

# Enable all kernel syscalls tracing
# lttng enable-event --kernel --syscall --all --channel=kernel

# Enable sched_* (scheduler) functions tracing
# lttng enable-event --kernel --channel=kernel "$(lttng list --kernel  | grep sched_ | awk '{ print $1 }' | xargs  | sed -e 's/ /,/g')"

# Enable net_* functions tracing
# lttng enable-event --kernel --channel=kernel "$(lttng list --kernel  | grep net_ | awk '{ print $1 }' | xargs  | sed -e 's/ /,/g')"

# Userspace tracing
# Add support for tracef() traces
# You need to link application with @-llttng-ust@
# lttng enable-event --userspace 'lttng_ust_tracef:*' --channel=user

# Add context for libc (including malloc, etc)
export LD_PRELOAD="$LD_PRELOAD":liblttng-ust-libc-wrapper.so
lttng enable-event --userspace  'lttng_ust_libc:*' --channel=user
lttng add-context --userspace -t vtid -t procname

# Add context for pthread (including mutexes)
export LD_PRELOAD="$LD_PRELOAD":liblttng-ust-pthread-wrapper.so
lttng enable-event --userspace 'lttng_ust_pthread:*' --channel=user

# Add context for function profiling
export LD_PRELOAD="$LD_PRELOAD":liblttng-ust-cyg-profile.so
lttng enable-event --userspace --all --channel=user
lttng add-context --userspace -t vpid -t vtid -t procname

# loop over lines in our for-loop, since triggers might have spaces in their names
OLDIFS="$IFS"
IFS='
'

# remove all triggers
for trigger in $(lttng list-triggers | sed -nr 's/- name: (.*)$/\1/p'); do
    echo "removing trigger: $trigger"
    lttng remove-trigger "$trigger"
done

IFS=$OLDIFS

# add start and end trigger
lttng add-trigger --name "jami call start" --condition=event-rule-matches --type=user --name='jami:call_start' --action=start-session $SESSION
lttng add-trigger --name "jami conference end" --condition=event-rule-matches --type=user --name="jami:conference_end" --action=stop-session $SESSION
