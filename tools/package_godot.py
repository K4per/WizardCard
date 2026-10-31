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
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PACKAGE_VERSION = "2.6.0-hotfix.1"


def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--package-version", default=DEFAULT_PACKAGE_VERSION,
                        help="portable package label; the executable reports its own runtime version")
    parser.add_argument("--content", type=Path, default=ROOT / "assets/release-v260",
                        help="runtime content directory containing catalog.json and the approved deck files")
    parser.add_argument("--output", type=Path,
                        help="empty package directory; defaults to dist/WizardCard-<package-version>-windows-x64")
    parser.add_argument("--staging-report", type=Path,
                        help="optional import report to include in a staging package")
    parser.add_argument("--include-presets", action="store_true",
                        help="include legacy public presets (disabled for the v2.6.0 release package)")
    parser.add_argument("--crt", type=Path, help="Directory containing the x64 VC runtime DLLs")
    args = parser.parse_args()
    lock = json.loads((ROOT / "godot/toolchain-lock.json").read_text())
    version = subprocess.check_output([str(args.godot), "--version"], text=True).strip()
    if version != lock["engine"]:
        raise SystemExit(f"Expected Godot {lock['engine']}, found {version}")
    output = (args.output or (ROOT / "dist" / f"WizardCard-{args.package_version}-windows-x64")).resolve()
    if not output.is_relative_to(ROOT / "dist"):
        raise SystemExit("Package output must be inside this repository's dist directory")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise SystemExit("Use an empty package directory; existing artifacts are preserved")
    if not (ROOT / "godot/bin/wizard_godot_bridge.release.dll").is_file():
        raise SystemExit("Build the matching Release Godot bridge before packaging")
    content = args.content.resolve()
    if not content.is_relative_to(ROOT / "assets") or not (content / "catalog.json").is_file():
        raise SystemExit("--content must be an assets directory containing catalog.json")
    for name in ("wizard_cli.exe", "wizard_network_probe.exe"):
        if not (args.build / args.configuration / name).is_file():
            raise SystemExit(f"Missing release verification tool: {name}")
    run(args.godot, "--headless", "--path", ROOT / "godot", "--import", "--frame-delay", "1000")
    run(args.godot, "--headless", "--path", ROOT / "godot", "--export-release", "Windows Desktop", output / "WizardCard.exe")
    manifest = json.loads((content / "catalog.json").read_text(encoding="utf-8"))
    data_files = ["catalog.json", "cards.json", "deck.json", "side-deck.json"]
    if args.include_presets or (content / "presets.json").is_file():
        data_files.append("presets.json")
    data_files += [entry["file"] for entry in manifest["definitions"]]
    for relative in data_files:
        source = content / relative
        destination = output / "assets" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    audio_source = content / "audio" if (content / "audio").is_dir() else ROOT / "assets/audio"
    shutil.copytree(audio_source, output / "assets/audio", ignore=shutil.ignore_patterns("*.py"))
    # The extension manifest resolves the native bridge as res://bin/...;
    # keep that layout in the portable package so deck construction and all
    # other native service calls work after export.
    bridge_destination = output / "bin" / "wizard_godot_bridge.release.dll"
    bridge_destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "godot/bin/wizard_godot_bridge.release.dll", bridge_destination)
    if args.crt:
        for file in args.crt.glob("*.dll"):
            shutil.copy2(file, output / file.name)
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md", "README.md", "CHANGELOG.md", "GLOSSARY.md"):
        shutil.copy2(ROOT / name, output / name)
    if args.staging_report and args.staging_report.is_file():
        destination = output / "docs" / "releases" / "alpha-v2.6.0-staging-import.md"
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.staging_report, destination)
    shutil.copytree(ROOT / "licenses", output / "licenses",
                    ignore=shutil.ignore_patterns("SFML.txt"))
    (output / "docs").mkdir(exist_ok=True)
    for name in ("alpha-v2.5-rules.md", "architecture.md", "network-play.md",
                 "card-catalog.md", "card-catalog.html", "sique-cards.md",
                 "releases/2.6.0-hotfix.1.md", "reports/2.6.0-hotfix.1.md",
                 "releases/2.6.0.md", "reports/2.6.0.md",
                 "releases/2.5.5-hotfix.1.md", "reports/2.5.5-hotfix.1.md",
                 "releases/2.5.5.md", "reports/2.5.5.md"):
        source = ROOT / "docs" / name
        if source.exists():
            destination = output / "docs" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    # The offline catalogue uses the same relative illustration paths as the
    # repository. Copy only the official cards' runtime art beside the docs.
    art_files = {entry["id"]: entry["file"]
                 for entry in json.loads((ROOT / "godot/art/assets.json").read_text(encoding="utf-8"))["assets"]}
    cards = json.loads((content / "cards.json").read_text(encoding="utf-8"))["cards"]
    for card in cards:
        relative = art_files["illustration." + card.get("illustrationId", card["id"])].removeprefix("res://")
        destination = output / "godot" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / "godot" / relative, destination)
    checks = output / "package-checks"
    checks.mkdir()
    recording = checks / "verification.json"
    cli = args.build / args.configuration / "wizard_cli.exe"
    for name, command in (("validate", [cli, "validate", output / "assets"]),
                          ("smoke", [cli, "smoke", output / "assets", recording, "42"]),
                          ("replay", [cli, "replay", output / "assets", recording])):
        result = subprocess.run([str(arg) for arg in command], cwd=ROOT, check=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (checks / (name + ".log")).write_text(result.stdout, encoding="utf-8")
        print(result.stdout, end="")
    verified = json.loads(recording.read_text(encoding="utf-8"))
    release = {key: verified[key] for key in
               ("programVersion", "rulesVersion", "cardSetVersion", "contentHash")}
    release.update({"packageVersion": args.package_version,
                    "status": "staging-blocked" if args.staging_report and args.staging_report.is_file() else "released"})
    release.update({"mechanicsRevision": manifest["metadata"]["mechanicsRevision"],
                    "cardCount": len(cards), "engine": version,
                    "catalogSha256": hashlib.sha256((output / "assets/cards.json").read_bytes()).hexdigest(),
                    "bridgeSha256": hashlib.sha256(bridge_destination.read_bytes()).hexdigest()})
    (output / "RELEASE.json").write_text(json.dumps(release, indent=2) + "\n", encoding="utf-8")
    if not bridge_destination.is_file():
        raise SystemExit("Portable package is missing bin/wizard_godot_bridge.release.dll")
    sfml_files = [path for path in output.rglob("*") if path.is_file() and "sfml" in path.name.lower()]
    if sfml_files:
        raise SystemExit("Portable package contains retired SFML files: " + ", ".join(map(str, sfml_files)))
    # Export exclusions can remove a global script class while editor tests still
    # pass. Boot the actual executable; release templates disable --script.
    with tempfile.TemporaryDirectory(prefix="wizardcard-package-") as startup_check:
        check_dir = Path(startup_check)
        log = check_dir / "startup.log"
        subprocess.run([str(output / "WizardCard.exe"), "--headless", "--quit-after", "8", "--log-file", str(log),
                        "--", "--assets", str(output / "assets"), "--user-data", str(check_dir / "data")],
                       cwd=check_dir, check=True, timeout=45)
        diagnostics = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        (checks / "startup.log").write_text(diagnostics, encoding="utf-8")
        if "Godot Engine v4.7.2" not in diagnostics or "ERROR:" in diagnostics:
            raise SystemExit("Exported package startup failed:\n" + diagnostics)
    inventory = {str(path.relative_to(output)).replace("\\", "/"): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in sorted(output.rglob("*")) if path.is_file()}
    (output / "SHA256.json").write_text(json.dumps(inventory, indent=2), encoding="utf-8")
    archive = output.parent / (output.name + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as package:
        for file in sorted(output.rglob("*")):
            if file.is_file():
                package.write(file, str(file.relative_to(output.parent)))
    archive_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
    (archive.parent / (archive.name + ".sha256")).write_text(archive_hash + "  " + archive.name + "\n", encoding="utf-8")
    print(json.dumps({"package": str(output), "zip": str(archive), "files": len(inventory),
                      "packageVersion": args.package_version,
                      "sha256": archive_hash}, indent=2))


if __name__ == "__main__":
    main()
