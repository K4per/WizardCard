"""Stage frontend tests with the exact CMake bridge, independent of editor flavor."""
import argparse
import os
from pathlib import Path
import shutil


def link_or_copy(source, destination):
    try:
        os.link(source, destination)
    except OSError:
        shutil.copy2(source, destination)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bridge", type=Path, required=True)
    args = parser.parse_args()
    source, output, bridge = (p.resolve() for p in (args.source, args.output, args.bridge))
    if not bridge.is_file():
        raise SystemExit(f"Build the test bridge first: {bridge}")
    marker = output / ".wizard-frontend-test-project"
    if output.exists():
        if not marker.is_file() or output == source or source.is_relative_to(output):
            raise SystemExit(f"Refusing to replace an unrecognized directory: {output}")
        shutil.rmtree(output)
    shutil.copytree(source, output, copy_function=link_or_copy,
                    ignore=shutil.ignore_patterns(".godot", "bin", "*.gdextension", "export_presets.cfg"))
    marker.touch()
    extension = '[configuration]\nentry_symbol="wizard_bridge_init"\ncompatibility_minimum="4.7"\nreloadable=false\n[libraries]\n'
    for system in ("windows", "linux"):
        for flavor in ("debug", "release"):
            extension += f'{system}.{flavor}.x86_64="{bridge.as_posix()}"\n'
    (output / "wizard_bridge.gdextension").write_text(extension, encoding="utf-8")
    print(f"Frontend test project: {output}; bridge: {bridge}")


if __name__ == "__main__":
    main()
