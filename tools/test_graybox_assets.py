"""The release must contain exactly the card illustrations and supporting fonts."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools import prepare_godot_frontend_assets as frontend
from tools.verify_godot_pck_art import audit_pck


ROOT = Path(__file__).resolve().parents[1]


class GrayboxAssetsTests(unittest.TestCase):
    def test_manifest_covers_the_shipped_and_supported_card_illustrations_only(self):
        expected = {"font.cjk", "font.cjk.bold", "font.cjk.black",
                    "font.cjk.license", "font.cjk.weights.license"}
        for name in ("assets/release-v260/cards.json", "assets/cards.json", "godot/data/card-previews-v2.json"):
            data = json.loads((ROOT / name).read_bytes())
            expected.update("illustration." + card.get("illustrationId", card["id"]) for card in data["cards"])
        manifest = json.loads((ROOT / "godot/art/assets.json").read_bytes())
        ids = {entry["id"] for entry in manifest["assets"]}
        self.assertFalse(expected - ids)
        self.assertEqual({name for name in ids if not name.startswith("illustration.")}, frontend.FONT_IDS)
        illustrations = [entry for entry in manifest["assets"] if entry["id"].startswith("illustration.")]
        self.assertEqual(len(illustrations), 212)
        self.assertEqual(sum(entry["id"].startswith("illustration.historical.") for entry in illustrations), 18)

    def test_clean_runtime_tree_and_orphan_import(self):
        with tempfile.TemporaryDirectory() as workspace:
            art = Path(workspace) / "godot/art"
            shutil.copytree(ROOT / "godot/art", art, ignore=shutil.ignore_patterns("*.import"))
            for name in ("assets/release-v260/cards.json", "assets/cards.json", "godot/data/card-previews-v2.json"):
                path = Path(workspace) / name
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, path)
            with patch.multiple(frontend, ROOT=art.parents[1], GODOT=art.parent, ART=art):
                self.assertEqual(frontend.check_art(), 217)
                (art / "orphan.png.import").touch()
                with self.assertRaisesRegex(ValueError, "orphan.png.import"):
                    frontend.check_art()

    def test_all_runtime_images_have_an_illustration_entry_and_no_orphan_import(self):
        manifest = json.loads((ROOT / "godot/art/assets.json").read_bytes())
        illustrations = {entry["file"].removeprefix("res://") for entry in manifest["assets"]
                         if entry["id"].startswith("illustration.")}
        runtime = ROOT / "godot/art"
        images = {file.relative_to(ROOT / "godot").as_posix() for file in runtime.rglob("*")
                  if file.suffix.lower() in {".png", ".jpg", ".jpeg", ".svg", ".webp", ".ico"}}
        self.assertEqual(illustrations, images)
        for file in runtime.rglob("*.import"):
            self.assertTrue(file.with_suffix("").is_file(), file)


@unittest.skipUnless(os.name == "nt", "requires Win32 resource APIs")
class NativeIconTests(unittest.TestCase):
    def test_remove_icons_from_real_windows_template_only(self):
        from tools.windows_icons import find_icons, strip_icons
        template = ROOT / ".cache/godot-templates/templates/windows_release_x86_64.exe"
        if not template.is_file():
            self.skipTest("official export template is unavailable")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "player.exe"
            shutil.copyfile(template, executable)
            self.assertEqual({kind for kind, name, language in find_icons(executable)}, {3, 14})
            strip_icons(executable)
            self.assertEqual(find_icons(executable), [])
            strip_icons(executable)


class ExportedArtTests(unittest.TestCase):
    def write_pck(self, path, files):
        # Minimal external PCK v4 with an unencrypted, end-of-file directory.
        body = bytearray()
        directory = bytearray(struct.pack("<I", len(files)))
        for name, data in files.items():
            encoded = name.encode("utf8")
            encoded += b"\0" * (-len(encoded) % 4)
            directory.extend(struct.pack("<I", len(encoded)) + encoded)
            directory.extend(struct.pack("<QQ16sI", len(body), len(data), hashlib.md5(data).digest(), 0))
            body.extend(data)
        base = 112
        header = struct.pack("<4sIIIIIQQ", b"GDPC", 4, 4, 7, 2, 2, base, base + len(body))
        path.write_bytes(header.ljust(base, b"\0") + body + directory)

    def test_only_the_declared_card_texture_is_shipped(self):
        illustration = "res://art/cards/card.png"
        identity = hashlib.md5(illustration.encode("utf8")).hexdigest()
        texture = f".godot/imported/card.png-{identity}.ctex"
        manifest = json.dumps({"assets": [{"id": "illustration.card", "file": illustration}]}).encode("utf8")
        files = {"art/assets.json": manifest, texture: b"card texture",
                 "art/cards/card.png.import": f'[remap]\npath="res://{texture}"\n\0'.encode()}
        with tempfile.TemporaryDirectory() as directory:
            manifest_file = Path(directory) / "manifest.json"
            pck = Path(directory) / "test.pck"
            manifest_file.write_bytes(manifest)
            self.write_pck(pck, files)
            self.assertEqual(audit_pck(pck, manifest_file), 1)
            for extra in ("art/background.png", ".godot/imported/decoration.ctex", "ui/button.png"):
                with self.subTest(extra=extra):
                    self.write_pck(pck, {**files, extra: b"unlisted"})
                    with self.assertRaises(ValueError):
                        audit_pck(pck, manifest_file)
            wrong = ".godot/imported/background.png-abcdef.ctex"
            files[wrong] = files.pop(texture)
            files["art/cards/card.png.import"] = f'[remap]\npath="res://{wrong}"\n'.encode()
            self.write_pck(pck, files)
            with self.assertRaisesRegex(ValueError, "art/cards/card.png"):
                audit_pck(pck, manifest_file)
