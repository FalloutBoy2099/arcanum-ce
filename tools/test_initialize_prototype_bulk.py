import json
from pathlib import Path
import struct
import tempfile
import unittest

import initialize_prototype_bulk as bulk


class PrototypeBulkTests(unittest.TestCase):
    def setUp(self):
        self.schema = ({5: [["OBJ_F_ITEM_INV_AID"], [bulk.BULK_FIELD], ["name"]]},
            {"OBJ_F_ITEM_INV_AID": "OD_TYPE_INT32", bulk.BULK_FIELD: "OD_TYPE_INT32", "name": "OD_TYPE_STRING"})
        self.data = struct.pack("<i", 119) + struct.pack("<h", -1) + bytes(22) + bytes(24)
        self.data += struct.pack("<i", 5) + bytes(12) + struct.pack("<2i", 123, 0)
        self.data += b"\x01" + struct.pack("<i", 4) + b"book\x00"

    def test_preserves_all_other_fields_and_bytes(self):
        prototype = bulk.read_item_prototype(self.data, self.schema)
        patched = bulk.patched_prototype(self.data, prototype, 4)
        offset = prototype["bulk_offset"]
        self.assertEqual(len(patched), len(self.data))
        self.assertEqual(patched[:offset], self.data[:offset])
        self.assertEqual(patched[offset + 4:], self.data[offset + 4:])
        self.assertEqual(bulk.read_item_prototype(patched, self.schema)["values"],
            {"OBJ_F_ITEM_INV_AID": 123, bulk.BULK_FIELD: 4, "name": b"book\x00"})

    def test_rejects_truncation_suffix_and_invalid_patch(self):
        for data in (self.data[:-1], self.data + b"x", bytes(4)):
            with self.assertRaises(ValueError):
                bulk.read_item_prototype(data, self.schema)
        for offset in (-1, len(self.data)):
            with self.assertRaises(ValueError):
                bulk.patched_prototype(self.data, {"bulk_offset": offset}, 4)

    def test_application_backup_and_stale_plan(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / "game"
            game.mkdir()
            path = game / "book.pro"
            path.write_bytes(self.data)
            prototype = bulk.read_item_prototype(self.data, self.schema)
            after = bulk.patched_prototype(self.data, prototype, 4)
            plan = {"game": str(game), "errors": [], "records": [{"path": "book.pro", "bulk": 4,
                "changed": True, "conflict": False, "before_sha256": bulk.sha256(self.data),
                "after_sha256": bulk.sha256(after)}]}
            backup = root / "backup"
            bulk.apply(plan, self.schema, backup)
            self.assertEqual(path.read_bytes(), after)
            self.assertEqual((backup / "book.pro").read_bytes(), self.data)
            self.assertEqual(json.loads((backup / "manifest.json").read_text())["records"], plan["records"])
            with self.assertRaises(ValueError):
                bulk.apply(plan, self.schema, root / "stale-backup")
            self.assertFalse((root / "stale-backup").exists())

    def test_conflicts_block_all_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan = {"game": str(root), "errors": [], "records": [{"conflict": True}]}
            with self.assertRaises(ValueError):
                bulk.apply(plan, self.schema, root / "backup")
            self.assertFalse((root / "backup").exists())

    def test_art_palette_offset_and_cell_rounding(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "book.art"
            header = [0] * 33
            header[2], header[3], header[8] = 8, 1, 1
            path.write_bytes(struct.pack("<33I", *header) + bytes(1024) + struct.pack("<7i", 33, 64, 0, 0, 0, 0, 0))
            width, height = bulk.art_dimensions(path)
            self.assertEqual((width, height), (33, 64))
            self.assertEqual(((width + 31) // 32) * ((height + 31) // 32), 4)


if __name__ == "__main__":
    unittest.main()
