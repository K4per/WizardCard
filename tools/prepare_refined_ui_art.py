"""Normalize approved ImageGen UI originals onto 1920x1080 alpha canvases.

Only trims empty alpha margins, resizes dimensions with nearest sampling, and
centers the unchanged artwork. Never recolors, paints, or removes subjects.
Run with a JSON list of name/source/prompt records; preserve originals in source/.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
COLLECTION = ROOT / "assets/art/godot-2d-v2/ui-components-1080p-v1"
SPECS = {
    "panel": ((440, 280), [62, 70, 62, 70]),
    "field": ((500, 72), [60, 30, 60, 30]),
    "slider-track": ((520, 32), [60, 14, 60, 14]),
    "slider-knob": ((36, 44), None),
    "toggle-on": ((104, 44), None),
    "toggle-off": ((104, 44), None),
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sources", type=Path)
    records = json.loads(parser.parse_args().sources.read_text(encoding="utf-8-sig"))
    if {row["name"] for row in records} != set(SPECS):
        raise ValueError("The six UI component originals are required")
    (COLLECTION / "source").mkdir(parents=True, exist_ok=True)
    (COLLECTION / "export").mkdir(parents=True, exist_ok=True)
    assets = []
    for row in records:
        name = row["name"]
        source = COLLECTION / "source" / f"{name}-original.png"
        generated_source = Path(row["source"])
        if generated_source.is_file() and generated_source.resolve() != source.resolve():
            shutil.copyfile(generated_source, source)
        elif not source.is_file():
            raise FileNotFoundError(f"Missing generated or preserved original: {name}")
        with Image.open(source) as original:
            image = original.convert("RGBA")
            bounds = image.getchannel("A").point(lambda alpha: 255 if alpha > 8 else 0).getbbox()
            if bounds is None or image.getchannel("A").getextrema()[0] != 0:
                raise ValueError(f"Missing transparent background: {name}")
            dimensions, margins = SPECS[name]
            component = image.crop(bounds).resize(dimensions, Image.Resampling.NEAREST)
            canvas = Image.new("RGBA", (1920, 1080))
            position = ((1920 - dimensions[0]) // 2, (1080 - dimensions[1]) // 2)
            canvas.paste(component, position)
            export = COLLECTION / "export" / f"{name}-1080p.png"
            canvas.save(export)
            entry = {
                "id": "c2d.v2.ui." + name,
                "file": export.relative_to(COLLECTION).as_posix(),
                "source": source.relative_to(COLLECTION).as_posix(),
                "size": [1920, 1080], "originalSize": list(image.size),
                "sourceBounds": list(bounds), "targetSize": list(dimensions),
                "region": [*position, *dimensions], "transparent": True,
                "sha256": digest(export), "sourceSha256": digest(source),
                "sampling": "nearest", "origin": "OpenAI built-in ImageGen",
                "normalization": "User-approved alpha-margin trim and nearest dimensional normalization only",
                "runtimeIntegrated": True,
            }
            if margins is not None:
                entry["nineSlice"] = margins
            assets.append(entry)
    manifest = {"version": 1, "designSize": [1920, 1080],
                "style": "C refined pixel fantasy / original AI card frames",
                "exportApproval": "User explicitly approved dimension normalization",
                "assets": assets}
    (COLLECTION / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (COLLECTION / "generation-prompts.json").write_text(json.dumps(records, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Normalized {len(assets)} original UI components; all exports are 1920x1080 RGBA")


if __name__ == "__main__":
    main()
