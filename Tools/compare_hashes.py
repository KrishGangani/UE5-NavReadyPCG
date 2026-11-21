"""P08 NavReadyPCG: step S2's determinism check. Compares the per-cell content hashes that UNRBenchmarkSubsystem logs.

Usage: python -I compare_hashes.py <folder with the S2_*.log files>

Pass criteria (01_Problem_Analysis.md SC2, as far as S2 can test it):
  1. seed 1, run a == run b                       (the same seed gives the same content, run to run)
  2. seed 1 packaged no-GPU == GPU == uncooked     (the same content in every build and RHI)
  3. seed 2 differs from seed 1 in every cell     (the seed really drives the content)
  4. every run generated all of its cells
  5. no cell encloses a pocket                     (the prop density leaves every open floor point reachable)
The order-dependent hash is reported but not required (hypothesis H6). Exit code 0 = pass, 1 = fail.
"""
import os
import re
import sys

CELL_LINE = re.compile(r"NRRun cell\s+(\d+) \((\d+),(\d+)\) seed (\d+):\s+(\d+) instances, content (0x[0-9A-F]{8}), "
                       r"ordered (0x[0-9A-F]{8}), generation .* s, pockets (\d+) of (\d+) navmesh points")
RESULT_LINE = re.compile(r"NRRun '([^']*)' result \(([^)]*)\): (\d+)/(\d+) cells generated, (\d+) instances, (\d+) pocket points, "
                         r"run content hash (0x[0-9A-F]{8})")
RUNS = ["S2_seed1_a", "S2_seed1_b", "S2_seed2", "S2_seed1_gpu", "S2_seed1_uncooked"]


def read_run(path: str) -> dict:
    cells, result = {}, None
    if not os.path.exists(path):
        return {"cells": cells, "result": result, "missing": True}
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if match := CELL_LINE.search(line):
                index, _, _, seed, instances, content, ordered, pockets, nav_points = match.groups()
                cells[int(index)] = {"seed": int(seed), "instances": int(instances), "content": content, "ordered": ordered,
                                     "pockets": int(pockets), "nav_points": int(nav_points)}
            elif match := RESULT_LINE.search(line):
                _, reason, generated, total, instances, pockets, run_hash = match.groups()
                result = {"reason": reason, "generated": int(generated), "total": int(total), "instances": int(instances),
                          "pockets": int(pockets), "run_hash": run_hash}
    return {"cells": cells, "result": result}


def main() -> int:
    folder = sys.argv[1]
    runs = {name: read_run(os.path.join(folder, name + ".log")) for name in RUNS}
    failures = []

    for name, run in runs.items():
        result = run["result"]
        if run.get("missing"):
            failures.append(f"{name}: no log ({name}.log not found)")
            continue
        if not result or result["generated"] != result["total"] or len(run["cells"]) != result["total"]:
            failures.append(f"{name}: not every cell generated ({result})")
        else:
            print(f"{name:18} {result['generated']}/{result['total']} cells, {result['instances']} instances, "
                  f"{result['pockets']} pocket points, run hash {result['run_hash']}")
        pocket_cells = [i for i, cell in run["cells"].items() if cell["pockets"]]
        if pocket_cells:
            failures.append(f"{name}: pockets in cells {pocket_cells}")

    reference = runs["S2_seed1_a"]["cells"]
    for name in ("S2_seed1_b", "S2_seed1_gpu", "S2_seed1_uncooked"):
        other = runs[name]["cells"]
        content_diff = [i for i in reference if other.get(i, {}).get("content") != reference[i]["content"]]
        ordered_diff = [i for i in reference if other.get(i, {}).get("ordered") != reference[i]["ordered"]]
        print(f"seed 1 a vs {name:18}: content differs in {len(content_diff)}/{len(reference)} cells {content_diff}; "
              f"ordered differs in {len(ordered_diff)} cells {ordered_diff}")
        if content_diff:
            failures.append(f"seed 1 content differs between S2_seed1_a and {name} in cells {content_diff}")

    seed2 = runs["S2_seed2"]["cells"]
    same = [i for i in reference if seed2.get(i, {}).get("content") == reference[i]["content"]]
    print(f"seed 1 vs seed 2: content identical in {len(same)}/{len(reference)} cells {same}")
    if not reference:
        failures.append("S2_seed1_a has no cell lines, so nothing could be compared")
    elif same:
        failures.append(f"seed 2 did not change cells {same}")

    instances = [cell["instances"] for cell in reference.values()]
    nav_points = [cell["nav_points"] for cell in reference.values()]
    if instances:
        print(f"instances per cell (seed 1): min {min(instances)}, max {max(instances)}, mean {sum(instances) / len(instances):.1f}")
        print(f"open sample points per cell (seed 1, of 196): min {min(nav_points)}, max {max(nav_points)}")

    for failure in failures:
        print("FAIL:", failure)
    print("S2 check:", "PASS" if not failures else "FAIL")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
