"""Read retired card-design snapshots from their compact source archive."""
from __future__ import annotations

import shutil
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

_TEMP: tempfile.TemporaryDirectory[str] | None = None
_CACHE: dict[tuple[Path, str], Path] = {}


def _extract_snapshot(archive: Path, version: str, destination: Path) -> Path:
    prefix = version + "/"
    found = False
    with zipfile.ZipFile(archive) as bundle:
        for item in bundle.infolist():
            if item.is_dir() or not item.filename.startswith(prefix):
                continue
            relative = PurePosixPath(item.filename)
            if ("\\" in item.filename or ":" in item.filename or relative.is_absolute()
                    or any(part in ("", ".", "..") for part in relative.parts)):
                raise ValueError(f"Invalid path in card source archive: {item.filename}")
            target = destination.joinpath(*relative.parts[1:])
            target.parent.mkdir(parents=True, exist_ok=True)
            with bundle.open(item) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output)
            found = True
    if not found:
        raise FileNotFoundError(f"Card source archive does not contain: {version}")
    return destination


def snapshot_dir(root: Path, version: str) -> Path:
    """Return a snapshot directory, extracting its archived copy when needed."""
    root = root.resolve()
    direct = root / "assets" / "design" / version
    if direct.is_dir():
        return direct

    key = (root, version)
    if key in _CACHE:
        return _CACHE[key]

    archive = root / "assets" / "source-archive" / "card-pool-snapshots.zip"
    if not archive.is_file():
        raise FileNotFoundError(f"Card source snapshot is missing: {version} ({archive})")

    global _TEMP
    if _TEMP is None:
        _TEMP = tempfile.TemporaryDirectory(prefix="wizardcard-card-snapshots-")
    destination = _extract_snapshot(archive, version, Path(_TEMP.name) / version)
    _CACHE[key] = destination
    return destination


def materialize_snapshot(root: Path, version: str) -> Path:
    """Restore a writable snapshot directory for an explicit source refresh."""
    root = root.resolve()
    direct = root / "assets" / "design" / version
    if direct.is_dir():
        return direct
    archive = root / "assets" / "source-archive" / "card-pool-snapshots.zip"
    if not archive.is_file():
        raise FileNotFoundError(f"Card source snapshot is missing: {version} ({archive})")
    direct.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="wizardcard-snapshot-", dir=direct.parent) as staging:
        staged = _extract_snapshot(archive, version, Path(staging) / version)
        shutil.move(str(staged), str(direct))
    _CACHE[(root, version)] = direct
    return direct


def snapshot_reference(root: Path, version: str, filename: str) -> str:
    """Return a stable repository reference for archived snapshot evidence."""
    direct = root / "assets" / "design" / version
    if direct.is_dir():
        return (direct / filename).relative_to(root).as_posix()
    return f"assets/source-archive/card-pool-snapshots.zip#{version}/{filename}"
