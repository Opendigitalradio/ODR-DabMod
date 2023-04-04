#!/usr/bin/env python
#
# This is an example program that illustrates
# how to interact with the zeromq remote control
# using JSON.
#
# LICENSE: see bottom of file

import sys
import zmq
from pprint import pprint
import json
import re
from http.server import BaseHTTPRequestHandler, HTTPServer
import time

re_url = re.compile(r"/([a-zA-Z0-9]+).json")

ZMQ_REMOTE = "tcp://localhost:9400"
HTTP_HOSTNAME = "localhost"
HTTP_PORT = 8080

class DabMuxServer(BaseHTTPRequestHandler):
    def err500(self, message):
        self.send_response(500)
        self.send_header("Content-type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps({"error": message}).encode())

    def do_GET(self):
        m = re_url.match(self.path)
        if m:
            sock = context.socket(zmq.REQ)
            poller = zmq.Poller()
            poller.register(sock, zmq.POLLIN)
            sock.connect(ZMQ_REMOTE)

            sock.send(b"ping")
            socks = dict(poller.poll(1000))
            if socks:
                if socks.get(sock) == zmq.POLLIN:
                    data = sock.recv()
                    if data != b"ok":
                        print(f"Received {data} to ping!", file=sys.stderr)
                        self.err500("ping failure")
                        return
            else:
                print("ZMQ error: ping timeout", file=sys.stderr)
                self.err500("ping timeout")
                return

            sock.send(b"showjson", flags=zmq.SNDMORE)
            sock.send(m.group(1).encode())

            socks = dict(poller.poll(1000))
            if socks:
                if socks.get(sock) == zmq.POLLIN:
                    data = sock.recv_multipart()
                    print("Received: {}".format(len(data)), file=sys.stderr)
                    parts = []
                    for i, part_data in enumerate(data):
                        part = part_data.decode()
                        print(" RX {}: {}".format(i, part.replace('\n',' ')), file=sys.stderr)

                        if i == 0 and part != "fail":
                            self.send_response(200)
                            self.send_header("Content-type", "application/json")
                            self.end_headers()
                            self.wfile.write(part_data)
                            return
                        parts.append(part)
                    self.err500("data error " + " ".join(parts))
                    return

            else:
                print("ZMQ error: timeout", file=sys.stderr)
                self.err500("timeout")
                return
        else:
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            self.wfile.write("""<html><head><title>ODR-DabMod RC HTTP server</title></head>\n""".encode())
            self.wfile.write("""<body>\n""".encode())
            for mod in ("sdr", "tist", "modulator", "tii", "ofdm", "gain", "guardinterval"):
                self.wfile.write(f"""<p><a href="{mod}.json">{mod}.json</a></p>\n""".encode())
            self.wfile.write("""</body></html>\n""".encode())


if __name__ == "__main__":
    context = zmq.Context()

    webServer = HTTPServer((HTTP_HOSTNAME, HTTP_PORT), DabMuxServer)
    print("Server started http://%s:%s" % (HTTP_HOSTNAME, HTTP_PORT))

    try:
        webServer.serve_forever()
    except KeyboardInterrupt:
        pass

    webServer.server_close()

    context.destroy(linger=5)


# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <http://unlicense.org>


