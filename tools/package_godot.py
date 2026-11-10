"""Export the pinned frontend and assemble a Windows ZIP or Linux tar.gz.

The Godot editor, matching release GDExtension and platform export templates must
already exist. No authoring art, local player data, tests or historical docs enter
the portable package. Run with --help for paths.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile
from release_source import provenance

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PACKAGE_VERSION = "2.6.3"


@dataclass(frozen=True)
class PlatformSpec:
    suffix: str
    preset: str
    executable: str
    bridge: str
    archive_suffix: str
    tool_suffix: str

    def native_tool(self, build, configuration, name):
        directory = build / configuration if self.tool_suffix else build
        return directory / (name + self.tool_suffix)


PLATFORMS = {
    "windows": PlatformSpec("windows-x64", "Windows Desktop", "WizardCard.exe",
                            "bin/wizard_godot_bridge.release.dll", ".zip", ".exe"),
    "linux": PlatformSpec("linux-x86_64", "Linux Desktop", "WizardCard.x86_64",
                          "bin/libwizard_godot_bridge.release.so", ".tar.gz", ""),
}
LINUX_REQUIREMENTS = {
    "buildBaseline": "Ubuntu 24.04 x86_64",
    "supportedSystems": ["Ubuntu 24.04+ x86_64", "Arch Linux x86_64"],
    "systemLibraries": ["glibc", "libstdc++.so.6", "libgcc_s.so.1", "libm.so.6"],
    "desktop": ["OpenGL 3.3 driver", "X11 or Wayland and xkbcommon",
                "fontconfig", "PulseAudio/PipeWire or ALSA"],
}


def validate_godot_version(version, target_platform, development=False):
    expected = json.loads((ROOT / "godot/toolchain-lock.json").read_text())["engine"]
    if version == expected:
        return
    if (development and target_platform == "linux"
            and re.fullmatch(r"4\.7\.2\.stable\.[A-Za-z0-9_-]+\.ed1daf0bf", version)):
        return
    raise ValueError(f"Expected Godot {expected}, found {version}")


def inspect_linux_binary(path):
    environment = {**os.environ, "LC_ALL": "C"}
    header = subprocess.run(["readelf", "-h", "-d", str(path)], capture_output=True, text=True, env=environment)
    if header.returncode or "Advanced Micro Devices X86-64" not in header.stdout:
        raise ValueError(f"Expected x86_64 ELF binary: {path}\n{header.stderr}")
    dependencies = subprocess.run(["ldd", str(path)], capture_output=True, text=True, env=environment)
    diagnostics = dependencies.stdout + dependencies.stderr
    if dependencies.returncode or "not found" in diagnostics:
        raise ValueError(f"Unresolved Linux dependencies for {path}:\n{diagnostics}")
    versions = subprocess.run(["readelf", "--version-info", str(path)],
                              capture_output=True, text=True, check=True, env=environment).stdout
    return {"readelf": header.stdout, "ldd": diagnostics,
            "requiredSymbolVersions": sorted(set(re.findall(r"(?:GLIBCXX|GLIBC|CXXABI)_[0-9.]+", versions)))}


def validate_inventory(output, spec):
    forbidden = []
    for path in output.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(output)
        name = path.name.lower()
        invalid = "sfml" in name
        if spec.suffix.startswith("linux"):
            invalid |= (path.suffix.lower() in {".exe", ".dll", ".pdb", ".lib", ".exp", ".cpp", ".hpp", ".h", ".gd"}
                        or "debug" in name and name.endswith(".so")
                        or any(part.lower() in {"src", "include", "tests", "fixtures", "_deps", ".godot"}
                               for part in relative.parts)
                        or name.startswith(("libstdc++", "libgcc_s", "libc.so", "libm.so", "ld-linux")))
        if invalid:
            forbidden.append(str(relative))
    if forbidden:
        raise ValueError("Portable package contains forbidden files: " + ", ".join(forbidden))


def write_linux_launcher(output):
    launcher = output / "run.sh"
    launcher.write_text('#!/usr/bin/env bash\nset -euo pipefail\n'
                        'package_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)\n'
                        'exec "$package_dir/WizardCard.x86_64" "$@"\n', encoding="utf-8")
    launcher.chmod(0o755)


def create_archive(output, spec):
    archive = output.parent / (output.name + spec.archive_suffix)
    if spec.archive_suffix == ".tar.gz":
        with tarfile.open(archive, "w:gz", compresslevel=6) as package:
            package.add(output, arcname=output.name)
    else:
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as package:
            for file in sorted(output.rglob("*")):
                if file.is_file():
                    package.write(file, str(file.relative_to(output.parent)))
    return archive


def verify_startup(output, spec, evidence, prefix="startup", launcher=False):
    evidence.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="wizardcard-package-") as startup_check:
        check_dir = Path(startup_check)
        log = check_dir / "startup.log"
        executable = output / ("run.sh" if launcher else spec.executable)
        command = [str(executable), "--headless", "--quit-after", "8", "--log-file", str(log),
                   "--", "--user-data", str(check_dir / "data")]
        # The second boot exercises automatic sibling-resource lookup.
        if not launcher:
            command += ["--assets", str(output / "assets")]
        result = subprocess.run(command, cwd=check_dir, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=45)
        diagnostics = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        (evidence / (prefix + ".log")).write_text(diagnostics + "\n" + result.stdout, encoding="utf-8")
        if (result.returncode or "Godot Engine v4.7.2" not in diagnostics
                or any(marker in diagnostics + result.stdout for marker in ("ERROR:", "SCRIPT ERROR", "handle_crash:"))
                or not (check_dir / "data/settings.json").is_file()):
            raise ValueError("Exported package startup failed:\n" + diagnostics + result.stdout)


def verify_archive(archive, package_name, spec, evidence):
    with tempfile.TemporaryDirectory(prefix="wizardcard-extracted-") as temporary:
        extraction = Path(temporary)
        with tarfile.open(archive) as package:
            package.extractall(extraction, filter="data")
        output = extraction / package_name
        for relative in (spec.executable, spec.bridge, "run.sh"):
            if not os.access(output / relative, os.X_OK):
                raise ValueError(f"Archive lost executable permission: {relative}")
        validate_inventory(output, spec)
        verify_startup(output, spec, evidence, "extracted-startup")
        verify_startup(output, spec, evidence, "extracted-launcher", launcher=True)


def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", choices=PLATFORMS, default="windows")
    parser.add_argument("--godot", required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--package-version", default=DEFAULT_PACKAGE_VERSION,
                        help="portable package label; the executable reports its own runtime version")
    parser.add_argument("--content", type=Path, default=ROOT / "assets/release-v260",
                        help="runtime content directory containing catalog.json and the approved deck files")
    parser.add_argument("--output", type=Path,
                        help="empty package directory; defaults to dist/WizardCard-<version>-<platform>")
    parser.add_argument("--staging-report", type=Path,
                        help="optional import report to include in a staging package")
    parser.add_argument("--include-presets", action="store_true",
                        help="include legacy public presets (disabled for the v2.6.0 release package)")
    parser.add_argument("--crt", type=Path, help="Directory containing the x64 VC runtime DLLs")
    args = parser.parse_args()
    source_info = provenance()
    spec = PLATFORMS[args.platform]
    args.godot = Path(shutil.which(args.godot) or args.godot).resolve()
    args.build = args.build.resolve()
    version = subprocess.check_output([str(args.godot), "--version"], text=True).strip()
    validate_godot_version(version, args.platform)
    if args.platform == "linux":
        system = platform.freedesktop_os_release()
        if system.get("ID") != "ubuntu" or system.get("VERSION_ID") != "24.04" or platform.machine() != "x86_64":
            raise ValueError("Linux release archives must be built on Ubuntu 24.04 x86_64; use the linux-godot CI job")
        if args.crt:
            raise ValueError("Linux packages use system runtimes; --crt is Windows-only")
    output = (args.output or (ROOT / "dist" / f"WizardCard-{args.package_version}-{spec.suffix}")).resolve()
    if not output.is_relative_to(ROOT / "dist"):
        raise SystemExit("Package output must be inside this repository's dist directory")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise SystemExit("Use an empty package directory; existing artifacts are preserved")
    if not (ROOT / "godot" / spec.bridge).is_file():
        raise SystemExit("Build the matching Release Godot bridge before packaging")
    content = args.content.resolve()
    if not content.is_relative_to(ROOT / "assets") or not (content / "catalog.json").is_file():
        raise SystemExit("--content must be an assets directory containing catalog.json")
    for name in ("wizard_cli", "wizard_network_probe"):
        if not spec.native_tool(args.build, args.configuration, name).is_file():
            raise SystemExit(f"Missing release verification tool: {name}")
    run(args.godot, "--headless", "--path", ROOT / "godot", "--import", "--frame-delay", "1000")
    run(args.godot, "--headless", "--path", ROOT / "godot", "--export-release", spec.preset, output / spec.executable)
    if not (output / spec.executable).is_file() or not (output / "WizardCard.pck").is_file():
        raise ValueError("Export must create the executable and external WizardCard.pck")
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
    if args.platform == "linux":
        shutil.copytree(ROOT / "assets/fonts", output / "assets/fonts")
    # The extension manifest resolves the native bridge as res://bin/...;
    # keep that layout in the portable package so deck construction and all
    # other native service calls work after export.
    bridge_destination = output / spec.bridge
    bridge_destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "godot" / spec.bridge, bridge_destination)
    if args.platform == "linux":
        (output / spec.executable).chmod(0o755)
        bridge_destination.chmod(0o755)
        write_linux_launcher(output)
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
                 "releases/2.6.1-hotfix.1.md", "reports/2.6.1-hotfix.1.md",
                 "releases/2.6.1.md", "releases/2.6.2.md", "releases/2.6.3.md",
                 "releases/2.6.0-hotfix.1.md", "reports/2.6.0-hotfix.1.md",
                 "releases/2.6.0.md", "reports/2.6.0.md",
                 "releases/2.5.5-hotfix.1.md", "reports/2.5.5-hotfix.1.md",
                 "releases/2.5.5.md", "reports/2.5.5.md"):
        source = ROOT / "docs" / name
        if source.exists():
            destination = output / "docs" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    if args.platform == "linux":
        shutil.copy2(ROOT / "docs/releases/2.6.0-hotfix.1-linux.md", output / "docs/releases/2.6.0-hotfix.1-linux.md")
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
    cli = spec.native_tool(args.build, args.configuration, "wizard_cli")
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
    release.update({**source_info, "packageVersion": args.package_version,
                    "status": "staging-blocked" if args.staging_report and args.staging_report.is_file() else "released"})
    release.update({"mechanicsRevision": manifest["metadata"]["mechanicsRevision"],
                    "cardCount": len(cards), "engine": version,
                    "catalogSha256": hashlib.sha256((output / "assets/cards.json").read_bytes()).hexdigest(),
                    "bridgeSha256": hashlib.sha256(bridge_destination.read_bytes()).hexdigest()})
    if args.platform == "linux":
        dependencies = {relative: inspect_linux_binary(output / relative)
                        for relative in (spec.executable, spec.bridge)}
        if "libstdc++.so.6" not in dependencies[spec.bridge]["readelf"]:
            raise ValueError("Linux bridge must use the system libstdc++.so.6 (GODOTCPP_USE_STATIC_CPP=OFF)")
        release.update({"platform": args.platform, "runtimeRequirements": LINUX_REQUIREMENTS,
                        "buildSystem": platform.freedesktop_os_release(), "linuxDependencies": dependencies})
    (output / "RELEASE.json").write_text(json.dumps(release, indent=2) + "\n", encoding="utf-8")
    validate_inventory(output, spec)
    # Export exclusions can remove a global script class while editor tests still
    # pass. Boot the actual executable; release templates disable --script.
    verify_startup(output, spec, checks)
    inventory = {str(path.relative_to(output)).replace("\\", "/"): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in sorted(output.rglob("*")) if path.is_file()}
    (output / "SHA256.json").write_text(json.dumps(inventory, indent=2), encoding="utf-8")
    archive = create_archive(output, spec)
    if args.platform == "linux":
        # Check the exact final archive. Keep extracted-archive evidence outside
        # it so recording the check never changes the bytes just verified.
        verify_archive(archive, output.name, spec, output.parent / (output.name + "-archive-checks"))
    archive_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
    (archive.parent / (archive.name + ".sha256")).write_text(archive_hash + "  " + archive.name + "\n", encoding="utf-8")
    print(json.dumps({"package": str(output), "archive": str(archive),
                      **({"zip": str(archive)} if args.platform == "windows" else {}), "files": len(inventory),
                      "packageVersion": args.package_version,
                      "sha256": archive_hash}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except ValueError as error:
        raise SystemExit(str(error)) from error
