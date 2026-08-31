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

raws = re.findall(r"GRAW seq=(\d+) info=0x([\da-f]+) tp=0x([\da-f]+)", text)
fingers = re.findall(r"GF seq=(\d+) slot=(\d+) xy=(\d+),(\d+) strength=(\d+) area=(\d+) confidence=(\d+)", text)
assert len(raws) == len(frames)
assert len(fingers) == len(frames) * 7
for offset in range(0, len(fingers), 7):
    group = fingers[offset:offset + 7]
    assert [int(f[1]) for f in group] == list(range(1, 8))
    assert len({f[0] for f in group}) == 1
assert any(int(r[1], 16) == 0x0200 and int(r[2], 16) == 0x45e3 for r in raws)
for slot in range(1, 8):
    x, y = (65535, 65535) if slot == 2 else (256 + slot, 512 + slot)
    confidence = int(slot in (1, 3, 7))
    assert any(tuple(map(int, f[1:])) == (slot, x, y, 0x122f + slot, 19 + slot, confidence)
               for f in fingers), f"Incorrect slot {slot} raw decoding"
assert "settings=0x28 rx=12 tx=13 max=3 split=3 confidence=20" in text
assert "res=2457,3072 bottom=30 top=511 beta=20 stationary=5 jitter=2" in text
assert re.search(r"GCFG seq=\d+ version=2 source=boot rc=-5", text)
# Error snapshots must not emit cached/uninitialized values.
for tail in re.split(r"GCFG seq=", text)[1:]:
    first = tail.splitlines()[0]
    if "rc=-" in first:
        assert "GCFGV" not in tail and "GCFGF" not in tail
print("Verified seven raw finger slots, raw flags and success/failure readback logs")

assert re.search(r"GISAVE seq=\d+ t=50 fc=2 history=3", text)
assert re.search(r"GIREL version=2 seq=\d+ t=110 fc=0 history=3 enabled=1 eval=50 saved=1", text)
assert re.search(r"GIGATE kind=scroll t=50 reason=accepted history=3 recent=3 gap=13 window=60 stale=35 min=1 speed=4", text)
assert len(re.findall(r"GISTART seq=\d+ t=110 ", text)) == 1
assert not re.search(r"GIREL version=2 seq=\d+ t=111 ", text)
assert re.search(r"GIREL version=2 seq=\d+ t=5010 fc=0 history=1 enabled=1 eval=23 saved=1", text)
assert re.search(r"GIGATE kind=scroll t=46 reason=stale_gap .*gap=36 ", text)
assert re.search(r"GIGATE kind=scroll t=45 reason=accepted .*gap=35 ", text)
assert re.search(r"GISTART seq=\d+ t=45 seed=-?\d+,-?\d+ active=1", text)
assert re.search(r"GIWORK seq=\d+ n=\d+ t=\d+ scroll=-?\d+,-?\d+ active=[01] elapsed=\d+", text)
assert re.search(r"GIREPORT t=\d+ code=6 value=-60 sync=1 rc=-11", text)
print("Verified actual inertia decisions, worker output and input-report failure (no retry)")
