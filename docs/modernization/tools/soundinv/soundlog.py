"""Summarizes a sound log: the SNDLOG lines a client writes while s_log is set (soundlog.patch on the old mixer).

    python -I docs/modernization/tools/soundinv/soundlog.py <qconsole.log> [<qconsole.log> ...]

For each log: the session's length and frames, the sounds started, how many channels played at each frame, the
sounds still audible that a new one took the channel of (kick), the replacements on an entity's channel, the sounds
stopped for being out of range at one frame (drop), the looping sounds merged by file, the Doppler factors, the same
sound started twice at the same instant at the same place, and the sounds of the defects read in the code (the rocket
launcher's lock, the disruptor's zoom loop, "no force").
"""

import collections
import re
import sys

LINE = re.compile(r"SNDLOG (-?\d+) (\w+) ?(\S*)(.*)$")
FIELD = re.compile(r"(\w+)=(\S+)")


def parse(path):
    """Returns the SNDLOG records of a log as (time, kind, sound, fields)."""

    records = []

    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.search(line)

            if not m:
                continue

            sound = m.group(3)
            rest = m.group(4)

            # a record without a sound has its first field where the sound would be
            if "=" in sound:
                rest = sound + rest
                sound = ""

            records.append((int(m.group(1)), m.group(2), sound, dict(FIELD.findall(rest))))

    return records


def top(counter, n=8):
    """The n most frequent entries of a counter, as one line each."""

    return [f"    {count:6} {key}" for key, count in counter.most_common(n)]


def summarize(path):
    """Prints the summary of one log."""

    records = parse(path)
    kinds = collections.Counter(r[1] for r in records)
    times = [r[0] for r in records]
    frames = [r for r in records if r[1] == "frame"]
    print(f"== {path}")

    if not records:
        print("  no SNDLOG line")
        return

    seconds = (max(times) - min(times)) / 1000.0
    print(f"  {seconds:.1f} s of game time, {len(frames)} frames ({len(frames) / max(seconds, 0.001):.0f} per second)")
    print(f"  records: {dict(kinds)}")

    busy = [int(r[3].get("busy", 0)) for r in frames]

    if busy:
        full = sum(1 for b in busy if b >= 32)
        print(f"  channels playing at a frame: mean {sum(busy) / len(busy):.1f}, most {max(busy)}, all 32 at {full} frames "
              f"({100.0 * full / len(busy):.1f} %)")

    starts = [r for r in records if r[1] == "start"]
    print(f"  sounds started: {len(starts)} ({len(starts) / max(seconds, 0.001):.1f} per second)")

    kicks = [r for r in records if r[1] == "kick"]
    audible = [r for r in kicks if r[3].get("audible") == "1"]
    print(f"  channels taken from another sound: {len(kicks)}, of a sound still playing: {len(audible)}")
    print("\n".join(top(collections.Counter(r[2] for r in audible))))

    replaces = [r for r in records if r[1] == "replace" and r[3].get("audible") == "1"]
    print(f"  sounds cut by the same entity's channel while playing: {len(replaces)}")
    print("\n".join(top(collections.Counter(f"{r[2]} chan={r[3].get('chan')} by chan={r[3].get('by_chan')}" for r in replaces), 6)))

    drops = [r for r in records if r[1] == "drop"]
    print(f"  sounds stopped for good when out of range at one frame: {len(drops)}")
    print("\n".join(top(collections.Counter(r[2] for r in drops), 6)))

    merges = [r for r in records if r[1] == "loopmerge"]

    if merges:
        most = max(int(r[3]["count"]) for r in merges)
        print(f"  looping sounds merged into one channel: at {len({r[0] for r in merges})} frames, at most {most} of one file")
        print("\n".join(top(collections.Counter(r[2] for r in merges), 4)))

    if kinds.get("loopfull"):
        print(f"  looping sounds dropped, more than the frame holds: {kinds['loopfull']}")

    dopplers = collections.defaultdict(list)

    for r in records:
        if r[1] == "doppler":
            dopplers[r[2]].append((float(r[3]["scale"]), float(r[3]["dist"])))

    for sound, values in dopplers.items():
        scale, dist = max(values)
        print(f"  Doppler on {sound}: {len(values)} frames, the pitch up to {scale:.2f} times at {dist:.0f} units")

    # the same sound started twice at one instant on one entity or at one place (within 32 units of distance)
    same = collections.Counter()
    seen = {}

    for r in starts:
        key = (r[0], r[2])

        if key in seen and abs(float(seen[key].get("dist", 0)) - float(r[3].get("dist", 0))) < 32:
            same[r[2]] += 1

        seen[key] = r[3]

    print(f"  the same sound started twice at one instant and place: {sum(same.values())}")
    print("\n".join(top(same, 6)))

    for label, pattern in (("rocket lock", r"/(lock|tick)\.wav$|/(lock|tick)$"), ("disruptor zoom loop", r"zoomloop"),
                           ("no force", r"noforce|force/fizz")):
        matched = [r for r in starts if re.search(pattern, r[2])]

        if matched:
            dists = sorted(float(r[3].get("dist", -1)) for r in matched)
            print(f"  {label}: {len(matched)} starts ({len(matched) / max(seconds, 0.001):.1f} per second), entity "
                  f"{collections.Counter(r[3].get('ent') for r in matched).most_common(2)}, distance {dists[0]:.0f} to {dists[-1]:.0f}")


def main():
    for path in sys.argv[1:]:
        summarize(path)


if __name__ == "__main__":
    main()
