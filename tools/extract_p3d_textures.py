#!/usr/bin/env python3
"""Extract embedded texture images from Pure3D containers by texture-name pattern."""

import argparse
import re
import struct
from pathlib import Path


TEXTURE = 0x00019000
IMAGE = 0x00019001
IMAGE_DATA = 0x00019002
FORMAT_EXT = {1: ".png", 2: ".tga", 3: ".bmp", 5: ".dxt", 6: ".dxt1", 8: ".dxt3", 10: ".dxt5"}


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def pstring(data: bytes, offset: int) -> tuple[str, int]:
    length = data[offset]
    start = offset + 1
    return data[start:start + length].decode("latin-1").rstrip("\0"), start + length


def chunks(data: bytes, begin: int, end: int):
    pos = begin
    while pos + 12 <= end:
        chunk_id, data_len, chunk_len = struct.unpack_from("<III", data, pos)
        if data_len < 12 or chunk_len < data_len or pos + chunk_len > end:
            return
        yield pos, chunk_id, data_len, chunk_len
        pos += chunk_len


def safe_name(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", name)


def extract_file(source: Path, output: Path, wanted: re.Pattern[str]) -> int:
    data = source.read_bytes()
    count = 0

    def walk(begin: int, end: int, texture_name: str | None = None):
        nonlocal count
        for pos, chunk_id, data_len, chunk_len in chunks(data, begin, end):
            current_name = texture_name
            payload = pos + 12
            if chunk_id == TEXTURE:
                current_name, _ = pstring(data, payload)
            elif chunk_id == IMAGE:
                image_name, cursor = pstring(data, payload)
                # version, width, height, bpp, palettized, alpha, format
                if cursor + 28 <= pos + data_len:
                    image_format = u32(data, cursor + 24)
                    effective_name = current_name or image_name
                    if wanted.search(effective_name) or wanted.search(image_name):
                        for dpos, did, _, dlen in chunks(data, pos + data_len, pos + chunk_len):
                            if did != IMAGE_DATA or dpos + 16 > dpos + dlen:
                                continue
                            size = u32(data, dpos + 12)
                            image_start = dpos + 16
                            if image_start + size > dpos + dlen:
                                continue
                            ext = FORMAT_EXT.get(image_format, Path(image_name).suffix or ".bin")
                            target = output / f"{source.stem}__{safe_name(effective_name)}{ext}"
                            suffix = 1
                            while target.exists():
                                target = output / f"{source.stem}__{safe_name(effective_name)}_mip{suffix}{ext}"
                                suffix += 1
                            target.write_bytes(data[image_start:image_start + size])
                            print(f"{source.name}: {effective_name} -> {target.name}")
                            count += 1
            child_begin = pos + data_len
            child_end = pos + chunk_len
            if child_begin < child_end:
                walk(child_begin, child_end, current_name)

    # The file root is itself a regular 12-byte chunk.
    walk(0, len(data))
    return count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("sources", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--pattern", default=r"_norm(?:\.|$)")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    wanted = re.compile(args.pattern, re.IGNORECASE)
    total = sum(extract_file(path, args.output, wanted) for path in args.sources)
    print(f"Extracted {total} image(s)")
    return 0 if total else 1


if __name__ == "__main__":
    raise SystemExit(main())
