# R1 diagnostic: map tools in mcp_executor.cpp (line-oriented)
from pathlib import Path
import re

REPO = Path(__file__).resolve().parents[3]
SRC = REPO / "common" / "mcp" / "mcp_executor.cpp"
OUT = Path(__file__).resolve().parent / "MCP_TOOL_MAP_R1.tsv"
MAN = Path(__file__).resolve().parent / "MCP_V1_OFFICIAL_MANIFEST.md"
man = MAN.read_text(encoding="utf-8", errors="replace") if MAN.exists() else ""


def family(name: str) -> str:
    n = name
    if n.startswith("simulation_"):
        return "simulation"
    if n.startswith("workflow_"):
        return "workflow"
    if n.startswith("library_") or n.startswith("get_symbol_"):
        return "library"
    if any(
        n.startswith(p)
        for p in (
            "schematic_",
            "sch_",
            "get_sch_",
            "get_selection_sch",
            "create_sch_",
            "delete_sch_",
            "update_sch_",
            "hit_test_sch",
        )
    ):
        return "SCH"
    if any(
        n.startswith(p)
        for p in (
            "pcb_",
            "get_pcb_",
            "get_board_",
            "get_nets",
            "get_selection_pcb",
            "get_pad_",
            "set_board_",
            "create_pcb_",
            "delete_pcb_",
            "update_pcb_",
            "hit_test_pcb",
        )
    ):
        return "PCB"
    if n in ("get_net_classes", "set_net_classes", "netclass_patch_session", "electrical_profile_prepare_session"):
        return "netclass"
    if n.startswith("get_text_") or n.startswith("expand_text_"):
        return "text"
    if n.startswith("get_project_") or n in ("get_workspace_overview", "get_open_documents"):
        return "workspace"
    if n == "qa_open_editor_frame":
        return "QA"
    return "other"


def doc_status(name: str) -> str:
    if f"`{name}`" not in man:
        return "manifest_unknown"
    return "manifest_mentioned"


lines = SRC.read_text(encoding="utf-8", errors="replace").splitlines()
name_re = re.compile(r'^\s*tool\.descriptor\.name\s*=\s*"([^"]+)"\s*;\s*$')
rows = [
    "tool_name\tinvoke_return_expr\tregistration_file\tcallback_heuristic\theuristic_family\tdoc_hint"
]
current_name = None
in_invoke = False
for i, line in enumerate(lines):
    nm = name_re.match(line)
    if nm:
        current_name = nm.group(1)
        in_invoke = False
        continue
    if current_name and "tool.invoke" in line and "=" in line:
        in_invoke = True
        continue
    if current_name and in_invoke:
        m = re.search(r"return\s+([^;]+);", line)
        if m:
            ret = re.sub(r"\s+", " ", m.group(1).strip())
            kind = "callback_bridge" if "Invoke" in ret else ("invoke_fn" if ret.startswith("invoke") else "other")
            rows.append(
                f"{current_name}\t{ret}\tcommon/mcp/mcp_executor.cpp\t{kind}\t{family(current_name)}\t{doc_status(current_name)}"
            )
            current_name = None
            in_invoke = False

OUT.write_text("\n".join(rows) + "\n", encoding="utf-8")
print(f"Wrote {len(rows)-1} tools to {OUT}")
