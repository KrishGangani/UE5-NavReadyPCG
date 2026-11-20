# P08 · NavReadyPCG: runtime generation that's ready to play

**Domain:** Procedural & runtime generation · navigation · **Difficulty:** ★★★★½ · **Engine:** UE 5.8.4

> **Status: planned; Phases 1–3 (problem analysis, research, architecture) are starting.** The ten projects are developed in parallel, and every Unreal Engine build and benchmark runs one at a time on the same machine, so measurements stay clean. Everything below is the plan. The success criteria are **targets fixed before any code exists, not results**. Real numbers will only come from controlled before/after runs, each traceable to a raw benchmark batch.

## The problem

Runtime PCG content isn't coordinated with the navmesh: PCG uses no nav build locks, and procedural instances never affect navigation. So AI path-fails or waits on fresh content, and the PCG budget ignores scheduler time

## Proposed solution

A budgeted pipeline: generate → lock navmesh → rebuild dirty areas → mark the cell "AI-ready" → activate AI. Determinism is checked by hashing the generated output

## Main Unreal Engine technologies

PCG runtime generation (production-ready since 5.7), `AddNavigationBuildLock`/`RemoveNavigationBuildLock`, dynamic navmesh, StateTree AI, hashing

## Success criteria (targets, defined before implementation)

Time-to-navigable per cell (ms); worst frame during generation; AI path failures per 1,000 requests near fresh content (target 0); determinism hash match rate across runs (target 100%)

## Existing work it must be compared against

PCG runtime scheduler, `pcg.FrameTime`, GPU nodes, FastGeo interop; forum nav-lock workaround

**The gap it targets:** PCG → nav-ready → AI-active orchestration with a budget and determinism checks (gap strength: Medium–Strong).

**Main risk:** Dynamic navmesh cost; PCG API churn between versions

## Why studios care

CDPR Expert AI "procedural content generation… systemic open world"; Rockstar "open-world, population and streaming systems"; PCG runtime-cost thread with Epic

## Method

Every project in the portfolio follows the same ten phases: problem analysis → research → architecture → foundation → implementation → explanation → testing → profiling → review → portfolio. Each phase is documented in `Explanation/` (files 00–12, including a line-by-line code walkthrough), and every claimed improvement must be measured against the existing solutions above on the same workload. Engine: Unreal Engine 5.8.4, C++.

## Folder layout

```text
P08_NavReadyPCG\
├── README.md        ← this page (becomes the project's portfolio page)
├── Explanation\     ← 00–12, created when the project starts
├── Benchmarks\      ← Raw\ + Summaries\
├── Media\           ← diagrams, screenshots, demo video
├── Tools\           ← analysis and run scripts
└── NavReadyPCG\   ← the Unreal Engine project (created in its Phase 4)
```
