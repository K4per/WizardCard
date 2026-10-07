"""Run two independent processes; compare terminal authoritative replay results."""
import argparse
import json
import subprocess
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--assets", type=Path, default=Path("assets"))
    parser.add_argument("--output", type=Path, default=Path("build/network-smoke"))
    parser.add_argument("--mode", choices=["probe", "client"], default="probe")
    parser.add_argument("--scenario", choices=["all", "normal", "surrender", "reconnect"], default="all")
    parser.add_argument("--port", type=int, default=32181)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    scenarios = ["normal", "surrender", "reconnect"] if args.scenario == "all" else [args.scenario]
    report = []
    for index, scenario in enumerate(scenarios):
        port = args.port + index
        children, logs = [], []
        try:
            for role in ["host", "guest"]:
                output = args.output / f"{scenario}-{role}"
                log = open(output.with_suffix(".log"), "w", encoding="utf-8")
                logs.append(log)
                if args.mode == "probe":
                    command = [str(args.exe.resolve()), role, str(args.assets.resolve()),
                               str(output.with_suffix(".json").resolve()), str(port), scenario, "127.0.0.1"]
                else:
                    command = [str(args.exe.resolve()), "--assets", str(args.assets.resolve()),
                               "--user-data", str(output.resolve()), "--network-smoke", role,
                               "--network-scenario", scenario, "--network-port", str(port)]
                children.append(subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT))
                time.sleep(0.2)
            deadline = time.monotonic() + args.timeout
            while any(p.poll() is None for p in children):
                if any(p.poll() not in (None, 0) for p in children):
                    raise RuntimeError(f"{scenario}: process failed; inspect {args.output}")
                if time.monotonic() > deadline:
                    raise RuntimeError(f"{scenario}: timeout; inspect {args.output}")
                time.sleep(0.05)
            if any(p.returncode != 0 for p in children):
                raise RuntimeError(f"{scenario}: process failed")
            for log in logs:
                log.close()
            results = []
            for role in ["host", "guest"]:
                output = args.output / f"{scenario}-{role}"
                if args.mode == "probe":
                    lines = output.with_suffix(".log").read_text(encoding="utf-8", errors="replace").splitlines()
                    result = json.loads(next(line for line in reversed(lines) if line.startswith("{")))
                else:
                    result = json.loads((output / "network-ui-audit.json").read_text(encoding="utf-8"))
                    assert result["settings"], "Settings were not exercised"
                    assert result["settingsTicks"] > 0, "Network did not tick during settings"
                results.append(result)
            assert results[0]["digest"] == results[1]["digest"], "Replay digests differ"
            assert results[0]["result"] == results[1]["result"] != -1, "Incomplete match"
            if scenario == "reconnect":
                assert results[1]["reconnected"], "Reconnect was not exercised"
            if args.mode=="client":
                assert results[1]["reducedMotion"], "Reduced motion guest was not exercised"
            report.append({"scenario": scenario, "mode": args.mode, "results": results})
            (args.output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
            print(f"{args.mode} {scenario}: passed, {results[0]['commands']} commands, digest {results[0]['digest']}", flush=True)
        finally:
            for p in children:
                if p.poll() is None:
                    p.terminate()
                    try:
                        p.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        p.kill()
                        p.wait()
            for log in logs:
                log.close()


if __name__ == "__main__":
    main()
