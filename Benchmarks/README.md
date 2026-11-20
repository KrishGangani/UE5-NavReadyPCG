# P08 · Benchmarks

Every number in this project's write-up must trace back to a file in this folder. The method is in [../Explanation/01_Problem_Analysis.md §7](../Explanation/01_Problem_Analysis.md). The results, batch by batch, are in [../Explanation/09_Testing_And_Benchmarks.md](../Explanation/09_Testing_And_Benchmarks.md).

**Status (2026-11-21):** no batch has been run yet. The first one is the baseline capture at the end of step S4, before the pipeline exists.

## Measurement rules

1. **Baselines first.** B0–B3 are captured before the NavReady pipeline exists, on the same workload, build and machine as every later run.
2. **Packaged Development build only** (never PIE, never the editor). The launcher engine can't build the Test configuration.
3. **Fixed step** (`-deterministic -FPS=60 -novsync`), real GPU, windowed 1280×720, no sound. The capture window is opened and closed by the game itself (`BenchStart` / `BenchEnd` CSV events), never by a timer. Durations that depend on worker threads (time-to-navigable, time-to-AI-active) are measured in **wall-clock** milliseconds.
4. **Pinned to the performance cores.** `run_bench.py` pins itself once (`pin_self`, mask from `Tools\cpu_affinity.py`, `0xFFFF` on this machine) before it records anything, and refuses to measure if that fails. It then starts the real game executable (`Build\Windows\NavReadyPCG\Binaries\Win64\NavReadyPCG.exe`) with `subprocess.Popen`; the game inherits the mask from its first instruction. Each run has a timeout, and the process tree is killed in a `finally` block. The mask read back from Windows (`applied_mask()`) is recorded in the environment.
5. **One discarded priming run per batch**, then **runs per configuration = the number of configurations, interleaved**, with the order rotated every round so every configuration sits in every position exactly once (5 runs each for the full matrix; 4 for the baseline batch). Combinations no success criterion needs are skipped.
6. **Statistics:** median, p95 and p99 per run (linear-interpolation percentiles, `Tools\csv_stats.py`), then the median across runs with the [min–max] across runs. FPS is never used as evidence. An improvement is claimed only when the run ranges don't overlap.
7. **The environment is recorded with every batch** (below), automatically, by `Tools\run_bench.py`.

## Configurations

| ID | Navigation handling | Agents become active |
|---|---|---|
| `B0` | Dynamic navmesh, no coordination | at spawn |
| `B1` | Global build lock while any cell generates; released with the default action (`Rebuild` → `RebuildAll`) | at spawn |
| `B2` | Global build lock; released with `NoRebuild` | at spawn |
| `B3` | B0's navigation | when navigation is globally idle |
| `NR` | NavReady: per-cell commit, per-cell readiness, content hash | when their own cell is AI-ready |

## Folders

| Folder | What goes in it | Rule |
|---|---|---|
| `Raw\` | CSV profiler captures (`.csv`), per-run reports (per-cell timings, hashes, path results), game logs (`.log`), Unreal Insights traces (`.utrace`), one `_manifest.json` per batch | **Never edited.** Anything re-processed is written to `Summaries\`. Only the manifests are in git (exact command lines, environment, per-run statistics); the captures are attached as `<batch>_raw.zip` to this repository's **Releases** page for every batch cited in the docs |
| `Summaries\` | Computed statistics, SC1–SC7 pass/fail tables, charts | Each summary names the batch it was computed from |

## File naming

`<batch>_<Config>_r<run>.<ext>`, with `<batch>` = `YYYY-MM-DD_HHMMSS[_label]`. `run_bench.py` refuses to overwrite an existing file.

- `<Config>`: `B0`, `B1`, `B2`, `B3`, `NR` (plus `NR-K<n>` for the `nr.CommitsPerFrame` sweep).
- Example: `2026-11-28_101500_B0_r1.csv` (+ `_report.csv` + `.log`), `2026-11-28_101500_manifest.json` in `Raw\`, and `2026-11-28_101500_summary.md` in `Summaries\`.
- `_r0_prime` files are the discarded priming run, kept for transparency and excluded from every summary.

## Environment record (written into every manifest and summary)

```text
Engine:            UE 5.8.4 (CL 58902052), launcher build
Build:             Development, standalone packaged (never PIE)
Compiler:          MSVC 14.44.35229 (VS 2022 17.14), Windows SDK 10.0.22621.0
CPU / GPU / RAM:   i7-14650HX (8P + 8E) / RTX 5070 Laptop (driver 610.47) / 32 GB
CPU affinity:      0xFFFF (performance cores only), read back from Windows after pinning the batch
Storage:           project on D: (Kingston NVMe), engine on C: (WD NVMe)
OS:                Windows 11 build 26200
Power:             AC, the same Windows power plan every batch (recorded)
Resolution:        1280x720 windowed, default scalability, no VSync, no sound
Command line:      exact, per run, in the manifest
Warm-up excluded:  until BenchStart (camera at the start of its path)    Capture: BenchStart → BenchEnd
```

## Run log

| Batch | Configs | Runs | Purpose | Summary |
|---|---|---|---|---|
| — | — | — | No batch yet (Phase 4 ran build and smoke checks only; they are in `09`) | — |
