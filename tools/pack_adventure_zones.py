#!/usr/bin/env python3
"""Compile hand-authored conditional Adventure extension zones into AZN1.

Pure standard-library offline packer. Runtime (SDL/Dreamcast) reads only AZN1,
never JSON, OBJ or glTF. This tool does not copy original Sonic R resources.
"""
import argparse
import json
import struct
from pathlib import Path

WEATHER = {"clear": 0, "rain": 1, "snow": 2}
TIME = {"sunrise": 0, "day": 1, "sunset": 2, "night": 3}
Q12 = 4096
MAX_ZONES = 4
MAX_TRIANGLES = 64
HEADER = struct.Struct("<4sHHHHII")
DESCRIPTOR = struct.Struct("<IBBBBHHI")
TRIANGLE = struct.Struct("<9i")


def mask(values, names, *, allowed=None):
    if not isinstance(values, list) or not values:
        raise ValueError("Mask fields must be nonempty arrays")
    mask_value = 0
    if values == ["any"]:
        values = list(allowed if allowed is not None else names)
    for value in values:
        if value not in names:
            raise ValueError(f"Unknown mask value: {value!r}")
        mask_value |= 1 << names[value]
    return mask_value


def point_q12(point):
    if not isinstance(point, list) or len(point) != 3:
        raise ValueError("Each vertex must be [x, y, z]")
    result = []
    for coordinate in point:
        if isinstance(coordinate, bool) or not isinstance(coordinate, (int, float)):
            raise ValueError("Vertex coordinates must be finite numbers")
        if not (-524288 < coordinate < 524288):
            raise ValueError("Vertex coordinate exceeds the Q20.12 world range")
        fixed = round(coordinate * Q12)
        if not -(2 ** 31) <= fixed < 2 ** 31:
            raise ValueError("Vertex coordinate overflows int32")
        result.append(fixed)
    return result


def validate_triangle(triangle):
    if not isinstance(triangle, list) or len(triangle) != 3:
        raise ValueError("Collision triangles must have exactly three vertices")
    a, b, c = [point_q12(point) for point in triangle]
    ab = [b[i] - a[i] for i in range(3)]
    ac = [c[i] - a[i] for i in range(3)]
    nx = ab[1] * ac[2] - ab[2] * ac[1]
    ny = ab[2] * ac[0] - ab[0] * ac[2]
    nz = ab[0] * ac[1] - ab[1] * ac[0]
    if ny == 0 or abs(nx) > abs(ny) or abs(nz) > abs(ny):
        raise ValueError("Triangle has degenerate X/Z footprint or steep normal")
    if any(abs(component) > 8192 * Q12 for component in ab + ac):
        raise ValueError("Triangle edges exceed the collision solver budget")
    return a + b + c


def pack(document):
    if set(document) != {"schema_version", "track_id", "zones"}:
        raise ValueError("Top-level fields must be schema_version, track_id and zones")
    if document["schema_version"] != 1:
        raise ValueError("Unsupported schema version")
    track_id = document["track_id"]
    if isinstance(track_id, bool) or not isinstance(track_id, int) or not (1 <= track_id <= 5):
        raise ValueError("Invalid Sonic R track ID (1..5)")
    zones = document["zones"]
    if not isinstance(zones, list) or len(zones) > MAX_ZONES:
        raise ValueError("Too many zones")
    descr = bytearray()
    tris = bytearray()
    first = 0
    ids = set()
    for z in zones:
        if set(z) != {"id", "acts", "weather", "time_of_day", "anchor", "triangles"}:
            raise ValueError("Zone requires id, acts, weather, time_of_day, anchor, triangles")
        zid = z["id"]
        if isinstance(zid, bool) or not isinstance(zid, int) or zid not in range(1, 256):
            raise ValueError("Zone IDs must be unique integers in 1..255")
        if zid in ids:
            raise ValueError("Duplicate zone ID")
        ids.add(zid)
        acts = mask(z["acts"], {x: x-1 for x in range(1, 9)})
        weather = mask(z["weather"], WEATHER, allowed=WEATHER)
        times = mask(z["time_of_day"], TIME, allowed=TIME)
        if z["anchor"] not in ("absolute", "spawn"):
            raise ValueError("Anchor must be 'absolute' or 'spawn'")
        flags = 1 if z["anchor"] == "spawn" else 0
        triangles = z["triangles"]
        if not isinstance(triangles, list) or not triangles:
            raise ValueError("A zone must contain at least one triangle")
        if first + len(triangles) > MAX_TRIANGLES:
            raise ValueError("More than 64 triangles requires the future spatial-index pack")
        descr += DESCRIPTOR.pack(zid, acts, weather, times, flags,
                                 first, len(triangles), 0)
        for triangle in triangles:
            tris += TRIANGLE.pack(*validate_triangle(triangle))
        first += len(triangles)
    zone_offset = HEADER.size
    tri_offset = zone_offset + len(descr)
    return (HEADER.pack(b"AZN1", 1, track_id, len(zones), first,
                        zone_offset, tri_offset) + descr + tris)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="JSON zone manifest")
    parser.add_argument("--output", "-o", type=Path, required=True,
                        help="Output .AZN path (e.g. game/ADVENTURE/ISLAND_ZONES.AZN)")
    args = parser.parse_args()
    data = pack(json.loads(args.source.read_text(encoding="utf-8")))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"Created {args.output} ({len(data)} bytes; AZN1 version 1)")


if __name__ == "__main__":
    main()
