"""Portable package contracts: layout, dependencies, modes and cwd isolation."""
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
import zipfile

from tools import package_godot as package


class PlatformTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "posix", "fixture uses a Unix executable")
    def test_cli_finds_godot_command_on_path_even_with_project_directory_named_godot(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            executable = root / "godot"
            executable.write_text("#!/bin/sh\nprintf 'path-lookup-probe\\n'\n")
            executable.chmod(0o755)
            result = subprocess.run(
                [os.sys.executable, str(package.ROOT / "tools/package_godot.py"),
                 "--godot", "godot", "--build", str(root)],
                cwd=package.ROOT, env={**os.environ, "PATH": str(root) + os.pathsep + os.environ["PATH"]},
                capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("found path-lookup-probe", result.stderr)

    def test_windows_defaults_keep_existing_paths(self):
        spec = package.PLATFORMS["windows"]
        self.assertEqual(spec.preset, "Windows Desktop")
        self.assertEqual(spec.executable, "WizardCard.exe")
        self.assertEqual(spec.bridge, "bin/wizard_godot_bridge.release.dll")
        self.assertEqual(spec.native_tool(Path("build/windows"), "Release", "wizard_cli"),
                         Path("build/windows/Release/wizard_cli.exe"))
        self.assertEqual(spec.archive_suffix, ".zip")

    def test_linux_spec_matches_export_and_single_configuration(self):
        spec = package.PLATFORMS["linux"]
        self.assertEqual(spec.preset, "Linux Desktop")
        self.assertEqual(spec.executable, "WizardCard.x86_64")
        self.assertEqual(spec.bridge, "bin/libwizard_godot_bridge.release.so")
        self.assertEqual(spec.native_tool(Path("build/linux-godot-release"), "Release", "wizard_network_probe"),
                         Path("build/linux-godot-release/wizard_network_probe"))
        self.assertEqual(spec.archive_suffix, ".tar.gz")
        self.assertEqual(spec.suffix, "linux-x86_64")

    def test_development_accepts_same_commit_but_release_requires_official(self):
        package.validate_godot_version("4.7.2.stable.official.ed1daf0bf", "linux")
        package.validate_godot_version("4.7.2.stable.arch_linux.ed1daf0bf", "linux", development=True)
        for version in ("4.7.2.stable.arch_linux.ed1daf0bf", "4.7.2.stable.official.deadbeef", "4.7.3.stable.official.ed1daf0bf"):
            with self.subTest(version=version), self.assertRaisesRegex(ValueError, "Godot"):
                package.validate_godot_version(version, "linux")
        with self.assertRaises(ValueError):
            package.validate_godot_version("4.7.2.stable.arch_linux.ed1daf0bf", "windows", development=True)


class ArchiveTests(unittest.TestCase):
    def test_tar_extract_preserves_executable_modes_and_sibling_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "WizardCard-linux"
            (output / "bin").mkdir(parents=True)
            for relative in ("WizardCard.x86_64", "run.sh", "bin/libwizard_godot_bridge.release.so"):
                path = output / relative
                path.write_text("executable")
                path.chmod(0o755)
            (output / "WizardCard.pck").write_text("resources")
            archive = package.create_archive(output, package.PLATFORMS["linux"])
            with tarfile.open(archive) as source:
                source.extractall(root / "unpacked", filter="data")
            extracted = root / "unpacked" / output.name
            self.assertEqual((extracted / "WizardCard.pck").read_text(), "resources")
            for relative in ("WizardCard.x86_64", "run.sh", "bin/libwizard_godot_bridge.release.so"):
                self.assertEqual((extracted / relative).stat().st_mode & 0o777, 0o755)

    def test_windows_still_creates_zip_with_package_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "WizardCard-windows"
            output.mkdir()
            (output / "WizardCard.exe").write_bytes(b"exe")
            archive = package.create_archive(output, package.PLATFORMS["windows"])
            with zipfile.ZipFile(archive) as source:
                self.assertEqual(source.read("WizardCard-windows/WizardCard.exe"), b"exe")

    def test_linux_inventory_rejects_wrong_platform_debug_sfml_and_sources(self):
        for relative in ("bad.exe", "bin/bad.DLL", "bin/libwizard_godot_bridge.debug.so",
                         "licenses/SFML.txt", "src/core.cpp", "tests/fixture.json", "foo.cpp",
                         "bin/libstdc++.so.6", "bin/libgcc_s.so.1"):
            with self.subTest(relative=relative), tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary)
                path = output / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"bad")
                with self.assertRaisesRegex(ValueError, "forbidden"):
                    package.validate_inventory(output, package.PLATFORMS["linux"])

    @unittest.skipUnless(os.name == "posix", "requires Unix executable mode")
    def test_startup_uses_extracted_siblings_from_an_unrelated_cwd(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "WizardCard-linux"
            (output / "assets").mkdir(parents=True)
            (output / "assets/cards.json").write_text("sibling resources")
            # Stand-in executable exercises the same OS contract as Godot:
            # sibling assets, isolated cwd, --log-file and --user-data.
            executable = output / "WizardCard.x86_64"
            executable.write_text("#!/usr/bin/env python3\n"
                                  "import json, pathlib, sys\n"
                                  "args = sys.argv\n"
                                  "assert pathlib.Path.cwd() != pathlib.Path(__file__).parent\n"
                                  "assert (pathlib.Path(__file__).parent / 'assets/cards.json').read_text() == 'sibling resources'\n"
                                  "data = pathlib.Path(args[args.index('--user-data') + 1])\n"
                                  "data.mkdir(parents=True)\n"
                                  "(data / 'settings.json').write_text('{}')\n"
                                  "pathlib.Path(args[args.index('--log-file') + 1]).write_text('Godot Engine v4.7.2\\n')\n")
            executable.chmod(0o755)
            bridge = output / "bin/libwizard_godot_bridge.release.so"
            bridge.parent.mkdir()
            bridge.write_bytes(b"bridge")
            bridge.chmod(0o755)
            package.write_linux_launcher(output)
            archive = package.create_archive(output, package.PLATFORMS["linux"])
            evidence = root / "evidence"
            package.verify_archive(archive, output.name, package.PLATFORMS["linux"], evidence)
            self.assertTrue((evidence / "extracted-startup.log").is_file())
            self.assertTrue((evidence / "extracted-launcher.log").is_file())


@unittest.skipUnless(os.name == "posix" and shutil.which("cc") and shutil.which("ldd") and shutil.which("readelf"),
                     "requires Linux ELF tools and C compiler")
class DependencyTests(unittest.TestCase):
    def test_missing_transitive_library_fails_before_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "dependency.c").write_text("int dependency(void) { return 7; }")
            (root / "bridge.c").write_text("extern int dependency(void); int bridge(void) { return dependency(); }")
            library = root / "libwizard_missing.so"
            subprocess.run(["cc", "-shared", "-fPIC", str(root / "dependency.c"), "-o", str(library)], check=True)
            bridge = root / "bridge.so"
            subprocess.run(["cc", "-shared", "-fPIC", str(root / "bridge.c"), "-L" + str(root),
                            "-Wl,-rpath," + str(root), "-lwizard_missing", "-o", str(bridge)], check=True)
            self.assertIn("libwizard_missing.so", package.inspect_linux_binary(bridge)["ldd"])
            library.unlink()
            with self.assertRaisesRegex(ValueError, "not found"):
                package.inspect_linux_binary(bridge)

    def test_non_elf_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "wrong.so"
            path.write_text("not an ELF file")
            with self.assertRaisesRegex(ValueError, "ELF"):
                package.inspect_linux_binary(path)


if __name__ == "__main__":
    unittest.main()
