#!/usr/bin/env python3
"""Stage a standalone central server release from a compatible Linux build."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile


ROOT = Path(__file__).resolve().parents[1]
VERSION = "2.6.2"
CONTENT_HASH = "cc7c6005e8f96820"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_binary(binary):
    header = subprocess.run(["readelf", "-h", str(binary)], check=True, capture_output=True, text=True).stdout
    if "Advanced Micro Devices X86-64" not in header:
        raise ValueError("server package currently supports Linux x86_64 builds only")
    dependencies = subprocess.run(["ldd", str(binary)], check=True, capture_output=True, text=True).stdout
    if "not found" in dependencies:
        raise ValueError("server binary has unresolved shared libraries:\n" + dependencies)
    return dependencies.strip()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def stage_release(build, output):
    binary = build / "wizard_server"
    if not binary.is_file():
        raise ValueError(f"missing server binary: {binary}")
    dependencies = verify_binary(binary)
    catalog = json.loads((ROOT / "assets/release-v260/catalog.json").read_text(encoding="utf-8"))
    metadata = catalog["metadata"]
    if (metadata.get("rulesVersion") != "2.6.2"
            or metadata.get("cardSetVersion") != "2.6.2"
            or metadata.get("mechanicsRevision") != 11):
        raise ValueError("assets/release-v260 is not the approved production catalogue")
    aggregate = dict(metadata)
    aggregate["cards"] = [json.loads((ROOT / "assets/release-v260" / row["file"]).read_text(encoding="utf-8"))
                          for row in catalog["definitions"]]
    fingerprint = 14695981039346656037
    for byte in json.dumps(aggregate, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8"):
        fingerprint = ((fingerprint ^ byte) * 1099511628211) & ((1 << 64) - 1)
    if f"{fingerprint:016x}" != CONTENT_HASH:
        raise ValueError("production content hash does not match the approved card definitions")
    if output.exists():
        raise FileExistsError(f"refusing to overwrite existing package directory: {output}")
    archive = Path(str(output) + ".tar.gz")
    checksum = Path(str(archive) + ".sha256")
    if archive.exists() or checksum.exists():
        raise FileExistsError("refusing to overwrite an existing release archive or checksum")
    output.parent.mkdir(parents=True, exist_ok=True)
    package_name = output.name

    with tempfile.TemporaryDirectory(prefix=".central-server-package-", dir=output.parent) as temporary:
        stage = Path(temporary) / package_name
        (stage / "bin").mkdir(parents=True)
        (stage / "assets").mkdir()
        (stage / "systemd").mkdir()
        (stage / "docs").mkdir()
        shutil.copy2(binary, stage / "bin/wizard_server")
        shutil.copy2(ROOT / "deploy/bin/wizard-server", stage / "bin/wizard-server")
        (stage / "bin/wizard_server").chmod(0o755)
        (stage / "bin/wizard-server").chmod(0o755)
        shutil.copytree(ROOT / "assets/release-v260", stage / "assets/release-v260")
        shutil.copy2(ROOT / "deploy/server.env.example", stage / "server.env.example")
        shutil.copy2(ROOT / "deploy/systemd/wizard-server.service", stage / "systemd/wizard-server.service")
        shutil.copy2(ROOT / "deploy/systemd/wizard-server-system.service", stage / "systemd/wizard-server-system.service")
        shutil.copy2(ROOT / "deploy/install-system-service.sh", stage / "install-system-service.sh")
        (stage / "install-system-service.sh").chmod(0o755)
        shutil.copy2(ROOT / "deploy/install-user-service.sh", stage / "install-user-service.sh")
        (stage / "install-user-service.sh").chmod(0o755)
        shutil.copy2(ROOT / "docs/central-server-deploy.md", stage / "docs/central-server-deploy.md")
        shutil.copy2(ROOT / "THIRD_PARTY_NOTICES.md", stage / "THIRD_PARTY_NOTICES.md")
        shutil.copytree(ROOT / "licenses", stage / "licenses")

        release = {
            "format": 1,
            "version": VERSION,
            "target": "linux-x86_64",
            "buildEnvironment": platform.platform(),
            "libc": platform.libc_ver(),
            "content": {
                "path": "assets/release-v260",
                "rulesVersion": metadata["rulesVersion"],
                "cardSetVersion": metadata["cardSetVersion"],
                "mechanicsRevision": metadata["mechanicsRevision"],
                "hash": CONTENT_HASH,
            },
            "privateKeyIncluded": False,
        }
        write_json(stage / "RELEASE.json", release)
        inventory = {}
        for path in sorted(stage.rglob("*")):
            if path.is_file() and path.name != "SHA256.json":
                inventory[path.relative_to(stage).as_posix()] = sha256(path)
        write_json(stage / "SHA256.json", inventory)

        shutil.move(str(stage), output)

    with tarfile.open(archive, "w:gz", compresslevel=6) as package:
        package.add(output, arcname=package_name)
    checksum.write_text(f"{sha256(archive)}  {archive.name}\n", encoding="ascii")
    print(json.dumps({
        "output": str(output),
        "archive": str(archive),
        "sha256": sha256(archive),
        "release": release,
        "dependencies": dependencies,
    }, indent=2))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True, help="CMake binary directory")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "dist" / f"WizardCard-CentralServer-{VERSION}-linux-x86_64")
    args = parser.parse_args()
    stage_release(args.build.resolve(), args.output.resolve())


if __name__ == "__main__":
    main()
