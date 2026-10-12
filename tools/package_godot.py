"""Export the pinned Windows frontend and assemble its native runtime data.

The Godot editor, matching release GDExtension and Windows export templates must
already exist. No authoring art, local player data, tests or historical docs enter
the portable package. Run with --help for paths.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--output", type=Path, default=ROOT / "dist/WizardCard-2.5.0-preview.1-windows-x64")
    parser.add_argument("--crt", type=Path, help="Directory containing the x64 VC runtime DLLs")
    args = parser.parse_args()
    lock = json.loads((ROOT / "godot/toolchain-lock.json").read_text())
    version = subprocess.check_output([str(args.godot), "--version"], text=True).strip()
    if version != lock["engine"]:
        raise SystemExit(f"Expected Godot {lock['engine']}, found {version}")
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "dist"):
        raise SystemExit("Package output must be inside this repository's dist directory")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise SystemExit("Use an empty package directory; existing artifacts are preserved")
    run(args.godot, "--headless", "--path", ROOT / "godot", "--import", "--frame-delay", "1000")
    run(args.godot, "--headless", "--path", ROOT / "godot", "--export-release", "Windows Desktop", output / "WizardCard.exe")
    manifest = json.loads((ROOT / "assets/catalog.json").read_text(encoding="utf-8"))
    data_files = ["catalog.json", "cards.json", "deck.json", "side-deck.json", "presets.json"]
    data_files += [entry["file"] for entry in manifest["definitions"]]
    for relative in data_files:
        source = ROOT / "assets" / relative
        destination = output / "assets" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    shutil.copytree(ROOT / "assets/audio", output / "assets/audio", ignore=shutil.ignore_patterns("*.py"))
    # The extension manifest resolves the native bridge as res://bin/...;
    # keep that layout in the portable package so deck construction and all
    # other native service calls work after export.
    bridge_destination = output / "bin" / "wizard_godot_bridge.release.dll"
    bridge_destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "godot/bin/wizard_godot_bridge.release.dll", bridge_destination)
    for name in ("wizard_cli.exe", "wizard_network_probe.exe"):
        shutil.copy2(args.build / args.configuration / name, output / name)
    if args.crt:
        for file in args.crt.glob("*.dll"):
            shutil.copy2(file, output / file.name)
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md", "README.md"):
        shutil.copy2(ROOT / name, output / name)
    shutil.copytree(ROOT / "licenses", output / "licenses")
    (output / "docs").mkdir()
    for name in ("alpha-v2.5-rules.md", "releases/2.5.0.md"):
        source = ROOT / "docs" / name
        if source.exists():
            shutil.copy2(source, output / "docs" / source.name)
    run(output / "wizard_cli.exe", "validate", output / "assets")
    run(output / "wizard_cli.exe", "smoke", output / "assets", output / "verification.json", "42")
    run(output / "wizard_cli.exe", "replay", output / "assets", output / "verification.json")
    if not bridge_destination.is_file():
        raise SystemExit("Portable package is missing bin/wizard_godot_bridge.release.dll")
    inventory = {str(path.relative_to(output)).replace("\\", "/"): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in sorted(output.rglob("*")) if path.is_file()}
    (output / "SHA256.json").write_text(json.dumps(inventory, indent=2), encoding="utf-8")
    # ``Path.with_suffix`` would treat the release candidate's ``.1`` as the
    # suffix and truncate the package name to ``...-rc.zip``.
    archive = output.parent / (output.name + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as package:
        for file in sorted(output.rglob("*")):
            if file.is_file():
                package.write(file, str(file.relative_to(output.parent)))
    print(json.dumps({"package": str(output), "zip": str(archive), "files": len(inventory),
                      "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()}, indent=2))


if __name__ == "__main__":
    main()
