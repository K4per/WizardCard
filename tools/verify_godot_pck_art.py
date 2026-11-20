"""Audit the artwork in an exported Godot 4.7 PCK, including its imported textures."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def read_directory(stream):
    # Standard unencrypted external PCK v4 as produced by both pinned export presets.
    header = stream.read(40)
    if len(header) != 40 or header[:4] != b"GDPC":
        raise ValueError("Invalid Godot PCK header")
    version, major, minor, patch, flags, base, directory = struct.unpack_from("<IIIIIQQ", header, 4)
    if version != 4 or major != 4 or minor != 7 or flags & 1:
        raise ValueError(f"Unsupported Godot PCK version or encrypted directory: {version}/{major}.{minor}.{patch}, flags {flags}")
    stream.seek(directory)
    count, = struct.unpack("<I", stream.read(4))
    files = {}
    for _ in range(count):
        length, = struct.unpack("<I", stream.read(4))
        if length > 16384:
            raise ValueError("Unreasonable PCK path length")
        name = stream.read(length).rstrip(b"\0").decode("utf-8")
        offset, size, md5, entry_flags = struct.unpack("<QQ16sI", stream.read(36))
        if name in files or entry_flags != 0:
            raise ValueError(f"Duplicate, removed or encrypted PCK entry: {name}")
        files[name] = (base + offset if flags & 2 else offset, size)
    return files


def audit_pck(pck, manifest):
    expected_manifest = manifest.read_bytes()
    entries = json.loads(expected_manifest)["assets"]
    with pck.open("rb") as stream:
        directory = read_directory(stream)

        def read(name):
            offset, size = directory[name]
            stream.seek(offset)
            data = stream.read(size)
            if len(data) != size:
                raise ValueError(f"Truncated PCK entry: {name}")
            return data

        if json.loads(read("art/assets.json")) != json.loads(expected_manifest):
            raise ValueError("PCK art manifest differs from the approved source manifest")
        allowed_art = {"art/assets.json"}
        textures = set()
        for entry in entries:
            name = entry["file"].removeprefix("res://")
            is_illustration = entry["id"].startswith("illustration.")
            if not is_illustration and not entry["id"].startswith("font.cjk"):
                raise ValueError(f"Unexpected art ID: {entry['id']}")
            if name.endswith(".txt"):
                if hashlib.sha256(read(name)).hexdigest() != entry["sha256"]:
                    raise ValueError(f"Font license hash mismatch: {name}")
                allowed_art.add(name)
                continue
            import_name = name + ".import"
            allowed_art.add(import_name)
            imported = read(import_name).decode("utf-8")
            resources = re.findall(r'^path(?:\.[\w.]+)?="res://([^"]+)"', imported, re.MULTILINE)
            if not resources or any(path not in directory for path in resources):
                raise ValueError(f"Missing imported resource for {name}")
            # Godot identifies an imported resource by its original basename and the MD5 of its res:// path.
            identity = hashlib.md5(entry["file"].encode("utf-8")).hexdigest()
            prefix = f".godot/imported/{Path(name).name}-{identity}."
            suffix = ".ctex" if is_illustration else ".fontdata"
            if any(not path.startswith(prefix) or not path.endswith(suffix) for path in resources):
                raise ValueError(f"Unexpected compiled resource for {name}")
            if is_illustration:
                textures.update(resources)
            # Godot may also preserve the source bytes when an include filter requests them.
            if name in directory:
                if hashlib.sha256(read(name)).hexdigest() != entry["sha256"]:
                    raise ValueError(f"Original packed art differs: {name}")
                allowed_art.add(name)
        actual_art = {name for name in directory if name.startswith("art/")}
        if actual_art != allowed_art:
            raise ValueError(f"Unexpected or missing packed art: {sorted(actual_art ^ allowed_art)}")
        actual_textures = {name for name in directory if name.endswith((".ctex", ".ctexarray", ".ctex3d", ".ccube"))}
        if actual_textures != textures:
            raise ValueError(f"Unexpected or missing compiled textures: {sorted(actual_textures ^ textures)}")
        raw_images = {name for name in directory if Path(name).suffix.lower() in
                      {".png", ".jpg", ".jpeg", ".webp", ".svg", ".ico", ".bmp", ".tga", ".dds", ".ktx", ".avif"}}
        if raw_images - allowed_art:
            raise ValueError(f"Unlisted packed images: {sorted(raw_images - allowed_art)}")
    return len(textures)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pck", type=Path, help="WizardCard.pck from the extracted portable client")
    parser.add_argument("--manifest", type=Path, default=Path(__file__).resolve().parents[1] / "godot/art/assets.json")
    args = parser.parse_args()
    try:
        print(f"Verified {audit_pck(args.pck, args.manifest)} packed illustration textures.")
    except (KeyError, ValueError, OSError, struct.error) as error:
        raise SystemExit(str(error)) from error
