"""Verify an extracted release, optionally running two packaged clients over public TLS."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time
import zipfile

from package_godot import PLATFORMS, verify_startup


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def verify_network(output, evidence, args):
    processes, logs = [], []
    try:
        for role in ("host", "guest"):
            report = evidence / (role + ".json")
            command = [str(output / "WizardCard.exe"), "--headless", "--log-file",
                       str(evidence / (role + "-godot.log")), "--", "--verify-central", "--role", role,
                       "--user-data", str(evidence / (role + "-data")),
                       "--report", str(report), "--stable-ms", "8000", "--gameplay-actions", "40"]
            if args.server_host:
                command += ["--hostname", args.server_host, "--port", str(args.server_port),
                            "--trusted-ca", str(args.trusted_ca.resolve())]
            if role == "guest":
                command += ["--room-code", room]
            log = (evidence / (role + ".log")).open("w", encoding="utf-8")
            logs.append(log)
            process = subprocess.Popen(command, cwd=evidence, stdout=log, stderr=subprocess.STDOUT)
            processes.append(process)
            if role == "host":
                deadline = time.monotonic() + 40
                while not report.exists():
                    if process.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError(f"Packaged host failed to create room; inspect {evidence}")
                    time.sleep(.05)
                room = read_json(report)["roomCode"]
                print("Public room=" + room, flush=True)
        deadline = time.monotonic() + 360
        while any(p.poll() is None for p in processes):
            if any(p.poll() not in (None, 0) for p in processes) or time.monotonic() > deadline:
                raise RuntimeError(f"Packaged match failed; inspect {evidence}")
            time.sleep(.05)
        for log in logs:
            log.flush()
        for path in evidence.glob("*.log"):
            if any(marker in path.read_text(encoding="utf-8", errors="replace")
                   for marker in ("ERROR:", "SCRIPT ERROR", "handle_crash:")):
                raise RuntimeError(f"Runtime error: {path}")
        recordings = []
        clients = []
        for role in ("host", "guest"):
            clients.append(read_json(evidence / (role + ".json")))
            assert clients[-1]["phase"] == "completed" and clients[-1]["replayDelivered"]
            assert clients[-1]["releasedSpellObserved"], "Both clients must observe a real spell resolution"
            replays = list((evidence / (role + "-data/replays")).glob("network-*.json"))
            assert len(replays) == 1, (role, replays)
            recordings.append(read_json(replays[0]))
        assert recordings[0] == recordings[1], "Both clients must receive the same verified replay"
        recording = recordings[0]
        for name in ("programVersion", "rulesVersion", "cardSetVersion"):
            assert recording[name] == "2.6.3", (name, recording[name])
        assert recording["digestSchema"] == 4 and recording["mechanicsRevision"] == 12
        commands = Counter(item["command"]["type"] for item in recording["commands"])
        assert commands[21] >= 4 and commands[18] == 2, commands
        assert all(client["openingPrivacy"] and client["openingTieObserved"] for client in clients)
        assert sum(commands.values()) >= 47, commands  # opening + 40 actions + surrender
        assert commands[17] + commands[5] > 0, "The match must include a real spell activation"
        return {"endpoint": f"{args.server_host or 'aq.rainplay.cn'}:{args.server_port}", "room": room,
                "clients": clients, "commands": dict(commands), "commandCount": sum(commands.values()),
                "matchingReplayDigest": recording["finalDigest"], "identicalReplay": True,
                "verifiedManualReconnects": 2, "unexpectedDisconnects": 0}
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
        for process in processes:
            process.wait(timeout=10)
        for log in logs:
            log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--production", action="store_true", help="verify the product default without address or certificate overrides")
    parser.add_argument("--server-host")
    parser.add_argument("--server-port", type=int, default=23513)
    parser.add_argument("--trusted-ca", type=Path)
    args = parser.parse_args()
    if args.production and (args.server_host or args.trusted_ca):
        parser.error("--production uses only the bundled defaults")
    if args.server_host and not args.trusted_ca:
        parser.error("--server-host requires --trusted-ca")
    archive, evidence = args.archive.resolve(), args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    expected = Path(str(archive) + ".sha256").read_text().split()[0]
    assert digest(archive) == expected, "Archive checksum mismatch"
    with tempfile.TemporaryDirectory(prefix="wizardcard-v263-extracted-") as directory:
        extraction = Path(directory)
        with zipfile.ZipFile(archive) as package:
            assert package.testzip() is None, "ZIP CRC failure"
            for member in package.namelist():
                assert (extraction / member).resolve().is_relative_to(extraction)
            package.extractall(extraction)
        output = extraction / archive.stem
        inventory = read_json(output / "SHA256.json")
        for name, expected in inventory.items():
            assert digest(output / name) == expected, name
        assert set(inventory) == {p.relative_to(output).as_posix() for p in output.rglob("*")
                                  if p.is_file() and p.name != "SHA256.json"}
        release = read_json(output / "RELEASE.json")
        for name in ("packageVersion", "programVersion", "rulesVersion", "cardSetVersion"):
            assert release[name] == "2.6.3", (name, release[name])
        assert release["contentHash"] == "61f9b39a65c02fff" and release["mechanicsRevision"] == 12
        assert release["bridgeSha256"] == digest(output / "bin/wizard_godot_bridge.release.dll")
        verify_startup(output, PLATFORMS["windows"], evidence, "extracted-startup")
        guide_command = [str(output / "WizardCard.exe"), "--headless", "--", "--verify-guide",
                         "--user-data", str(evidence / "offline-guide-data"), "--report", str(evidence / "offline-guide.json")]
        guide = subprocess.run(guide_command, cwd=evidence, capture_output=True, text=True, timeout=45)
        (evidence / "offline-guide.log").write_text(guide.stdout + guide.stderr, encoding="utf-8")
        assert guide.returncode == 0 and read_json(evidence / "offline-guide.json")["passed"], guide.stdout + guide.stderr
        report = {"archive": str(archive), "bytes": archive.stat().st_size, "sha256": digest(archive),
                  "verifiedFiles": len(inventory), "zipCrc": "passed", "release": release,
                  "startupFromUnrelatedDirectory": "passed", "offlineGuide": read_json(evidence / "offline-guide.json")}
        if args.server_host or args.production:
            report["network"] = verify_network(output, evidence, args)
        (evidence / "verification.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
