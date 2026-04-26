#!/usr/bin/env python3
"""
IPC test for the OpenDocument command in eeschema.

Verifies that the eeschema handler can navigate the schematic editor to a
specific sub-sheet by file path, and returns correct error codes for bad
inputs (wrong document type, nonexistent sheet).

The test discovers sub-sheets automatically from the open schematic via
GetSchematicHierarchy — no project-specific paths required.  It skips
the navigation check if the open schematic has no sub-sheets.

Usage
-----
  python3 test_open_document.py [--sub-sheet PATH]

  --sub-sheet   Override: explicit path to a sub-sheet .kicad_sch file.
                If omitted, the first sub-sheet found in the hierarchy is used.

Dependencies: pynng, protobuf
  python3 -m pip install pynng protobuf
"""

import argparse
import os
import sys

import pynng

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_HERE, "proto"))

from common import envelope_pb2
from common.commands import editor_commands_pb2, project_commands_pb2
from common.types import base_types_pb2
from schematic import schematic_commands_pb2, schematic_types_pb2

SOCKET_PATH = "ipc:///tmp/kicad/api.sock"
CLIENT_NAME = "ipc-open-document-test"

AS_OK          = 1
AS_UNHANDLED   = 5
AS_BAD_REQUEST = 3


# ---------------------------------------------------------------------------
# IPC helpers
# ---------------------------------------------------------------------------

def _pack(msg) -> bytes:
    req = envelope_pb2.ApiRequest()
    req.header.client_name = CLIENT_NAME
    req.message.Pack(msg)
    return req.SerializeToString()


def _unpack(data: bytes) -> envelope_pb2.ApiResponse:
    resp = envelope_pb2.ApiResponse()
    resp.ParseFromString(data)
    return resp


def _send(sock, msg, timeout_ms: int = 10_000) -> envelope_pb2.ApiResponse:
    sock.send_timeout = timeout_ms
    sock.recv_timeout = timeout_ms
    sock.send(_pack(msg))
    return _unpack(sock.recv())


def _check(resp: envelope_pb2.ApiResponse, label: str):
    code = resp.status.status
    if code != AS_OK:
        raise RuntimeError(
            f"{label} failed: status={code} msg={resp.status.error_message!r}")


# ---------------------------------------------------------------------------
# Discovery helpers
# ---------------------------------------------------------------------------

def _get_open_schematic_doc(sock):
    """Return the first open schematic DocumentSpecifier, or raise."""
    req = editor_commands_pb2.GetOpenDocuments()
    req.type = base_types_pb2.DOCTYPE_SCHEMATIC
    resp = _send(sock, req)
    _check(resp, "GetOpenDocuments")
    docs_resp = editor_commands_pb2.GetOpenDocumentsResponse()
    resp.message.Unpack(docs_resp)
    if not docs_resp.documents:
        raise RuntimeError("No schematic is open in KiCad.")
    return docs_resp.documents[0]


def _get_open_schematic_sheet_paths(sock) -> list[list[str]]:
    """Return the sheet_path UUID lists for each open schematic document.

    board_filename is always empty for schematic docs; the active sheet is
    identified by the UUID sequence in sheet_path (matching kicad_ipc.py's
    iter_schematic_sheets behaviour).
    """
    req = editor_commands_pb2.GetOpenDocuments()
    req.type = base_types_pb2.DOCTYPE_SCHEMATIC
    resp = _send(sock, req)
    _check(resp, "GetOpenDocuments")
    docs_resp = editor_commands_pb2.GetOpenDocumentsResponse()
    resp.message.Unpack(docs_resp)
    return [[item.value for item in d.sheet_path.path] for d in docs_resp.documents]


def _collect_sheet_filenames(sheet, project_path: str) -> list[str]:
    """Recursively collect absolute filenames for all children of a SheetInstance."""
    results = []
    for child in sheet.children:
        if child.filename:
            fn = child.filename
            if not os.path.isabs(fn):
                fn = os.path.join(project_path, fn)
            results.append(os.path.normpath(fn))
        results.extend(_collect_sheet_filenames(child, project_path))
    return results


def _discover_sub_sheet(sock) -> str | None:
    """Return the absolute path of the first sub-sheet in the hierarchy, or None."""
    doc_spec = _get_open_schematic_doc(sock)

    req = schematic_commands_pb2.GetSchematicHierarchy()
    req.document.CopyFrom(doc_spec)
    resp = _send(sock, req)
    _check(resp, "GetSchematicHierarchy")

    hier_resp = schematic_commands_pb2.SchematicHierarchyResponse()
    resp.message.Unpack(hier_resp)

    project_path = doc_spec.project.path
    for top in hier_resp.top_level_sheets:
        sheets = _collect_sheet_filenames(top, project_path)
        if sheets:
            return sheets[0]
    return None


# ---------------------------------------------------------------------------
# Individual checks
# ---------------------------------------------------------------------------

def check_navigate_to_sheet(sock, path: str):
    """OpenDocument navigates to the given sheet and returns a valid DocumentSpecifier.

    Also verifies via GetOpenDocuments that the editor's active sheet actually
    changed — this mirrors connect_pin_ipc.py's open_sheet() + iter_schematic_sheets()
    pattern and catches any regression where the handler returns OK without
    actually changing the active sheet.
    """
    print(f"[check] navigate to sheet: {path!r}")

    req = project_commands_pb2.OpenDocument()
    req.type = base_types_pb2.DOCTYPE_SCHEMATIC
    req.path = path

    resp = _send(sock, req)
    _check(resp, "OpenDocument")

    result = project_commands_pb2.OpenDocumentResponse()
    resp.message.Unpack(result)

    assert result.HasField("document"), "response missing document field"
    assert result.document.type == base_types_pb2.DOCTYPE_SCHEMATIC, \
        f"response document type mismatch: {result.document.type}"

    # Verify the editor actually navigated by checking that GetOpenDocuments
    # returns a document whose sheet_path matches the one in the response.
    # board_filename is always empty for schematic docs (kicad_ipc.py comment);
    # sheet identity is carried by the sheet_path UUID sequence instead.
    expected_uuids = [item.value for item in result.document.sheet_path.path]
    open_sheet_paths = _get_open_schematic_sheet_paths(sock)
    sheet_visible = expected_uuids in open_sheet_paths
    assert sheet_visible, (
        f"GetOpenDocuments sheet_path did not match after OpenDocument.\n"
        f"  Expected UUIDs: {expected_uuids!r}\n"
        f"  Open sheet paths: {open_sheet_paths!r}"
    )

    print(f"         ok — navigated, sheet_path confirmed in GetOpenDocuments")
    return result.document


def check_not_found(sock):
    """OpenDocument returns AS_BAD_REQUEST for a path not in the hierarchy."""
    fake = "/nonexistent/path/fake_sheet.kicad_sch"
    print(f"[check] not-found: {fake!r}")

    req = project_commands_pb2.OpenDocument()
    req.type = base_types_pb2.DOCTYPE_SCHEMATIC
    req.path = fake

    resp = _send(sock, req)
    code = resp.status.status
    assert code == AS_BAD_REQUEST, \
        f"expected AS_BAD_REQUEST ({AS_BAD_REQUEST}), got {code}: {resp.status.error_message!r}"

    msg = resp.status.error_message
    assert fake in msg or "not found" in msg.lower(), \
        f"error message doesn't mention the path: {msg!r}"

    print(f"         ok — got AS_BAD_REQUEST: {msg!r}")


def check_wrong_type(sock):
    """OpenDocument with DOCTYPE_PCB returns AS_UNHANDLED so pcbnew can handle it."""
    print("[check] wrong document type (DOCTYPE_PCB)")

    req = project_commands_pb2.OpenDocument()
    req.type = base_types_pb2.DOCTYPE_PCB
    req.path = "/some/board.kicad_pcb"

    resp = _send(sock, req)
    code = resp.status.status
    assert code == AS_UNHANDLED, \
        f"expected AS_UNHANDLED ({AS_UNHANDLED}), got {code}: {resp.status.error_message!r}"

    print(f"         ok — got AS_UNHANDLED as expected")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def run_test(sub_sheet_override: str | None):
    print(f"[test] connecting to KiCad at {SOCKET_PATH}")

    with pynng.Req0() as sock:
        sock.dial(SOCKET_PATH)

        check_wrong_type(sock)
        check_not_found(sock)

        # Discover or use the provided sub-sheet path.
        sub_sheet = sub_sheet_override or _discover_sub_sheet(sock)

        if not sub_sheet:
            print("[skip] no sub-sheets found in open schematic — skipping navigation check")
        else:
            check_navigate_to_sheet(sock, sub_sheet)

    print()
    print("[PASS] all OpenDocument checks passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sub-sheet", default=None, metavar="PATH",
                        help="Override: explicit path to a sub-sheet (auto-discovered if omitted)")
    args = parser.parse_args()

    run_test(args.sub_sheet)
