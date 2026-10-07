"""Validate the supplied art/audio pack without editing source assets (Python 3.9+)."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def validate(assets):
    art = assets / "art"
    manifest_file = art / "ui/a-gilded-v2/manifest.json"
    manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
    errors, ids, originals = [], set(), 0
    for entry in manifest["assets"]:
        asset_id = entry["id"]
        if asset_id in ids:
            errors.append(f"duplicate asset ID: {asset_id}")
        ids.add(asset_id)
        path = (art / entry["file"]).resolve()
        if not path.is_relative_to(art.resolve()) or not path.is_file():
            errors.append(f"missing or invalid path: {asset_id}")
            continue
        data = path.read_bytes()
        if data[:8] != b"\x89PNG\r\n\x1a\n" or len(data) < 24:
            errors.append(f"not PNG: {asset_id}")
            continue
        width, height = struct.unpack(">II", data[16:24])
        if [width, height] != entry["size"]:
            errors.append(f"wrong dimensions: {asset_id}")
        if "insets" in entry:
            left, top, right, bottom = entry["insets"]
            if min(left, top, right, bottom) < 0 or left + right >= width or top + bottom >= height:
                errors.append(f"invalid nine-slice: {asset_id}")
        if "sha256" in entry:
            originals += 1
            if hashlib.sha256(data).hexdigest() != entry["sha256"]:
                errors.append(f"original checksum mismatch: {asset_id}")
        if "source" in entry and not (art / entry["source"]).is_file():
            errors.append(f"missing component source: {asset_id}")
    required = {"analysis_cost", "cast_cost", "rank", "speed", "body", "ring_slot", "capacity", "mana_income", "life", "mana", "load"}
    errors.extend(f"missing required icon: {key}" for key in sorted(required - ids))
    artwork = json.loads((art / "runtime.json").read_text(encoding="utf-8"))["cards"]
    for card, file in artwork.items():
        if not (art / file).is_file():
            errors.append(f"missing card illustration: {card}")
    for path in [art / "branding/wizardcard-logo-v1.png", assets / "fonts/NotoSansCJKsc-Regular.otf"]:
        if not path.is_file():
            errors.append(f"missing UI dependency: {path.name}")
    audio = json.loads((assets / "audio/manifest.json").read_text(encoding="utf-8"))
    for entry in audio["events"]:
        path = assets / "audio" / entry["file"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            errors.append(f"audio checksum mismatch: {entry['file']}")
    return {"ok": not errors, "artPack": manifest["version"], "artAssets": len(ids),
            "originalChecksums": originals, "cardIllustrations": len(artwork),
            "audioFiles": len({entry["file"] for entry in audio["events"]}), "audioEvents": len(audio["events"]),
            "manifestSha256": hashlib.sha256(manifest_file.read_bytes()).hexdigest(), "errors": errors}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, default=Path(__file__).resolve().parents[1] / "assets")
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args()
    report = validate(arguments.assets.resolve())
    text = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if arguments.report:
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(text, encoding="utf-8")
    print(text)
    raise SystemExit(0 if report["ok"] else 1)
