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
    reporoot = os.path.dirname(builddir)
    exe = os.path.join(builddir, "kicad", "kicad-mcp.exe")
    if not os.path.exists(exe):
        raise SystemExit(f"Missing kicad-mcp.exe at {exe}")

    env = os.environ.copy()
    env["KICAD_CONFIG_HOME"] = os.path.join(builddir, "ipc-v1-config")
    env["KICAD_RUN_FROM_BUILD_DIR"] = "1"
    env["KICAD_USE_EXTERNAL_PYTHONHOME"] = "1"
    env["PYTHONHOME"] = os.path.join(
        builddir, "vcpkg_installed", "x64-windows", "tools", "python3"
    )

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
        if p.stdin is None or p.stdout is None:
            raise SystemExit("Failed to open stdio pipes")

        _write_msg(
            p.stdin,
            {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
        )
        _read_msg(p.stdout)  # initialize response (validated elsewhere)

        _write_msg(
            p.stdin,
            {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "tools/call",
                "params": {"name": "get_workspace_overview", "arguments": {}},
            },
        )
        r = _read_msg(p.stdout)

        if "error" in r:
            raise SystemExit(f"tools/call error: {r['error']}")

        payload = (r.get("result", {}).get("content") or [{}])[0].get("json", {})
        print("get_workspace_overview=", json.dumps(payload, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()

