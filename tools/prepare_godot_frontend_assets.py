"""Copy selected original art into Godot; validate committed runtime art with --check.

Synchronization requires the local C pixel collection. Verification deliberately
does not: CI and clean checkouts use only godot/art/assets.json and its files.
No image resizing, recoloring, rasterization, or source edits take place.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
GODOT = ROOT / "godot"
ART = GODOT / "art"
COLLECTION = ROOT / "assets/art/godot-2d-v1"
SOURCE_MANIFEST = COLLECTION / "manifests/c-pixel-v1/manifest.json"
RUNTIME_MANIFEST = ART / "assets.json"
MENU_COLLECTION = ROOT / "assets/art/godot-2d-v2/main-menu-1080p-v1"
MENU_MANIFEST = MENU_COLLECTION / "manifest.json"
UI_COLLECTION = ROOT / "assets/art/godot-2d-v2/ui-components-1080p-v1"
UI_MANIFEST = UI_COLLECTION / "manifest.json"
FONT_COLLECTION = ROOT / "assets/art/fonts/noto-sans-cjk-sc"
FONT_MANIFEST = FONT_COLLECTION / "manifest.json"
DECK_COLLECTION = ROOT / "assets/art/godot-2d-v2/deck-builder-1080p-v1"
DECK_MANIFEST = DECK_COLLECTION / "manifest.json"
PREVIEW_COLLECTION = ROOT / "assets/art/godot-2d-v2/card-illustrations-v2-20261011"
PREVIEW_MANIFEST = PREVIEW_COLLECTION / "manifest.json"
METADATA = (
    "size", "nineSlice", "safeRect", "artRect", "nameRect", "effectRect",
    "rarityAnchor", "valueAnchors", "cardType", "view", "anchor", "layer",
    "overlay", "sampling", "origin", "license", "region", "textSafeRect", "targetSize",
    "licenseAsset", "fontFamily", "fontStyle", "fontWeight", "version", "copyright", "upstream",
)
REQUIRED = {
    "font.cjk", "font.cjk.bold", "font.cjk.black", "font.cjk.license", "font.cjk.weights.license", "c2d.background.realm", "c2d.brand.logo",
    "c2d.card.back.ai", "c2d.card.missing-art", "c2d.zone.slot",
    "c2d.overlay.target", "c2d.panel.surface",
    "c2d.v2.menu.background", "c2d.v2.menu.logo",
    *("c2d.v2.deck." + component for component in
      ("background", "detail-panel", "grid-panel", "list-entry", "empty-slot", "quantity-badge", "drop-highlight")),
    *("c2d.v2.ui." + component for component in
      ("panel", "field", "slider-track", "slider-knob", "toggle-on", "toggle-off")),
    *(f"c2d.v2.menu.button-{state}" for state in
      ("normal", "hover", "pressed", "focus", "disabled")),
    *(f"c2d.button.primary.{state}" for state in
      ("normal", "hover", "pressed", "focus", "disabled")),
    *(f"c2d.card.detail.{kind}.ai" for kind in
      ("analysis", "talisman", "incantation", "formation", "rune")),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def confined_path(base: Path, relative: str) -> Path:
    """Accept portable relative files and prevent traversal or symlink escapes."""
    if not isinstance(relative, str) or not relative or "\\" in relative or ":" in relative:
        raise ValueError(f"Invalid relative path: {relative!r}")
    parts = relative.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise ValueError(f"Invalid relative path: {relative!r}")
    path = PurePosixPath(relative)
    if path.is_absolute():
        raise ValueError(f"Absolute path is forbidden: {relative}")
    resolved = (base / path).resolve()
    if not resolved.is_relative_to(base.resolve()):
        raise ValueError(f"Path escapes its root: {relative}")
    return resolved


def selected(asset_id: str) -> bool:
    # All views use the original AI frames. Historical simplified templates
    # remain in the source collection, but are not selected for runtime use.
    if asset_id == "c2d.card.back.compact":
        return False
    if asset_id.startswith(("c2d.card.field.", "c2d.card.hand.",
                            "c2d.card.builder.", "c2d.card.detail.")):
        return asset_id.endswith(".ai")
    return not asset_id.startswith(("c2d.fx.", "c2d.result."))


def copied_entry(asset_id: str, source: Path, relative: str, expected_hash: str | None = None,
                 metadata: dict | None = None) -> dict:
    digest = sha256(source)
    if expected_hash is not None and digest != expected_hash:
        raise ValueError(f"Original source hash changed: {source.relative_to(ROOT)}")
    destination = confined_path(ART, relative)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    return dict(metadata or {}, id=asset_id, file=f"res://art/{relative}",
                source=source.relative_to(ROOT).as_posix(), sha256=digest)


def synchronize() -> None:
    previous = read_json(RUNTIME_MANIFEST) if RUNTIME_MANIFEST.exists() else {}
    original = read_json(SOURCE_MANIFEST)
    entries = []
    for asset in original["assets"]:
        if not selected(asset["id"]):
            continue
        original_file = confined_path(COLLECTION, asset["file"])
        relative = PurePosixPath(asset["file"]).relative_to("export/c-pixel-v1").as_posix()
        metadata = {key: asset[key] for key in METADATA if key in asset}
        if asset["id"].startswith("c2d.card.detail.") and asset["id"].endswith(".ai"):
            # Safe pixel rectangles measured against the unchanged 1086x1448
            # original, rather than the approximate authoring anchors.
            kind = asset["cardType"]
            strip_y = 817 if kind == "talisman" else 800
            metadata["runtimeLayout"] = {
                "name": [150, 112, 650, 68],
                "art": [119, 228, 850, 534],
                "type": [238, strip_y + 12, 124, 46],
                "speed": [415, strip_y + 12, 340, 46],
                "rarity": [857, strip_y + 1, 112, 70],
                "effect": [148, 955, 785, 200 if kind == "formation" else 235],
                "identifier": [783, 1333, 179, 33],
                "values": [[950, 147], [144, 1288]],
            }
            metadata["runtimeViews"] = ["field", "hand", "builder", "detail"]
        metadata["authoringSource"] = (
            COLLECTION.relative_to(ROOT) / asset["source"]
        ).as_posix()
        entries.append(copied_entry(asset["id"], original_file, relative,
                                    asset["sha256"], metadata))
    illustrations_file = SOURCE_MANIFEST.parent / original["existingIllustrations"]
    illustrations = read_json(illustrations_file)
    if len(illustrations) != 30:
        raise ValueError("Expected the preserved collection of 30 illustrations")
    for illustration in illustrations:
        source = confined_path(ROOT, illustration["file"])
        entries.append(copied_entry(f"illustration.{illustration['id']}", source,
                                    f"illustrations/{illustration['id']}{source.suffix}",
                                    illustration["sha256"],
                                    {"sampling": "linear", "origin": "Existing WizardCard illustration"}))
    entries.append(copied_entry("font.cjk", ROOT / original["font"],
                                "fonts/NotoSansCJKsc-Regular.otf",
                                metadata={"license": "SIL Open Font License 1.1",
                                          "licenseAsset": "font.cjk.license"}))
    entries.append(copied_entry("font.cjk.license", ROOT / original["fontLicense"],
                                "fonts/OFL.txt", metadata={"license": "SIL Open Font License 1.1"}))
    fonts = read_json(FONT_MANIFEST)
    entries.append(copied_entry("font.cjk.weights.license", FONT_COLLECTION / fonts["licenseFile"],
                                "fonts/NotoSansCJK-OFL.txt", fonts["licenseSha256"],
                                {"license": fonts["license"], "upstream": fonts["licenseUrl"]}))
    for asset in fonts["assets"]:
        source = confined_path(FONT_COLLECTION, asset["file"])
        metadata = {key: asset[key] for key in METADATA if key in asset}
        entries.append(copied_entry(asset["id"], source, "fonts/" + source.name,
                                    asset["sha256"], metadata))
    for asset in read_json(MENU_MANIFEST)["assets"]:
        source = confined_path(MENU_COLLECTION, asset["file"])
        metadata = {key: asset[key] for key in METADATA if key in asset}
        metadata["authoringSource"] = confined_path(MENU_COLLECTION, asset["source"]).relative_to(ROOT).as_posix()
        entries.append(copied_entry(asset["id"], source, "menu-v2/" + source.name,
                                    asset["sha256"], metadata))
    for asset in read_json(UI_MANIFEST)["assets"]:
        source = confined_path(UI_COLLECTION, asset["file"])
        metadata = {key: asset[key] for key in METADATA if key in asset}
        metadata["authoringSource"] = confined_path(UI_COLLECTION, asset["source"]).relative_to(ROOT).as_posix()
        entries.append(copied_entry(asset["id"], source, "ui-v2/" + source.name,
                                    asset["sha256"], metadata))
    for asset in read_json(DECK_MANIFEST)["assets"]:
        source = confined_path(DECK_COLLECTION, asset["file"])
        metadata = {key: asset[key] for key in METADATA if key in asset}
        metadata["authoringSource"] = confined_path(DECK_COLLECTION, asset["source"]).relative_to(ROOT).as_posix()
        entries.append(copied_entry(asset["id"], source, "deck-v2/" + source.name,
                                    asset["sha256"], metadata))
    for asset in read_json(PREVIEW_MANIFEST)["assets"]:
        source = confined_path(PREVIEW_COLLECTION, asset["file"])
        metadata = {key: asset[key] for key in METADATA if key in asset}
        entries.append(copied_entry(asset["id"], source, "illustrations-v2/" + source.name,
                                    asset["sha256"], metadata))
    manifest = {
        "format": 1,
        "collection": "WizardCard C pixel fantasy frontend foundation",
        "sourceManifest": SOURCE_MANIFEST.relative_to(ROOT).as_posix(),
        "sourceManifestSha256": sha256(SOURCE_MANIFEST),
        "additionalSourceManifests": [{"file": MENU_MANIFEST.relative_to(ROOT).as_posix(),
                                       "sha256": sha256(MENU_MANIFEST)},
                                      {"file": UI_MANIFEST.relative_to(ROOT).as_posix(),
                                       "sha256": sha256(UI_MANIFEST)},
                                      {"file": FONT_MANIFEST.relative_to(ROOT).as_posix(),
                                       "sha256": sha256(FONT_MANIFEST)},
                                      {"file": DECK_MANIFEST.relative_to(ROOT).as_posix(),
                                       "sha256": sha256(DECK_MANIFEST)},
                                      {"file": PREVIEW_MANIFEST.relative_to(ROOT).as_posix(),
                                       "sha256": sha256(PREVIEW_MANIFEST)}],
        "palette": original["nativePalette"],
        "assets": sorted(entries, key=lambda entry: entry["id"]),
    }
    # Later registered batches (Storm, Sique) are independent of the foundation.
    # Preserve them when refreshing that foundation instead of dropping their IDs.
    generated_ids = {entry["id"] for entry in manifest["assets"]}
    for entry in previous.get("assets", []):
        if entry["id"] not in generated_ids:
            runtime = confined_path(ART, entry["file"].removeprefix("res://art/"))
            if sha256(runtime) != entry["sha256"]:
                raise ValueError("Registered supplemental asset changed: " + entry["id"])
            manifest["assets"].append(entry)
    known_sources = {entry["file"] for entry in manifest["additionalSourceManifests"]}
    manifest["additionalSourceManifests"].extend(entry for entry in previous.get("additionalSourceManifests", [])
                                                 if entry["file"] not in known_sources)
    manifest["assets"].sort(key=lambda entry: entry["id"])
    RUNTIME_MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                                encoding="utf-8")


def verify() -> tuple[int, int]:
    manifest = read_json(RUNTIME_MANIFEST)
    if manifest.get("format") != 1 or not isinstance(manifest.get("assets"), list):
        raise ValueError("Invalid runtime asset manifest")
    ids, paths, byte_count = set(), set(), 0
    for entry in manifest["assets"]:
        asset_id = entry["id"]
        if not isinstance(asset_id, str) or not asset_id or asset_id in ids:
            raise ValueError(f"Duplicate or invalid asset ID: {asset_id!r}")
        ids.add(asset_id)
        resource = entry["file"]
        if not isinstance(resource, str) or not resource.startswith("res://art/"):
            raise ValueError(f"Asset must stay in res://art: {asset_id}")
        path = confined_path(ART, resource.removeprefix("res://art/"))
        if path in paths:
            raise ValueError(f"Duplicate runtime file: {resource}")
        paths.add(path)
        if sha256(path) != entry["sha256"]:
            raise ValueError(f"Runtime asset hash mismatch: {asset_id}")
        # Provenance is repository-relative, but it need not exist in CI.
        confined_path(ROOT, entry["source"])
        if "authoringSource" in entry:
            confined_path(ROOT, entry["authoringSource"])
        byte_count += path.stat().st_size
        if path.suffix.lower() == ".png":
            header = path.read_bytes()[:24]
            if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
                raise ValueError(f"Invalid PNG: {resource}")
            dimensions = list(struct.unpack(">II", header[16:24]))
            if "size" in entry and dimensions != entry["size"]:
                raise ValueError(f"PNG dimensions disagree with manifest: {asset_id}")
            if "runtimeLayout" in entry:
                layout = entry["runtimeLayout"]
                for key in ("name", "art", "type", "speed", "rarity", "effect", "identifier"):
                    rectangle = layout[key]
                    if (len(rectangle) != 4 or any(type(v) is not int for v in rectangle)
                            or min(rectangle[:2]) < 0 or min(rectangle[2:]) <= 0
                            or rectangle[0] + rectangle[2] > dimensions[0]
                            or rectangle[1] + rectangle[3] > dimensions[1]):
                        raise ValueError(f"Invalid card layout {key}: {asset_id}")
            if "region" in entry:
                region = entry["region"]
                if (len(region) != 4 or any(type(v) is not int for v in region)
                        or min(region[:2]) < 0 or min(region[2:]) <= 0
                        or region[0] + region[2] > dimensions[0]
                        or region[1] + region[3] > dimensions[1]):
                    raise ValueError(f"Invalid texture region: {asset_id}")
            if "nineSlice" in entry:
                margins = entry["nineSlice"]
                if (len(margins) != 4 or any(type(v) is not int or v < 0 for v in margins)
                        or margins[0] + margins[2] >= dimensions[0]
                        or margins[1] + margins[3] >= dimensions[1]):
                    raise ValueError(f"Invalid nine-slice margins: {asset_id}")
    missing = REQUIRED - ids
    if missing:
        raise ValueError(f"Missing frontend foundation assets: {', '.join(sorted(missing))}")
    from catalog_source import load_catalog
    for card in load_catalog()["cards"]:
        if "illustration." + card.get("illustrationId", card["id"]) not in ids:
            raise ValueError("Missing official card illustration: " + card["id"])
    previews = read_json(GODOT / "data/card-previews-v2.json")
    if previews.get("usage") != "illustrated-preview" or len(previews["cards"]) != 32:
        raise ValueError("Expected 32 display-only 2.0 preview cards")
    card_ids = set()
    for card in previews["cards"]:
        if not card["id"].startswith("v2-") or card["id"] in card_ids:
            raise ValueError("Duplicate or invalid 2.0 preview card id")
        card_ids.add(card["id"])
        if "illustration." + card["illustrationId"] not in ids:
            raise ValueError("Missing 2.0 card illustration: " + card["id"])
        if card["typeKey"] not in ("analysis", "incantation", "talisman", "formation"):
            raise ValueError("Unsupported 2.0 card frame: " + card["id"])
    return len(ids), byte_count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Validate committed runtime files only")
    args = parser.parse_args()
    try:
        if not args.check:
            synchronize()
        count, size = verify()
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Frontend asset validation failed: {error}", file=sys.stderr)
        return 1
    print(f"Frontend assets verified: {count} assets, {size:,} bytes ({size / 1024**2:.2f} MiB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
