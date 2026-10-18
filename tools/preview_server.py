#!/usr/bin/env python3
"""Preview the phone interface on your computer, exactly as the device serves it.

    python3 tools/preview_server.py sdcard      then open http://localhost:8080
"""
import http.server, json, os, sys, time

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "sdcard")
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
START = time.time()
TYPES = {".html": "text/html", ".json": "application/json", ".jpg": "image/jpeg", ".pdf": "application/pdf",
         ".txt": "text/plain", ".tsv": "text/plain", ".md": "text/plain", ".css": "text/css", ".js": "application/javascript"}

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/api/status":
            body = json.dumps({"version": "preview", "clients": 1, "uptime": int(time.time() - START),
                               "sdTotalMB": 15193, "sdUsedMB": 9}).encode()
            return self.send(200, "application/json", body)
        if path in ("/", "/index.html"):
            path = "/web/index.html"
        full = os.path.normpath(os.path.join(ROOT, path.lstrip("/")))
        if not full.startswith(ROOT):
            return self.send(403, "text/plain", b"no")
        ctype = TYPES.get(os.path.splitext(full)[1], "application/octet-stream")
        gz = full + ".gz"
        if os.path.exists(gz) and (path == "/web/index.html" or not os.path.exists(full)):
            return self.send(200, ctype, open(gz, "rb").read(), gzip=True)
        if os.path.isfile(full):
            return self.send(200, ctype, open(full, "rb").read())
        self.send(404, "text/plain", b"not found")

    def send(self, code, ctype, body, gzip=False):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        if gzip:
            self.send_header("Content-Encoding", "gzip")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass

print(f"Serving {ROOT} at http://localhost:{PORT}")
http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
