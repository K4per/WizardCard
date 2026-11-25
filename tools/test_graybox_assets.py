"""Release art includes only approved card illustrations, effects, branding and fonts."""
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
    def test_manifest_covers_the_shipped_and_supported_card_illustrations_and_branding(self):
        expected = {"font.cjk", "font.cjk.bold", "font.cjk.black",
                    "font.cjk.license", "font.cjk.weights.license"}
        for name in ("assets/release-v260/cards.json", "assets/cards.json", "godot/data/card-previews-v2.json"):
            data = json.loads((ROOT / name).read_bytes())
            expected.update("illustration." + card.get("illustrationId", card["id"]) for card in data["cards"])
        manifest = json.loads((ROOT / "godot/art/assets.json").read_bytes())
        ids = {entry["id"] for entry in manifest["assets"]}
        self.assertFalse(expected - ids)
        self.assertEqual({name for name in ids if not name.startswith("illustration.")},
                         frontend.FONT_IDS | {"branding.app.icon", "c2d.v2.menu.logo", "fx.damage-border"})
        by_id = {entry["id"]: entry for entry in manifest["assets"]}
        self.assertEqual(by_id["branding.app.icon"]["sha256"],
                         "a7f523a893477b28c1503980e4a24f932233717bfa06a8bc8b1468b09a3a7e4f")
        self.assertEqual(by_id["c2d.v2.menu.logo"]["sha256"],
                         "470c860be6bc68cd696472445a94a76c46f6f66477b9c821b5af51a225193b40")
        self.assertEqual(by_id["c2d.v2.menu.logo"]["region"], [440, 268, 1040, 543])
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
                self.assertEqual(frontend.check_art(), 220)
                (art / "orphan.png.import").touch()
                with self.assertRaisesRegex(ValueError, "orphan.png.import"):
                    frontend.check_art()

    def test_all_runtime_images_have_a_manifest_entry_and_no_orphan_import(self):
        manifest = json.loads((ROOT / "godot/art/assets.json").read_bytes())
        illustrations = {entry["file"].removeprefix("res://") for entry in manifest["assets"]
                         if entry["id"].startswith("illustration.")}
        runtime = ROOT / "godot/art"
        images = {file.relative_to(ROOT / "godot").as_posix() for file in runtime.rglob("*")
                  if file.suffix.lower() in {".png", ".jpg", ".jpeg", ".svg", ".webp", ".ico"}}
        self.assertEqual(illustrations | {"art/menu-v2/logo-1080p.png", "art/branding/wizardcard-app-v1.png",
                                         "art/fx/damage-border.png"}, images)
        for file in runtime.rglob("*.import"):
            self.assertTrue(file.with_suffix("").is_file(), file)


@unittest.skipUnless(os.name == "nt", "requires Win32 resource APIs")
class NativeIconTests(unittest.TestCase):
    def test_default_godot_icon_is_rejected(self):
        from tools.windows_icons import verify_application_icon
        template = ROOT / ".cache/godot-templates/templates/windows_release_x86_64.exe"
        if not template.is_file():
            self.skipTest("official export template is unavailable")
        with self.assertRaisesRegex(ValueError, "WizardCard"):
            verify_application_icon(template)


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

    def test_only_approved_textures_are_shipped(self):
        card = "res://art/cards/card.png"
        entries = [{"id": "illustration.card", "file": card}]
        entries += [{"id": name, "file": path} for name, path in frontend.BRAND_FILES.items()]
        entries.append({"id": "fx.damage-border", "file": "res://art/fx/damage-border.png"})
        files = {}
        icon = frontend.BRAND_FILES["branding.app.icon"].removeprefix("res://")
        files[icon] = b"embedded raw application PNG"
        for entry in entries:
            path = entry["file"]
            name = path.removeprefix("res://")
            identity = hashlib.md5(path.encode("utf8")).hexdigest()
            texture = f".godot/imported/{Path(name).name}-{identity}.ctex"
            files[texture] = b"imported image"
            files[name + ".import"] = f'[remap]\npath="res://{texture}"\n\0'.encode()
            if name in files:
                entry["sha256"] = hashlib.sha256(files[name]).hexdigest()
        manifest = json.dumps({"assets": entries}).encode("utf8")
        files["art/assets.json"] = manifest
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
            for name in (icon, frontend.BRAND_FILES["c2d.v2.menu.logo"].removeprefix("res://") + ".import",
                         "art/fx/damage-border.png.import"):
                with self.subTest(missing=name):
                    self.write_pck(pck, {key: value for key, value in files.items() if key != name})
                    with self.assertRaises((ValueError, KeyError)):
                        audit_pck(pck, manifest_file)
            wrong = ".godot/imported/background.png-abcdef.ctex"
            original = next(name for name in files if name.startswith(".godot/imported/card.png-"))
            files[wrong] = files.pop(original)
            files["art/cards/card.png.import"] = f'[remap]\npath="res://{wrong}"\n'.encode()
            self.write_pck(pck, files)
            with self.assertRaisesRegex(ValueError, "art/cards/card.png"):
                audit_pck(pck, manifest_file)
