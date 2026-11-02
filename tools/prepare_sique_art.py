"""Reproduce transparent Sique illustrations from the 16 supplied JPEG originals.

Requires Pillow. No generative image operation is performed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
BATCH = ROOT / "assets/art/sique-20261020"
MAPPING = {
    "17BEFF374B770A63726A15D41EE4989E": ("sique-parsley", "味契·欧芹"),
    "5786E479ED805F4C5A7542362ECC444D": ("sique-mushroom-soup", "味契·奶油蘑菇浓汤"),
    "7230DE37440586EEADA84E040BBB3287": ("sique-chili", "味契·辣椒"),
    "8996E989FAC5DC2009620132D4629F25": ("sique-vinegar", "味契·香醋"),
    "9CF664CD78F632A9877E3E0353E6A6BE": ("sique-salt", "味契·海盐"),
    "9E66331A8F097F2E7198AD651AA6D555": ("sique-ingredients", "味契·原料召来"),
    "C0861238A9675BA5D3263F1CF155DC76": ("sique-cauldron", "味契·魔力坩埚"),
    "D8AD87A172CDA719D9257CB9B7A7EB62": ("sique-woodpile", "味契·柴堆"),
    "DF1EEAAC1C9F882244791946E66E7098": ("sique-campfire", "味契·篝火"),
    "E42C1620D308CAB12834E5620722C85F": ("sique-divine-stew", "味契·神恩炖肉"),
    "ECBFB61D4CA3610ADC661014183999B2": ("sique-sugar", "味契·冰糖"),
    "F3198D1EAE79031285C21BB9B84798D8": ("sique-aroma-lure", "味契·飘香诱惑"),
    "F461857F8870A11FB905DB5B7711F0A3": ("sique-flavor-shift", "味契·风味转化"),
    "F810FE21182B8B04CB60B37180C01968": ("sique-pancakes", "味契·糖浆煎饼"),
    "FAD884B74490E63F1E9A6D1C0CB974FB": ("sique-refire", "味契·烈火回炉"),
    "FC90017D1C7F281972F41341E9CE4E0B": ("sique-boiling-surge", "味契·沸汤滚袭"),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(originals: Path, register: bool = True) -> None:
    from PIL import Image, ImageChops, ImageDraw
    exports = BATCH / "export"
    sources = BATCH / "source"
    exports.mkdir(parents=True, exist_ok=True)
    sources.mkdir(parents=True, exist_ok=True)
    entries = []
    for filename, (card_id, name) in sorted(MAPPING.items(), key=lambda pair: pair[1][0]):
        original = originals / (filename + ".jpg")
        source = sources / original.name
        if original.resolve() != source.resolve():
            shutil.copyfile(original, source)
        image = Image.open(source).convert("RGB")
        if image.size != (1536, 1536):
            raise ValueError(f"Unexpected original dimensions: {source}")
        # The supplied images have a near-black matte and a lower-right source mark.
        # Mark removal is confined to its empty corner, outside the illustration.
        red, green, blue = image.split()
        maximum = ImageChops.lighter(ImageChops.lighter(red, green), blue)
        alpha = maximum.point(lambda value: max(0, min(255, (value - 24) * 255 // 12)))
        ImageDraw.Draw(alpha).rectangle((1360, 1380, 1535, 1535), fill=0)
        image.putalpha(alpha)
        bounds = alpha.point(lambda value: 255 if value >= 48 else 0).getbbox()
        image = image.crop(bounds)
        image.thumbnail((900, 900), Image.Resampling.NEAREST)
        canvas = Image.new("RGBA", (1024, 1024))
        canvas.paste(image, ((1024-image.width)//2, (1024-image.height)//2))
        output = exports / (card_id + ".png")
        canvas.save(output, optimize=True)
        entries.append({"id": "illustration." + card_id, "cardId": card_id, "name": name,
                        "file": "export/" + output.name, "source": "source/" + source.name,
                        "sourceSha256": sha256(source), "sha256": sha256(output), "size": [1024, 1024],
                        "sourceCrop": list(bounds), "sampling": "nearest", "transparent": True,
                        "origin": "User-supplied Sique JPEG; deterministic matte removal and crop"})
    manifest = {"format": 1, "collection": "Sique supplied art for Alpha v2.5.1", "license": "User-supplied project art; original provenance retained in source filenames and hashes",
                "processing": {"script": "tools/prepare_sique_art.py", "sourceSize": [1536, 1536],
                               "alpha": "clamp((max(R,G,B)-24)/12,0,1)", "removeSourceMark": [1360, 1380, 176, 156],
                               "cropAlphaThreshold": 48, "maximumSubjectSize": [900, 900], "canvasSize": [1024, 1024], "resample": "nearest"},
                "assets": entries}
    manifest_path = BATCH / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if register:
        runtime_path = ROOT / "godot/art/assets.json"
        runtime = json.loads(runtime_path.read_text(encoding="utf-8"))
        runtime["assets"] = [entry for entry in runtime["assets"] if entry["id"] not in {e["id"] for e in entries}]
        for entry in entries:
            export = BATCH / entry["file"]
            destination = ROOT / "godot/art/card" / export.name
            shutil.copyfile(export, destination)
            runtime["assets"].append({key: entry[key] for key in ("id", "sha256", "size", "sampling", "origin")} |
                                     {"file": "res://art/card/" + export.name, "source": export.relative_to(ROOT).as_posix()})
        relative = manifest_path.relative_to(ROOT).as_posix()
        additional = [entry for entry in runtime.get("additionalSourceManifests", []) if entry["file"] != relative]
        additional.append({"file": relative, "sha256": sha256(manifest_path)})
        runtime["additionalSourceManifests"] = additional
        runtime["assets"].sort(key=lambda entry: entry["id"])
        runtime_path.write_text(json.dumps(runtime, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared and registered {len(entries)} Sique illustrations")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--originals", type=Path, default=BATCH / "source")
    parser.add_argument("--no-register", action="store_true")
    args = parser.parse_args()
    prepare(args.originals, not args.no_register)
