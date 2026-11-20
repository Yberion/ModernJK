"""Compares the statistics of scripted scenarios (plan steps 14 and 20) between two runs of gamestats_windows.sh or
gamestats.sh, for example two builds of the module: prints SAME, or the lines that differ.
Usage: python -I gamestats_scn.py <directory of the runs> <tagA> <tagB> [<tagA> <tagB> ...]
   or: python -I gamestats_scn.py --dump <directory of the runs> <prefix> <tag>... (the lines, for a diff)
(runs in <directory>/gs_<tag> for the Windows script, <directory>/<tag> for the Linux one). A run may play several
scenarios one after the other; each is named from its command in the run's measure.cfg ("saber 96 fast idle" is
saber_fast_idle: the type and the arguments after the repetitions; outside free for all, the game type before it,
duel_saber_fast_idle). The statistics are the lines printed after each
"scenario done"; the events left by the free play before the scenario (bodies fading, items respawning with the global
sound of a powerup, doors) and the death animation numbers (cycled by a static counter in player_die) are ignored."""
import re
import sys
from pathlib import Path

D = Path(sys.argv[1])
IGNORE = re.compile(r" (EV_BODYFADE|EV_ITEM_RESPAWN|EV_GLOBAL_SOUND|EV_PLAYDOORSOUND|EV_DEATH[123]) \d+")
KEEP = ("saber swings", "force statistics", "event statistics", "damage statistics", "shot statistics",
        "style statistics", "special move statistics", "scenario statistics", "score statistics", "team statistics",
        "item statistics", "vehicle statistics", "npc statistics", "hit location statistics")
# the game types' names in the scenarios' names (g_gametype; free for all has none)
GAMETYPES = {1: "holocron", 2: "jedimaster", 3: "duel", 4: "powerduel", 6: "team", 7: "siege", 8: "ctf", 9: "cty"}


def run_dir(tag):
    d = D / f"gs_{tag}" / "base"
    return d if d.exists() else D / tag / "base"


def names(base):
    """the scenarios of a run, in their order, from its measure.cfg"""
    out = []
    gametype = []
    for line in (base / "measure.cfg").read_bytes().decode("latin-1").splitlines():
        words = line.split()
        if words[:2] == ["set", "g_gametype"] and int(words[2]) in GAMETYPES:
            gametype = [GAMETYPES[int(words[2])]]
        if len(words) >= 3 and words[0] == "scenario":
            out.append("_".join(gametype + [words[1]] + words[3:]).replace("=", ""))
    return out


def stats(tag):
    """[(name, lines)] for every scenario the run played"""
    base = run_dir(tag)
    lines = (base / "qconsole.log").read_bytes().decode("latin-1").splitlines()
    done = [k for k, l in enumerate(lines) if "scenario done" in l]
    expected = names(base)
    if len(done) != len(expected):
        sys.exit(f"{base}: {len(done)} 'scenario done' for {len(expected)} scenarios (the run ended before the repetitions)")
    out = []
    for name, k in zip(expected, done):
        block = []
        for l in lines[k + 1:k + 18]:  # 15 lines used, the next scenario's first line ends it
            l = re.sub(r"^\S+ \S+ ", "", l)  # time stamp of logfile 2
            if l.startswith("scenario ") and not l.startswith("scenario statistics"):
                break  # the next scenario
            l = re.sub(r"game statistics over [^:]*:", "", l)  # duration and frame count differ with the rate
            if any(x in l for x in KEEP):
                block.append(IGNORE.sub("", l))
        out.append((name, block))
    return out


args = sys.argv[2:]
if sys.argv[1] == "--dump":
    # --dump <directory> <prefix> <tag>...: the compared lines of each scenario of each run, to compare two platforms'
    # or two builds' files with diff (the prefix is kept for the command line's compatibility)
    D = Path(args[0])
    for tag in args[2:]:
        for name, block in stats(tag):
            for line in block:
                print(f"{name}: {line}")
    sys.exit(0)
for a, b in zip(args[0::2], args[1::2]):
    sa, sb = stats(a), stats(b)
    print(f"{a} / {b}: {'SAME' if sa == sb else 'DIFF'}")
    for (na, xa), (nb, xb) in zip(sa, sb):
        for x, y in zip(xa, xb):
            if x != y:
                print(f"  {a} {na}: {x}")
                print(f"  {b} {nb}: {y}")
