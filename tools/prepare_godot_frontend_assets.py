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
METADATA = (
    "size", "nineSlice", "safeRect", "artRect", "nameRect", "effectRect",
    "rarityAnchor", "valueAnchors", "cardType", "view", "anchor", "layer",
    "overlay", "sampling", "origin", "license",
)
REQUIRED = {
    "font.cjk", "font.cjk.license", "c2d.background.realm", "c2d.brand.logo",
    "c2d.card.back.compact", "c2d.card.missing-art", "c2d.zone.slot",
    "c2d.overlay.target", "c2d.panel.surface",
    *(f"c2d.button.primary.{state}" for state in
      ("normal", "hover", "pressed", "focus", "disabled")),
    *(f"c2d.card.{view}.{kind}" for view in ("field", "hand", "builder", "detail")
      for kind in ("analysis", "talisman", "incantation", "formation", "rune")),
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
    # Native templates cover each target card size. Large AI card bodies are
    # omitted to keep small cards readable and anonymous cards type-neutral.
    return not asset_id.startswith(("c2d.fx.", "c2d.result.")) and not (
        asset_id.startswith("c2d.card.") and asset_id.endswith(".ai")
    )


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
    original = read_json(SOURCE_MANIFEST)
    entries = []
    for asset in original["assets"]:
        if not selected(asset["id"]):
            continue
        original_file = confined_path(COLLECTION, asset["file"])
        relative = PurePosixPath(asset["file"]).relative_to("export/c-pixel-v1").as_posix()
        metadata = {key: asset[key] for key in METADATA if key in asset}
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
    manifest = {
        "format": 1,
        "collection": "WizardCard C pixel fantasy frontend foundation",
        "sourceManifest": SOURCE_MANIFEST.relative_to(ROOT).as_posix(),
        "sourceManifestSha256": sha256(SOURCE_MANIFEST),
        "palette": original["nativePalette"],
        "assets": sorted(entries, key=lambda entry: entry["id"]),
    }
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
            if "nineSlice" in entry:
                margins = entry["nineSlice"]
                if (len(margins) != 4 or any(type(v) is not int or v < 0 for v in margins)
                        or margins[0] + margins[2] >= dimensions[0]
                        or margins[1] + margins[3] >= dimensions[1]):
                    raise ValueError(f"Invalid nine-slice margins: {asset_id}")
    missing = REQUIRED - ids
    if missing:
        raise ValueError(f"Missing frontend foundation assets: {', '.join(sorted(missing))}")
    if len([asset_id for asset_id in ids if asset_id.startswith("illustration.")]) != 30:
        raise ValueError("Runtime collection must preserve all 30 illustrations")
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
