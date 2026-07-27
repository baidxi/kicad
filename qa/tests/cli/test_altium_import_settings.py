#
# This program source code file is part of KiCad, a free EDA CAD application.
#
# Copyright The KiCad Developers, see AUTHORS.txt for contributors.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#

"""
Tests for kicad-cli fp upgrade / sym upgrade --settings flag
(Altium import customization via JSON settings file)
"""

import os
import re
import shutil
from pathlib import Path

import pytest

from conftest import KiTestFixture
import utils


def get_altium_test_data_dir() -> Path:
    """Return path to the Altium PcbLib test data."""
    return Path(os.path.dirname(__file__)).parent.parent / "data" / "pcbnew" / "plugins" / "altium" / "pcblib"


def get_settings_file() -> Path:
    """Return path to the test settings JSON file."""
    return Path(os.path.dirname(__file__)).parent.parent / "data" / "cli" / "altium_import_settings_test" / "test_settings.json"


def get_fp_upgrade_cmd(input_path: str, output_path: str, settings: str = None) -> list[str]:
    cmd = [
        utils.kicad_cli(),
        "fp",
        "upgrade",
        "--force",
        str(input_path),
        "--output",
        str(output_path),
    ]
    if settings:
        cmd.extend(["--settings", str(settings)])
    return cmd


def parse_kicad_mod_text_thicknesses(filepath: Path) -> list[float]:
    """Extract all text thickness values from a .kicad_mod file."""
    content = filepath.read_text(encoding="utf-8")
    # Match (thickness X.XX) inside (font ...) blocks
    return [float(m) for m in re.findall(r'\(thickness\s+([0-9.]+)\)', content)]


def parse_kicad_mod_pad_names(filepath: Path) -> list[str]:
    """Extract all pad names/numbers from a .kicad_mod file."""
    content = filepath.read_text(encoding="utf-8")
    # Match (pad "NAME" ...) patterns
    return re.findall(r'\(pad\s+"([^"]+)"', content)


def parse_kicad_mod_layers(filepath: Path) -> list[str]:
    """Extract all layer references from a .kicad_mod file."""
    content = filepath.read_text(encoding="utf-8")
    return re.findall(r'\(layer\s+"([^"]+)"\)', content)


class TestFpUpgradeSettings:
    """Tests for fp upgrade --settings (Altium import customization)."""

    def test_fp_upgrade_altium_with_settings(self, kitest: KiTestFixture):
        """
        Test that fp upgrade of an Altium .PcbLib with --settings applies
        the custom text thickness from the JSON settings file.
        """
        altium_lib = get_altium_test_data_dir() / "Espressif ESP32-WROOM-32.PcbLib"

        if not altium_lib.exists():
            pytest.skip("Altium PcbLib test data not found")

        settings_file = get_settings_file()
        assert settings_file.exists(), f"Settings file not found: {settings_file}"

        output_path = kitest.get_output_path("cli/fp_upgrade_settings/with_settings/")
        output_lib = Path(output_path) / "ESP32.pretty"

        shutil.rmtree(output_lib, ignore_errors=True)

        cmd = get_fp_upgrade_cmd(altium_lib, output_lib, settings_file)
        stdout, stderr, exitcode = utils.run_and_capture(cmd)

        assert exitcode == 0, f"fp upgrade failed: stderr={stderr}"
        assert output_lib.is_dir()

        fp_files = list(output_lib.glob("*.kicad_mod"))
        assert len(fp_files) >= 1, "No footprints generated"

        # Check that text thickness uses the custom value (0.2032) not default (0.15)
        for fp_file in fp_files:
            thicknesses = parse_kicad_mod_text_thicknesses(fp_file)
            assert len(thicknesses) > 0, f"No text thickness found in {fp_file.name}"

            for thickness in thicknesses:
                # The settings specify 0.2032mm; allow small floating-point tolerance
                assert abs(thickness - 0.2032) < 0.001, \
                    f"Expected text thickness ~0.2032, got {thickness} in {fp_file.name}"

    def test_fp_upgrade_altium_without_settings(self, kitest: KiTestFixture):
        """
        Test that fp upgrade of an Altium .PcbLib WITHOUT --settings uses
        the default text thickness of 0.15mm.
        """
        altium_lib = get_altium_test_data_dir() / "Espressif ESP32-WROOM-32.PcbLib"

        if not altium_lib.exists():
            pytest.skip("Altium PcbLib test data not found")

        output_path = kitest.get_output_path("cli/fp_upgrade_settings/without_settings/")
        output_lib = Path(output_path) / "ESP32_default.pretty"

        shutil.rmtree(output_lib, ignore_errors=True)

        cmd = get_fp_upgrade_cmd(altium_lib, output_lib)
        stdout, stderr, exitcode = utils.run_and_capture(cmd)

        assert exitcode == 0, f"fp upgrade failed: stderr={stderr}"
        assert output_lib.is_dir()

        fp_files = list(output_lib.glob("*.kicad_mod"))
        assert len(fp_files) >= 1

        # Check that text thickness uses the default value (0.15)
        for fp_file in fp_files:
            thicknesses = parse_kicad_mod_text_thicknesses(fp_file)

            for thickness in thicknesses:
                assert abs(thickness - 0.15) < 0.001, \
                    f"Expected default text thickness ~0.15, got {thickness} in {fp_file.name}"

    def test_fp_upgrade_settings_missing_file(self, kitest: KiTestFixture):
        """
        Test that fp upgrade with --settings pointing to a nonexistent file
        returns an error.
        """
        altium_lib = get_altium_test_data_dir() / "Espressif ESP32-WROOM-32.PcbLib"

        if not altium_lib.exists():
            pytest.skip("Altium PcbLib test data not found")

        output_path = kitest.get_output_path("cli/fp_upgrade_settings/missing/")
        output_lib = Path(output_path) / "ESP32_missing.pretty"

        shutil.rmtree(output_lib, ignore_errors=True)

        cmd = get_fp_upgrade_cmd(altium_lib, output_lib, "/nonexistent/settings.json")
        stdout, stderr, exitcode = utils.run_and_capture(cmd)

        assert exitcode != 0

    def test_fp_upgrade_settings_invalid_json(self, kitest: KiTestFixture):
        """
        Test that fp upgrade with an invalid JSON settings file returns an error.
        """
        altium_lib = get_altium_test_data_dir() / "Espressif ESP32-WROOM-32.PcbLib"

        if not altium_lib.exists():
            pytest.skip("Altium PcbLib test data not found")

        output_path = kitest.get_output_path("cli/fp_upgrade_settings/invalid_json/")
        output_lib = Path(output_path) / "ESP32_invalid.pretty"
        bad_json = Path(output_path) / "bad_settings.json"

        shutil.rmtree(output_lib, ignore_errors=True)
        output_path.mkdir(parents=True, exist_ok=True)

        # Write invalid JSON
        bad_json.write_text("{ this is not valid json }", encoding="utf-8")

        cmd = get_fp_upgrade_cmd(altium_lib, output_lib, bad_json)
        stdout, stderr, exitcode = utils.run_and_capture(cmd)

        assert exitcode != 0


class TestFpUpgradeLayerMapping:
    """Tests for fp upgrade --settings layer mapping."""

    def test_fp_upgrade_altium_layer_mapping(self, kitest: KiTestFixture):
        """
        Test that fp upgrade of an Altium .PcbLib with --settings applies
        the layer mapping from the JSON settings file, and that without
        settings the default mapping is used.

        Compare layer usage between with-settings and without-settings outputs.
        """
        altium_lib = get_altium_test_data_dir() / "Espressif ESP32-WROOM-32.PcbLib"

        if not altium_lib.exists():
            pytest.skip("Altium PcbLib test data not found")

        settings_file = get_settings_file()

        # Upgrade with settings
        output_with = kitest.get_output_path("cli/fp_upgrade_layer_mapping/with/")
        lib_with = Path(output_with) / "ESP32_mapped.pretty"
        shutil.rmtree(lib_with, ignore_errors=True)

        cmd = get_fp_upgrade_cmd(altium_lib, lib_with, settings_file)
        stdout, stderr, exitcode = utils.run_and_capture(cmd)
        assert exitcode == 0, f"fp upgrade with settings failed: stderr={stderr}"

        # Upgrade without settings
        output_without = kitest.get_output_path("cli/fp_upgrade_layer_mapping/without/")
        lib_without = Path(output_without) / "ESP32_unmapped.pretty"
        shutil.rmtree(lib_without, ignore_errors=True)

        cmd = get_fp_upgrade_cmd(altium_lib, lib_without)
        stdout, stderr, exitcode = utils.run_and_capture(cmd)
        assert exitcode == 0, f"fp upgrade without settings failed: stderr={stderr}"

        fps_with = list(lib_with.glob("*.kicad_mod"))
        fps_without = list(lib_without.glob("*.kicad_mod"))

        assert len(fps_with) == len(fps_without)
        assert len(fps_with) >= 1

        # Aggregate the layers used across the whole library for each variant.
        # The settings map MECHANICAL_13 -> F.Fab and MECHANICAL_15 -> F.CrtYd;
        # the default mapping sends those Altium mechanical layers to User.13 /
        # User.15. Asserting on those specific layers proves the remapping took
        # effect, rather than merely that the two outputs differ.
        layers_with = set()
        for fp in fps_with:
            layers_with.update(parse_kicad_mod_layers(fp))

        layers_without = set()
        for fp in fps_without:
            layers_without.update(parse_kicad_mod_layers(fp))

        # Default mapping: Altium mechanical layers land on User.13 / User.15.
        assert "User.13" in layers_without, \
            f"Expected default MECHANICAL_13 -> User.13, got layers {sorted(layers_without)}"
        assert "User.15" in layers_without, \
            f"Expected default MECHANICAL_15 -> User.15, got layers {sorted(layers_without)}"

        # With settings: those Altium layers are remapped, so User.13 / User.15
        # disappear and F.CrtYd (from MECHANICAL_15) appears.
        assert "User.13" not in layers_with, \
            "MECHANICAL_13 should be remapped away from User.13 with settings"
        assert "User.15" not in layers_with, \
            "MECHANICAL_15 should be remapped away from User.15 with settings"
        assert "F.CrtYd" in layers_with, \
            f"Expected MECHANICAL_15 -> F.CrtYd with settings, got layers {sorted(layers_with)}"
        assert "F.CrtYd" not in layers_without, \
            "F.CrtYd should not appear without the layer-mapping settings"


class TestSymUpgradeSettings:
    """Tests for sym upgrade --settings argument handling."""

    def test_sym_upgrade_settings_missing_file(self, kitest: KiTestFixture):
        """
        Test that sym upgrade with --settings pointing to a nonexistent file
        returns an error when converting a non-KiCad library.

        Note: no Altium .SchLib test data is currently available,
        so this tests error handling only.
        """
        # Use a fake input path - the settings file check should fail first
        # for real Altium libraries, but we can still test the arg parsing
        output_path = kitest.get_output_path("cli/sym_upgrade_settings/missing/")
        fake_output = Path(output_path) / "output.kicad_sym"

        cmd = [
            utils.kicad_cli(),
            "sym",
            "upgrade",
            "--force",
            "/nonexistent/lib.SchLib",
            "--output",
            str(fake_output),
            "--settings",
            "/nonexistent/settings.json",
        ]
        stdout, stderr, exitcode = utils.run_and_capture(cmd)

        # Should fail (either because input doesn't exist or settings doesn't exist)
        assert exitcode != 0

    def test_sym_upgrade_help_shows_settings(self, kitest: KiTestFixture):
        """Test that sym upgrade --help mentions the --settings option."""
        cmd = [utils.kicad_cli(), "sym", "upgrade", "--help"]
        stdout, stderr, exitcode = utils.run_and_capture(cmd)

        assert exitcode == 0
        assert "--settings" in stdout

    def test_fp_upgrade_help_shows_settings(self, kitest: KiTestFixture):
        """Test that fp upgrade --help mentions the --settings option."""
        cmd = [utils.kicad_cli(), "fp", "upgrade", "--help"]
        stdout, stderr, exitcode = utils.run_and_capture(cmd)

        assert exitcode == 0
        assert "--settings" in stdout
