"""Verify first-time extension discovery in a fresh project, retaining evidence."""
from pathlib import Path
import argparse
import shutil
import subprocess
import uuid

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot', required=True)
    parser.add_argument('--probe', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    source = Path(args.probe).resolve()
    target = Path(args.output).resolve() / uuid.uuid4().hex
    (target / 'tests').mkdir(parents=True)
    for name in ('project.godot', 'wizard_bridge.gdextension', 'tests/bridge_smoke.gd', 'tests/boundary_smoke.gd'):
        shutil.copyfile(source / name, target / name)
    process = subprocess.run([args.godot, '--headless', '--path', str(target), '--import', '--frame-delay', '1000'],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=55)
    output = process.stdout.decode('utf-8', errors='replace')
    (target / 'import.log').write_text(output, encoding='utf-8')
    print(output)
    if process.returncode or 'SCRIPT ERROR' in output or 'ERROR:' in output:
        raise SystemExit(f'Cold import failed: exit={process.returncode}, evidence={target}')
    print(f'Cold import passed; evidence={target}')

if __name__ == '__main__':
    main()
