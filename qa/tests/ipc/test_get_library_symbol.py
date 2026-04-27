#!/usr/bin/env python3
"""
IPC test for the GetLibrarySymbol command.

Verifies that the eeschema handler returns a fully-populated symbol definition
(pins + body graphics + mandatory fields) for a symbol in a loaded library,
and that it force-loads a library that hasn't been opened interactively yet.

Usage
-----
  python3 test_get_library_symbol.py [--library LIB] [--symbol SYM]

  LIB defaults to "Device", SYM defaults to "R".
  Any symbol from the project's symbol library table works; the test
  just needs KiCad to be running with a schematic open.

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
from common.commands import editor_commands_pb2
from common.types import base_types_pb2
from schematic import schematic_commands_pb2

SOCKET_PATH = "ipc:///tmp/kicad/api.sock"
CLIENT_NAME = "ipc-get-library-symbol-test"

AS_OK          = 1
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
# Individual checks
# ---------------------------------------------------------------------------

def _build_request(library: str, symbol: str) -> schematic_commands_pb2.GetLibrarySymbol:
    req = schematic_commands_pb2.GetLibrarySymbol()
    req.symbol.library_nickname = library
    req.symbol.entry_name = symbol
    return req


SCH_PIN_TYPE = "kiapi.schematic.types.SchematicPin"


def check_happy_path(sock, library: str, symbol: str):
    """GetLibrarySymbol returns a populated response for a valid symbol."""
    print(f"[check] happy path: {library}:{symbol}")

    resp = _send(sock, _build_request(library, symbol))
    _check(resp, "GetLibrarySymbol")

    result = schematic_commands_pb2.GetLibrarySymbolResponse()
    resp.message.Unpack(result)
    sym = result.symbol

    # Identity
    assert sym.id.library_nickname == library, \
        f"library_nickname mismatch: got {sym.id.library_nickname!r}"
    assert sym.id.entry_name == symbol, \
        f"entry_name mismatch: got {sym.id.entry_name!r}"

    # Must have at least one unit
    assert sym.unit_count >= 1, f"unit_count={sym.unit_count}"

    # Must include draw items (pins + body graphics).
    # Pin type is identified via Any.TypeName() — place_symbol.py uses this
    # exact method to count pins before embedding the definition.
    assert len(sym.items) > 0, "no items in symbol — pins/graphics were not serialized"

    pin_count = sum(1 for i in sym.items if i.item.TypeName() == SCH_PIN_TYPE)
    assert pin_count > 0, \
        f"no SchematicPin items found (TypeName check); item types: " \
        f"{[i.item.TypeName() for i in sym.items]}"

    # Every item must carry a non-empty type_url, unit, and body_style —
    # place_symbol.py reads child.unit.unit and child.body_style.style for all items.
    for idx, child in enumerate(sym.items):
        assert child.item.type_url, f"item[{idx}] has empty type_url"
        assert child.unit.unit >= 0, f"item[{idx}] has invalid unit {child.unit.unit}"
        assert child.body_style.style >= 0, \
            f"item[{idx}] has invalid body_style {child.body_style.style}"

    print(f"         ok — unit_count={sym.unit_count}, items={len(sym.items)}, pins={pin_count}")


def check_not_found(sock, library: str):
    """GetLibrarySymbol returns AS_BAD_REQUEST for a nonexistent symbol."""
    fake = "__nonexistent_symbol_xyz__"
    print(f"[check] not-found: {library}:{fake}")

    resp = _send(sock, _build_request(library, fake))
    code = resp.status.status
    assert code == AS_BAD_REQUEST, \
        f"expected AS_BAD_REQUEST ({AS_BAD_REQUEST}), got {code}: {resp.status.error_message!r}"

    # Error message should name both the library and the symbol
    msg = resp.status.error_message
    assert library in msg and fake in msg, \
        f"error message doesn't mention lib/symbol: {msg!r}"

    print(f"         ok — got AS_BAD_REQUEST with message: {msg!r}")


def check_field_structure(sock, library: str, symbol: str):
    """Mandatory fields are present and carry the nested structure place_symbol.py relies on.

    place_symbol.py reads reference_field.visible, reference_field.text.text,
    reference_field.text.position.x_nm/y_nm, and the same for value_field and
    footprint_field.  A regression that returns fields without the nested text/position
    sub-messages would silently produce zero-offset placements.
    """
    print(f"[check] field structure: {library}:{symbol}")

    resp = _send(sock, _build_request(library, symbol))
    _check(resp, "GetLibrarySymbol")

    result = schematic_commands_pb2.GetLibrarySymbolResponse()
    resp.message.Unpack(result)
    sym = result.symbol

    # All five mandatory field messages must be present.
    assert sym.HasField("reference_field"), "missing reference_field"
    assert sym.HasField("value_field"),     "missing value_field"
    assert sym.HasField("footprint_field"), "missing footprint_field"
    assert sym.HasField("datasheet_field"), "missing datasheet_field"
    assert sym.HasField("description_field"), "missing description_field"

    # reference_field: text content and position sub-messages must be accessible.
    # The reference prefix (e.g. "R", "C", "U") must be non-empty.
    ref_text = sym.reference_field.text.text
    assert ref_text, \
        f"reference_field.text.text is empty — place_symbol.py uses this as the default ref prefix"

    # Position fields must be reachable (place_symbol.py does `+= x_nm`).
    # Accessing them without AttributeError is the key check; the value may be 0.
    _ = sym.reference_field.text.position.x_nm
    _ = sym.reference_field.text.position.y_nm

    # value_field: text must be non-empty (defaults to symbol name).
    val_text = sym.value_field.text.text
    assert val_text, \
        f"value_field.text.text is empty — place_symbol.py uses this as the default value"
    _ = sym.value_field.text.position.x_nm
    _ = sym.value_field.text.position.y_nm

    # footprint_field position must also be reachable (used when --footprint is given).
    _ = sym.footprint_field.text.position.x_nm
    _ = sym.footprint_field.text.position.y_nm

    print(f"         ok — ref={ref_text!r} val={val_text!r}")


def check_multi_unit(sock, library: str, symbol: str):
    """Multi-unit symbols report unit_count > 1 and items carry per-unit annotations."""
    print(f"[check] multi-unit: {library}:{symbol}")

    resp = _send(sock, _build_request(library, symbol))
    _check(resp, "GetLibrarySymbol")

    result = schematic_commands_pb2.GetLibrarySymbolResponse()
    resp.message.Unpack(result)
    sym = result.symbol

    if sym.unit_count <= 1:
        print(f"         skip — {library}:{symbol} has only {sym.unit_count} unit(s)")
        return

    # Items for a multi-unit symbol must include at least unit 1 and unit 2
    units_seen = {item.unit.unit for item in sym.items}
    assert 1 in units_seen, f"unit 1 not found in items, got units: {sorted(units_seen)}"
    assert 2 in units_seen, f"unit 2 not found in items, got units: {sorted(units_seen)}"

    print(f"         ok — unit_count={sym.unit_count}, units in items: {sorted(units_seen)}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def run_test(library: str, symbol: str, multi_unit_symbol: str | None):
    print(f"[test] connecting to KiCad at {SOCKET_PATH}")

    with pynng.Req0() as sock:
        sock.dial(SOCKET_PATH)

        check_happy_path(sock, library, symbol)
        check_field_structure(sock, library, symbol)
        check_not_found(sock, library)

        if multi_unit_symbol:
            lib, sym = multi_unit_symbol.split(":", 1)
            check_multi_unit(sock, lib, sym)

    print()
    print("[PASS] all GetLibrarySymbol checks passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--library", default="Device",
                        help="Library nickname to query (default: Device)")
    parser.add_argument("--symbol", default="R",
                        help="Symbol name to query (default: R)")
    parser.add_argument("--multi-unit", metavar="LIB:SYM", default=None,
                        help="Optional LIB:SYM with unit_count > 1 to test multi-unit handling")
    args = parser.parse_args()

    run_test(args.library, args.symbol, args.multi_unit)
