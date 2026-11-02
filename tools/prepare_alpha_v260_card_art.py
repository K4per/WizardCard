"""Prepare and register the Alpha v2.6.0 card art supplied in the design pool.

RGB sources have a checkerboard matte baked into their pixels. The script
removes pale neutral background components while keeping the source image
unchanged. RGBA sources are copied byte-for-byte and retain their alpha.
"""
from __future__ import annotations

from collections import deque
import hashlib
import json
from pathlib import Path
import shutil
import sys

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
DESIGN = Path(r"E:\Project\WizardCard-Design\Alpha 现有卡池")
STAGING = ROOT / "assets/source-archive/alpha-v260-xlsx-import/staging"
RELEASE = ROOT / "assets/release-v260"
SOURCE_OUTPUT = ROOT / "assets/art/alpha-v260/current-pool"
RUNTIME_OUTPUT = ROOT / "godot/art/card"
RUNTIME_MANIFEST = ROOT / "godot/art/assets.json"
SOURCE_MANIFEST = SOURCE_OUTPUT / "manifest.json"
BACKGROUND_MIN = 160
BACKGROUND_CHROMA_MAX = 55
ENCLOSED_BACKGROUND_MIN_PIXELS = 128

sys.path.insert(0, str(ROOT / "tools"))
import import_alpha_v260_xlsx as importer  # noqa: E402


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def is_background(rgb: tuple[int, int, int]) -> bool:
    low, high = min(rgb), max(rgb)
    return low >= BACKGROUND_MIN and high - low <= BACKGROUND_CHROMA_MAX


def remove_checkerboard(image: Image.Image) -> tuple[Image.Image, int]:
    """Remove edge-connected and large enclosed pale neutral components."""
    rgb_image = image.convert("RGB")
    width, height = rgb_image.size
    pixels = rgb_image.load()
    candidate = bytearray(width * height)
    for y in range(height):
        row = y * width
        for x in range(width):
            if is_background(pixels[x, y]):
                candidate[row + x] = 1

    visited = bytearray(width * height)
    removed = bytearray(width * height)
    transparent_count = 0
    for start in range(width * height):
        if not candidate[start] or visited[start]:
            continue
        queue = deque([start])
        visited[start] = 1
        component: list[int] = []
        touches_edge = False
        while queue:
            index = queue.popleft()
            component.append(index)
            y, x = divmod(index, width)
            touches_edge |= x == 0 or y == 0 or x == width - 1 or y == height - 1
            if x and candidate[index - 1] and not visited[index - 1]:
                visited[index - 1] = 1
                queue.append(index - 1)
            if x + 1 < width and candidate[index + 1] and not visited[index + 1]:
                visited[index + 1] = 1
                queue.append(index + 1)
            if y and candidate[index - width] and not visited[index - width]:
                visited[index - width] = 1
                queue.append(index - width)
            if y + 1 < height and candidate[index + width] and not visited[index + width]:
                visited[index + width] = 1
                queue.append(index + width)

        if touches_edge or len(component) >= ENCLOSED_BACKGROUND_MIN_PIXELS:
            for index in component:
                removed[index] = 1
            transparent_count += len(component)

    alpha = Image.new("L", (width, height), 255)
    alpha_pixels = alpha.load()
    for index, value in enumerate(removed):
        if value:
            alpha_pixels[index % width, index // width] = 0
    output = rgb_image.convert("RGBA")
    output.putalpha(alpha)
    return output, transparent_count


def source_for(card: dict) -> Path | None:
    if card["type"] == "formation":
        stem = importer.formation_art_name(card["name"])
        folder = DESIGN / "阵法卡卡图"
    elif card["type"] in {"analytic", "word", "talisman"}:
        stem = card["name"]
        folder = DESIGN / "法术卡卡图"
    else:
        return None
    if not stem:
        return None
    path = folder / f"{stem}.png"
    return path if path.is_file() else None


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def update_art_mapping(cards: list[dict]) -> None:
    lines = [
        "# Alpha v2.6.0 卡图映射",
        "",
        "设计目录卡图优先于历史复用。RGB 源的棋盘底由 `tools/prepare_alpha_v260_card_art.py` 扣除；RGBA 源保留原始 alpha。逐张源文件、哈希和透明处理记录见 `assets/art/alpha-v260/current-pool/manifest.json`。",
        "",
        "| 卡名 | 类型 | 状态 | illustrationId | mapping | manifestId | 源文件 | 运行文件 | 透明处理 |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    for card in cards:
        art = card.get("art", {})
        lines.append(
            f"| {card['name']} | {card['type']} | {art.get('status', 'missing')} | "
            f"{card.get('illustrationId', '')} | {art.get('mapping') or '—'} | "
            f"{art.get('manifestId') or '—'} | {art.get('source') or '—'} | "
            f"{art.get('processedFile') or '历史运行资源'} | {art.get('processing') or '沿用历史透明资源'} |"
        )
    (ROOT / "assets/source-archive/alpha-v260-xlsx-import/art-mapping.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")


def update_cards(cards: list[dict], by_id: dict[str, dict]) -> list[str]:
    processed_files: list[str] = []
    for card in cards:
        source = source_for(card)
        if source is None:
            continue
        card_id = f"v260-art-{card['type']}-{card['id'].rsplit('-', 1)[-1]}"
        source_output = SOURCE_OUTPUT / f"{card_id}.png"
        runtime_output = RUNTIME_OUTPUT / f"{card_id}.png"
        source_output.parent.mkdir(parents=True, exist_ok=True)
        runtime_output.parent.mkdir(parents=True, exist_ok=True)

        with Image.open(source) as original:
            source_mode = original.mode
            source_size = list(original.size)
            if "A" in original.getbands():
                rgba = original.convert("RGBA")
                alpha_histogram = rgba.getchannel("A").histogram()
                if alpha_histogram[0] == 0:
                    raise ValueError(f"Alpha channel has no transparent pixels: {source}")
                rgba.save(source_output, format="PNG", optimize=True)
                processing = "源图已有有效 alpha；保留透明像素"
                transparent_pixels = alpha_histogram[0]
                alpha_source = "source-alpha-preserved"
            else:
                rgba, transparent_pixels = remove_checkerboard(original)
                if transparent_pixels == 0:
                    raise ValueError(f"No checkerboard pixels were removed from RGB source: {source}")
                rgba.save(source_output, format="PNG", optimize=True)
                processing = "脚本移除边缘连通及大面积浅色棋盘背景"
                alpha_source = "edge-connected-neutral-matte-removal"
        shutil.copyfile(source_output, runtime_output)
        if Image.open(source_output).getchannel("A").getextrema()[0] != 0:
            raise ValueError(f"Processed image is not transparent: {source_output}")

        asset_id = "illustration." + card_id
        relative_source = source_output.relative_to(ROOT).as_posix()
        resource_file = "res://art/card/" + source_output.name
        art_entry = {
            "id": asset_id,
            "file": resource_file,
            "source": relative_source,
            "sourceSha256": digest(source),
            "sha256": digest(runtime_output),
            "size": source_size,
            "sourceMode": source_mode,
            "sampling": "nearest",
            "origin": "User-supplied Alpha v2.6 current-pool card art; " + alpha_source,
            "processing": processing,
            "transparentPixels": transparent_pixels,
        }
        by_id[asset_id] = art_entry
        card["illustrationId"] = card_id
        card["art"] = {
            "status": "matched",
            "source": str(source),
            "mapping": "用户指定 Alpha 现有卡池卡图",
            "manifestId": card_id,
            "processedFile": relative_source,
            "processing": processing,
        }
        processed_files.append(relative_source)
    return processed_files


def sync_card_files(cards: list[dict], directory: Path) -> None:
    for card in cards:
        path = directory / "cards" / f"{card['id']}.json"
        if path.is_file():
            current = json.loads(path.read_text(encoding="utf-8"))
            current["illustrationId"] = card["illustrationId"]
            current["art"] = card["art"]
            write_json(path, current)


def write_preview(cards: list[dict]) -> Path:
    cards = [card for card in cards if card.get("art", {}).get("processedFile")]
    preview_path = ROOT / "output/alpha-v260-transparent-art-preview.png"
    preview_path.parent.mkdir(parents=True, exist_ok=True)
    columns, cell_width, cell_height = 5, 240, 280
    rows = (len(cards) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_width, rows * cell_height), (35, 45, 55))
    draw = ImageDraw.Draw(sheet)
    for index, card in enumerate(cards):
        art_file = card.get("art", {}).get("processedFile")
        if not art_file:
            continue
        image_path = ROOT / art_file
        if not image_path.is_file():
            continue
        image = Image.open(image_path).convert("RGBA")
        image.thumbnail((216, 216), Image.Resampling.LANCZOS)
        cell_x, cell_y = index % columns * cell_width, index // columns * cell_height
        x, y = cell_x + (cell_width - image.width) // 2, cell_y + 4
        for yy in range(y, y + image.height, 16):
            for xx in range(x, x + image.width, 16):
                tone = (55, 65, 75) if ((xx - x) // 16 + (yy - y) // 16) % 2 else (85, 95, 105)
                draw.rectangle((xx, yy, min(xx + 15, x + image.width - 1),
                                min(yy + 15, y + image.height - 1)), fill=tone)
        sheet.paste(image, (x, y), image)
        draw.text((cell_x + 6, cell_y + 230), f"{card['number']} {card['name']}", fill="white")
    sheet.save(preview_path)
    return preview_path


def main() -> int:
    aggregate_path = STAGING / "cards.json"
    aggregate = json.loads(aggregate_path.read_text(encoding="utf-8"))
    cards = aggregate["cards"]
    runtime_manifest = json.loads(RUNTIME_MANIFEST.read_text(encoding="utf-8"))
    by_asset_id = {entry["id"]: entry for entry in runtime_manifest["assets"]}

    SOURCE_OUTPUT.mkdir(parents=True, exist_ok=True)
    processed_files = update_cards(cards, by_asset_id)
    if len(processed_files) != 49:
        raise ValueError(f"Expected 49 current-pool art files, prepared {len(processed_files)}")

    runtime_manifest["assets"] = sorted(by_asset_id.values(), key=lambda entry: entry["id"])
    write_json(RUNTIME_MANIFEST, runtime_manifest)
    manifest_entries = [by_asset_id["illustration." + Path(path).stem] for path in processed_files]
    source_by_manifest_id = {
        card["art"]["manifestId"]: card["art"]["source"]
        for card in cards if card.get("art", {}).get("processedFile")
    }
    write_json(SOURCE_MANIFEST, {
        "format": 1,
        "collection": "Alpha v2.6.0 current-pool card art",
        "designDirectory": str(DESIGN),
        "processingScript": "tools/prepare_alpha_v260_card_art.py",
        "rgbProcessing": {
            "candidate": f"min(R,G,B) >= {BACKGROUND_MIN} and max(R,G,B)-min(R,G,B) <= {BACKGROUND_CHROMA_MAX}",
            "removedComponents": "edge-connected or at least 128 pixels",
            "preserveEnclosedDarkDetails": True,
            "outputFormat": "RGBA PNG",
        },
        "assets": [
            {
                "id": entry["id"],
                "file": entry["source"],
                "source": source_by_manifest_id[entry["id"].removeprefix("illustration.")],
                "sourceSha256": entry["sourceSha256"],
                "sourceMode": entry["sourceMode"],
                "sha256": entry["sha256"],
                "size": entry["size"],
                "processing": entry["processing"],
                "transparentPixels": entry["transparentPixels"],
            }
            for entry in manifest_entries
        ],
    })

    aggregate["cards"] = cards
    write_json(aggregate_path, aggregate)
    sync_card_files(cards, STAGING)
    report_path = ROOT / "assets/source-archive/alpha-v260-xlsx-import/import-report.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    report_cards = {card["id"]: card for card in report["cards"]}
    for card in cards:
        report_cards[card["id"]].update({
            "illustrationId": card["illustrationId"],
            "art": card["art"],
        })
    report["cards"] = [report_cards[card["id"]] for card in cards]
    report["stagingContentHash"] = hashlib.sha256(
        json.dumps(cards, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    report["art"]["preparedTransparentDesignFiles"] = len(processed_files)
    report["art"]["rgbBackgroundsRemoved"] = sum(
        entry["sourceMode"] != "RGBA" for entry in manifest_entries
    )
    report["art"]["sourceAlphaPreserved"] = sum(
        entry["sourceMode"] == "RGBA" for entry in manifest_entries
    )
    write_json(report_path, report)

    release_cards_path = RELEASE / "cards.json"
    if release_cards_path.is_file():
        release_aggregate = json.loads(release_cards_path.read_text(encoding="utf-8"))
        release_by_id = {card["id"]: card for card in release_aggregate["cards"]}
        for card in cards:
            release_by_id[card["id"]].update({
                "illustrationId": card["illustrationId"],
                "art": card["art"],
            })
        release_aggregate["cards"] = [release_by_id[card["id"]] for card in cards]
        write_json(release_cards_path, release_aggregate)
        sync_card_files(cards, RELEASE)
        release_metadata_path = RELEASE / "RELEASE.json"
        if release_metadata_path.is_file():
            release_metadata = json.loads(release_metadata_path.read_text(encoding="utf-8"))
            release_metadata["sourceAggregateSha256"] = digest(aggregate_path)
            write_json(release_metadata_path, release_metadata)

    update_art_mapping(cards)
    preview_path = write_preview(cards)
    print(json.dumps({
        "prepared": len(processed_files),
        "rgbBackgroundsRemoved": report["art"]["rgbBackgroundsRemoved"],
        "sourceAlphaPreserved": report["art"]["sourceAlphaPreserved"],
        "sourceManifest": SOURCE_MANIFEST.relative_to(ROOT).as_posix(),
        "registeredRuntimeAssets": len(by_asset_id),
        "preview": preview_path.relative_to(ROOT).as_posix(),
    }, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
