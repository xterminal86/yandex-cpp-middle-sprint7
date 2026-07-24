#!/bin/bash

python3 -c '
from http.server import HTTPServer, BaseHTTPRequestHandler
class H(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", "4096")
        self.end_headers()
        self.wfile.write(b"A"*4096)
s = HTTPServer(("127.0.0.1", 8000), H)
print("Stub server is waiting for request...")
s.handle_request()
' &

pyPid=$!

echo "Python stub server PID = ${pyPid}"

sleep 1

../build/AsyncHttpProxy 9999 &

proxyPid=$!

echo "Proxy server PID = ${proxyPid}"

# Send request to 127.0.0.1:8000 via proxy 127.0.0.1:9999
curl -v -x 127.0.0.1:9999 127.0.0.1:8000

kill -9 ${proxyPid}
