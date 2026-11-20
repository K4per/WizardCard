"""Play the AI, tutorial and replay entry points from a Windows release ZIP."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile


def sha256(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--replay", required=True, type=Path, help="Existing recording valid for this release")
    parser.add_argument("--evidence", required=True, type=Path, help="New or empty output directory")
    args = parser.parse_args()
    evidence = args.evidence.resolve()
    evidence.mkdir(exist_ok=True, parents=True)
    if any(evidence.iterdir()):
        parser.error("--evidence must be empty so old captures cannot mask failures")
    results = {}
    with tempfile.TemporaryDirectory(prefix="wizardcard-play-") as work:
        work = Path(work)
        with zipfile.ZipFile(args.archive) as archive:
            archive.extractall(work)
        executable, = work.rglob("WizardCard.exe")
        for route in ("ai", "tutorial", "replay"):
            data = work / ("player-" + route)
            data.mkdir()
            if route == "replay":
                (data / "replays").mkdir()
                shutil.copyfile(args.replay, data / "replays" / "acceptance.json")
            log = evidence / (route + ".log")
            command = [str(executable), "--log-file", str(log), "--",
                       "--verify-play-entry", route, "--user-data", str(data)]
            subprocess.run(command, cwd=work, check=True, timeout=75)
            contents = log.read_text(encoding="utf-8", errors="replace")
            if any(marker in contents for marker in ("ERROR:", "SCRIPT ERROR", "handle_crash:")):
                raise RuntimeError(f"Runtime error in {log}")
            screenshot = data / (route + ".png")
            if not screenshot.is_file() or screenshot.stat().st_size < 1024:
                raise RuntimeError(f"Missing rendered capture for {route}. See {log}")
            dest = evidence / screenshot.name
            shutil.copyfile(screenshot, dest)
            results[route] = {"image": dest.name, "sha256": sha256(dest)}
            print(f"PASS {route}: {dest}", flush=True)
    report = {"archive": args.archive.name, "archiveSha256": sha256(args.archive),
              "replaySha256": sha256(args.replay), "entries": results}
    (evidence / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
