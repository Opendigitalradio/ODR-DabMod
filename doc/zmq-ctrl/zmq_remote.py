#!/usr/bin/env python
#
# This is an example program that illustrates
# how to interact with the zeromq remote control
#
# LICENSE: see bottom of file

import sys
import zmq

context = zmq.Context()

sock = context.socket(zmq.REQ)

poller = zmq.Poller()
poller.register(sock, zmq.POLLIN)

if len(sys.argv) < 2:
    print("Usage: program url cmd [args...]", file=sys.stderr)
    sys.exit(1)

sock.connect(sys.argv[1])

message_parts = sys.argv[2:]

# first do a ping test

print("ping", file=sys.stderr)
sock.send(b"ping")

socks = dict(poller.poll(1000))
if socks:
    if socks.get(sock) == zmq.POLLIN:

        data = sock.recv_multipart()
        print("Received: {}".format(len(data)), file=sys.stderr)
        for i,part in enumerate(data):
            print("   {}".format(part), file=sys.stderr)

        for i, part in enumerate(message_parts):
            if i == len(message_parts) - 1:
                f = 0
            else:
                f = zmq.SNDMORE

            print("Send {}({}): '{}'".format(i, f, part), file=sys.stderr)

            sock.send(part.encode(), flags=f)

        data = sock.recv_multipart()

        print("Received: {}".format(len(data)), file=sys.stderr)
        for i, part in enumerate(data):
            if message_parts[0] == 'showjson':
                # This allows you to pipe the JSON into another tool
                print(part.decode())
            else:
                print(" RX {}: {}".format(i, part.decode().replace('\n',' ')), file=sys.stderr)

else:
    print("ZMQ error: timeout", file=sys.stderr)
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


