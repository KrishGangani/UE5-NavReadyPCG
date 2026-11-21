"""P08 NavReadyPCG: step S3's probe check. Does NavReady's "ready" moment agree with the engine's own task count?

Usage: python -I check_probe.py <capture.csv or a glob: the newest match is used> [--cell 0] [--tolerance 1]
       python -I check_probe.py --selftest

For a run in which only one cell builds (-NRCells=1), the cell's tiles are the only navigation work after its commit,
so the engine's remaining tile tasks (CSV stat NavTasks/NumRemainingTasks_<navigation data>, recorded every navigation
tick) must drop to 0 when the probe says the cell is ready. The check finds, in the capture:
  C = the frame of the event NavReady/CellCommitted_<cell>
  R = the frame of the event NavReady/CellReady_<cell>
  Z = the first frame after C at which the task count is 0, after it was above 0 at least once since C
and passes if |R - Z| <= tolerance (default 1 frame: navigation ticks before the tickables, in the same frame).
Exit code 0 = pass, 1 = fail or inconclusive (no tasks seen, an event missing).
"""
from __future__ import annotations

import argparse
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import csv_stats  # noqa: E402

TASKS_PREFIX = "NavTasks/NumRemainingTasks_"


def event_frame(header: list[str], rows: list[list[str]], name: str) -> int | None:
    return next((i for i, row in enumerate(rows) if name in csv_stats.frame_events(header, row)), None)


def check(header: list[str], rows: list[list[str]], cell: int, tolerance: int) -> tuple[bool, str]:
    tasks_column = next((h for h in header if h.startswith(TASKS_PREFIX)), None)
    if tasks_column is None:
        return False, f"no {TASKS_PREFIX}* column in the capture"
    index = header.index(tasks_column)
    tasks = [float(row[index]) if index < len(row) and row[index].strip() else 0.0 for row in rows]

    commit = event_frame(header, rows, f"NavReady/CellCommitted_{cell}")
    ready = event_frame(header, rows, f"NavReady/CellReady_{cell}")
    if commit is None or ready is None:
        return False, f"event missing: committed at {commit}, ready at {ready}"

    # Tasks may already run in the commit frame itself: in the baselines the props reach navigation while PCG spawns
    # them, before NavReady sees the cell generated (S3 code review).
    seen_work, zero = tasks[commit] > 0, None
    for frame in range(commit + 1, len(tasks)):
        if tasks[frame] > 0:
            seen_work = True
        elif seen_work:
            zero = frame
            break
    if zero is None:
        return False, f"inconclusive: committed at frame {commit}, ready at {ready}, " + (
            "the task count never returned to 0" if seen_work else "no tile tasks seen after the commit")
    peak = max(tasks[commit:zero])
    delta = ready - zero
    passed = abs(delta) <= tolerance
    return passed, (f"{tasks_column}: committed at frame {commit}, tasks peaked at {peak:.0f}, back to 0 at frame {zero}; "
                    f"NavReady ready at frame {ready} (difference {delta:+d} frames, tolerance {tolerance})")


def selftest() -> None:
    header = ["EVENTS", "FrameTime", "NavTasks/NumRemainingTasks_RecastNavMesh-Default"]
    def rows(ready_frame: int) -> list[list[str]]:
        tasks = [0, 0, 0, 3, 5, 2, 0, 0, 0]
        out = []
        for frame, count in enumerate(tasks):
            events = []
            if frame == 2:
                events.append("NavReady/CellCommitted_0##1.0")
            if frame == ready_frame:
                events.append("NavReady/CellReady_0##1.1")
            out.append([";".join(events), "16.6", str(count)])
        return out
    assert check(header, rows(6), 0, 1)[0]            # ready the frame the count returns to 0
    assert check(header, rows(7), 0, 1)[0]            # one frame later: within tolerance
    assert not check(header, rows(8), 0, 1)[0]        # two frames late
    assert not check(header, rows(4), 0, 1)[0]        # ready while tasks still run
    no_tasks = [[r[0], r[1], "0"] for r in rows(6)]
    assert not check(header, no_tasks, 0, 1)[0]       # inconclusive
    early = [[r[0], r[1], c] for r, c in zip(rows(3), ["0", "0", "4", "0", "0", "0", "0", "0", "0"])]
    assert check(header, early, 0, 1)[0]              # tasks only in the commit frame, ready the next frame
    print("selftest: OK (agreement, tolerance, early and late readiness, no tasks, tasks only in the commit frame)")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("capture", nargs="?")
    parser.add_argument("--cell", type=int, default=0)
    parser.add_argument("--tolerance", type=int, default=1)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    if args.selftest:
        selftest()
        return 0
    if not args.capture:
        parser.error("give a capture, or --selftest")
    path = args.capture
    if any(ch in path for ch in "*?"):
        matches = glob.glob(path)
        if not matches:
            print(f"FAIL: no capture matches {path}")
            return 1
        path = max(matches, key=os.path.getmtime)
    print(f"capture: {os.path.basename(path)}")
    header, rows, _, finalised = csv_stats.load_capture(path)
    passed, detail = check(header, rows, args.cell, args.tolerance)
    print(("PASS: " if passed else "FAIL: ") + detail + ("" if finalised else "  [capture not finalised]"))
    return 0 if passed and finalised else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
