import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
from collections import Counter


ITEM_GROUPS = ["WEAPON", "AMMO", "ARMOR", "GOLD", "FOOD", "SCROLL", "KEY", "KEY_RING", "WRITTEN", "GENERIC"]
BULK_FIELD = "OBJ_F_ITEM_PAD_I_1"


class Reader:
    def __init__(self, data, offset=0):
        self.data = data
        self.offset = offset

    def take(self, size):
        if size < 0 or size > len(self.data) - self.offset:
            raise ValueError("Truncated prototype")
        start = self.offset
        self.offset += size
        return self.data[start:self.offset]

    def integer(self):
        return struct.unpack("<i", self.take(4))[0]


def load_schema(repository):
    header = (repository / "src/game/obj.h").read_text(encoding="utf-8")
    source = (repository / "src/game/obj.c").read_text(encoding="utf-8")
    enum = header.split("typedef enum ObjectField {", 1)[1].split("} ObjectField;", 1)[0]
    fields = re.findall(r"\bOBJ_F_[A-Z0-9_]+\b", enum)
    types = dict(re.findall(r"object_fields\[(OBJ_F_[A-Z0-9_]+)\]\.type = (OD_TYPE_[A-Z0-9_]+);", source))

    def group(name):
        begin = "OBJ_F_BEGIN" if not name else f"OBJ_F_{name}_BEGIN"
        end = "OBJ_F_END" if not name else f"OBJ_F_{name}_END"
        return fields[fields.index(begin) + 1:fields.index(end)]

    return {number: [group(""), group("ITEM"), group(name)] for number, name in enumerate(ITEM_GROUPS, 5)}, types


def skip_field(reader, kind):
    if kind == "OD_TYPE_INT32":
        return reader.integer()
    presence = reader.take(1)[0]
    if not presence:
        return None
    if kind == "OD_TYPE_INT64":
        return reader.take(8)
    if kind == "OD_TYPE_HANDLE":
        return reader.take(24)
    if kind == "OD_TYPE_STRING":
        return reader.take(reader.integer() + 1)
    if kind.endswith("_ARRAY"):
        element_size, count, _ = struct.unpack("<3i", reader.take(12))
        if element_size <= 0 or count < 0:
            raise ValueError("Invalid prototype array")
        reader.take(element_size * count)
        bitmap_words = reader.integer()
        reader.take(bitmap_words * 4)
        return None
    raise ValueError(f"Unsupported field type: {kind}")


def read_item_prototype(data, schema):
    reader = Reader(data)
    if reader.integer() != 119:
        raise ValueError("Unsupported prototype version")
    if struct.unpack("<h", reader.take(24)[:2])[0] != -1:
        raise ValueError("Expected prototype, not instance")
    reader.take(24)
    object_type = reader.integer()
    groups, types = schema
    if object_type not in groups:
        return None
    selected = groups[object_type]
    reader.take(4 * sum((len(group) + 31) // 32 for group in selected))
    values = {}
    offsets = {}
    for group in selected:
        for field in group:
            offsets[field] = reader.offset
            values[field] = skip_field(reader, types[field])
    if reader.offset != len(data):
        raise ValueError(f"Unexpected prototype suffix: {len(data) - reader.offset} bytes")
    return {"type": object_type, "values": values, "bulk_offset": offsets[BULK_FIELD]}


class GameAssets:
    def __init__(self, manifest):
        self.roots = [Path(line) for line in reversed(manifest.read_text(encoding="utf-8-sig").splitlines()) if line and Path(line).is_dir()]
        self.messages = {}

    def find(self, relative):
        relative = relative.replace("\\", "/")
        for root in self.roots:
            path = root / relative
            if path.is_file():
                return path
        raise FileNotFoundError(relative)

    def mes(self, relative):
        if relative not in self.messages:
            data = self.find(relative).read_bytes()
            text = data.decode("utf-8-sig" if data.startswith(b"\xef\xbb\xbf") else "cp1252")
            self.messages[relative] = {int(key): value for key, value in re.findall(r"\{\s*(-?\d+)\s*\}\s*\{([^}]*)\}", text)}
        return self.messages[relative]

    def item_art(self, aid):
        aid &= 0xFFFFFFFF
        if aid >> 28 != 6:
            raise ValueError(f"Non-item inventory ART: {aid:08x}")
        number = (aid >> 17) & 0x7FF
        subtype = (aid >> 6) & 15
        item_type = aid & 15
        coverage = (aid >> 14) & 7
        disposition = (aid >> 12) & 3
        key = number + 20 * (subtype + 50 * item_type)
        if item_type == 2 and coverage != 0:
            key += 20 * (5 * coverage + 10)
        table = ["item_ground", "item_inven", "item_paper", "item_schematic"][disposition]
        filename = self.mes(f"art/item/{table}.mes")[key].strip()
        return self.find(f"art/item/{filename}")


def art_dimensions(path):
    with path.open("rb") as stream:
        header = stream.read(132)
        if len(header) != 132:
            raise ValueError("Truncated ART header")
        words = struct.unpack("<33I", header)
        if words[2] != 8 or words[8] < 1:
            raise ValueError("Unsupported ART header")
        palette_count = sum(value != 0 for value in words[3:7])
        stream.seek(132 + 1024 * palette_count)
        metadata = stream.read(28)
        if len(metadata) != 28:
            raise ValueError("Truncated ART frame")
        width, height = struct.unpack_from("<2i", metadata)
        if width <= 0 or height <= 0 or width > 32768 or height > 32768:
            raise ValueError("Invalid ART dimensions")
        return width, height


def patched_prototype(data, prototype, bulk):
    if bulk < 1 or bulk > 0x7FFFFFFF:
        raise ValueError("Invalid bulk value")
    offset = prototype["bulk_offset"]
    if offset < 0 or offset + 4 > len(data):
        raise ValueError("Invalid bulk field offset")
    return data[:offset] + struct.pack("<i", bulk) + data[offset + 4:]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def prepare(game, schema, assets):
    records = []
    skipped = Counter()
    errors = []
    for path in sorted(game.rglob("*.pro")):
        if "save" in {part.lower() for part in path.relative_to(game).parts}:
            continue
        try:
            data = path.read_bytes()
            prototype = read_item_prototype(data, schema)
            if prototype is None:
                skipped["not_item"] += 1
                continue
            aid = prototype["values"]["OBJ_F_ITEM_INV_AID"]
            image = assets.item_art(aid)
            width, height = art_dimensions(image)
            columns, rows = (width + 31) // 32, (height + 31) // 32
            bulk = columns * rows
            old = prototype["values"][BULK_FIELD]
            after = patched_prototype(data, prototype, bulk)
            check = read_item_prototype(after, schema)
            expected = dict(prototype["values"])
            expected[BULK_FIELD] = bulk
            if check["values"] != expected or len(after) != len(data):
                raise ValueError("Prototype preservation check failed")
            records.append({"path": str(path.relative_to(game)), "image": str(image), "aid": aid & 0xFFFFFFFF,
                "width": width, "height": height, "columns": columns, "rows": rows, "old_bulk": old, "bulk": bulk,
                "offset": prototype["bulk_offset"], "before_sha256": sha256(data), "after_sha256": sha256(after),
                "conflict": old not in (0, bulk), "changed": data != after})
        except (ValueError, KeyError, FileNotFoundError, struct.error) as error:
            errors.append({"path": str(path.relative_to(game)), "error": str(error)})
    return {"game": str(game), "field": BULK_FIELD, "records": records, "skipped": dict(skipped), "errors": errors}


def apply(plan, schema, backup):
    if plan["errors"] or any(record["conflict"] for record in plan["records"]):
        raise ValueError("Unresolved prototypes or existing reserved values; no changes applied")
    game = Path(plan["game"])
    changes = [record for record in plan["records"] if record["changed"]]
    for record in changes:
        if sha256((game / record["path"]).read_bytes()) != record["before_sha256"]:
            raise ValueError(f"Prototype changed since planning: {record['path']}")
    backup.mkdir(parents=True, exist_ok=False)
    (backup / "manifest.json").write_text(json.dumps(plan, ensure_ascii=False, indent=2), encoding="utf-8")
    for record in changes:
        source = game / record["path"]
        saved = backup / record["path"]
        saved.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, saved)
    for record in changes:
        path = game / record["path"]
        data = path.read_bytes()
        if sha256(data) != record["before_sha256"]:
            raise ValueError(f"Prototype changed during application: {record['path']}")
        after = patched_prototype(data, read_item_prototype(data, schema), record["bulk"])
        temporary = path.with_name(path.name + ".bulk.tmp")
        with temporary.open("xb") as stream:
            stream.write(after)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        if sha256(path.read_bytes()) != record["after_sha256"]:
            raise ValueError(f"Prototype verification failed: {record['path']}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--repositories", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--backup", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    schema = load_schema(Path(__file__).resolve().parents[1])
    plan = prepare(args.game.resolve(), schema, GameAssets(args.repositories))
    args.report.write_text(json.dumps(plan, ensure_ascii=False, indent=2), encoding="utf-8")
    summary = {"items": len(plan["records"]), "changes": sum(record["changed"] for record in plan["records"]),
        "conflicts": sum(record["conflict"] for record in plan["records"]), "skipped": plan["skipped"], "errors": plan["errors"][:20], "error_count": len(plan["errors"])}
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    if args.apply:
        if args.backup is None:
            parser.error("--apply requires --backup")
        apply(plan, schema, args.backup)
        print("All planned prototype changes verified")


if __name__ == "__main__":
    main()
