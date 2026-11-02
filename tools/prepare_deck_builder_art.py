"""Normalize approved original ImageGen deck UI assets to 1080p canvases.

Only trims empty alpha margins and normalizes dimensions, preserving originals.
No recoloring, new painting or background removal is performed.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import shutil
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
COLLECTION = ROOT / "assets/art/godot-2d-v2/deck-builder-1080p-v1"
SPECS = {
    "background": ((1920, 1080), None),
    "detail-panel": ((360, 760), [56, 66, 56, 66]),
    "grid-panel": ((640, 640), [70, 70, 70, 70]),
    "list-entry": ((1200, 112), [100, 48, 100, 48]),
    "empty-slot": ((150, 200), None),
    "quantity-badge": ((64, 26), None),
    "drop-highlight": ((150, 200), [32, 32, 32, 32]),
}

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    records = json.loads((COLLECTION / "generation-prompts.json").read_text(encoding="utf-8-sig"))
    if {row["name"] for row in records} != set(SPECS):
        raise ValueError("All seven original deck assets are required")
    for folder in ("source", "export"):
        (COLLECTION / folder).mkdir(parents=True, exist_ok=True)
    assets = []
    for record in records:
        name = record["name"]
        source = COLLECTION / "source" / (name + "-original.png")
        if not source.exists():
            shutil.copyfile(record["source"], source)
        with Image.open(source) as original:
            image = original.convert("RGBA")
            alpha = image.getchannel("A")
            bounds = alpha.point(lambda a: 255 if a > 8 else 0).getbbox()
            if bounds is None:
                raise ValueError("Empty artwork: " + name)
            if name != "background" and alpha.getextrema()[0] != 0:
                raise ValueError("ImageGen must provide real transparency: " + name)
            if name == "drop-highlight" and alpha.getpixel((image.width // 2, image.height // 2)) != 0:
                raise ValueError("Drop outline must have a transparent center")
            dimensions, margins = SPECS[name]
            component = image.crop(bounds).resize(dimensions, Image.Resampling.NEAREST)
            canvas = Image.new("RGBA", (1920, 1080))
            position = ((1920 - dimensions[0]) // 2, (1080 - dimensions[1]) // 2)
            canvas.paste(component, position)
            export = COLLECTION / "export" / (name + "-1080p.png")
            canvas.save(export)
            entry = dict(id="c2d.v2.deck." + name, file=export.relative_to(COLLECTION).as_posix(),
                         source=source.relative_to(COLLECTION).as_posix(), size=[1920, 1080],
                         originalSize=list(image.size), sourceBounds=list(bounds),
                         targetSize=list(dimensions), region=[*position, *dimensions],
                         transparent=name != "background", sampling="nearest",
                         sha256=digest(export), sourceSha256=digest(source),
                         origin="OpenAI built-in ImageGen", runtimeIntegrated=True,
                         normalization="User-approved dimension normalization and empty-alpha trim only")
            if margins is not None:
                entry["nineSlice"] = margins
            assets.append(entry)
    manifest = dict(version=1, designSize=[1920, 1080], mainDeckSize=40, sideDeckSize=10,
                    style="C refined pixel fantasy", assets=assets)
    (COLLECTION / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Prepared seven original deck assets; all runtime exports are 1920x1080 RGBA")

if __name__ == "__main__":
    main()
