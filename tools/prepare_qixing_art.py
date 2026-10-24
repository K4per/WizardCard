"""Create transparent runtime mirrors for the supplied Qixing card art.

The source artwork is retained as JPEG. The generated PNGs remove only the
near-black background connected to the canvas edge, so dark details enclosed by
the illustration remain opaque.
"""

from __future__ import annotations

import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import shutil

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
BATCH = ROOT / "assets/art/alpha-v254-20261024"
SOURCE_DIR = BATCH / "source/七星卡图"
EXPORT_DIR = BATCH / "export/qixing"
RUNTIME_DIR = ROOT / "godot/art/cards"
RUNTIME_MANIFEST = ROOT / "godot/art/assets.json"
BACKGROUND_MAX = 40


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def edge_background_alpha(image: Image.Image) -> Image.Image:
    """Return an alpha mask for dark pixels reachable from the image edge."""
    rgb = image.convert("RGB")
    width, height = rgb.size
    pixels = rgb.load()
    visited = bytearray(width * height)
    queue: deque[int] = deque()

    def is_background(index: int) -> bool:
        x = index % width
        y = index // width
        return max(pixels[x, y]) <= BACKGROUND_MAX

    for x in range(width):
        for y in (0, height - 1):
            index = y * width + x
            if not visited[index] and is_background(index):
                visited[index] = 1
                queue.append(index)
    for y in range(height):
        for x in (0, width - 1):
            index = y * width + x
            if not visited[index] and is_background(index):
                visited[index] = 1
                queue.append(index)

    while queue:
        index = queue.popleft()
        x = index % width
        y = index // width
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if nx < 0 or nx >= width or ny < 0 or ny >= height:
                continue
            neighbor = ny * width + nx
            if not visited[neighbor] and is_background(neighbor):
                visited[neighbor] = 1
                queue.append(neighbor)

    alpha = Image.new("L", (width, height), 255)
    alpha_pixels = alpha.load()
    for index, is_removed in enumerate(visited):
        if is_removed:
            alpha_pixels[index % width, index // width] = 0
    return alpha


def prepare() -> None:
    source_manifest = json.loads((BATCH / "manifest.json").read_text(encoding="utf-8"))
    qixing = [entry for entry in source_manifest["assets"] if entry["id"].startswith("illustration.qixing-")]
    asset_order = [entry["id"] for entry in source_manifest["assets"]]
    if len(qixing) != 16:
        raise ValueError(f"Expected 16 Qixing illustrations, found {len(qixing)}")

    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    runtime = json.loads(RUNTIME_MANIFEST.read_text(encoding="utf-8"))
    by_id = {entry["id"]: entry for entry in runtime["assets"]}
    manifest_entries = []

    for source_entry in qixing:
        source = BATCH / source_entry["file"]
        card_id = source_entry["cardId"]
        output = EXPORT_DIR / f"{card_id}.png"
        image = Image.open(source).convert("RGB")
        if image.size != (1536, 1536):
            raise ValueError(f"Unexpected dimensions for {source}: {image.size}")
        image.putalpha(edge_background_alpha(image))
        image.save(output, format="PNG", optimize=True)

        runtime_output = RUNTIME_DIR / output.name
        shutil.copyfile(output, runtime_output)
        entry = dict(by_id[source_entry["id"]])
        entry.update({
            "file": f"res://art/cards/{output.name}",
            "source": output.relative_to(ROOT).as_posix(),
            "authoringSource": source.relative_to(ROOT).as_posix(),
            "sha256": sha256(runtime_output),
            "size": [1536, 1536],
            "sampling": "linear",
            "origin": "User-supplied Qixing JPEG; edge-connected matte removal to RGBA PNG",
            "transparent": True,
        })
        by_id[source_entry["id"]] = entry
        manifest_entries.append({
            "id": source_entry["id"],
            "cardId": card_id,
            "file": output.relative_to(BATCH).as_posix(),
            "source": source_entry["file"],
            "sourceSha256": sha256(source),
            "sha256": sha256(output),
            "size": [1536, 1536],
            "sampling": "linear",
            "transparent": True,
            "origin": "User-supplied Qixing JPEG; edge-connected matte removal to RGBA PNG",
        })

    source_manifest["processing"] = {
        "script": "tools/prepare_qixing_art.py",
        "sourceSize": [1536, 1536],
        "alpha": "edge-connected pixels where max(R,G,B) <= 40 become 0",
        "preserveEnclosedDarkDetails": True,
        "outputFormat": "RGBA PNG",
    }
    manifest_by_id = {
        entry["id"]: entry for entry in source_manifest["assets"] if not entry["id"].startswith("illustration.qixing-")
    }
    manifest_by_id.update({entry["id"]: entry for entry in manifest_entries})
    source_manifest["assets"] = [manifest_by_id[asset_id] for asset_id in asset_order]
    (BATCH / "manifest.json").write_text(
        json.dumps(source_manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    runtime["assets"] = sorted(by_id.values(), key=lambda entry: entry["id"])
    relative = (BATCH / "manifest.json").relative_to(ROOT).as_posix()
    additional = [entry for entry in runtime.get("additionalSourceManifests", []) if entry["file"] != relative]
    additional.append({"file": relative, "sha256": sha256(BATCH / "manifest.json")})
    runtime["additionalSourceManifests"] = additional
    RUNTIME_MANIFEST.write_text(json.dumps(runtime, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {len(manifest_entries)} transparent Qixing illustrations")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    prepare()
