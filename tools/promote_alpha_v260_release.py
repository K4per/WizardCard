"""Materialize the approved Alpha v2.6.0 content snapshot for the frontend.

The importer keeps its auditable workbook staging tree under
``assets/source-archive``.  This command promotes that exact snapshot into a
small runtime content directory without touching the 2.5.5 compatibility
assets used by historical regression fixtures.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGING = ROOT / "assets/source-archive/alpha-v260-xlsx-import/staging"
DEFAULT_OUTPUT = ROOT / "assets/release-v260"


def read(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def write(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def promote(source: Path, output: Path) -> dict[str, object]:
    catalog = read(source / "catalog.json")
    if not isinstance(catalog, dict) or catalog.get("format") != 1:
        raise ValueError("staging catalog must use format 1")
    metadata = catalog.get("metadata")
    definitions = catalog.get("definitions")
    if not isinstance(metadata, dict) or not isinstance(definitions, list) or len(definitions) != 92:
        raise ValueError("expected the approved 92-card v2.6 staging catalog")
    if metadata.get("rulesVersion") != "2.6.0" or metadata.get("mechanicsRevision") != 10:
        raise ValueError("staging catalog is not Alpha v2.6.0/revision 10")
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)

    released_metadata = dict(metadata)
    released_metadata.update({"cardSetVersion": "2.6.0", "status": "released"})
    released_catalog = {"format": 1, "metadata": released_metadata, "definitions": definitions}
    write(output / "catalog.json", released_catalog)

    staged_cards = read(source / "cards.json")
    if not isinstance(staged_cards, dict) or len(staged_cards.get("cards", [])) != len(definitions):
        raise ValueError("staging aggregate does not contain all v2.6 definitions")
    aggregate = dict(staged_cards)
    aggregate.update(released_metadata)
    write(output / "cards.json", aggregate)

    for entry in definitions:
        relative = Path(str(entry["file"]))
        if relative.is_absolute() or ".." in relative.parts or relative.suffix != ".json":
            raise ValueError(f"invalid definition path: {relative}")
        source_file = source / relative
        if not source_file.is_file():
            raise ValueError(f"missing definition: {source_file}")
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_file, destination)

    for name in ("deck.json", "side-deck.json"):
        shutil.copy2(source / name, output / name)

    fixture = read(source / "test-decks.json")
    if not isinstance(fixture, dict) or fixture.get("validation", {}).get("mainCount") != 40:
        raise ValueError("v2.6 deck fixture is not a valid 40-card main deck")
    preset = {
        "id": "v260-starter",
        "name": "Alpha v2.6.0 入门",
        "description": "Alpha v2.6.0 正式卡池的 40+10 入门牌组。",
        "baseFormation": fixture["initialFormation"],
        "cards": fixture["main"],
        "sideCards": fixture["side"],
    }
    write(output / "presets.json", [preset])

    audio_source = ROOT / "assets/audio"
    if audio_source.is_dir():
        shutil.copytree(audio_source, output / "audio")

    release = {
        "format": 1,
        "status": "released",
        "rulesVersion": "2.6.0",
        "cardSetVersion": "2.6.0",
        "mechanicsRevision": 10,
        "cardCount": len(definitions),
        "source": str(source.relative_to(ROOT)).replace("\\", "/"),
        "sourceCatalogSha256": digest(source / "catalog.json"),
        "sourceAggregateSha256": digest(source / "cards.json"),
    }
    write(output / "RELEASE.json", release)
    return release


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=STAGING)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "assets"):
        raise SystemExit("release output must stay under assets/")
    release = promote(source, output)
    print(json.dumps({"output": str(output), **release}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
