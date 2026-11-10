"""Record and verify the locked source used by both release packagers."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def provenance():
    receipt = ROOT / "SOURCE.json"
    if receipt.is_file():
        value = json.loads(receipt.read_text(encoding="utf-8"))
        for name, expected in value["files"].items():
            path = (ROOT / name).resolve()
            if not path.is_relative_to(ROOT) or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
                raise ValueError("Source input differs from locked commit: " + name)
        return {key: value[key] for key in ("sourceCommit", "sourceTree")}
    subprocess.run(["git", "diff-index", "--quiet", "HEAD", "--"], cwd=ROOT, check=True)
    return {"sourceCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "sourceTree": subprocess.check_output(["git", "rev-parse", "HEAD^{tree}"], cwd=ROOT, text=True).strip()}


def export(output):
    value = provenance()
    names = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", "HEAD"], cwd=ROOT, text=True).splitlines()
    names = [name for name in names if Path(name).parts[0] in
             {"src", "include", "tests", "tools", "cmake", "deploy", "licenses", "docs"}
             or name in {"CMakeLists.txt", "CMakePresets.json", "LICENSE", "README.md", "THIRD_PARTY_NOTICES.md"}
             or (name.startswith("assets/") and name.endswith(".json") and "source-archive" not in name)]
    paths = sorted({name.split("/")[0] for name in names if not name.startswith("assets/")})
    paths += [name for name in names if name.startswith("assets/")]
    # git archive applies checkout conversions; Windows defaults would make
    # the exported Linux launchers and installers unusable by /bin/sh.
    archive = subprocess.check_output(
        ["git", "-c", "core.autocrlf=false", "-c", "core.eol=lf", "archive", "HEAD", "--", *paths], cwd=ROOT)
    value["files"] = {}
    with tarfile.open(fileobj=io.BytesIO(archive)) as source, tarfile.open(output, "w:gz") as target:
        for member in source:
            data = source.extractfile(member).read() if member.isfile() else None
            if data is not None:
                value["files"][member.name] = hashlib.sha256(data).hexdigest()
            target.addfile(member, io.BytesIO(data) if data is not None else None)
        data = (json.dumps(value, indent=2) + "\n").encode()
        member = tarfile.TarInfo("SOURCE.json")
        member.size = len(data)
        target.addfile(member, io.BytesIO(data))
    print(json.dumps({"sourceCommit": value["sourceCommit"], "files": len(value["files"]), "output": str(output)}))


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--export", type=Path, required=True)
    export(parser.parse_args().export)
