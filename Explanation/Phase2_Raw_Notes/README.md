# Phase 2 raw notes

These are the working notes behind [01b_Research_Findings.md](../01b_Research_Findings.md). They are kept so every statement in the findings can be traced back to the exact engine line or web page it came from.

| File | Scope |
|---|---|
| [A_pcg_runtime.md](A_pcg_runtime.md) | PCG and navigation (none), which outputs can affect navigation, `pcg.FrameTime` vs the runtime-generation scheduler, runtime cvars, the world actor, component API, determinism inputs, maturity labels |
| [B_navigation.md](B_navigation.md) | Build locks and what releasing them does, the navigation tick, dirty areas, octree registration, runtime generation modes, Recast tile generation, per-area queries, path queries, navigation CSV stats |
| [C_existing_work.md](C_existing_work.md) | Engine features, the Epic forum threads (including a correction to the portfolio research), official docs, community tools, the gap restated |
| [D_measurement.md](D_measurement.md) | Facts inherited from UE5-UpdateBudget and re-opened, where PCG and navigation costs appear in CSV, wall-clock vs fixed-step time, performance-core pinning |

**The findings file is authoritative.** These notes are the audit trail. All engine line numbers are from UE 5.8.4 (CL 58902052).
