from pathlib import Path

p = Path(r"C:\Users\le1t\Projects\ProjectV\tests\FluidCATests.cpp")
src = p.read_text(encoding="utf-8")
old = """\t\tfor (int y = 0; y < topY; ++y) {
\t\t\tint aboveHad = 0;
\t\t\tfor (int yy = y + 1; yy <= topY; ++yy) {
\t\t\t\taboveHad += beforeCount[yy];
\t\t\t}
\t\t\tif (aboveHad == 0) {
\t\t\t\tcontinue; // top layer may shrink freely
\t\t\t}
\t\t\tEXPECT_TRUE(context, afterCount[y] >= beforeCount[y] - fellFrom[y]);
\t\t}"""
new = """\t\tfor (int y = 0; y < topY; ++y) {
\t\t\tint aboveHad = 0;
\t\t\tfor (int yy = y + 1; yy <= topY; ++yy) {
\t\t\t\taboveHad += beforeCount[yy];
\t\t\t}
\t\t\tif (aboveHad == 0) {
\t\t\t\tcontinue; // top layer may shrink freely
\t\t\t}
\t\t\tif (afterCount[y] < beforeCount[y] - fellFrom[y]) {
\t\t\t\tstd::fprintf(stderr, "[n1] tick=%d y=%d before=%d after=%d fell=%d aboveHad=%d\\n",
\t\t\t\t\ttick, y, beforeCount[y], afterCount[y], fellFrom[y], aboveHad);
\t\t\t}
\t\t\tEXPECT_TRUE(context, afterCount[y] >= beforeCount[y] - fellFrom[y]);
\t\t}"""
assert src.count(old) == 1
p.write_text(src.replace(old, new), encoding="utf-8")
print("n1 debug ok")
