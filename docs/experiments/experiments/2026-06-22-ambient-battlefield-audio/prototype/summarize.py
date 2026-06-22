"""Compute mean time/psnr across seeds from results_raw.csv"""
import csv
import sys
from collections import defaultdict

rows = []
with open('results_raw.csv') as f:
    reader = csv.DictReader(f)
    for r in reader:
        r['mean_time_us'] = float(r['mean_time_us'])
        r['mean_active'] = float(r['mean_active'])
        r['psnr_db'] = float(r['psnr_db'])
        r['sources'] = int(r['sources'])
        rows.append(r)

# Group by (strategy, scene, sources)
groups = defaultdict(list)
for r in rows:
    key = (r['strategy'], r['scene'], r['sources'])
    groups[key].append(r)

print("strategy,scene,sources,mean_time_us,mean_active,mean_psnr_db,min_psnr_db")
for key in sorted(groups.keys()):
    grp = groups[key]
    n = len(grp)
    t = sum(r['mean_time_us'] for r in grp) / n
    a = sum(r['mean_active'] for r in grp) / n
    p = sum(r['psnr_db'] for r in grp) / n
    p_min = min(r['psnr_db'] for r in grp)
    print(f"{key[0]},{key[1]},{key[2]},{t:.3f},{a:.1f},{p:.1f},{p_min:.1f}")
