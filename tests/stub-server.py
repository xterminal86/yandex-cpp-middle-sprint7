from http.server import HTTPServer, BaseHTTPRequestHandler

import time

#GarbageSize = 1024 * 1024 * 10;
GarbageSize = 1024;

class H(BaseHTTPRequestHandler):
  def do_GET(self):
    #time.sleep(15)
    self.send_response(200)
    self.send_header("Content-Type", "text/html")
    self.send_header("Content-Length", f"{ GarbageSize }")
    self.end_headers()
    self.wfile.write(b"A"*GarbageSize)

s = HTTPServer(("127.0.0.1", 8000), H)
print("Server is running...")
s.serve_forever()
