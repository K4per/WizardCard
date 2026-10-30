"""Audit source and Godot runtime art inventories.

The repository intentionally keeps authoring material beside archived and
regenerable files. This check follows the two manifests instead of guessing
from directory names, verifies every runtime reference, and writes a compact
report suitable for release evidence.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import defaultdict
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "assets" / "art"
RUNTIME_ROOT = ROOT / "godot" / "art"
INVENTORY = SOURCE_ROOT / "ART_INVENTORY.json"
RUNTIME_MANIFEST = RUNTIME_ROOT / "assets.json"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def category(path: Path) -> str:
    parts = path.relative_to(SOURCE_ROOT).parts
    if "prototypes" in parts or "design" in parts:
        return "archive"
    if "source" in parts or "concepts" in parts or "manifests" in parts:
        return "authoring"
    if parts and parts[0] in {"godot-2d-v1", "godot-2d-v2", "fonts"}:
        return "candidate"
    return "legacy"


def image_size(path: Path) -> list[int] | None:
    if path.suffix.lower() == ".png":
        header = path.read_bytes()[:24]
        if header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR":
            return list(struct.unpack(">II", header[16:24]))
    return None


def check_inventory() -> tuple[dict[str, dict], list[str]]:
    data = json.loads(INVENTORY.read_text(encoding="utf-8"))
    entries = data.get("sourceFiles", data.get("existingFiles"))
    if not isinstance(entries, list):
        raise ValueError("ART_INVENTORY.json: existingFiles must be an array")
    indexed: dict[str, dict] = {}
    errors: list[str] = []
    for entry in entries:
        relative = entry.get("file") if isinstance(entry, dict) else None
        if not isinstance(relative, str) or relative in indexed:
            errors.append(f"invalid or duplicate inventory path: {relative!r}")
            continue
        if not relative.replace("\\", "/").startswith("assets/art/"):
            errors.append(f"inventory path must stay in assets/art: {relative}")
            continue
        path = (ROOT / relative).resolve()
        if not path.is_relative_to(SOURCE_ROOT.resolve()) or not path.is_file():
            errors.append(f"missing inventory file: {relative}")
            continue
        indexed[relative] = entry
        if entry.get("sha256") != digest(path) or entry.get("bytes") != path.stat().st_size:
            errors.append(f"inventory content changed: {relative}")
    return indexed, errors


def runtime_files() -> tuple[set[str], list[str]]:
    manifest = json.loads(RUNTIME_MANIFEST.read_text(encoding="utf-8"))
    entries = manifest.get("assets")
    if not isinstance(entries, list):
        raise ValueError("godot/art/assets.json: assets must be an array")
    ids: set[str] = set()
    files: set[str] = set()
    errors: list[str] = []
    for entry in entries:
        asset_id = entry.get("id")
        resource = entry.get("file")
        if not isinstance(asset_id, str) or asset_id in ids:
            errors.append(f"duplicate runtime asset ID: {asset_id!r}")
        if not isinstance(resource, str) or not resource.startswith("res://art/"):
            errors.append(f"invalid runtime resource path: {asset_id!r}")
            continue
        relative = resource.removeprefix("res://")
        path = (ROOT / "godot" / relative).resolve()
        if not path.is_relative_to(RUNTIME_ROOT.resolve()) or not path.is_file():
            errors.append(f"missing runtime asset: {resource}")
        elif entry.get("sha256") != digest(path):
            errors.append(f"runtime asset hash mismatch: {asset_id}")
        if relative in files:
            errors.append(f"duplicate runtime file: {resource}")
        ids.add(asset_id)
        files.add(relative)
    return files, errors


def write_inventory(source_files: dict[str, Path], runtime_fileset: set[str], previous: dict[str, dict]) -> None:
    runtime_manifest = json.loads(RUNTIME_MANIFEST.read_text(encoding="utf-8"))
    source_ids: dict[str, list[str]] = defaultdict(list)
    for entry in runtime_manifest["assets"]:
        source = entry.get("source")
        if isinstance(source, str):
            source_ids[source].append(str(entry["id"]))
    source_entries: list[dict] = []
    for relative, path in sorted(source_files.items()):
        item: dict = {
            "file": relative,
            "extension": path.suffix.lower(),
            "bytes": path.stat().st_size,
            "sha256": digest(path),
            "category": path.relative_to(SOURCE_ROOT).parts[0],
        }
        dimensions = image_size(path)
        if dimensions:
            item["size"] = dimensions
        if relative in source_ids:
            item["runtimeAssetIds"] = sorted(source_ids[relative])
            item["usage"] = "registered-runtime-source"
        elif category(path) == "archive":
            item["usage"] = "historical-or-design-archive"
        elif category(path) == "authoring":
            item["usage"] = "authoring-source-or-validation"
        else:
            item["usage"] = "candidate-or-legacy; verify references before removal"
        source_entries.append(item)
    runtime_entries: list[dict] = []
    combined_hashes: dict[str, list[str]] = defaultdict(list)
    for relative, item in ((entry["file"], entry) for entry in source_entries):
        combined_hashes[item["sha256"]].append(relative)
    for relative in sorted(runtime_fileset):
        path = RUNTIME_ROOT / relative.removeprefix("art/")
        entry = next(entry for entry in runtime_manifest["assets"] if entry["file"] == "res://" + relative)
        runtime_entries.append({"file": "godot/" + relative, "assetId": entry["id"],
                                "source": entry.get("source"), "bytes": path.stat().st_size,
                                "sha256": digest(path)})
        combined_hashes[digest(path)].append("godot/" + relative)
    duplicates = []
    for checksum, paths in sorted(combined_hashes.items()):
        if len(paths) < 2:
            continue
        source_paths = [item for item in paths if item.startswith("assets/art/")]
        runtime_paths = [item for item in paths if item.startswith("godot/art/")]
        mapped = any(item.get("source") in source_paths and item["file"] in runtime_paths
                     for item in runtime_entries)
        duplicates.append({"sha256": checksum, "files": paths,
                           "reason": "Manifest-registered source/runtime byte copy" if mapped else
                                     "Same hash; retain pending purpose and visual-reference review"})
    old_count = len(previous)
    old_bytes = sum(int(entry.get("bytes", 0)) for entry in previous.values())
    INVENTORY.write_text(json.dumps({
        "format": 2,
        "scope": "Current production source collections and required runtime inputs; Godot runtime mirror listed separately. Retired design archives and unused legacy exports are excluded.",
        "comparison": {"previousSourceFiles": old_count, "currentSourceFiles": len(source_entries),
                       "sourceFileDelta": len(source_entries) - old_count, "previousBytes": old_bytes,
                       "currentBytes": sum(item["bytes"] for item in source_entries)},
        "sourceFiles": source_entries,
        "runtimeFiles": runtime_entries,
        "duplicateHashes": duplicates,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail on stale inventory entries")
    parser.add_argument("--update-inventory", action="store_true", help="regenerate ART_INVENTORY.json from both manifests")
    parser.add_argument("--output", type=Path, default=ROOT / "build/art-asset-report.json")
    args = parser.parse_args()
    try:
        inventory, errors = check_inventory()
        runtime, runtime_errors = runtime_files()
        errors.extend(runtime_errors)
        source_files = {str(path.relative_to(ROOT)).replace("\\", "/"): path
                        for path in SOURCE_ROOT.rglob("*") if path.is_file() and path.name != INVENTORY.name}
        if args.update_inventory:
            write_inventory(source_files, runtime, inventory)
            inventory, errors = check_inventory()
            runtime, runtime_errors = runtime_files()
            errors.extend(runtime_errors)
        stale = sorted(set(inventory) - set(source_files))
        unlisted = sorted(set(source_files) - set(inventory))
        hashes: dict[str, list[str]] = defaultdict(list)
        for relative, path in source_files.items():
            hashes[digest(path)].append(relative)
        duplicates = [paths for paths in hashes.values() if len(paths) > 1]
        report = {
            "format": 1,
            "source": {"files": len(source_files), "bytes": sum(p.stat().st_size for p in source_files.values())},
            "inventory": {"files": len(inventory), "bytes": sum(int(e.get("bytes", 0)) for e in inventory.values())},
            "runtimeManifest": {"files": len(runtime), "bytes": sum((RUNTIME_ROOT / p.removeprefix("art/")).stat().st_size for p in runtime if (RUNTIME_ROOT / p.removeprefix("art/")).is_file())},
            "categories": {name: sum(1 for p in source_files if category(ROOT / p) == name) for name in ("legacy", "candidate", "authoring", "archive")},
            "inventoryStale": stale,
            "inventoryUnlisted": unlisted,
            "duplicateHashes": duplicates,
            "errors": errors,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        if errors or (args.check and (stale or unlisted)):
            if stale:
                print(f"Inventory has {len(stale)} stale entries", file=sys.stderr)
            if unlisted:
                print(f"Inventory has {len(unlisted)} unlisted files", file=sys.stderr)
            for error in errors:
                print(error, file=sys.stderr)
            return 1
        source_bytes = sum(path.stat().st_size for path in source_files.values())
        runtime_bytes = sum((RUNTIME_ROOT / path.removeprefix("art/")).stat().st_size for path in runtime)
        print(f"Art assets: {len(source_files)} source files ({source_bytes:,} bytes), "
              f"{len(runtime)} runtime assets ({runtime_bytes:,} bytes); report {args.output}")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Art asset validation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
