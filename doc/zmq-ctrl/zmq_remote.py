#!/usr/bin/env python

import sys
import zmq

context = zmq.Context()

sock = context.socket(zmq.REQ)

poller = zmq.Poller()
poller.register(sock, zmq.POLLIN)

if len(sys.argv) < 2:
    print("Usage: program url cmd [args...]")
    sys.exit(1)

sock.connect(sys.argv[1])

message_parts = sys.argv[2:]

# first do a ping test

print("ping")
sock.send(b"ping")

socks = dict(poller.poll(1000))
if socks:
    if socks.get(sock) == zmq.POLLIN:

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

            sock.send(part.encode(), flags=f)

        data = sock.recv_multipart()

        print("Received: {}".format(len(data)))
        for i,part in enumerate(data):
            print(" RX {}: {}".format(i, part))

else:
    print("ZMQ error: timeout")
    context.destroy(linger=5)

