from unittest import TestCase
from Measure import Measure
import socket


class TestMeasure(TestCase):

    def _open_socks(self):
        sock_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock_server.bind(('localhost', 1234))
        sock_server.listen(1)

        sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock_client.connect(('localhost', 1234))

        conn_server, addr_server = sock_server.accept()
        return conn_server, sock_client

    def test__recv_exact(self):
        m = Measure(1234, 1)
        payload = b"test payload"

        conn_server, sock_client = self._open_socks()
        conn_server.send(payload)
        rec = m._recv_exact(sock_client, len(payload))

        self.assertEqual(rec, payload,
                "Did not receive the same message as sended. (%s, %s)" %
                (rec, payload))

    def test_get_samples(self):
        self.fail()
