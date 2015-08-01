#!/usr/bin/env python2

import sys
import zmq

context = zmq.Context()

sock = context.socket(zmq.REQ)

if len(sys.argv) < 2:
    print("Usage: program url cmd [args...]")
    sys.exit(1)

sock.connect(sys.argv[1])

message_parts = sys.argv[2:]

# first do a ping test

print("ping")
sock.send("ping")
data = sock.recv_multipart()
print("Received: {}".format(len(data)))
for i,part in enumerate(data):
    print("   {}".format(part))

for i, part in enumerate(message_parts):
    if i == len(message_parts) - 1:
        f = 0
    else:
        f = zmq.SNDMORE

    print("Send {}({}): '{}'".format(i, f, part))

    sock.send(part, flags=f)

data = sock.recv_multipart()

print("Received: {}".format(len(data)))
for i,part in enumerate(data):
    print(" RX {}: {}".format(i, part))

