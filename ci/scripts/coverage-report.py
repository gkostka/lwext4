#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Print a per-file coverage table from a gcovr JSON summary and check the
totals against the floor file.

    coverage-report.py <coverage.json> <floor file> [summary.md]

The floor file holds "line <percent>" and "branch <percent>" lines ("#"
starts a comment). Exits 1 if a total is below its floor.
"""
import json
import math
import sys


def pct(covered, total):
    return 100.0 * covered / total if total else 100.0


def read_floor(path):
    floor = {}
    with open(path) as f:
        for raw in f:
            line = raw.split("#", 1)[0].split()
            if not line:
                continue
            if len(line) != 2 or line[0] not in ("line", "branch"):
                sys.exit(f"{path}: bad line: {raw.rstrip()}")
            floor[line[0]] = float(line[1])
    return floor


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    with open(sys.argv[1]) as f:
        data = json.load(f)
    floor = read_floor(sys.argv[2])

    rows = []
    for fe in sorted(data["files"], key=lambda e: e["filename"]):
        rows.append((fe["filename"],
                     fe["line_covered"], fe["line_total"],
                     fe["branch_covered"], fe["branch_total"]))
    total = ("TOTAL",
             data["line_covered"], data["line_total"],
             data["branch_covered"], data["branch_total"])

    head = ("File", "Lines", "Line %", "Branches", "Branch %", "Uncovered lines")
    table = []
    for name, lc, lt, bc, bt in rows + [total]:
        table.append((name, f"{lc}/{lt}", f"{pct(lc, lt):.1f}",
                      f"{bc}/{bt}", f"{pct(bc, bt):.1f}", str(lt - lc)))

    widths = [max(len(r[i]) for r in [head] + table) for i in range(len(head))]
    def fmt(r):
        return "  ".join(c.ljust(w) if i == 0 else c.rjust(w)
                         for i, (c, w) in enumerate(zip(r, widths)))
    print(fmt(head))
    print("-" * len(fmt(head)))
    for r in table[:-1]:
        print(fmt(r))
    print("-" * len(fmt(head)))
    print(fmt(table[-1]))

    line_pct = pct(total[1], total[2])
    branch_pct = pct(total[3], total[4])
    status = 0
    verdict = []
    for metric, value in (("line", line_pct), ("branch", branch_pct)):
        lim = floor.get(metric, 0.0)
        if value < lim:
            verdict.append(f"FAIL: {metric} coverage {value:.2f}% is below "
                           f"the floor {lim:g}% (ci/coverage-floor)")
            status = 1
        else:
            verdict.append(f"OK: {metric} coverage {value:.2f}% >= floor "
                           f"{lim:g}%")
            if math.floor(value) > lim:
                verdict.append(f"    the floor can be raised to "
                               f"{math.floor(value)} in ci/coverage-floor")
    print()
    print("\n".join(verdict))

    if len(sys.argv) == 4:
        with open(sys.argv[3], "w") as f:
            f.write("## Coverage of src/\n\n")
            f.write("| " + " | ".join(head) + " |\n")
            f.write("|" + "---|" * len(head) + "\n")
            for r in table[:-1]:
                f.write("| " + " | ".join(r) + " |\n")
            f.write("| " + " | ".join(f"**{c}**" for c in table[-1])
                    + " |\n\n")
            f.write("\n".join(v.strip() for v in verdict) + "\n")
    return status


if __name__ == "__main__":
    sys.exit(main())
