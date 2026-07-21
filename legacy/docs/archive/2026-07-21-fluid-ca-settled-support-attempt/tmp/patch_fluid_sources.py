from pathlib import Path

p = Path(r"C:\Users\le1t\Projects\ProjectV\src\voxel\VoxelWorldFluidBodies.cpp")
src = p.read_text(encoding="utf-8")

# 1) Remove streamMark array
old = (
	"\tstd::vector<int32_t> columnMark(totalCells, -1); // == basinGen → column (x,z) already consumed a chain this tick\n"
	"\tstd::vector<int32_t> streamMark(totalCells, -1); // == basinGen → in the fall column of an active exit (stream cell, not a source)\n"
)
new = "\tstd::vector<int32_t> columnMark(totalCells, -1); // == basinGen → column (x,z) already consumed a chain this tick\n"
assert src.count(old) == 1, "streamMark decl"
src = src.replace(old, new)

# 2) Remove streamMark pre-pass
old = """\t\t// Mark stream cells: fluid columns below any active exit of the basin (stream in transit — not chain sources)
\t\tfor (const int32_t m : members) {
\t\t\tfor (const int32_t cell : bodyCells[static_cast<size_t>(m)]) {
\t\t\t\tint cx = 0;
\t\t\t\tint cy = 0;
\t\t\t\tint cz = 0;
\t\t\t\tdecode(cell, cx, cy, cz);
\t\t\t\tfor (const auto &nb : kNeighbor6) {
\t\t\t\t\tconst int nx = cx + nb[0];
\t\t\t\t\tconst int ny = cy + nb[1];
\t\t\t\t\tconst int nz = cz + nb[2];
\t\t\t\t\tif (ny > cy || !isInsideLocal(nx, ny, nz)) {
\t\t\t\t\t\tcontinue;
\t\t\t\t\t}
\t\t\t\t\tconst int32_t nFlat = static_cast<int32_t>(index(nx, ny, nz));
\t\t\t\t\tif (next[nFlat] != kAirMat || ny == 0 || next[nFlat - width] != kAirMat || distMark[nFlat] != basinGen) {
\t\t\t\t\t\tcontinue; // not a live fall-entry
\t\t\t\t\t}
\t\t\t\t\tfor (int ty = ny - 1; ty >= 0; --ty) { // the fluid column this exit feeds
\t\t\t\t\t\tconst int32_t f = static_cast<int32_t>(index(nx, ty, nz));
\t\t\t\t\t\tif (next[f] != kFluidMat) {
\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\t}
\t\t\t\t\t\tstreamMark[f] = basinGen;
\t\t\t\t\t}
\t\t\t\t}
\t\t\t}
\t\t}

\t\t// ---- Chains per member body ----
"""
new = "\t\t// ---- Chains per member body ----\n"
assert src.count(old) == 1, "streamMark prepass"
src = src.replace(old, new)

# 3) Fix bodyId on column settle
old = """\t\t\t\t\t\t\tnext[above - width] = next[above];
\t\t\t\t\t\t\tnext[above] = kAirMat;
\t\t\t\t\t\t\tbodyId[above] = -1;
\t\t\t\t\t\t\tabove += width;
"""
new = """\t\t\t\t\t\t\tnext[above - width] = next[above];
\t\t\t\t\t\t\tbodyId[above - width] = id;
\t\t\t\t\t\t\tnext[above] = kAirMat;
\t\t\t\t\t\t\tbodyId[above] = -1;
\t\t\t\t\t\t\tabove += width;
"""
assert src.count(old) == 1, "settle bodyId"
src = src.replace(old, new)

# 4) Replace refreshSources + serveOne
old = """\t\t\tint budget = kFluidChainBudgetPerBody;
\t\t\tint chainOrdinal = 0;
\t\t\tconst auto refreshSources = [&](int &topY, std::vector<int32_t> &sourcesTop, int &maxSourceD) {
\t\t\t\ttopY = -1; // current top layer of the body (all sources live here)
\t\t\t\tfor (const int32_t cell : cells) {
\t\t\t\t\ttopY = std::max(topY, static_cast<int>((cell / width) % height));
\t\t\t\t}
\t\t\t\tsourcesTop.clear();
\t\t\t\tmaxSourceD = -1;
\t\t\t\tfor (const int32_t cell : cells) {
\t\t\t\t\tif (static_cast<int>((cell / width) % height) == topY && groundMark[cell] == basinGen) {
\t\t\t\t\t\tsourcesTop.push_back(cell);
\t\t\t\t\t\tif (distMark[cell] == basinGen) {
\t\t\t\t\t\t\tmaxSourceD = std::max(maxSourceD, distVal[cell]);
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t}
\t\t\t};
\t\t\tconst auto serveOne = [&](const Dest &dest) -> bool { // guard + one chain into dest
\t\t\t\tint topY = 0;
\t\t\t\tstd::vector<int32_t> sourcesTop;
\t\t\t\tint maxSourceD = -1;
\t\t\t\trefreshSources(topY, sourcesTop, maxSourceD);
\t\t\t\tif (dest.y > topY || next[dest.flat] != kAirMat || bodyId[dest.flat] >= 0) { // above the top layer or consumed
\t\t\t\t\treturn false;
\t\t\t\t}
\t\t\t\tstd::vector<int32_t> sameYFiltered;
\t\t\t\tconst std::vector<int32_t> *eligible = &sourcesTop;
\t\t\t\tif (dest.y == topY) { // same level: only sources strictly farther from T
\t\t\t\t\tif (dest.dist >= maxSourceD) {
\t\t\t\t\t\treturn false;
\t\t\t\t\t}
\t\t\t\t\tfor (const int32_t source : sourcesTop) {
\t\t\t\t\t\tif (distMark[source] == basinGen && distVal[source] > dest.dist) {
\t\t\t\t\t\t\tsameYFiltered.push_back(source);
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t\teligible = &sameYFiltered;
\t\t\t\t}
\t\t\t\tconst int len = static_cast<int>(eligible->size());
\t\t\t\tif (len == 0) {
\t\t\t\t\treturn false;
\t\t\t\t}
\t\t\t\tfor (int j = 0; j < len; ++j) {
\t\t\t\t\tconst int32_t source = (*eligible)[(static_cast<size_t>(chainOrdinal + j)) % static_cast<size_t>(len)];
\t\t\t\t\tif (columnMark[columnKey(source)] == basinGen) {
\t\t\t\t\t\tcontinue; // this column already moved a cell this tick — settles at FALL rate, stream stays whole
\t\t\t\t\t}
\t\t\t\t\tif (streamMark[source] == basinGen) {
\t\t\t\t\t\tcontinue; // stream cell in transit — it falls and merges at the bottom
\t\t\t\t\t}
\t\t\t\t\tif (applyChainTo(source, dest.flat, id, cells)) {
\t\t\t\t\t\tcolumnMark[columnKey(source)] = basinGen;
\t\t\t\t\t\t++chainOrdinal;
\t\t\t\t\t\treturn true;
\t\t\t\t\t}
\t\t\t\t}
\t\t\t\treturn false;
\t\t\t};
"""

new = """\t\t\tint budget = kFluidChainBudgetPerBody;
\t\t\tint chainOrdinal = 0;
\t\t\t// Excess bottom on settled support: grounded, ∉T, below Solid or Fluid∈T (bottom-first drain).
\t\t\tconst auto refreshSources = [&](std::vector<int32_t> &sources, int &maxSourceY) {
\t\t\t\tsources.clear();
\t\t\t\tmaxSourceY = -1;
\t\t\t\tfor (const int32_t cell : cells) {
\t\t\t\t\tif (groundMark[cell] != basinGen || targetMark[cell] == basinGen) {
\t\t\t\t\t\tcontinue; // only unsettled excess
\t\t\t\t\t}
\t\t\t\t\tint cx = 0;
\t\t\t\t\tint cy = 0;
\t\t\t\t\tint cz = 0;
\t\t\t\t\tdecode(cell, cx, cy, cz);
\t\t\t\t\tif (cy > 0) {
\t\t\t\t\t\tconst int32_t belowFlat = cell - width;
\t\t\t\t\t\tconst uint8_t below = next[belowFlat];
\t\t\t\t\t\tif (IsSolidMaterial(below)) {
\t\t\t\t\t\t\t; // rests on Solid
\t\t\t\t\t\t} else if (below == kFluidMat && targetMark[belowFlat] == basinGen) {
\t\t\t\t\t\t\t; // rests on T (settled water)
\t\t\t\t\t\t} else {
\t\t\t\t\t\t\tcontinue; // rests on transit fluid / Air — not the excess bottom
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t\tsources.push_back(cell);
\t\t\t\t\tmaxSourceY = std::max(maxSourceY, cy);
\t\t\t\t}
\t\t\t};
\t\t\tconst auto filterSourcesForDest = [&](const Dest &dest, const std::vector<int32_t> &sources,
\t\t\t\t\t\t\t\t\t\t\t\t   std::vector<int32_t> &out) {
\t\t\t\tout.clear();
\t\t\t\tfor (const int32_t source : sources) {
\t\t\t\t\tconst int sy = static_cast<int>((source / width) % height);
\t\t\t\t\tif (sy > dest.y) {
\t\t\t\t\t\tout.push_back(source); // dest below source
\t\t\t\t\t} else if (sy == dest.y && distMark[source] == basinGen && distVal[source] > dest.dist) {
\t\t\t\t\t\tout.push_back(source); // same level: only farther from T
\t\t\t\t\t}
\t\t\t\t}
\t\t\t};
\t\t\tconst auto serveOne = [&](const Dest &dest) -> bool { // guard + one chain into dest
\t\t\t\tstd::vector<int32_t> sources;
\t\t\t\tint maxSourceY = -1;
\t\t\t\trefreshSources(sources, maxSourceY);
\t\t\t\tif (dest.y > maxSourceY || next[dest.flat] != kAirMat || bodyId[dest.flat] >= 0) {
\t\t\t\t\treturn false;
\t\t\t\t}
\t\t\t\tstd::vector<int32_t> eligible;
\t\t\t\tfilterSourcesForDest(dest, sources, eligible);
\t\t\t\tconst int len = static_cast<int>(eligible.size());
\t\t\t\tif (len == 0) {
\t\t\t\t\treturn false;
\t\t\t\t}
\t\t\t\tfor (int j = 0; j < len; ++j) {
\t\t\t\t\tconst int32_t source = eligible[(static_cast<size_t>(chainOrdinal + j)) % static_cast<size_t>(len)];
\t\t\t\t\tif (columnMark[columnKey(source)] == basinGen) {
\t\t\t\t\t\tcontinue; // one chain per column per tick
\t\t\t\t\t}
\t\t\t\t\tif (applyChainTo(source, dest.flat, id, cells)) {
\t\t\t\t\t\tcolumnMark[columnKey(source)] = basinGen;
\t\t\t\t\t\t++chainOrdinal;
\t\t\t\t\t\treturn true;
\t\t\t\t\t}
\t\t\t\t}
\t\t\t\treturn false;
\t\t\t};
"""
assert src.count(old) == 1, "refresh/serveOne"
src = src.replace(old, new)

# 5) Replace budget loop
old = """\t\t\tbool progressed = false;
\t\t\twhile (budget > 0) {
\t\t\t\tint topY = 0;
\t\t\t\tstd::vector<int32_t> sourcesTop;
\t\t\t\tint maxSourceD = -1;
\t\t\t\trefreshSources(topY, sourcesTop, maxSourceD);
\t\t\t\tprogressed = false;
\t\t\t\tsize_t destIndex = 0;
\t\t\t\twhile (destIndex < dests.size() && !progressed) { // equal-D groups, rotated within a group — uniform overflow
\t\t\t\t\tsize_t groupEnd = destIndex + 1;
\t\t\t\t\twhile (groupEnd < dests.size() && dests[groupEnd].dist == dests[destIndex].dist) {
\t\t\t\t\t\t++groupEnd;
\t\t\t\t\t}
\t\t\t\t\tconst size_t groupSize = groupEnd - destIndex;
\t\t\t\t\tfor (size_t j = 0; j < groupSize && !progressed; ++j) {
\t\t\t\t\t\tconst Dest &dest = dests[destIndex + (static_cast<size_t>(chainOrdinal) + j) % groupSize];
\t\t\t\t\t\tif (dest.exit) {
\t\t\t\t\t\t\tcontinue; // already served above
\t\t\t\t\t\t}
\t\t\t\t\t\t// guard: dest.y < topY → any top-layer source; dest.y == topY → only sources strictly farther from T
\t\t\t\t\t\tif (dest.y > topY || next[dest.flat] != kAirMat || bodyId[dest.flat] >= 0) {
\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t}
\t\t\t\t\t\tstd::vector<int32_t> sameYFiltered;
\t\t\t\t\t\tconst std::vector<int32_t> *eligible = &sourcesTop;
\t\t\t\t\t\tif (dest.y == topY) {
\t\t\t\t\t\t\tif (dest.dist >= maxSourceD) {
\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\tfor (const int32_t source : sourcesTop) {
\t\t\t\t\t\t\t\tif (distMark[source] == basinGen && distVal[source] > dest.dist) {
\t\t\t\t\t\t\t\t\tsameYFiltered.push_back(source);
\t\t\t\t\t\t\t\t}
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\teligible = &sameYFiltered;
\t\t\t\t\t\t}
\t\t\t\t\t\tconst int len = static_cast<int>(eligible->size());
\t\t\t\t\t\tif (len == 0) {
\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t}
\t\t\t\t\t\tconst int stride = std::max(1, std::min(budget, len)); // spread successive chains across the layer
\t\t\t\t\t\tfor (int j2 = 0; j2 < len; ++j2) {
\t\t\t\t\t\t\tconst int32_t source = (*eligible)[(static_cast<int64_t>(chainOrdinal + j2) * stride) % len];
\t\t\t\t\t\t\tif (columnMark[columnKey(source)] == basinGen) {
\t\t\t\t\t\t\t\tcontinue; // one chain per column per tick
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\tif (streamMark[source] == basinGen) {
\t\t\t\t\t\t\t\tcontinue; // stream cell in transit — it falls and merges at the bottom
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\tif (applyChainTo(source, dest.flat, id, cells)) {
\t\t\t\t\t\t\t\tcolumnMark[columnKey(source)] = basinGen;
\t\t\t\t\t\t\t\t++chainOrdinal;
\t\t\t\t\t\t\t\tprogressed = true;
\t\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\t\t}
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t\tdestIndex = groupEnd;
\t\t\t\t}
\t\t\t\tif (!progressed) { // bay fallback: fillable Air hole with ≥3 lateral body neighbours, source loosely bound (≤1)
\t\t\t\t\tstd::vector<int32_t> bays;
\t\t\t\t\tfor (const int32_t cell : cells) {
\t\t\t\t\t\tint cx = 0;
\t\t\t\t\t\tint cy = 0;
\t\t\t\t\t\tint cz = 0;
\t\t\t\t\t\tdecode(cell, cx, cy, cz);
\t\t\t\t\t\tfor (int d = 1; d <= 4; ++d) {
\t\t\t\t\t\t\tconst int nx = cx + kNeighbor6[d][0];
\t\t\t\t\t\t\tconst int nz = cz + kNeighbor6[d][2];
\t\t\t\t\t\t\tif (!isInsideLocal(nx, cy, nz)) {
\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\tconst int32_t nFlat = static_cast<int32_t>(index(nx, cy, nz));
\t\t\t\t\t\t\tif (next[nFlat] == kAirMat && bodyId[nFlat] != id && targetMark[nFlat] != basinGen && fillable(nFlat, basinGen) &&
\t\t\t\t\t\t\t\tlateralBodyNeighbours(nFlat, id) >= 3) {
\t\t\t\t\t\t\t\tbays.push_back(nFlat);
\t\t\t\t\t\t\t}
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t\tstd::sort(bays.begin(), bays.end());
\t\t\t\t\tbays.erase(std::unique(bays.begin(), bays.end()), bays.end());
\t\t\t\t\tstd::vector<int32_t> loose;
\t\t\t\t\tfor (const int32_t source : sourcesTop) {
\t\t\t\t\t\tif (lateralBodyNeighbours(source, id) <= 1) {
\t\t\t\t\t\t\tloose.push_back(source);
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t\tfor (const int32_t bay : bays) {
\t\t\t\t\t\tfor (const int32_t source : loose) {
\t\t\t\t\t\t\tif (applyChainTo(source, bay, id, cells)) {
\t\t\t\t\t\t\t\tprogressed = true;
\t\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\t\t}
\t\t\t\t\t\t}
\t\t\t\t\t\tif (progressed) {
\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t}
"""

new = """\t\t\tbool progressed = false;
\t\t\twhile (budget > 0) {
\t\t\t\tstd::vector<int32_t> sources;
\t\t\t\tint maxSourceY = -1;
\t\t\t\trefreshSources(sources, maxSourceY);
\t\t\t\tprogressed = false;
\t\t\t\tsize_t destIndex = 0;
\t\t\t\twhile (destIndex < dests.size() && !progressed) { // equal-D groups, rotated within a group — uniform overflow
\t\t\t\t\tsize_t groupEnd = destIndex + 1;
\t\t\t\t\twhile (groupEnd < dests.size() && dests[groupEnd].dist == dests[destIndex].dist) {
\t\t\t\t\t\t++groupEnd;
\t\t\t\t\t}
\t\t\t\t\tconst size_t groupSize = groupEnd - destIndex;
\t\t\t\t\tfor (size_t j = 0; j < groupSize && !progressed; ++j) {
\t\t\t\t\t\tconst Dest &dest = dests[destIndex + (static_cast<size_t>(chainOrdinal) + j) % groupSize];
\t\t\t\t\t\tif (dest.exit) {
\t\t\t\t\t\t\tcontinue; // already served above
\t\t\t\t\t\t}
\t\t\t\t\t\tif (dest.y > maxSourceY || next[dest.flat] != kAirMat || bodyId[dest.flat] >= 0) {
\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t}
\t\t\t\t\t\tstd::vector<int32_t> eligible;
\t\t\t\t\t\tfilterSourcesForDest(dest, sources, eligible);
\t\t\t\t\t\tconst int len = static_cast<int>(eligible.size());
\t\t\t\t\t\tif (len == 0) {
\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t}
\t\t\t\t\t\tconst int stride = std::max(1, std::min(budget, len)); // spread successive chains across the layer
\t\t\t\t\t\tfor (int j2 = 0; j2 < len; ++j2) {
\t\t\t\t\t\t\tconst int32_t source = eligible[(static_cast<int64_t>(chainOrdinal + j2) * stride) % len];
\t\t\t\t\t\t\tif (columnMark[columnKey(source)] == basinGen) {
\t\t\t\t\t\t\t\tcontinue; // one chain per column per tick
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\tif (applyChainTo(source, dest.flat, id, cells)) {
\t\t\t\t\t\t\t\tcolumnMark[columnKey(source)] = basinGen;
\t\t\t\t\t\t\t\t++chainOrdinal;
\t\t\t\t\t\t\t\tprogressed = true;
\t\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\t\t}
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t\tdestIndex = groupEnd;
\t\t\t\t}
\t\t\t\tif (!progressed) { // bay fallback: fillable Air hole with ≥3 lateral body neighbours, source loosely bound (≤1)
\t\t\t\t\tstd::vector<int32_t> bays;
\t\t\t\t\tfor (const int32_t cell : cells) {
\t\t\t\t\t\tint cx = 0;
\t\t\t\t\t\tint cy = 0;
\t\t\t\t\t\tint cz = 0;
\t\t\t\t\t\tdecode(cell, cx, cy, cz);
\t\t\t\t\t\tfor (int d = 1; d <= 4; ++d) {
\t\t\t\t\t\t\tconst int nx = cx + kNeighbor6[d][0];
\t\t\t\t\t\t\tconst int nz = cz + kNeighbor6[d][2];
\t\t\t\t\t\t\tif (!isInsideLocal(nx, cy, nz)) {
\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\tconst int32_t nFlat = static_cast<int32_t>(index(nx, cy, nz));
\t\t\t\t\t\t\tif (next[nFlat] == kAirMat && bodyId[nFlat] != id && targetMark[nFlat] != basinGen && fillable(nFlat, basinGen) &&
\t\t\t\t\t\t\t\tlateralBodyNeighbours(nFlat, id) >= 3) {
\t\t\t\t\t\t\t\tbays.push_back(nFlat);
\t\t\t\t\t\t\t}
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t\tstd::sort(bays.begin(), bays.end());
\t\t\t\t\tbays.erase(std::unique(bays.begin(), bays.end()), bays.end());
\t\t\t\t\tstd::vector<int32_t> loose;
\t\t\t\t\tfor (const int32_t source : sources) {
\t\t\t\t\t\tif (lateralBodyNeighbours(source, id) <= 1) {
\t\t\t\t\t\t\tloose.push_back(source);
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t\tfor (const int32_t bay : bays) {
\t\t\t\t\t\tfor (const int32_t source : loose) {
\t\t\t\t\t\t\tif (columnMark[columnKey(source)] == basinGen) {
\t\t\t\t\t\t\t\tcontinue;
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\tif (applyChainTo(source, bay, id, cells)) {
\t\t\t\t\t\t\t\tcolumnMark[columnKey(source)] = basinGen;
\t\t\t\t\t\t\t\tprogressed = true;
\t\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\t\t}
\t\t\t\t\t\t}
\t\t\t\t\t\tif (progressed) {
\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t}
"""
assert src.count(old) == 1, "budget loop"
src = src.replace(old, new)

p.write_text(src, encoding="utf-8")
print("engine patched OK")
assert "streamMark" not in src
assert "sourcesTop" not in src
assert "refreshSources(sources, maxSourceY)" in src
print("sanity OK")
