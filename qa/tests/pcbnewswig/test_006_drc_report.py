"""Unit-test WriteDRCReport from a script running outside KiCad."""

import pcbnew


class TestDRCReport:
    """A standalone script has no PGM_BASE, so DRC must not depend on one."""

    def test_write_drc_report(self, tmp_path):
        """Verify: WriteDRCReport() runs DRC and writes a complete report."""
        pcb = pcbnew.LoadBoard("../data/pcbnew/complex_hierarchy.kicad_pcb")
        report = tmp_path / "drc.rpt"

        assert pcbnew.WriteDRCReport(pcb, str(report), pcbnew.EDA_UNITS_MM, False)
        assert "** End of Report **" in report.read_text()
