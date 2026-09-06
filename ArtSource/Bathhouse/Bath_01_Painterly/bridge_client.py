"""Client for the installed Blender Lab MCP add-on's local execution bridge."""
import json
import pathlib
import socket
import sys

code = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8") if len(sys.argv) > 1 else "import bpy; result = {'version': bpy.app.version_string, 'objects': len(bpy.data.objects)}"
with socket.create_connection(("127.0.0.1", 9876), timeout=10) as conn:
    conn.settimeout(1800)
    conn.sendall(json.dumps({"type": "execute", "code": code, "strict_json": True}).encode() + b"\0")
    chunks = bytearray()
    while not chunks.endswith(b"\0"):
        chunk = conn.recv(65536)
        if not chunk:
            raise RuntimeError("Blender disconnected before returning a result")
        chunks.extend(chunk)
    response = json.loads(chunks[:-1])
    print(json.dumps(response, ensure_ascii=False, indent=2))
    if response.get("status") != "ok":
        sys.exit(1)
