"""Check the packaged content against CE and exercise reversible installation."""

import json
from pathlib import Path
import re
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import install_automaton_gunner as port


def enum_values(name):
    source = (ROOT / "src/game/script.h").read_text()
    body = re.search(r"typedef enum " + name + r" \{(.*?)\}", source, re.S)[1]
    body = re.sub(r"//[^\n]*|/\*.*?\*/", "", body, flags=re.S)
    result = {}
    value = 0
    for entry in body.split(","):
        entry = entry.strip()
        if not entry:
            continue
        if "=" in entry:
            entry, explicit = entry.split("=")
            value = int(explicit.strip(), 0)
        result[entry.strip()] = value
        value += 1
    return result


class AutomatonGunnerTest(unittest.TestCase):
    def test_payload_matches_original_manifest(self):
        self.assertEqual(len(port.validate_bundle()), 7)

    def test_prototypes_fit_engine_ranges(self):
        source = (ROOT / "src/game/proto.c").read_text()
        tables = []
        for symbol in ("dword_5B37FC", "dword_5B384C"):
            body = re.search(symbol + r"\[OBJ_TYPE_COUNT\] = \{(.*?)\};", source, re.S)[1]
            tables.append(dict((kind, int(value)) for kind, value in re.findall(r"/\*\s*(OBJ_TYPE_\w+)\s*\*/\s*(\d+)", body)))
        for path in (port.BUNDLE / "data/proto").glob("*.pro"):
            data = path.read_bytes()
            self.assertEqual(struct.unpack_from("<I", data)[0], 119)
            # Prototype ObjectID.d.a, after version and null instance ObjectID.
            prototype_id = struct.unpack_from("<I", data, 36)[0]
            self.assertEqual(prototype_id, int(path.name[:6]))
            basic_id = prototype_id - 20
            kind = "OBJ_TYPE_" + path.stem.split(" - ")[1].upper()
            if kind == "OBJ_TYPE_NPC":
                kind = "OBJ_TYPE_UNIQUE_NPC"
            self.assertLessEqual(tables[0][kind], basic_id)
            self.assertLess(basic_id, tables[1][kind])
        self.assertEqual(tables[1]["OBJ_TYPE_AMMO"], 7043)
        self.assertEqual(tables[1]["OBJ_TYPE_UNIQUE_NPC"], 28473)

    def test_deploy_and_collapse_use_supported_actions(self):
        actions = enum_values("ScriptActionType")
        conditions = enum_values("ScriptConditionType")
        expected = {
            "39990": [("SAT_TRANSFORM_ATTACHEE_INTO_BASIC_PROTOTYPE", 28472), ("SAT_CREATE_ITEM", 6039), ("SAT_CREATE_ITEM", 7042), ("SAT_CRITTER_FOLLOW", 0), ("SAT_RETURN_AND_RUN_DEFAULT", 0)],
            "39991": [("SAT_CRITTER_DISBAND", 0), ("SAT_TRANSFORM_ATTACHEE_INTO_BASIC_PROTOTYPE", 15172), ("SAT_RETURN_AND_SKIP_DEFAULT", 0)],
        }
        for path in (port.BUNDLE / "data/scr").glob("*.scr"):
            data = path.read_bytes()
            count = struct.unpack_from("<I", data, 52)[0]
            self.assertEqual(len(data), 64 + 132 * count)
            self.assertEqual(count, len(expected[path.name[:5]]))
            for index, (action, operand) in enumerate(expected[path.name[:5]]):
                offset = 64 + 132 * index
                self.assertEqual(struct.unpack_from("<I", data, offset)[0], conditions["SCT_TRUE"])
                self.assertEqual(struct.unpack_from("<I", data, offset + 44)[0], actions[action])
                self.assertEqual(struct.unpack_from("<i", data, offset + 56)[0], operand)
                self.assertEqual(struct.unpack_from("<I", data, offset + 88)[0], actions["SAT_DO_NOTHING"])

    def test_merge_preserves_translation_and_custom_entries(self):
        original = "// mod personnel\r\n{50}{Épée à deux mains}\r\n{15172}{Inutilisé}\r\n".encode("cp1252")
        patch = (port.BUNDLE / "translations/description.fr.mes").read_bytes()
        merged = port.merge_messages(original, patch)
        self.assertIn("{50}{Épée à deux mains}".encode("cp1252"), merged)
        self.assertIn(b"{15172}{Auto-artilleur}", merged)
        self.assertIn(b"{28472}{Auto-artilleur}", merged)
        self.assertEqual(port.merge_messages(merged, patch), merged)

    def make_game(self, root):
        game = root / "game"
        for relative, data in {
            "Arcanum5.dat/mes/description.mes": b"{50}{Sword}\n{15172}{UNUSED}\n",
            "ArcanumX_fr.dat/mes/description.mes": b"{50}{Epee}\n{15172}{Auto-artilleur}\n",
            "Arcanum5.dat/rules/schematic.mes": b"{5073}{15133}\n{5074}{6039}\n{5075}{15172}\n{5076}{1}\n",
            "data/proto/015192 - Generic.pro": b"previous prototype",
        }.items():
            path = game / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        return game

    def test_installation_backup_verification_and_idempotence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = self.make_game(root)
            backup = root / "backup"
            records = port.install(game, backup)
            self.assertEqual(len(records), 9)
            self.assertEqual((backup / "data/proto/015192 - Generic.pro").read_bytes(), b"previous prototype")
            self.assertEqual(json.loads((backup / "installed-files.json").read_text()), records)
            for path, data in port.install_plan(game).items():
                self.assertEqual(path.read_bytes(), data)
            self.assertEqual(port.install(game, root / "unused-backup"), [])
            self.assertFalse((root / "unused-backup").exists())

    def test_conflicts_fail_before_writing(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = self.make_game(root)
            conflict = game / "data/proto/028492 - Another NPC.pro"
            conflict.write_bytes(b"other mod")
            with self.assertRaisesRegex(ValueError, "Conflicting resource ID"):
                port.install(game, root / "backup")
            self.assertFalse((root / "backup").exists())
            self.assertEqual((game / "data/proto/015192 - Generic.pro").read_bytes(), b"previous prototype")


if __name__ == "__main__":
    unittest.main()
