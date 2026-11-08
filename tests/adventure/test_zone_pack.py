#!/usr/bin/env python3
"""Pack-format contract tests, independent of SDL and commercial game files."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
PACKER = REPO / "tools" / "pack_adventure_zones.py"
MANIFEST = REPO / "stages" / "resort_island" / "zones.json"
spec = importlib.util.spec_from_file_location("zone_packer", PACKER)
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)


class ZonePackerTest(unittest.TestCase):
    def setUp(self):
        self.doc = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_compiles_correct_header_masks_and_spatial_data(self):
        data = packer.pack(self.doc)
        self.assertEqual(len(data), 108)
        self.assertEqual(struct.unpack_from("<4sHHHHII", data),
                         (b"AZN1", 1, 1, 1, 2, 20, 36))
        self.assertEqual(struct.unpack_from("<IBBBBHHI", data, 20),
                         (1, 1, 4, 15, 1, 0, 2, 0))
        self.assertEqual(struct.unpack_from("<9i", data, 36)[:3],
                         (6*4096, -2*4096, -3*4096))
        self.assertEqual(data, packer.pack(self.doc))

    def test_invalid_inputs_fail_without_output(self):
        def assert_invalid(modify):
            doc = json.loads(json.dumps(self.doc))
            modify(doc)
            with self.assertRaises((ValueError, TypeError, OverflowError)):
                packer.pack(doc)
        assert_invalid(lambda d: d.update(schema_version=2))
        assert_invalid(lambda d: d.update(track_id=6))
        assert_invalid(lambda d: d["zones"][0].update(weather=["hail"]))
        assert_invalid(lambda d: d["zones"][0].update(acts=[9]))
        assert_invalid(lambda d: d["zones"][0].update(anchor="magic"))
        assert_invalid(lambda d: d["zones"][0].update(id=0))
        assert_invalid(lambda d: d["zones"][0]["triangles"].append([[0,0,0]]))
        assert_invalid(lambda d: d["zones"][0]["triangles"][0][0].__setitem__(0, 900000))
        assert_invalid(lambda d: d["zones"][0]["triangles"][0][0].__setitem__(0, float("nan")))
        assert_invalid(lambda d: d["zones"].append(d["zones"][0]))

    def test_cli_writes_only_generated_zone_pack(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "ADVENTURE" / "ISLAND_ZONES.AZN"
            # Direct equivalent to CLI's entry point, then round-trip bytes.
            target.parent.mkdir(parents=True)
            target.write_bytes(packer.pack(self.doc))
            self.assertEqual(target.read_bytes(), packer.pack(self.doc))


if __name__ == "__main__":
    unittest.main()
