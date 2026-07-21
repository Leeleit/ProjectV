#!/usr/bin/env python3
"""Reconstruct full per-tick state from deduped voxel-ascii-tick.log and analyze fluid behavior."""
import re, sys

path = sys.argv[1] if len(sys.argv) > 1 else "build/windows-clang-debug/bin/voxel-ascii-tick.log"

state = {}          # y -> {z -> row}
cur_tick = None
cur_y = None
x0 = 0

hdr_tick = re.compile(r"^# tick=(\d+)")
hdr_y = re.compile(r"^y=(-?\d+) \(x=(-?\d+)\.\.(-?\d+), z=(-?\d+)\.\.(-?\d+)\)")
row_re = re.compile(r"^z=\s*(-?\d+): (.*)$")

snapshots = []  # (tick, {y: {z: row}})

with open(path, encoding="utf-8") as f:
    for line in f:
        line = line.rstrip("\n")
        m = hdr_tick.match(line)
        if m:
            if cur_tick is not None:
                snapshots.append((cur_tick, {y: dict(zs) for y, zs in state.items()}))
            cur_tick = int(m.group(1))
            continue
        m = hdr_y.match(line)
        if m:
            cur_y = int(m.group(1))
            x0 = int(m.group(2))
            state.setdefault(cur_y, {})
            continue
        m = row_re.match(line)
        if m and cur_y is not None:
            state[cur_y][int(m.group(1))] = m.group(2)
if cur_tick is not None:
    snapshots.append((cur_tick, {y: dict(zs) for y, zs in state.items()}))

def fluid_count_per_y(snap):
    return {y: sum(row.count("~") for row in zs.values()) for y, zs in snap.items()}

def total_fluid(snap):
    return sum(sum(row.count("~") for row in zs.values()) for zs in snap.values())

print(f"parsed {len(snapshots)} ticks: first={snapshots[0][0]} last={snapshots[-1][0]}")
print(f"total fluid: first={total_fluid(snapshots[0][1])} last={total_fluid(snapshots[-1][1])}")
print()

ys_all = sorted({y for _, s in snapshots for y in s})
ys_all = [y for y in ys_all if any(fluid_count_per_y(s).get(y, 0) for _, s in snapshots)]
print("tick\t" + "\t".join(f"y{y}" for y in ys_all) + "\ttotal")
step = max(1, len(snapshots) // 45)
for i in range(0, len(snapshots), step):
    t, s = snapshots[i]
    c = fluid_count_per_y(s)
    print(f"{t}\t" + "\t".join(str(c.get(y, 0)) for y in ys_all) + f"\t{total_fluid(s)}")
t, s = snapshots[-1]
c = fluid_count_per_y(s)
print(f"{t}\t" + "\t".join(str(c.get(y, 0)) for y in ys_all) + f"\t{total_fluid(s)}  LAST")

def gap_count(snap):
    cells = {}
    for y, zs in snap.items():
        for z, row in zs.items():
            for xi, ch in enumerate(row):
                if ch in ("~", "G"):
                    cells.setdefault((x0 + xi, z), {})[y] = ch
    gaps = 0
    for col in cells.values():
        fy = sorted(y for y, ch in col.items() if ch == "~")
        if len(fy) < 2:
            continue
        for y in range(fy[0] + 1, fy[-1]):
            if y not in col:
                gaps += 1
    return gaps

print()
print("air-gap cells between fluid cells in same (x,z) column:")
for i in [0, len(snapshots)//4, len(snapshots)//2, 3*len(snapshots)//4, len(snapshots)-1]:
    t, s = snapshots[i]
    print(f"  tick {t}: gaps={gap_count(s)}")
