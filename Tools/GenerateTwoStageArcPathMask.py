"""Generate a grayscale progress mask for the TwoStageArc crescent.

R is normalized along the visible crescent from the upper tip to the lower tip.
The material uses this value as a threshold; it never pans the source artwork.
"""

from pathlib import Path
import math
import struct
import zlib


SIZE = 1280
CENTER_X = 640.0
CENTER_Y = 640.0
START_ANGLE = math.radians(-94.0)
END_ANGLE = math.radians(159.0)
SPAN = END_ANGLE - START_ANGLE
OUTPUT = Path(__file__).resolve().parents[1] / "Content/RawContent/VFX/NiagaraSystem/NS/PlayerTwoStageArc/Textures/T_TwoStageArc_Path.png"


def chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)


def write_png(path: Path) -> None:
    rows = []
    for y in range(SIZE):
        row = bytearray([0])
        for x in range(SIZE):
            angle = math.atan2(y - CENTER_Y, x - CENTER_X)
            while angle < START_ANGLE:
                angle += math.tau
            progress = max(0.0, min(1.0, (angle - START_ANGLE) / SPAN))
            value = int(round(progress * 255.0))
            row.extend((value, value, value, 255))
        rows.append(bytes(row))

    raw = b"".join(rows)
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(raw, 9)))
    png.extend(chunk(b"IEND", b""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


if __name__ == "__main__":
    write_png(OUTPUT)
    print(f"Wrote {OUTPUT}")
