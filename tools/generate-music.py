import argparse
import collections
import itertools
import sys

import mido

# number of microbits
NUMBER_OF_MICROBITS = 3

parser = argparse.ArgumentParser()
parser.add_argument('filename', nargs='+')
parser.add_argument('--min-length-ms', default=5000, type=int)


def pairs(items, sentinel=object()):
    last = sentinel
    for item in items:
        if last is not sentinel:
            yield (last, item)
        last = item


def midi_period_us(note):
    if note == 0:
        return 0
    freq = 27.5 * 2 ** ((note - 21) / 12)
    return int(round(1000000 / freq))


def midi_velocity(velocity):
    '''
    >>> midi_velocity(0)
    0
    >>> midi_velocity(100)
    127
    >>> midi_velocity(50) < 63
    True
    '''
    assert 0 <= velocity <= 10000
    return int(round(128 ** (velocity / 100.0) - 1))


def find_duration(start_msg, start_msg_time, midi):
    # This is for finding the duration of a note
    duration = 0
    time = 0

    for msg in midi:
        # check for the correct message type
        time += msg.time
        if not (msg.type == 'note_on' or msg.type == 'note_off'):
            continue
        elif msg.note == start_msg.note and msg.velocity == 0 and start_msg_time < time:
            duration = time - start_msg_time
            break

    return duration


class Microbit:
    notes = []
    time = 0
    number = 0

    def __init__(self, number):
        self.number = number
        self.time = 0
        self.notes = []

    def increaseClock(self, duration):
        self.time += duration

    def isFree(self, t):
        return self.time <= t

    def addNote(self, period, duration, velocity, time):
        pause_duration = round((time - self.time) * 1000)
        self.notes.append((0, round(pause_duration), velocity))
        self.increaseClock(pause_duration / 1000)
        self.notes.append((period, duration, velocity))
        self.increaseClock(duration / 1000)
        # print("update notes on microbit")
        # print(self.number)
        # print(self.notes)

    def getOutput(self):
        return self.notes


def midi_to_events(midi):
    '''
    ReturnType: [[Tuple]]
    Given a midi file, emit 3-sized array with events for three separate microbits.
    '''
    duration = 0
    time = 0

    # initialisation
    microbits = [Microbit(i) for i in range(NUMBER_OF_MICROBITS)]
    # print(microbits)

    for msg in midi:
        time += msg.time
        # print(f"msg.time {msg.time} / time {time}")
        period = 0
        velocity = 0

        if msg.type == 'note_on' and msg.velocity > 0:
            duration = find_duration(msg, time, midi)
            # print(msg)
            # print(f"{duration} --duration--")
            period = midi_period_us(msg.note)
            velocity = midi_velocity(msg.velocity)

        duration = int(round(duration * 1000))
        # if duration <= 0:
        #     continue

        if velocity <= 10:
            continue

        i = 0
        while i < (NUMBER_OF_MICROBITS):
            if microbits[i].isFree(time):
                # print("added note to microbit")
                # print(i)
                microbits[i].addNote(period, duration, velocity, time)
                i = NUMBER_OF_MICROBITS
            else:
                i += 1

    output = [microbits[i].getOutput() for i in range(NUMBER_OF_MICROBITS)]
    # print(output)
    return output


def deduplicate_rests(events):
    '''
    ArgType: [Tuples]
    >>> list(deduplicate_rests([(1, 2, 3), (0, 2, 0), (0, 3, 0)]))
    [(1, 2, 3), (0, 5, 0)]
    '''
    current_rest_duration = 0
    for event in events:
        period = event[0]
        duration = event[1]

        if period == 0:
            current_rest_duration += duration
        else:
            if current_rest_duration > 0:
                yield (0, current_rest_duration, 0)
                current_rest_duration = 0
            yield event
    if current_rest_duration > 0:
        yield (0, current_rest_duration, 0)


def segment_on_breaks(events, min_duration=100, min_rest_duration=50):
    segment = []
    for event in events:
        period, duration, _ = event
        is_rest = period == 0
        segment.append(event)
        if (
                (not is_rest and duration >= min_duration) or
                (is_rest and duration >= min_rest_duration)
        ):
            yield segment
            segment = []
    if segment:
        yield segment


def merge_event_segments(segments, min_duration):
    '''
    Given an iterable of event segments, emit new event segments such that
    every segment has a duration of at least min_duration.
    '''
    current_segment = []
    current_duration = 0
    for segment in segments:
        current_segment.extend(segment)
        current_duration += sum(
            d
            for _, d, _ in segment
        )

        if current_duration >= min_duration:
            yield current_segment
            current_segment = []
            current_duration = 0

    if current_segment:
        yield current_segment


def compress_segments(segments):
    '''
    Given an iterable of event segments, return a list of events and indices
    into the events to reduce the overall size.

    Returns: (events, segments)
        events: list of (period, duration) pairs
        segments: list of (start_index, length) pairs
    '''
    # deduplicate the segments
    output_segments = []
    d = collections.defaultdict(list)
    for i, segment in enumerate(segments):
        output_segments.append(None)
        segment = list(segment)
        d[tuple(segment)].append(i)

    # get the segments in order of the first usage
    output_events = []
    by_first_index = lambda item: item[1][0]
    for segment, indices in sorted(d.items(), key=by_first_index):
        start_index = len(output_events)
        length = len(segment)

        output_events.extend(segment)
        for i in indices:
            output_segments[i] = (start_index, length)

    assert all(s is not None for s in output_segments)

    return output_events, output_segments


def record_length(items, fn):
    n = 0
    for item in items:
        yield item
        n += 1
    fn(n)


class Transformer:
    def __init__(self, midis, min_length_ms, microbit_nb):
        self.midis = midis
        self.min_length_ms = min_length_ms
        self.microbit_nb = microbit_nb

        self.metadata = []

    def __iter__(self):
        yield 'typedef struct {'
        yield ' int period_us;'
        yield ' int duration_ms;'
        yield ' int velocity;'
        yield '} music_event_t;'
        yield 'const music_event_t _song_events[] = {'

        midi_lengths = []

        event_segments = [
            record_length(
                merge_event_segments(
                    segment_on_breaks(
                        deduplicate_rests(
                            midi_to_events(midi)[self.microbit_nb],
                        )
                    ),
                    self.min_length_ms,
                ),
                midi_lengths.append,
            )
            for midi in self.midis
        ]
        event_segments = itertools.chain(*event_segments)

        events, segments = compress_segments(event_segments)

        for e in events:
            yield '    {.period_us = %s, .duration_ms = %s, .velocity = %s},' % e
        yield '};'
        yield ''

        # yield '#endif'

        assert sum(midi_lengths) == len(segments)
        start = 0
        it = iter(segments)
        for midi_length in midi_lengths:
            durations = []
            for _ in range(midi_length):
                start_index, length = next(it)
                durations.append(sum(
                    d
                    for _, d, _ in events[start_index:start_index + length]
                ))
            self.metadata.append((start, durations))
            start += midi_length


def main(args):
    midis = [
        mido.MidiFile(filename)
        for filename in args.filename
    ]
    for i in range(NUMBER_OF_MICROBITS):
        with open(f"note{i}.cpp", mode='w') as note:
            t = Transformer(midis, args.min_length_ms, i)
            for line in t:
                print(line)
                note.write(line + "\n")

    # for filename, (start, durations) in zip(args.filename, t.metadata):
    #     durations = ' '.join(str(d) for d in durations)
    #     print(
    #         '{}: tools/play-music -p /dev/ttyACM* -s {} {}'.format(
    #             filename, start, durations,
    #         ),
    #         file=sys.stderr,
    #     )


if __name__ == '__main__':
    main(parser.parse_args())


    # Slash out everything except for the first array
    # output three different cpp files (note1.cpp)
    # structure
    # include "stdint.h"
    #
    # typedef
    # struct
    # {
    #     uint16_t
    # period_us;
    # uint16_t
    # duration_ms;
    # uint16_t
    # velocity;
    # } music_event_t;
    #
    # const
    # music_event_t
    # _song_events[] = {
    #     {.period_us = 3822,.duration_ms = 1000,.velocity = 78},
    # {.period_us = 3405,.duration_ms = 1000,.velocity = 78},
    # };
