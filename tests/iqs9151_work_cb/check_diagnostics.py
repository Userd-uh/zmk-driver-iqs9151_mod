"""Check real native-driver log output (not source text)."""
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text()
rows = re.findall(r"(GIN|GPRE|GSTEP|GOUT) seq=(\d+) ([^\r\n]+)", text)
assert rows and len(rows) % 4 == 0, "Incomplete diagnostic records"
frames = []
for offset in range(0, len(rows), 4):
    group = rows[offset:offset + 4]
    assert [r[0] for r in group] == ["GIN", "GPRE", "GSTEP", "GOUT"]
    assert len({r[1] for r in group}) == 1
    frames.append(" ".join(r[2] for r in group))
for fingers in (2, 3):
    assert any(f"fc={fingers} " in f and f"step{fingers}=1,0" in f
               and "active=1 scroll=1,0" in f for f in frames)
assert any("fc=0 " in f and "ended=1" in f for f in frames)
counts = [int(re.search(r"fc=(\d+)", f)[1]) for f in frames]
assert any(counts[i:i+3] == [3, 2, 3] for i in range(len(counts)-2))
print(f"Verified {len(frames)} diagnostic frames, tiny steps and 3->2->3")
