import argparse
import os
import uuid


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--projectdir", required=True)
    ap.add_argument("--name", required=False, default="v1")
    ap.add_argument("--gen-version", required=False, default="10.0")
    args = ap.parse_args()

    project_dir = os.path.abspath(args.projectdir)
    name = args.name
    gen_version = args.gen_version

    sch_path = os.path.join(project_dir, f"{name}.kicad_sch")
    pcb_path = os.path.join(project_dir, f"{name}.kicad_pcb")

    kicad_uuid = str(uuid.uuid4())

    sch_wire_uuid = "11111111-1111-1111-1111-111111111111"
    sch_symbol_uuid = "22222222-2222-2222-2222-222222222222"
    sch_pin_uuid = "33333333-3333-3333-3333-333333333333"

    sch_txt = (
        "(kicad_sch\n"
        "\t(version 20260306)\n"
        "\t(generator \"eeschema\")\n"
        f"\t(generator_version \"{gen_version}\")\n"
        f"\t(uuid {kicad_uuid})\n"
        "\t(paper \"A4\")\n"
        "\t(lib_symbols\n"
        "\t\t(symbol \"power:GND\" (power) (pin_names (offset 0)) (in_bom yes) (on_board yes)\n"
        "\t\t\t(symbol \"GND_1_1\"\n"
        "\t\t\t\t(pin power_in line (at 0 0 270) (length 0) hide\n"
        "\t\t\t\t\t(name \"GND\" (effects (font (size 1.27 1.27))))\n"
        "\t\t\t\t\t(number \"1\" (effects (font (size 1.27 1.27))))\n"
        "\t\t\t\t)\n"
        "\t\t\t)\n"
        "\t\t)\n"
        "\t)\n"
        "\t(wire (pts (xy 0 0) (xy 10 0))\n"
        "\t\t(stroke (width 0) (type default))\n"
        f"\t\t(uuid {sch_wire_uuid})\n"
        "\t)\n"
        "\t(symbol (lib_id \"power:GND\") (at 0 0 0) (unit 1)\n"
        "\t\t(in_bom yes) (on_board yes) (dnp no) (fields_autoplaced)\n"
        f"\t\t(uuid {sch_symbol_uuid})\n"
        "\t\t(property \"Reference\" \"#PWR\" (at 0 -6.35 0)\n"
        "\t\t\t(effects (font (size 1.27 1.27)) hide)\n"
        "\t\t)\n"
        "\t\t(property \"Value\" \"GND\" (at 0 -3.81 0)\n"
        "\t\t\t(effects (font (size 1.27 1.27)))\n"
        "\t\t)\n"
        f"\t\t(pin \"1\" (uuid {sch_pin_uuid}))\n"
        "\t)\n"
        "\t(sheet_instances\n"
        "\t\t(path \"/\"\n"
        "\t\t\t(page \"1\")\n"
        "\t\t)\n"
        "\t)\n"
        "\t(embedded_fonts no)\n"
        ")\n"
    )

    # Minimal PCB content with:
    # - one deterministic footprint + pad so `get_pad_shape_as_polygon` has a real target
    # - one drawable edge so board extent/layers paths remain exercised
    # - explicit net declarations so `BOARD::GetNetInfo()` contains at least one real net.
    pcb_txt = (
        f"(kicad_pcb (version 20260206) (generator \"pcbnew\") (generator_version \"{gen_version}\")\n"
        "\t(net 0 \"\")\n"
        "\t(net 1 \"N_STUB\")\n"
        "\t(gr_line (start 0 0) (end 10 0) (layer \"Edge.Cuts\") (width 0.15))\n"
        "\t(footprint \"QA:PAD1\" (layer \"F.Cu\")\n"
        "\t\t(at 5 5)\n"
        "\t\t(attr board_only)\n"
        "\t\t(fp_text reference \"U1\" (at 0 -2) (layer \"F.SilkS\") hide)\n"
        "\t\t(fp_text value \"QA_PAD\" (at 0 2) (layer \"F.Fab\") hide)\n"
        "\t\t(pad \"1\" smd rect (at 0 0) (size 1 1) (layers \"F.Cu\" \"F.Paste\" \"F.Mask\") (net 1 \"N_STUB\"))\n"
        "\t)\n"
        ")\n"
    )

    os.makedirs(project_dir, exist_ok=True)
    with open(sch_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(sch_txt)
    with open(pcb_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(pcb_txt)

    print(f"wrote {sch_path}")
    print(f"wrote {pcb_path}")


if __name__ == "__main__":
    main()

