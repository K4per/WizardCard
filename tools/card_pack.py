"""Deterministic, self-contained developer card packs (not player decks)."""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile

from catalog_source import load_catalog

ROOT = Path(__file__).resolve().parents[1]
FORMAT = "wizardcard.card-pack"
VERSION = 1
MAX_BYTES = 256 * 1024 * 1024
CARD_FIELDS = set("id name rarity type number illustrationId text flavor target cost castCost rank analysisTurns duration burden extraDiscard concentrationUpkeep body capacity income rings maxRank ringBonus refundCast baseEligible immediate concentration perpetual recipe opponentDestroyProtected incomeBonus school speed capacityBonus rankBonus castCostBonus damageBonus responseOnly baseAnalysisDiscount quickRankOne resistances immunities refundSetting targetCount tags traits effects onPrepare onEnd onDraw onMain onCast onLeave onMana responses level".split())
CARD_FIELDS.add("summonCondition")  # Legacy display-only condition retained for roundtrips.
EFFECT_FIELDS = {"kind", "amount", "damageType", "recipient"}
RECIPE_FIELDS = {"kind", "amount", "definition", "school", "readyOnly", "distinctSchools", "fireDamage"}
RESPONSE_FIELDS = set("id name sources handCost preloadedCost burden extraDiscard targetCount requiresSource target eventOwner windows phases effects".split())


def confined(root: Path, relative: str) -> Path:
    if (not isinstance(relative, str) or not relative or "\\" in relative or ":" in relative
            or any(p in ("", ".", "..") for p in relative.split("/"))
            or PurePosixPath(relative).is_absolute()):
        raise ValueError(f"Invalid package path: {relative!r}")
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()):
        raise ValueError(f"Package path escapes root: {relative}")
    return path


def document(path: Path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"{path}: duplicate JSON key {key}")
            result[key] = value
        return result
    if path.stat().st_size > MAX_BYTES:
        raise ValueError(f"{path}: package exceeds 256 MiB")
    return json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique)


def encoded(value) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def fields(value, allowed: set[str], location: str) -> None:
    if not isinstance(value, dict):
        raise ValueError(f"{location}: expected object")
    unknown = set(value) - allowed
    if unknown:
        raise ValueError(f"{location}: unsupported fields {', '.join(sorted(unknown))}")


def check_cards(cards: list) -> None:
    if not isinstance(cards, list) or not cards:
        raise ValueError("cards: expected nonempty list")
    ids, numbers = set(), set()
    for card in cards:
        fields(card, CARD_FIELDS, "cards")
        card_id = card.get("id")
        if not isinstance(card_id, str) or not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", card_id):
            raise ValueError(f"cards/id: invalid stable ID {card_id!r}")
        if card_id in ids:
            raise ValueError(f"cards/{card_id}: duplicate ID")
        ids.add(card_id)
        number = card.get("number")
        if number is not None and (not isinstance(number, str) or number in numbers):
            raise ValueError(f"cards/{card_id}/number: invalid or duplicate number {number!r}")
        numbers.add(number)
        for key in ("effects", "onPrepare", "onEnd", "onDraw", "onMain", "onCast", "onLeave", "onMana", "recipe", "responses"):
            rows = card.get(key, [])
            if not isinstance(rows, list):
                raise ValueError(f"cards/{card_id}/{key}: expected list")
            allowed = RECIPE_FIELDS if key == "recipe" else RESPONSE_FIELDS if key == "responses" else EFFECT_FIELDS
            for i, row in enumerate(rows):
                location = f"cards/{card_id}/{key}/{i}"
                fields(row, allowed, location)
                if key == "responses":
                    if not isinstance(row.get("effects", []), list):
                        raise ValueError(f"{location}/effects: expected list")
                    for effect in row.get("effects", []):
                        fields(effect, EFFECT_FIELDS, location + "/effects")


def native_validate(directory: Path, validator: Path) -> None:
    if not validator.is_file():
        raise ValueError(f"Native validator missing: {validator}; build wizard_cli or pass --validator")
    result = subprocess.run([str(validator.resolve()), "validate-catalog", str(directory.resolve())],
                            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
    if result.returncode:
        raise ValueError("Native catalog validation: " + (result.stderr or result.stdout).strip())
    if "definitions; hash " not in result.stdout:
        raise ValueError("Validator lacks validate-catalog support; rebuild wizard_cli")


def resource_index(assets: Path, runtime_manifest: Path | None) -> dict:
    result = {}
    saved = assets / "card-pack-resources.json"
    if saved.is_file():
        for entry in document(saved)["resources"]:
            result[entry["id"]] = (confined(assets, entry["file"]), entry["sha256"])
    if runtime_manifest and runtime_manifest.is_file():
        for entry in document(runtime_manifest)["assets"]:
            if entry["id"].startswith("illustration."):
                resource = entry["file"]
                if not resource.startswith("res://art/"):
                    raise ValueError(f"Invalid runtime illustration path: {resource}")
                result.setdefault(entry["id"], (confined(runtime_manifest.parent, resource[10:]), entry["sha256"]))
    return result


def export_pack(assets: Path, output: Path, card_ids: list[str] | None, runtime_manifest: Path | None) -> dict:
    catalog = load_catalog(assets)
    selected = set(card_ids or [card["id"] for card in catalog["cards"]])
    cards = sorted((card for card in catalog["cards"] if card["id"] in selected), key=lambda card: card["id"])
    missing = selected - {card["id"] for card in cards}
    if missing:
        raise ValueError("Unknown card IDs: " + ", ".join(sorted(missing)))
    check_cards(cards)
    index, resources = resource_index(assets, runtime_manifest), []
    for resource_id in sorted({"illustration." + card.get("illustrationId", card["id"]) for card in cards}):
        if resource_id not in index:
            raise ValueError(f"Missing required illustration: {resource_id}")
        path, expected = index[resource_id]
        data = path.read_bytes()
        checksum = hashlib.sha256(data).hexdigest()
        if checksum != expected:
            raise ValueError(f"Resource hash mismatch: {resource_id}")
        resources.append({"id": resource_id, "file": "card-pack-resources/" + resource_id[13:] + path.suffix.lower(),
                          "sha256": checksum, "encoding": "base64", "data": base64.b64encode(data).decode("ascii")})
    pack = {"format": FORMAT, "version": VERSION, "pool": {key: value for key, value in catalog.items() if key != "cards"},
            "cards": cards, "resources": resources}
    data = encoded(pack)
    if len(data) > MAX_BYTES:
        raise ValueError("Export exceeds 256 MiB package limit")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    return pack


def import_pack(package: Path, assets: Path, validator: Path, overwrite: bool = False) -> int:
    pack = document(package)
    fields(pack, {"format", "version", "pool", "cards", "resources"}, "package")
    if pack.get("format") != FORMAT or type(pack.get("version")) is not int or pack["version"] != VERSION:
        raise ValueError("package/version: unsupported card-pack format or version")
    metadata, cards, resources = pack.get("pool"), pack.get("cards"), pack.get("resources")
    if not isinstance(metadata, dict) or "cards" in metadata:
        raise ValueError("pool: expected catalog metadata")
    fields(metadata, {"rulesVersion", "cardSetVersion", "baseLoadCapacity", "mechanicsRevision"}, "pool")
    if set(metadata) != {"rulesVersion", "cardSetVersion", "baseLoadCapacity", "mechanicsRevision"}:
        raise ValueError("pool: all four catalog metadata fields are required")
    check_cards(cards)
    if not isinstance(resources, list):
        raise ValueError("resources: expected list")
    blobs, resource_ids, resource_paths = {}, set(), set()
    for entry in resources:
        fields(entry, {"id", "file", "sha256", "encoding", "data"}, "resources")
        resource_id, relative = entry.get("id"), entry.get("file")
        confined(assets, relative)
        if not relative.startswith("card-pack-resources/") or Path(relative).suffix.lower() not in {".png", ".jpg", ".jpeg", ".webp"}:
            raise ValueError(f"resources/{resource_id}: unsupported image path {relative}")
        if not isinstance(resource_id, str) or not resource_id.startswith("illustration.") or resource_id in resource_ids or relative.casefold() in resource_paths:
            raise ValueError(f"resources: duplicate or invalid resource {resource_id!r}")
        if entry.get("encoding") != "base64":
            raise ValueError(f"resources/{resource_id}: expected base64 encoding")
        try:
            raw = base64.b64decode(entry["data"], validate=True)
        except (ValueError, TypeError) as error:
            raise ValueError(f"resources/{resource_id}: invalid base64") from error
        if not raw or hashlib.sha256(raw).hexdigest() != entry.get("sha256"):
            raise ValueError(f"resources/{resource_id}: SHA-256 mismatch")
        extension = Path(relative).suffix.lower()
        valid_signature = (raw.startswith(b"\x89PNG\r\n\x1a\n") if extension == ".png" else
                           raw.startswith(b"\xff\xd8\xff") if extension in {".jpg", ".jpeg"} else
                           raw.startswith(b"RIFF") and raw[8:12] == b"WEBP")
        if not valid_signature:
            raise ValueError(f"resources/{resource_id}: invalid {extension} signature")
        blobs[relative] = raw
        resource_ids.add(resource_id)
        resource_paths.add(relative.casefold())
    expected = {"illustration." + card.get("illustrationId", card["id"]) for card in cards}
    if resource_ids != expected:
        raise ValueError(f"resources: missing {sorted(expected - resource_ids)}; unreferenced {sorted(resource_ids - expected)}")
    existing = load_catalog(assets) if (assets / "catalog.json").is_file() else {**metadata, "cards": []}
    if {k: v for k, v in existing.items() if k != "cards"} != metadata:
        raise ValueError("pool: metadata is incompatible with target catalog")
    old_cards = {card["id"]: card for card in existing["cards"]}
    conflicts = set(old_cards) & {card["id"] for card in cards}
    if conflicts and not overwrite:
        raise ValueError("cards: ID conflicts require --overwrite: " + ", ".join(sorted(conflicts)))
    old_cards.update({card["id"]: card for card in cards})
    combined = sorted(old_cards.values(), key=lambda card: card["id"])
    check_cards(combined)
    manifest = document(assets / "catalog.json") if (assets / "catalog.json").is_file() else {"format": 1, "metadata": metadata, "definitions": []}
    definitions = {entry["id"]: entry["file"] for entry in manifest["definitions"]}
    for card in cards:
        definitions.setdefault(card["id"], f"cards/{card['type']}/{card['id']}.json")
    manifest["definitions"] = [{"id": card["id"], "file": definitions[card["id"]]} for card in combined]
    writes = {definitions[card["id"]]: encoded(card) for card in cards}
    for card in cards:
        relative = definitions[card["id"]]
        target = confined(assets, relative)
        if target.exists() and card["id"] not in conflicts and not overwrite:
            raise ValueError(f"cards/{card['id']}: occupied definition path requires --overwrite: {relative}")
    writes.update(blobs)
    saved_resources = document(assets / "card-pack-resources.json")["resources"] if (assets / "card-pack-resources.json").is_file() else []
    old_resources = {entry["id"]: entry for entry in saved_resources}
    for entry in resources:
        previous = old_resources.get(entry["id"])
        if previous and previous != {k: entry[k] for k in ("id", "file", "sha256")} and not overwrite:
            raise ValueError(f"resources/{entry['id']}: conflict requires --overwrite")
        target = confined(assets, entry["file"])
        if target.exists() and target.read_bytes() != blobs[entry["file"]] and not overwrite:
            raise ValueError(f"resources/{entry['id']}: file conflict requires --overwrite")
        old_resources[entry["id"]] = {k: entry[k] for k in ("id", "file", "sha256")}
    writes["card-pack-resources.json"] = encoded({"format": 1, "resources": sorted(old_resources.values(), key=lambda e: e["id"])})
    writes["cards.json"] = encoded({**metadata, "cards": combined})
    writes["catalog.json"] = encoded(manifest)
    # Validate the entire candidate catalog before opening any target for writing.
    assets.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".card-pack-", dir=assets.parent) as temporary:
        stage = Path(temporary) / "stage"
        stage.mkdir()
        for card in combined:
            relative = definitions[card["id"]]
            candidate = confined(stage, relative)
            candidate.parent.mkdir(parents=True, exist_ok=True)
            candidate.write_bytes(writes.get(relative, encoded(card)))
        for relative, raw in writes.items():
            candidate = confined(stage, relative)
            candidate.parent.mkdir(parents=True, exist_ok=True)
            candidate.write_bytes(raw)
        native_validate(stage, validator)
        backup = Path(temporary) / "backup"
        committed, created_dirs = [], []
        target_existed = assets.exists()
        try:
            # Catalog is committed last. Restore every replaced file if a write fails.
            for relative in sorted(writes, key=lambda value: (value == "catalog.json", value)):
                target = confined(assets, relative)
                parent = target.parent
                while not parent.exists():
                    created_dirs.append(parent)
                    parent = parent.parent
                target.parent.mkdir(parents=True, exist_ok=True)
                saved = backup / relative
                existed = target.exists()
                if existed:
                    saved.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(target, saved)
                os.replace(confined(stage, relative), target)
                committed.append((target, saved, existed))
        except OSError:
            for target, saved, existed in reversed(committed):
                if existed:
                    os.replace(saved, target)
                else:
                    target.unlink(missing_ok=True)
            for directory in sorted(set(created_dirs), key=lambda p: len(p.parts), reverse=True):
                if directory.exists() and not any(directory.iterdir()):
                    directory.rmdir()
            if not target_existed and assets.exists() and not any(assets.iterdir()):
                assets.rmdir()
            raise
    return len(cards)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    export = sub.add_parser("export")
    export.add_argument("output", type=Path)
    export.add_argument("--assets", type=Path, default=ROOT / "assets")
    export.add_argument("--cards", nargs="+")
    export.add_argument("--runtime-manifest", type=Path, default=ROOT / "godot/art/assets.json")
    load = sub.add_parser("import")
    load.add_argument("package", type=Path)
    load.add_argument("--assets", type=Path, required=True)
    load.add_argument("--overwrite", action="store_true")
    load.add_argument("--validator", type=Path, default=ROOT / "build/windows/Release/wizard_cli.exe")
    args = parser.parse_args()
    try:
        if args.command == "export":
            pack = export_pack(args.assets.resolve(), args.output, args.cards, args.runtime_manifest)
            print(f"Exported {len(pack['cards'])} cards and {len(pack['resources'])} resources: {args.output}")
        else:
            count = import_pack(args.package, args.assets.resolve(), args.validator, args.overwrite)
            print(f"Imported {count} cards: {args.assets}")
        return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"Card pack error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
