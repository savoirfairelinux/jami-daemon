import hashlib
import io
import json
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("update_contrib")


class UpdateContribTest(unittest.TestCase):
    def setUp(self):
        output = mock.patch("sys.stdout", new_callable=io.StringIO)
        output.start()
        self.addCleanup(output.stop)
        self.temp_dir = tempfile.TemporaryDirectory(prefix="update contrib ")
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name).resolve()
        self.package = self.root / "src" / "dhtnet"
        self.port = self.root / "vcpkg" / "ports" / "dhtnet"
        self.package.mkdir(parents=True)
        self.port.mkdir(parents=True)
        self.script = self.package.parent / "update_contrib"
        shutil.copy2(SCRIPT, self.script)
        (self.package / "rules.mak").write_text(
            "DHTNET_VERSION := old # keep this comment\n"
            "DHTNET_URL := https://example.org/$(DHTNET_VERSION).tar.gz\n"
            "$(TARBALLS)/dhtnet-$(DHTNET_VERSION).tar.gz:\n"
            "\t$(call download,$(DHTNET_URL))\n"
        )
        self.extra_checksum = "a" * 128 + "  extra.tar.gz\n"
        (self.package / "SHA512SUMS").write_text(
            "0" * 128 + "  dhtnet-old.tar.gz\n" + self.extra_checksum
        )
        (self.port / "portfile.cmake").write_text(
            "set(DHTNET_REF old)\n"
            "vcpkg_download_distfile(ARCHIVE\n"
            '    URLS "https://example.org/vcpkg/${DHTNET_REF}.tar.gz"\n'
            '    FILENAME "dhtnet-${DHTNET_REF}.tar.gz"\n'
            "    SHA512 " + "0" * 128 + "\n)\n"
        )
        (self.port / "vcpkg.json").write_text(json.dumps({
            "name": "dhtnet", "version-date": "2026-10-09"
        }, indent=2) + "\n")
        self.updater = runpy.run_path(str(self.script), run_name="update_contrib")

    def download(self, command, **kwargs):
        destination = Path(command[command.index("-o") + 1])
        destination.write_bytes(b"vcpkg" if "/vcpkg/" in command[-1] else b"make")
        return subprocess.CompletedProcess(command, 0)

    def update(self, version="new", *arguments):
        with mock.patch("subprocess.run", side_effect=self.download) as download:
            self.updater["main"](["dhtnet", version, *arguments])
        return download

    def run_update(self, *arguments):
        return subprocess.run(
            [str(self.script), "dhtnet", "new", *arguments],
            capture_output=True, text=True, cwd=self.root,
        )

    def files(self):
        return {
            file.relative_to(self.root): file.read_bytes()
            for directory in (self.package, self.port)
            for file in directory.iterdir()
        }

    def test_updates_make_and_vcpkg_without_package_json(self):
        self.update()

        self.assertIn(
            "DHTNET_VERSION := new # keep this comment",
            (self.package / "rules.mak").read_text(),
        )
        self.assertEqual(
            (self.package / "SHA512SUMS").read_text(),
            hashlib.sha512(b"make").hexdigest()
            + "  dhtnet-new.tar.gz\n" + self.extra_checksum,
        )
        portfile = (self.port / "portfile.cmake").read_text()
        self.assertIn("set(DHTNET_REF new)", portfile)
        self.assertIn(hashlib.sha512(b"vcpkg").hexdigest(), portfile)
        manifest = json.loads((self.port / "vcpkg.json").read_text())
        self.assertEqual(manifest["version-date"], "2026-10-09")
        self.assertEqual(manifest["port-version"], 1)

    def test_download_failure_does_not_modify_metadata(self):
        before = self.files()

        def download(command, **kwargs):
            if "/vcpkg/" in command[-1]:
                raise subprocess.CalledProcessError(22, command)
            Path(command[command.index("-o") + 1]).write_bytes(b"make")
            return subprocess.CompletedProcess(command, 0)

        with mock.patch("subprocess.run", side_effect=download):
            with self.assertRaises((ValueError, RuntimeError, subprocess.CalledProcessError)):
                self.updater["main"](["dhtnet", "new"])
        self.assertEqual(before, self.files())

    def test_dry_run_does_not_modify_metadata(self):
        before = self.files()
        self.update("new", "--dry-run")
        self.assertEqual(before, self.files())

    def test_make_only_and_legacy_json(self):
        shutil.rmtree(self.port)
        package_file = self.package / "package.json"
        package_file.write_text('{"version": "vold", "patches": ["keep.patch"]}\n')
        self.update()
        self.assertEqual(json.loads(package_file.read_text()), {
            "version": "vnew", "patches": ["keep.patch"],
        })

    def test_vcpkg_only(self):
        shutil.rmtree(self.package)
        self.update()
        self.assertIn("set(DHTNET_REF new)", (self.port / "portfile.cmake").read_text())

    def test_github_release_and_auxiliary_download(self):
        portfile = self.port / "portfile.cmake"
        auxiliary = (
            'vcpkg_download_distfile(AUX\n    URLS "https://example.org/aux"\n'
            '    FILENAME "aux.tar.gz"\n    SHA512 ' + "a" * 128 + "\n)\n"
        )
        portfile.write_text(
            "vcpkg_from_github(\n    REPO example/project\n    REF v1.0.0\n"
            "    SHA512 " + "0" * 128 + "\n)\n" + auxiliary
        )
        manifest = self.port / "vcpkg.json"
        manifest.write_text('{"name": "dhtnet", "version": "1.0.0", "port-version": 2}\n')
        download = self.update("2.0.0")
        self.assertIn("REF v2.0.0", portfile.read_text())
        self.assertTrue(portfile.read_text().endswith(auxiliary))
        self.assertEqual(json.loads(manifest.read_text()), {"name": "dhtnet", "version": "2.0.0"})
        self.assertEqual(download.call_args_list[-1].args[0][-1],
                         "https://github.com/example/project/archive/v2.0.0.tar.gz")

    def test_manifest_version_reference_and_port_override(self):
        portfile = self.port / "portfile.cmake"
        portfile.write_text(
            'vcpkg_from_github(\n    REPO example/project\n    REF "n${VERSION}"\n'
            "    SHA512 " + "0" * 128 + "\n)\n"
        )
        (self.port / "vcpkg.json").write_text('{"name": "dhtnet", "version": "1.0.0"}\n')
        self.port = self.port.rename(self.port.with_name("custom-port"))
        download = self.update("2.0.0", "--port", "custom-port")
        self.assertIn('REF "n${VERSION}"', (self.port / "portfile.cmake").read_text())
        self.assertTrue(download.call_args_list[-1].args[0][-1].endswith("/n2.0.0.tar.gz"))

    def test_hash_updates_preserve_numeric_manifest_versions(self):
        manifest = self.port / "vcpkg.json"
        manifest.write_text('{"name": "dhtnet", "version": "1.0.0", "port-version": 2}\n')
        self.update("a" * 40)
        data = json.loads(manifest.read_text())
        self.assertEqual(data["version"], "1.0.0")
        self.assertEqual(data["port-version"], 3)

    def test_gitlab_and_fork_ref_override(self):
        (self.port / "portfile.cmake").write_text(
            'vcpkg_from_gitlab(\n    GITLAB_URL "https://gitlab.example.org/"\n'
            '    REPO "group/project"\n    REF ' + "a" * 40 + "\n"
            "    SHA512 " + "0" * 128 + "\n)\n"
        )
        (self.port / "vcpkg.json").write_text('{"name": "dhtnet", "version": "1.0.0"}\n')
        before = self.files()
        with self.assertRaisesRegex(ValueError, "--vcpkg-ref"):
            self.update("2.0.0")
        self.assertEqual(before, self.files())
        download = self.update("2.0.0", "--vcpkg-ref", "b" * 40)
        self.assertEqual(download.call_args_list[-1].args[0][-1],
                         "https://gitlab.example.org/group/project/-/archive/"
                         + "b" * 40 + "/project-" + "b" * 40 + ".tar.gz")

    def test_same_version_is_idempotent(self):
        self.update()
        before = self.files()
        self.update()
        self.assertEqual(before, self.files())

    def test_duplicate_checksum_is_rejected(self):
        sums_file = self.package / "SHA512SUMS"
        sums_file.write_text(sums_file.read_text() * 2)
        before = self.files()
        with self.assertRaisesRegex(ValueError, "Expected one checksum"):
            self.update()
        self.assertEqual(before, self.files())

    def test_empty_download_is_rejected(self):
        before = self.files()

        def download(command, **kwargs):
            Path(command[command.index("-o") + 1]).touch()

        with mock.patch("subprocess.run", side_effect=download):
            with self.assertRaisesRegex(ValueError, "empty archive"):
                self.updater["main"](["dhtnet", "new"])
        self.assertEqual(before, self.files())

    def test_missing_checksum_file_is_created(self):
        sums_file = self.package / "SHA512SUMS"
        sums_file.unlink()
        self.update()
        self.assertEqual(sums_file.read_text(),
                         hashlib.sha512(b"make").hexdigest() + "  dhtnet-new.tar.gz\n")

    def test_failed_replacement_rolls_back_and_cleans_up(self):
        before = self.files()
        original_replace = self.updater["os"].replace
        replacements = 0

        def replace(source, destination):
            nonlocal replacements
            replacements += 1
            if replacements == 2:
                raise OSError("test write failure")
            original_replace(source, destination)

        with mock.patch("os.replace", side_effect=replace):
            with self.assertRaisesRegex(OSError, "test write failure"):
                self.update()
        self.assertEqual(before, self.files())
        self.assertFalse(list(self.root.rglob(".update_contrib-*")))

    def test_invalid_cli_arguments(self):
        for arguments in (("--port", "../outside"), ("--vcpkg-ref", "$(touch bad)")):
            with self.subTest(arguments=arguments):
                result = self.run_update(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("Traceback", result.stderr)

    def test_builtin_port_alias_in_both_directions(self):
        self.package = self.package.rename(self.package.with_name("upnp"))
        self.port = self.port.rename(self.port.with_name("libupnp"))
        for name in ("upnp", "libupnp"):
            with self.subTest(name=name):
                with mock.patch("subprocess.run", side_effect=self.download):
                    self.updater["main"]([name, "new"])
                self.assertIn("DHTNET_VERSION := new", (self.package / "rules.mak").read_text())
                self.assertIn("set(DHTNET_REF new)", (self.port / "portfile.cmake").read_text())

    def test_identical_urls_download_once(self):
        portfile = self.port / "portfile.cmake"
        portfile.write_text(portfile.read_text().replace("example.org/vcpkg/", "example.org/"))
        download = self.update()
        self.assertEqual(download.call_count, 1)

    def test_invalid_manifest_leaves_metadata_unchanged(self):
        manifest = self.port / "vcpkg.json"
        for data in (["not an object"], {"version-date": "2026-10-09", "port-version": "bad"}):
            with self.subTest(data=data):
                manifest.write_text(json.dumps(data))
                before = self.files()
                with self.assertRaises(ValueError):
                    self.update()
                self.assertEqual(before, self.files())

    def test_short_commit_replaces_entire_old_pin(self):
        rules_file = self.package / "rules.mak"
        rules_file.write_text(rules_file.read_text().replace("old", "1234567"))
        sums_file = self.package / "SHA512SUMS"
        sums_file.write_text(sums_file.read_text().replace("old", "1234567"))
        portfile = self.port / "portfile.cmake"
        portfile.write_text(portfile.read_text().replace("old", "1234567" + "a" * 33))
        package_file = self.package / "package.json"
        package_file.write_text(json.dumps({"version": "1234567" + "a" * 33}))
        self.update("b" * 40)
        self.assertIn("set(DHTNET_REF " + "b" * 40 + ")", portfile.read_text())
        self.assertEqual(json.loads(package_file.read_text())["version"], "b" * 40)

    def test_vcpkg_only_version_string_is_updated(self):
        shutil.rmtree(self.package)
        manifest = self.port / "vcpkg.json"
        manifest.write_text('{"name": "dhtnet", "version-string": "old"}\n')
        self.update()
        self.assertEqual(json.loads(manifest.read_text())["version-string"], "new")

    def test_non_version_url_variables_are_rejected(self):
        portfile = self.port / "portfile.cmake"
        portfile.write_text(portfile.read_text().replace("DHTNET_REF", "HOST"))
        before = self.files()
        with self.assertRaisesRegex(ValueError, "Not a vcpkg version/ref variable"):
            self.update()
        self.assertEqual(before, self.files())

    def test_new_checksum_is_removed_on_write_failure(self):
        (self.package / "SHA512SUMS").unlink()
        before = self.files()
        original_replace = self.updater["os"].replace

        def replace(source, destination):
            if destination == self.package / "rules.mak":
                raise OSError("test write failure")
            original_replace(source, destination)

        with mock.patch("os.replace", side_effect=replace):
            with self.assertRaisesRegex(OSError, "test write failure"):
                self.update()
        self.assertEqual(before, self.files())
        self.assertFalse(list(self.root.rglob(".update_contrib-*")))

    def test_checked_in_overlay_formats(self):
        aliases = {port: name for name, port in self.updater["PORT_NAMES"].items()}
        ports_dir = SCRIPT.parent.parent / "vcpkg" / "ports"
        for port_dir in sorted(ports_dir.iterdir()):
            if not (port_dir / "portfile.cmake").exists():
                continue
            with self.subTest(port=port_dir.name):
                package_dir = SCRIPT.parent / aliases.get(port_dir.name, port_dir.name)
                rules = (package_dir / "rules.mak").read_text()
                match = self.updater["re"].search(
                    r"^[A-Z0-9_]+_(?:VERSION|VER|HASH)[ \t]*[:+?]?=[ \t]*([^\s#]+)",
                    rules, self.updater["re"].MULTILINE,
                )
                current = match[1]
                version = "f" * 40 if self.updater["is_commit"](current) else "99.0.0"
                updates = {}
                urls = []

                def checksum(url):
                    urls.append(url)
                    return "a" * 128

                old_version = self.updater["make_update"](package_dir, version, updates, checksum)
                self.updater["vcpkg_update"](
                    port_dir, version, old_version, updates, checksum,
                    source_ref="f" * 40 if port_dir.name in ("shiftmedia-libgnutls", "portaudio") else None,
                )
                self.assertEqual(len(urls), 2)
                self.assertTrue(all(url.startswith(("http://", "https://")) for url in urls))
                manifest_file = port_dir / "vcpkg.json"
                original = json.loads(manifest_file.read_text())
                updated = json.loads(updates[manifest_file])
                if "version" in original:
                    self.assertEqual(updated["version"], original["version"]
                                     if self.updater["is_commit"](version) else version)
                else:
                    self.assertEqual(updated["version-date"], original["version-date"])
                if port_dir.name == "shiftmedia-libgnutls":
                    auxiliary = (port_dir / "portfile.cmake").read_text().split("vcpkg_download_distfile", 1)[1]
                    self.assertTrue(updates[port_dir / "portfile.cmake"].endswith(auxiliary))


if __name__ == "__main__":
    unittest.main()