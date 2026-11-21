"""P08 NavReadyPCG - run a benchmark batch on the packaged build and collect every artefact (adapted from UE5-UpdateBudget).

Usage:
    python -I run_bench.py --configs B0 [--runs N] [--seed 1] [--cells 16] [--label NAME] [--no-prime] [--dry-run]

What one batch does:
  1. Pins itself to the performance cores (cpu_affinity.pin_self), so every game it starts inherits that mask from its
     first instruction; refuses to measure on a hybrid CPU if that fails. Then records the environment once, including
     the mask read back from Windows.
  2. Runs one PRIMING run (discarded) so file, driver and shader caches are warm, then every config --runs times,
     INTERLEAVED: the config order rotates every round, so drift spreads over all configs.
  3. Every run: the real game executable (not the bootstrap), REAL TIME capped at 60 fps (t.MaxFPS, not fixed step: see
     below), real GPU at 1280x720 windowed, no VSync, no sound, -traceautostart=0, the CSV capture opened and closed by the game's BenchStart / BenchEnd events, and -NRExit
     (the game's exit code is the run's verdict). GameUserSettings.ini is deleted first (it can override VSync and caps).
  4. After each run: moves the CSV capture, the run report (Saved/NRRuns/<name>.csv) and the run's log into
     Benchmarks/Raw as <batch>_<config>_r<run>.<ext>. Raw data is never overwritten; the manifest is rewritten after
     every run, so a crash loses nothing.
  5. Writes Benchmarks/Summaries/<batch>_summary.md: per config, the median across runs [min-max] of each run's
     per-cell M1 (generation start -> navigation-ready) median and p95, its settle part (generated -> ready), and the
     frame statistics over the capture.
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import glob
import json
import math
import os
import shutil
import statistics
import subprocess
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)
import cpu_affinity  # noqa: E402  (same folder)
import csv_stats  # noqa: E402

PROJECT_DIR = os.path.dirname(TOOLS_DIR)
BUILD_DIR = os.path.join(PROJECT_DIR, "Build", "Windows")
# The real game executable, launched directly: the bootstrap Build\Windows\NavReadyPCG.exe only starts this one, so
# launching it directly makes the game a direct child, with a reliable exit code and kill.
EXE = os.path.join(BUILD_DIR, "NavReadyPCG", "Binaries", "Win64", "NavReadyPCG.exe")
SAVED = os.path.join(BUILD_DIR, "NavReadyPCG", "Saved")
RAW_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Raw")
SUMMARY_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Summaries")
ENGINE_BUILD_VERSION = r"C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version"
MIN_RUNS = 4
RUN_TIMEOUT_SECONDS = 300.0   # engine start, warm-up, the 70 s flight, settling, pocket check and shutdown, with margin

# Real time, capped at 60 fps, instead of UE5-UpdateBudget's fixed step (-deterministic -FPS=60). The workload mixes
# game-time events (the camera path, the agents' requests) with real-time work (tile builds on worker threads). A fixed
# step runs game time as fast as frames allow, without waiting (UnrealEngine.cpp:3005-3017), so game time would run ahead
# of the wall clock by a factor that depends on each configuration's frame cost, and the configurations would no longer
# see the same workload in real time. With the cap, game time follows the wall clock while frames stay under 16.7 ms,
# and the capture shows every frame that doesn't. t.MaxFPS: UnrealEngine.cpp:12136-12138, 12234-12237.
COMMON_ARGS = ("-windowed -ResX=1280 -ResY=720 -nosound -unattended -traceautostart=0 -novsync "
               "-ini:Engine:[ConsoleVariables]:t.MaxFPS=60 "
               "-NRRun -NRExit -csvStartOnEvent=BenchStart -csvStopOnEvent=BenchEnd")

# Extra arguments per configuration (01_Problem_Analysis.md section 6.2). B1-B3 were dropped by the owner's scope cut.
CONFIGS = {
    "B0": "-NRConfig=B0",   # uncoordinated: props navigation-relevant from the start
    "NR": "-NRConfig=NR",   # NavReady: per-cell commit, per-cell readiness, hash at settle
}

SUMMARY_COLUMNS = ("GameThreadTime", "FrameTime", "Exclusive/GameThread/Tickables", "Exclusive/GameThread/NavigationBuild",
                   "Exclusive/GameThread/NavReady")


def environment_record() -> dict:
    """Facts that must accompany every result (Benchmarks/README.md)."""
    with open(ENGINE_BUILD_VERSION, encoding="utf-8") as handle:
        build = json.load(handle)

    def powershell(command: str) -> str:
        return subprocess.run(["powershell", "-NoProfile", "-Command", command], capture_output=True, text=True).stdout.strip()

    return {
        "date": dt.datetime.now().isoformat(timespec="seconds"),
        "engine": f"{build['MajorVersion']}.{build['MinorVersion']}.{build['PatchVersion']} (CL {build['Changelist']})",
        "build_config": "Development (packaged)",
        "cpu": powershell("(Get-CimInstance Win32_Processor).Name"),
        "gpus": powershell("(Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name + ' (driver ' + $_.DriverVersion + ')' }) -join '; '"),
        "ram_gb": powershell("[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB,1)"),
        "os": powershell("$o=Get-CimInstance Win32_OperatingSystem; $o.Caption + ' build ' + $o.BuildNumber"),
        "power_plan": powershell("powercfg /getactivescheme"),
        "on_ac_power": powershell("(Get-CimInstance Win32_Battery | Select-Object -First 1).BatteryStatus") == "2",
        "resolution": "1280x720 windowed, default scalability",
        "common_args": COMMON_ARGS,
        "cpu_affinity": cpu_affinity.applied_mask(),
    }


def unique_path(path: str) -> str:
    if os.path.exists(path):
        raise FileExistsError(f"refusing to overwrite raw data: {path}")
    return path


def read_report(path: str) -> list[dict]:
    """The director's run report: '#' comment lines, then one CSV row per cell."""
    with open(path, encoding="utf-8") as handle:
        return list(csv.DictReader(line for line in handle if not line.startswith("#")))


def cell_metrics(cells: list[dict]) -> dict:
    """Per-cell M1 (start -> ready) and settle (generated -> ready), in ms, over the cells that reached ready."""
    m1, settle = [], []
    for cell in cells:
        start, done, ready = float(cell["generation_start_s"]), float(cell["generation_done_s"]), float(cell["ready_s"])
        if start >= 0.0 and done >= 0.0 and ready >= 0.0:
            m1.append(1000.0 * (ready - start))
            settle.append(1000.0 * (ready - done))
    result = {"cells": len(cells), "cells_ready": len(m1),
              "dirtied_while_ready": sum(int(cell["dirtied_while_ready"]) for cell in cells)}
    if m1:
        result["m1_ms"] = csv_stats.summarize(m1)
        result["settle_ms"] = csv_stats.summarize(settle)
    return result


def launch(name: str, config: str, args) -> tuple[str, int | None, float, float]:
    """Starts one run and waits for it. Returns (command, exit code or None on timeout, start time, wall seconds)."""
    command = f'"{EXE}" {COMMON_ARGS} -NRSeed={args.seed} -NRCells={args.cells} -NRRunTag={name} {CONFIGS[config]}'
    if args.dry_run:
        print("DRY:", command)
        return command, 0, time.time(), 0.0
    settings = os.path.join(SAVED, "Config", "Windows", "GameUserSettings.ini")
    if os.path.exists(settings):
        os.remove(settings)
    started = time.time()
    exit_code = cpu_affinity.run_game(command, RUN_TIMEOUT_SECONDS)   # Popen + timeout + kill in a finally block
    return command, exit_code, started, time.time() - started


def collect(name: str, started: float) -> dict:
    """Moves this run's artefacts into Benchmarks/Raw and returns what was found."""
    record: dict = {}
    os.makedirs(RAW_DIR, exist_ok=True)

    captures = [p for p in glob.glob(os.path.join(SAVED, "Profiling", "CSV", "*.csv")) if os.path.getmtime(p) >= started]
    if captures:
        destination = unique_path(os.path.join(RAW_DIR, name + ".csv"))
        shutil.move(max(captures, key=os.path.getmtime), destination)
        record["capture"] = os.path.basename(destination)
        try:
            analysis = csv_stats.analyse(destination, SUMMARY_COLUMNS)
            record["stats"] = analysis["columns"]
            record["finalised"] = analysis["finalised"]
            record["frames"] = analysis["frames"]
            header, data, _, _ = csv_stats.load_capture(destination)
            values = csv_stats.column_values(header, data, "GameThreadTime") if "GameThreadTime" in header else []
            record["frames_over_33ms"] = sum(1 for value in values if value > 33.3)
        except (ValueError, IndexError) as error:
            record["analysis_error"] = str(error)

    report = os.path.join(SAVED, "NRRuns", name + ".csv")
    if os.path.exists(report):
        destination = unique_path(os.path.join(RAW_DIR, name + "_cells.csv"))
        shutil.move(report, destination)
        record["cells_report"] = os.path.basename(destination)
        record["cells"] = cell_metrics(read_report(destination))

    log = os.path.join(SAVED, "Logs", "NavReadyPCG.log")
    if os.path.exists(log) and os.path.getmtime(log) >= started:   # only this run's log, never a stale one
        with open(log, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        record["log_errors"] = text.count("Error:")
        record["log_warnings"] = text.count("Warning:")
        shutil.copy(log, unique_path(os.path.join(RAW_DIR, name + ".log")))
    else:
        record["log_missing"] = True
    return record


def is_failure(record: dict) -> bool:
    cells = record.get("cells", {})
    return (record.get("exit_code") != 0 or "stats" not in record or record.get("finalised") is False
            or "analysis_error" in record or record.get("log_errors") or record.get("log_warnings") or record.get("log_missing")
            or not cells or cells.get("cells_ready") != cells.get("cells"))


def write_manifest(batch: str, environment: dict, records: list[dict]) -> None:
    os.makedirs(RAW_DIR, exist_ok=True)
    with open(os.path.join(RAW_DIR, batch + "_manifest.json"), "w", encoding="utf-8") as handle:
        json.dump({"environment": environment, "runs": records}, handle, indent=2)


def median_range(values: list[float]) -> str:
    return f"{statistics.median(values):.1f} [{min(values):.1f}-{max(values):.1f}]" if values else "n/a"


def write_summary(batch: str, environment: dict, records: list[dict]) -> str:
    os.makedirs(SUMMARY_DIR, exist_ok=True)
    groups: dict[str, list[dict]] = {}
    for record in records:
        if not record.get("priming") and not is_failure(record):
            groups.setdefault(record["config"], []).append(record)

    lines = [f"# Benchmark batch {batch}", "", "## Environment", "", "```json", json.dumps(environment, indent=2), "```", "",
             "## Results", "", "Each cell: the median across runs of each run's statistic, then [min-max across runs]. "
             "M1 = generation start -> navigation-ready, settle = generated -> ready, per cell, wall-clock ms.", "",
             "| Config | Runs | M1 median | M1 p95 | Settle median | Settle p95 | GameThreadTime p99 (ms) | GameThreadTime max (ms) "
             "| Frames > 33.3 ms | NavReady median / max (ms) | Dirtied while ready |",
             "|---|---|---|---|---|---|---|---|---|---|---|"]
    for config in sorted(groups):
        group = groups[config]

        def cell_stat(metric: str, stat: str) -> list[float]:
            return [g["cells"][metric][stat] for g in group if metric in g["cells"]]

        def frame_stat(column: str, stat: str) -> list[float]:
            return [g["stats"][column][stat] for g in group if column in g["stats"]]

        nav_ready = (f"{median_range(frame_stat('Exclusive/GameThread/NavReady', 'median'))} / "
                     f"{median_range(frame_stat('Exclusive/GameThread/NavReady', 'max'))}")
        lines.append(f"| {config} | {len(group)} | {median_range(cell_stat('m1_ms', 'median'))} | {median_range(cell_stat('m1_ms', 'p95'))} "
                     f"| {median_range(cell_stat('settle_ms', 'median'))} | {median_range(cell_stat('settle_ms', 'p95'))} "
                     f"| {median_range(frame_stat('GameThreadTime', 'p99'))} | {median_range(frame_stat('GameThreadTime', 'max'))} "
                     f"| {median_range([float(g.get('frames_over_33ms', 0)) for g in group])} | {nav_ready} "
                     f"| {sum(g['cells']['dirtied_while_ready'] for g in group)} |")
    failures = [r["name"] for r in records if not r.get("priming") and is_failure(r)]
    lines += ["", f"Failed runs (excluded above): {failures or 'none'}.",
              f"Raw files: `Benchmarks/Raw/{batch}_*` (capture, cell report and log per run) and `{batch}_manifest.json`."]
    path = os.path.join(SUMMARY_DIR, batch + "_summary.md")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--configs", default="B0")
    parser.add_argument("--runs", type=int, default=0,
                        help=f"runs per config; 0 = the smallest multiple of the number of configs that is at least {MIN_RUNS}")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--cells", type=int, default=16)
    parser.add_argument("--cooldown", type=float, default=5.0)
    parser.add_argument("--label", default="")
    parser.add_argument("--no-prime", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    if not 1 <= args.cells <= 16:
        parser.error("--cells must be 1 to 16 (the game would clamp it and the manifest would be wrong)")
    if args.label and not all(ch.isalnum() or ch in "-_" for ch in args.label):
        parser.error("--label may contain only letters, digits, '-' and '_' (it becomes part of file names and -NRRunTag)")
    configs = [c.strip() for c in args.configs.split(",") if c.strip()]
    unknown = [c for c in configs if c not in CONFIGS]
    if unknown:
        parser.error(f"unknown config(s) {unknown}; available: {sorted(CONFIGS)}")
    if args.runs <= 0:
        args.runs = math.ceil(MIN_RUNS / len(configs)) * len(configs)
    elif args.runs % len(configs):
        print(f"WARNING: --runs {args.runs} is not a multiple of {len(configs)} configs; the rotation is unbalanced")
    if not os.path.exists(EXE) and not args.dry_run:
        parser.error(f"packaged build not found: {EXE} (run Tools\\package.bat first)")

    if not args.dry_run:
        # Pin once, before anything is recorded or started (Benchmarks/README.md): every game started below inherits the
        # mask, and the environment record reads back the mask actually applied.
        mask = cpu_affinity.performance_core_mask()
        if mask and not cpu_affinity.pin_self(mask):
            parser.error(f"could not pin the batch to the performance cores (0x{mask:X}); refusing to measure")
        # Keep the display and system awake while the batch runs (ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED).
        if cpu_affinity.KERNEL32 is not None:
            cpu_affinity.KERNEL32.SetThreadExecutionState(0x80000000 | 0x00000001 | 0x00000002)

    batch = dt.datetime.now().strftime("%Y-%m-%d_%H%M%S") + (f"_{args.label}" if args.label else "")
    environment = environment_record() if not args.dry_run else {}
    records: list[dict] = []

    def run(config: str, run_index: int, priming: bool = False) -> None:
        name = f"{batch}_{config}_r{run_index}" + ("_prime" if priming else "")
        command, exit_code, started, wall = launch(name, config, args)
        record = {"name": name, "config": config, "run": run_index, "priming": priming, "seed": args.seed,
                  "cells_requested": args.cells, "exit_code": exit_code, "wall_seconds": round(wall, 1), "command": command}
        if args.dry_run:
            return
        record.update(collect(name, started))
        records.append(record)
        write_manifest(batch, environment, records)
        m1 = record.get("cells", {}).get("m1_ms", {})
        print(f"  {name}: exit {exit_code}, {wall:.0f} s, cells ready {record.get('cells', {}).get('cells_ready')}, "
              f"M1 median {m1.get('median', float('nan')):.1f} ms p95 {m1.get('p95', float('nan')):.1f} ms, "
              f"log errors {record.get('log_errors')} warnings {record.get('log_warnings')}"
              + ("  [PRIMING, discarded]" if priming else ""))
        time.sleep(args.cooldown)

    if not args.no_prime:
        run(configs[0], 0, priming=True)
    for run_index in range(1, args.runs + 1):
        rotation = (run_index - 1) % len(configs)
        for config in configs[rotation:] + configs[:rotation]:   # interleave: rotate the order every round
            run(config, run_index)

    if args.dry_run:
        return 0
    print("summary:", write_summary(batch, environment, records))
    failures = [r["name"] for r in records if not r.get("priming") and is_failure(r)]
    if failures:
        print("ATTENTION - failed runs (exit code, capture, cell report, log errors or warnings, or a cell never ready):", failures)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
