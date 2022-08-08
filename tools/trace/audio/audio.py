#!/usr/bin/env python3
"""
Quick-and-dirty script to plot lttng trace data of the jami audio system

Depends on `python3-bt2` package:
https://babeltrace.org/docs/v2.0/python/bt2/

If using a virtual environment, you need to link the system babeltrace2 python3
  package to your virtual environment, something like:
`ln -s /usr/lib/python3/dist-packages/bt2 "$VIRTUAL_ENV"/lib/python3*/site-packages/`

"""

import re
import dataclasses
from typing import Dict, List
import sys

import bt2
import matplotlib.pyplot as plt


@dataclasses.dataclass
class Intervals:
    """Keep track of timestamps and the interval intervals between them"""

    intervals: List[int] = dataclasses.field(default_factory=list)
    times: List[int] = dataclasses.field(default_factory=list)
    last_timestamp: int = 0

    def add_event(self, timestamp: int):
        """Add a new timestamp and calculate the interval"""
        if self.last_timestamp == 0:
            self.intervals.append(0)
        else:
            self.intervals.append(timestamp - self.last_timestamp)

        self.times.append(timestamp)

        self.last_timestamp = timestamp

    def filter(self, earliest: int, latest: int):
        """Filter out entries that are not in between `earliest` and `latest` timestamps"""
        new_intervals = []
        new_times = []
        for i, val in enumerate(self.times):
            if val > earliest and val < latest:
                new_times.append(val)
                new_intervals.append(self.intervals[i])

        self.times = new_times
        self.intervals = new_intervals


def analyze(filename: str):
    """Process trace file"""
    # running_averages: Dict[str, RunningAverage] = {}
    intervals: Dict[str, Intervals] = {}
    desired_event_regex = re.compile("jami:(call|audio|conference)")

    for message in bt2.TraceCollectionMessageIterator(filename):
        # pylint: disable=protected-access
        if not isinstance(message, bt2._EventMessageConst):
            continue
        ns_from_origin = message.default_clock_snapshot.ns_from_origin
        name = message.event.name

        if not desired_event_regex.match(name):
            continue

        if "id" in message.event.payload_field and name != "jami:conference_add_participant":
            name = f"{name}({str(message.event.payload_field['id'])})"

        if not name in intervals:
            # running_averages[name] = RunningAverage()
            intervals[name] = Intervals()

        # running_averages[name].add_val(ns_from_origin)
        intervals[name].add_event(ns_from_origin)

        # print(f"{ns_from_origin / 10e9:.9f}: {name}")

    for key, val in intervals.items():
        print(
            f"event: {key}, average: {(sum(val.intervals) / len(val.intervals)) / 1e9:.9f}"
        )

    earliest_recorded = intervals["jami:audio_layer_put_recorded_end"].times[0]
    latest_recorded = intervals["jami:audio_layer_put_recorded_end"].times[-1]

    earliest_recorded += int(1e9)  # start graph 1 second later

    print(
        f"earliest: {earliest_recorded / 1e9 :.9f}, latest: {latest_recorded / 1e9:.9f}"
    )

    for _, interval_val in intervals.items():
        interval_val.filter(earliest_recorded, latest_recorded)

    # convert from unix timestamp to seconds since start
    for _, interval_val in intervals.items():
        for i, _ in enumerate(interval_val.times):
            interval_val.times[i] -= earliest_recorded

    plot(intervals, filename)


def plot(intervals: Dict[str, Intervals], filename: str):
    """Plot audio event data"""
    add_part = intervals["jami:conference_add_participant"]

    fig, axis = plt.subplots(figsize=(20, 9))

    axis.set_xlabel("seconds since start")
    axis.set_ylabel("interval between events (seconds)")
    axis.set_title("jami:audio*")

    # only plot audio events
    events_to_plot_regex = re.compile("jami:audio")

    # plot each desired interval
    for interval_key, interval_val in intervals.items():
        if not events_to_plot_regex.match(interval_key):
            continue

        axis.plot(
            [i / 1e9 for i in interval_val.times],
            [i / 1e9 for i in interval_val.intervals],
            label=interval_key,
        )

    # generate vertical line for each time a new participant is added to conference
    for time in add_part.times:
        axis.axvline(
            time / 1e9,
            color="black",
            ls="--",
        )

    axis.legend()

    # save it as out.png
    filename = "out.png"
    fig.savefig(filename, dpi=100)
    print(f"png saved as {filename}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: python3 audio.py <lttng trace directory>", file=sys.stderr)
        sys.exit(-1)
    analyze(sys.argv[1])
