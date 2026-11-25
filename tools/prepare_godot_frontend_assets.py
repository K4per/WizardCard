"""Validate committed Godot artwork or restore matching bytes from the recorded sources."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
GODOT = ROOT / "godot"
ART = GODOT / "art"
FONT_IDS = {"font.cjk", "font.cjk.bold", "font.cjk.black", "font.cjk.license", "font.cjk.weights.license"}
BRAND_FILES = {
    "branding.app.icon": "res://art/branding/wizardcard-app-v1.png",
    "c2d.v2.menu.logo": "res://art/menu-v2/logo-1080p.png",
}
TEXTURE_FILES = {**BRAND_FILES, "fx.damage-border": "res://art/fx/damage-border.png"}
FONT_SUFFIXES = {".otf", ".txt"}
IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".webp", ".svg", ".ico", ".bmp", ".tga", ".dds", ".ktx", ".avif"}


def read_json(path):
    return json.loads(path.read_bytes())


def approved_entries():
    manifest = read_json(ART / "assets.json")
    expected = FONT_IDS | TEXTURE_FILES.keys()
    for path in (ROOT / "assets/release-v260/cards.json", ROOT / "assets/cards.json",
                 GODOT / "data/card-previews-v2.json"):
        for card in read_json(path)["cards"]:
            expected.add("illustration." + card.get("illustrationId", card["id"]))
    entries = manifest["assets"]
    ids = {entry["id"] for entry in entries}
    invalid = {name for name in ids if name not in FONT_IDS and name not in TEXTURE_FILES and not name.startswith("illustration.")}
    if manifest["format"] != 1 or len(entries) != len(ids) or expected - ids or invalid:
        raise ValueError(f"Invalid runtime art manifest: missing={sorted(expected - ids)}, invalid={sorted(invalid)}")
    paths = {}
    for entry in entries:
        resource = entry["file"]
        if not resource.startswith("res://art/"):
            raise ValueError(f"Unexpected runtime art path: {resource}")
        path = (GODOT / resource.removeprefix("res://")).resolve()
        asset_id = entry["id"]
        if asset_id in TEXTURE_FILES:
            valid = resource == TEXTURE_FILES[asset_id]
        elif asset_id in FONT_IDS:
            valid = path.parent.name == "fonts" and path.suffix in FONT_SUFFIXES
        else:
            valid = path.parent.name in {"card", "cards"} and path.suffix in IMAGE_SUFFIXES and not path.name.startswith("c2d_")
        if (not valid or path.parent.parent != ART or path in paths or
                resource != "res://" + path.relative_to(GODOT).as_posix()):
            raise ValueError(f"Invalid or duplicate art resource: {resource}")
        paths[path] = entry
    return paths


def check_art(restore=False):
    entries = approved_entries()
    allowed = set(entries) | {ART / "assets.json", ART / "README.md"}
    for path, entry in entries.items():
        if path.suffix != ".txt":
            allowed.add(Path(str(path) + ".import"))
        digest = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else ""
        if digest == entry["sha256"]:
            continue
        if restore:
            source = (ROOT / entry["source"]).resolve()
            if (source.is_relative_to(ROOT / "assets") and source.is_file() and
                    hashlib.sha256(source.read_bytes()).hexdigest() == entry["sha256"]):
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, path)
                continue
        raise ValueError(f"Restore the committed bytes of {entry['file']} (SHA-256 {entry['sha256']})")
    actual = {path for path in ART.rglob("*") if path.is_file()}
    unexpected = actual - allowed
    if unexpected:
        raise ValueError(f"Unexpected runtime art files: {sorted(str(p.relative_to(ART)) for p in unexpected)}")
    return len(entries)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Check the committed runtime files without writing")
    args = parser.parse_args()
    try:
        print(f"Verified {check_art(restore=not args.check)} Godot illustration, effect, branding, font and license entries.")
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(str(error)) from error
