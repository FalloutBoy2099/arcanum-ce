"""Install the CE data port; never run the original executable patcher.

Requires extracted UAP data directories (as used by this checkout).
Only the three relevant description entries are merged into existing catalogs.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


ROOT = Path(__file__).resolve().parents[1]
BUNDLE = ROOT / "resources" / "automaton_gunner"
ENTRY = re.compile(rb"\{\s*(\d+)\s*\}\s*\{([^}]*)\}")


def merge_messages(original, patch):
    """Keep encoding, layout and all unrelated bytes, including custom entries."""
    for match in ENTRY.finditer(patch):
        key, value = match.groups()
        pattern = re.compile(rb"(\{\s*" + key + rb"\s*\}\s*\{)[^}]*(\})")
        original, count = pattern.subn(lambda old: old[1] + value + old[2], original)
        if count == 0:
            newline = b"\r\n" if b"\r\n" in original else b"\n"
            original = original.rstrip(b"\r\n") + newline + b"{" + key + b"}{" + value + b"}" + newline
    return original


def validate_bundle():
    manifest = json.loads((BUNDLE / "AutomatonGunner-Nexus-manifest.json").read_text())
    payload = {}
    for relative, expected in manifest["payload_files"].items():
        if relative == "data/mes/description.mes":
            continue  # Deliberately replaced by small language patches.
        data = (BUNDLE / relative).read_bytes()
        if hashlib.sha256(data).hexdigest().upper() != expected:
            raise ValueError(f"Corrupt bundled resource: {relative}")
        payload[relative] = data
    if len(payload) != 7:
        raise ValueError("Incomplete Automaton Gunner payload")
    return payload


def install_plan(game):
    game = game.resolve()
    payload = validate_bundle()
    english = game / "Arcanum5.dat/mes/description.mes"
    if not english.is_file():
        raise ValueError("This installer requires extracted Arcanum5.dat UAP data")
    recipe = game / "Arcanum5.dat/rules/schematic.mes"
    entries = {int(m[1]): m[2].strip() for m in ENTRY.finditer(recipe.read_bytes())}
    if any(entries.get(key) != value for key, value in {5073: b"15133", 5074: b"6039", 5075: b"15172", 5076: b"1"}.items()):
        raise ValueError("The installed Automaton Gunner recipe differs from UAP; merge it before installation")
    # A second filename with the same prototype/script ID is ambiguous to CE.
    for relative in payload:
        path = Path(relative)
        if path.parent.name in ("proto", "scr"):
            prefix = re.match(r"\d+", path.name)[0]
            folder = game / path.parent
            for existing in folder.glob(prefix + "*"):
                if existing.name.casefold() != path.name.casefold():
                    raise ValueError(f"Conflicting resource ID: {existing}")
    plan = {game / relative: data for relative, data in payload.items()}
    patches = {language: (BUNDLE / f"translations/description.{language}.mes").read_bytes() for language in ("en", "fr")}
    catalogs = [game / f"{pack}/mes/description.mes" for pack in ("Arcanum3.dat", "Arcanum5.dat", "ArcanumX.dat")]
    catalogs += [game / "ArcanumX_fr.dat/mes/description.mes"]
    # A loose catalog overrides both language packs, so do not silently create one.
    loose = game / "data/mes/description.mes"
    if loose.exists():
        raise ValueError("A loose data/mes/description.mes overrides translations; merge it explicitly first")
    for path in catalogs:
        if path.is_file():
            language = "fr" if path.parts[-3].lower().endswith("_fr.dat") else "en"
            plan[path] = merge_messages(path.read_bytes(), patches[language])
    return plan


def install(game, backup):
    plan = install_plan(game)
    changed = {path: data for path, data in plan.items() if not path.is_file() or path.read_bytes() != data}
    if not changed:
        return []
    backup.mkdir(parents=True, exist_ok=False)
    records = []
    for path, data in changed.items():
        relative = path.relative_to(game.resolve())
        existed = path.is_file()
        if existed:
            saved = backup / relative
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, saved)
        records.append({"path": relative.as_posix(), "existed": existed, "sha256_after": hashlib.sha256(data).hexdigest()})
    # Record every destination before writing, including files to remove on rollback.
    (backup / "installed-files.json").write_text(json.dumps(records, indent=2) + "\n")
    for path, data in changed.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        if path.read_bytes() != data:
            raise OSError(f"Installation verification failed: {path}")
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--backup", type=Path)
    parser.add_argument("--check", action="store_true", help="Verify installed files without writing")
    args = parser.parse_args()
    if args.check:
        plan = install_plan(args.game)
        missing = [str(path) for path, data in plan.items() if not path.is_file() or path.read_bytes() != data]
        if missing:
            parser.exit(1, "Not installed or outdated:\n" + "\n".join(missing) + "\n")
        print(f"Verified {len(plan)} installed resources/catalogs")
    else:
        if args.backup is None:
            parser.error("--backup is required when installing")
        records = install(args.game, args.backup)
        print(f"Installed and verified {len(records)} files; backup: {args.backup}")


if __name__ == "__main__":
    main()
