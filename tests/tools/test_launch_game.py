from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


class LaunchGameTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        shutil.copy2(REPO / "launch_game.sh", self.root)
        self.installation = self.root / "game installation"
        self.installation.mkdir()
        (self.installation / "sTrAtEgY.dLl").touch()
        self.config = self.root / "config.ini"
        self.config.write_text(f"install_dir={self.installation}\n")
        self.pack = self.root / "work/hd/gameplay-pack"
        for relative in ("build/native/rebellion", "build/native/Rebellion.app/Contents/MacOS/Rebellion"):
            executable = self.root / relative
            executable.parent.mkdir(parents=True, exist_ok=True)
            executable.write_text('#!/usr/bin/env bash\nprintf "%s" "${REB_HD_ASSET_DIR-unset}" > launched.txt\n')
            executable.chmod(0o755)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        uv = self.bin / "uv"
        uv.write_text(
            '#!/usr/bin/env bash\n'
            'printf "%s\\n" "$@" > generated.txt\n'
            'while [[ $# -gt 0 ]]; do\n'
            '  if [[ "$1" == --out ]]; then mkdir -p "$2"; fi\n'
            '  shift\n'
            'done\n'
            'exit "${FAKE_UV_STATUS:-0}"\n'
        )
        uv.chmod(0o755)
        self.environment = os.environ.copy()
        self.environment.pop("REB_HD_ASSET_DIR", None)
        self.environment.update({"REBELLION_CONFIG": str(self.config), "PATH": f"{self.bin}:{os.environ['PATH']}"})

    def launch_result(self) -> subprocess.CompletedProcess[str]:
        result = subprocess.run(
            ["bash", str(self.root / "launch_game.sh"), "--no-build"], env=self.environment,
            capture_output=True, text=True, check=True,
        )
        self.assertTrue((self.root / "launched.txt").exists(), result.stderr)
        return result

    def launch(self) -> str:
        self.launch_result()
        return (self.root / "launched.txt").read_text()

    def test_missing_pack_uses_selected_installation_and_is_reused(self) -> None:
        self.assertEqual(self.launch(), str(self.pack))
        arguments = (self.root / "generated.txt").read_text().splitlines()
        self.assertEqual(arguments[arguments.index("--module") + 1], str(self.installation / "sTrAtEgY.dLl"))
        self.assertEqual(arguments[arguments.index("--recipe") + 1], str(self.root / "tools/hd_gameplay_recipe.json"))
        (self.root / "generated.txt").unlink()
        self.assertEqual(self.launch(), str(self.pack))
        self.assertFalse((self.root / "generated.txt").exists())

    def test_environment_override_and_disable_skip_generation(self) -> None:
        for value in ("", "/custom/hd"):
            with self.subTest(value=value):
                self.environment["REB_HD_ASSET_DIR"] = value
                self.assertEqual(self.launch(), value)
                self.assertFalse((self.root / "generated.txt").exists())

    def test_config_override_and_disable_skip_generation(self) -> None:
        for value in ("", "/custom/hd"):
            with self.subTest(value=value):
                self.config.write_text(f"install_dir={self.installation}\n HD_ASSET_DIR = {value}\n")
                self.assertEqual(self.launch(), "unset")
                self.assertFalse((self.root / "generated.txt").exists())

    def test_config_parser_ignores_registry_and_uses_first_setting(self) -> None:
        self.config.write_bytes(
            f"; ignored\r\n INSTALL_DIR = {self.installation}\r\ninstall_dir=/missing\r\n"
            "[ REGISTRY ]\r\nhd_asset_dir=\r\n".encode()
        )
        self.assertEqual(self.launch(), str(self.pack))

    def launch_failure(self) -> str:
        result = subprocess.run(
            ["bash", str(self.root / "launch_game.sh"), "--no-build"], env=self.environment,
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / "launched.txt").exists())
        return result.stderr

    def test_missing_original_assets_fail_with_no_hd_hint(self) -> None:
        (self.installation / "sTrAtEgY.dLl").unlink()
        self.assertIn("STRATEGY.DLL was not found", self.launch_failure())
        self.assertFalse((self.root / "generated.txt").exists())

    def test_missing_uv_fails_with_no_hd_hint(self) -> None:
        self.environment["PATH"] = "/usr/bin:/bin"
        self.assertIn("--no-hd", self.launch_failure())
        self.assertFalse(self.pack.exists())

    def test_outdated_icons_rebuild_without_asking_and_current_icons_are_kept(self) -> None:
        self.pack.mkdir(parents=True)
        manifest = self.pack / "manifest.json"
        manifest.write_text('{"encyclopedia_extraction": {"revision": 4}}\n')
        self.assertIn("Updating HD icons", self.launch_result().stdout)
        arguments = (self.root / "generated.txt").read_text().splitlines()
        self.assertTrue(arguments[arguments.index("--install-dir") - 1].endswith("build_encyclopedia_icons.py"))
        (self.root / "generated.txt").unlink()
        manifest.write_text('{"encyclopedia_extraction": {"revision": 4, "pack_revision": 2}}\n')
        self.assertEqual(self.launch(), str(self.pack))
        self.assertFalse((self.root / "generated.txt").exists())

    def test_failed_generation_removes_partial_pack_and_retries(self) -> None:
        self.environment["FAKE_UV_STATUS"] = "1"
        self.assertEqual(self.launch(), "unset")
        self.assertFalse(self.pack.exists())
        self.environment["FAKE_UV_STATUS"] = "0"
        self.assertEqual(self.launch(), str(self.pack))


if __name__ == "__main__":
    unittest.main()
