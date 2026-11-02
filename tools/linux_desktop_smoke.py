"""Run rendered Godot smoke under an existing X11 or Wayland display server."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--driver", choices=("x11", "wayland"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--assets", type=Path, default=ROOT / "assets/release-v260")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = output / "report.json"
    report.unlink(missing_ok=True)
    report.with_suffix(".json.ready").unlink(missing_ok=True)
    command = [str(args.godot.resolve()), "--path", str(ROOT / "godot"),
               "--display-driver", args.driver, "--audio-driver", "Dummy",
               "--script", "res://tests/linux_desktop_smoke.gd", "--",
               "--assets", str(args.assets.resolve()), "--user-data", str(output / "data"),
               "--report", str(report), "--expected-driver", args.driver]
    if args.driver == "x11":
        command.append("--real-input")
    with (output / "startup.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen(command, cwd=output, stdout=log, stderr=subprocess.STDOUT)
        try:
            if args.driver == "x11":
                deadline = time.monotonic() + 30
                while not Path(str(report) + ".ready").exists():
                    if process.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError("Desktop smoke did not become ready; see " + str(output / "startup.log"))
                    time.sleep(0.1)
                windows = subprocess.check_output(["xdotool", "search", "--onlyvisible", "--pid", str(process.pid)], text=True).split()
                subprocess.run(["xdotool", "windowfocus", "--sync", windows[-1]], check=True, timeout=10)
                subprocess.run(["xdotool", "key", "--clearmodifiers", "Return"], check=True, timeout=10)
            code = process.wait(timeout=90)
            if code:
                raise RuntimeError(f"Desktop smoke exited {code}; see {output / 'startup.log'}")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
    diagnostics = (output / "startup.log").read_text(encoding="utf-8", errors="replace")
    if any(marker in diagnostics for marker in ("ERROR:", "SCRIPT ERROR", "handle_crash:")):
        raise RuntimeError(diagnostics)
    result = json.loads(report.read_text(encoding="utf-8"))
    if result["failures"] or result["displayDriver"].lower() != args.driver:
        raise RuntimeError(str(result))
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
