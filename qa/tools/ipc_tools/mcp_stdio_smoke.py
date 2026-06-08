import argparse
import json
import os
import subprocess
import sys


def _write_msg(w, obj):
    payload = json.dumps(obj, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    header = f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii")
    w.write(header)
    w.write(payload)
    w.flush()


def _read_exact(r, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = r.read(n - len(buf))
        if not chunk:
            raise RuntimeError("EOF while reading payload")
        buf += chunk
    return buf


def _read_msg(r):
    header = b""
    while b"\r\n\r\n" not in header:
        b1 = r.read(1)
        if not b1:
            raise RuntimeError("EOF while reading header")
        header += b1

    head_text = header.decode("ascii", errors="replace")
    content_len = None
    for line in head_text.split("\r\n"):
        if line.lower().startswith("content-length:"):
            content_len = int(line.split(":", 1)[1].strip())
            break
    if content_len is None:
        raise RuntimeError(f"Missing Content-Length header: {head_text!r}")

    payload = _read_exact(r, content_len)
    return json.loads(payload.decode("utf-8"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--builddir", required=True)
    ap.add_argument("--with-project", action="store_true")
    args = ap.parse_args()

    builddir = os.path.abspath(args.builddir)
    reporoot = os.path.dirname(os.path.dirname(builddir))

    exe = os.path.join(builddir, "kicad", "kicad-mcp.exe")
    if not os.path.exists(exe):
        raise SystemExit(f"Missing kicad-mcp.exe at {exe}")

    env = os.environ.copy()
    env["KICAD_CONFIG_HOME"] = os.path.join(builddir, "ipc-v1-config")
    env["KICAD_RUN_FROM_BUILD_DIR"] = "1"
    env["KICAD_USE_EXTERNAL_PYTHONHOME"] = "1"
    env["PYTHONHOME"] = os.path.join(builddir, "vcpkg_installed", "x64-windows", "tools", "python3")

    path_prefix = [
        os.path.join(builddir, "kicad"),
        os.path.join(builddir, "eeschema"),
        os.path.join(builddir, "pcbnew"),
        os.path.join(builddir, "common"),
        os.path.join(builddir, "common", "gal"),
        os.path.join(builddir, "api"),
        os.path.join(builddir, "vcpkg_installed", "x64-windows", "bin"),
        os.path.join(builddir, "vcpkg_installed", "x64-windows", "tools", "python3"),
    ]
    env["PATH"] = ";".join(path_prefix + [env.get("PATH", "")])

    cmd = [exe]
    if args.with_project:
        proj = os.path.join(builddir, "ipc-v1-proj", "v1.kicad_pro")
        if not os.path.exists(proj):
            raise SystemExit(f"Missing project file at {proj}")
        cmd += ["--project", proj]

    with subprocess.Popen(
        cmd,
        cwd=reporoot,
        env=env,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=None,
    ) as p:
        assert p.stdin is not None
        assert p.stdout is not None

        def call_tool(tool_name, arguments):
            _write_msg(
                p.stdin,
                {
                    "jsonrpc": "2.0",
                    "id": call_tool.next_id,
                    "method": "tools/call",
                    "params": {"name": tool_name, "arguments": arguments},
                },
            )
            call_tool.next_id += 1
            resp = _read_msg(p.stdout)

            if "error" in resp:
                return ("error", resp["error"])

            payload = (resp.get("result", {}).get("content") or [{}])[0].get("json", {})
            return ("ok", payload)

        call_tool.next_id = 10

        def assert_ok(name, expected):
            status, payload = call_tool(name, expected.pop("arguments"))
            if status != "ok":
                raise SystemExit(f"Expected {name} OK, got error: {payload}")
            return payload

        _write_msg(p.stdin, {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}})
        r1 = _read_msg(p.stdout)
        if r1.get("result", {}).get("serverInfo", {}).get("name") != "kicad-mcp":
            raise SystemExit(f"Bad initialize response: {r1}")

        _write_msg(p.stdin, {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}})
        r2 = _read_msg(p.stdout)
        tools = r2.get("result", {}).get("tools", [])
        names = {t.get("name") for t in tools}
        required = {
            "get_project_info",
            "get_workspace_overview",
            "get_kicad_binary_path",
            "get_plugin_settings_path",
            "get_text_variables",
            "expand_text_variables",
            "get_net_classes",
        }
        missing = sorted(required - names)
        if missing:
            raise SystemExit(f"Missing tools from tools/list: {missing}. Full r2={r2}")

        enable_text_geometry = (
            "get_text_extents" in names and
            "get_text_as_shapes" in names
        )

        # Keep the existing smoke checks for V1 tools.
        _write_msg(
            p.stdin,
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {"name": "get_project_info", "arguments": {}},
            },
        )
        r3 = _read_msg(p.stdout)
        content0 = (r3.get("result", {}).get("content") or [{}])[0]
        loaded = content0.get("json", {}).get("loaded")
        if args.with_project and loaded is not True:
            raise SystemExit(f"Expected loaded=true, got: {r3}")
        if (not args.with_project) and loaded is not False:
            raise SystemExit(f"Expected loaded=false, got: {r3}")

        _write_msg(
            p.stdin,
            {
                "jsonrpc": "2.0",
                "id": 4,
                "method": "tools/call",
                "params": {"name": "get_workspace_overview", "arguments": {}},
            },
        )
        r4 = _read_msg(p.stdout)
        content0 = (r4.get("result", {}).get("content") or [{}])[0]
        loaded2 = content0.get("json", {}).get("project", {}).get("loaded")
        if args.with_project and loaded2 is not True:
            raise SystemExit(f"Expected project.loaded=true, got: {r4}")
        if (not args.with_project) and loaded2 is not False:
            raise SystemExit(f"Expected project.loaded=false, got: {r4}")

        # 5) get_open_documents (expected stable GUI-only behavior)
        _write_msg(
            p.stdin,
            {
                "jsonrpc": "2.0",
                "id": 5,
                "method": "tools/call",
                "params": {"name": "get_open_documents", "arguments": {}},
            },
        )
        r5 = _read_msg(p.stdout)
        content0 = (r5.get("result", {}).get("content") or [{}])[0]
        open_sch = content0.get("json", {}).get("open_schematic_documents", None)
        open_pcb = content0.get("json", {}).get("open_pcb_documents", None)
        if not isinstance(open_sch, list) or not isinstance(open_pcb, list):
            raise SystemExit(f"Unexpected get_open_documents payload: {r5}")
        print("get_open_documents(all)=", json.dumps(content0.get("json", {}), ensure_ascii=False))
        # In headless stdio (`kicad-mcp`), we expect no actual editor frames.
        if len(open_sch) != 0 or len(open_pcb) != 0:
            print(
                f"WARNING: headless returned non-empty open docs: sch={len(open_sch)} pcb={len(open_pcb)}"
            )

        # V1.1 tools: validate functional behaviour.
        # 1) get_kicad_binary_path (headless safe)
        status, payload = call_tool(
            "get_kicad_binary_path",
            {"binary_name": "kicad-cli"},
        )
        if status != "ok":
            raise SystemExit(f"get_kicad_binary_path failed: {payload}")
        print("get_kicad_binary_path=", json.dumps(payload, ensure_ascii=False))
        if not isinstance(payload.get("path"), str) or not payload["path"].endswith(".exe"):
            raise SystemExit(f"Unexpected get_kicad_binary_path payload: {payload}")

        # 2) get_plugin_settings_path (headless safe)
        status, payload = call_tool(
            "get_plugin_settings_path",
            {"identifier": "com.example.plugin"},
        )
        if status != "ok":
            raise SystemExit(f"get_plugin_settings_path failed: {payload}")
        print("get_plugin_settings_path=", json.dumps(payload, ensure_ascii=False))
        if not isinstance(payload.get("path"), str) or "plugins" not in payload["path"]:
            raise SystemExit(f"Unexpected get_plugin_settings_path payload: {payload}")

        # 3) get_text_variables (project required)
        status, payload = call_tool("get_text_variables", {})
        if not args.with_project:
            if status != "error":
                raise SystemExit(f"Expected get_text_variables error without project, got: {payload}")
            print("get_text_variables(error)=", json.dumps(payload, ensure_ascii=False))
        else:
            if status != "ok":
                raise SystemExit(f"Expected get_text_variables OK with project, got: {payload}")
            print("get_text_variables=", json.dumps(payload, ensure_ascii=False))
            vars_obj = payload.get("variables")
            if not isinstance(vars_obj, dict):
                raise SystemExit(f"Unexpected get_text_variables variables type: {type(vars_obj)} payload={payload}")

        # 4) expand_text_variables (project required)
        status, payload = call_tool(
            "expand_text_variables",
            {"text": ["Hello MCP", "${MISSING_VAR}"]},
        )
        if not args.with_project:
            if status != "error":
                raise SystemExit(f"Expected expand_text_variables error without project, got: {payload}")
            print("expand_text_variables(error)=", json.dumps(payload, ensure_ascii=False))
        else:
            if status != "ok":
                raise SystemExit(f"Expected expand_text_variables OK with project, got: {payload}")
            print("expand_text_variables=", json.dumps(payload, ensure_ascii=False))
            out = payload.get("text")
            if not isinstance(out, list) or len(out) != 2:
                raise SystemExit(f"Unexpected expand_text_variables output: {payload}")
            if out[0] != "Hello MCP":
                raise SystemExit(f"Unexpected expanded text[0]={out[0]}")

        if enable_text_geometry:
            # 5) get_text_extents (project not required)
            text_obj = {
                "position": {"xNm": 0, "yNm": 0},
                "attributes": {
                    "fontName": "",
                    "horizontalAlignment": 2,  # HA_CENTER
                    "verticalAlignment": 2,  # VA_CENTER
                    "angle": {"valueDegrees": 0.0},
                    "lineSpacing": 0.0,
                    "strokeWidth": {"valueNm": 50},
                    "italic": False,
                    "bold": False,
                    "underlined": False,
                    "visible": True,
                    "mirrored": False,
                    "multiline": False,
                    "keepUpright": False,
                    "size": {"xNm": 100000, "yNm": 50000},
                },
                "text": "MCP",
                "hyperlink": "",
            }

            status, payload = call_tool("get_text_extents", {"text": text_obj})
            if status != "ok":
                raise SystemExit(f"get_text_extents failed: {payload}")
            # Expected Box2 shape
            pos = payload.get("position")
            size = payload.get("size")
            if not isinstance(pos, dict) or not isinstance(size, dict):
                raise SystemExit(f"Unexpected get_text_extents payload: {payload}")
            if size.get("x_nm", 0) <= 0 or size.get("y_nm", 0) <= 0:
                raise SystemExit(f"Expected extents size > 0, got: {payload}")
            print("get_text_extents=", json.dumps(payload, ensure_ascii=False))

            # 6) get_text_as_shapes (project not required)
            status, payload = call_tool("get_text_as_shapes", {"text": [text_obj]})
            if status != "ok":
                raise SystemExit(f"get_text_as_shapes failed: {payload}")
            tws = payload.get("text_with_shapes")
            if not isinstance(tws, list) or len(tws) != 1:
                raise SystemExit(f"Unexpected get_text_as_shapes payload: {payload}")
            shapes = tws[0].get("shapes", {})
            shape_list = shapes.get("shapes", [])
            if not isinstance(shape_list, list) or len(shape_list) == 0:
                raise SystemExit(f"Expected non-empty shapes in get_text_as_shapes: {payload}")
            print("get_text_as_shapes=", json.dumps(payload, ensure_ascii=False))

        # 7) get_net_classes (project required)
        status, payload = call_tool("get_net_classes", {})
        if not args.with_project:
            if status != "error":
                raise SystemExit(f"Expected get_net_classes error without project, got: {payload}")
            print("get_net_classes(error)=", json.dumps(payload, ensure_ascii=False))
        else:
            if status != "ok":
                raise SystemExit(f"Expected get_net_classes OK with project, got: {payload}")
            classes = payload.get("net_classes")
            if not isinstance(classes, list) or len(classes) == 0:
                raise SystemExit(f"Unexpected get_net_classes payload: {payload}")
            names2 = {c.get("name") for c in classes if isinstance(c, dict)}
            if "Default" not in names2:
                raise SystemExit(f"Expected Default netclass in get_net_classes: {payload}")
            print("get_net_classes=", json.dumps(payload, ensure_ascii=False))

        print("OK")

        try:
            p.kill()
        except Exception:
            pass


if __name__ == "__main__":
    main()

