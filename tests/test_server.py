#!/usr/bin/env python3
"""Integration tests for ircserv.

Each test starts the real ./ircserv, talks to it over TCP like an IRC client
would, and checks what the server does. Only the Python standard library is
used, so it runs anywhere python3 is installed (including GitHub Actions).

The server does not send replies yet, so the tests rely on the one thing we
can observe from outside: whether the server keeps a connection open or
closes it. The rule we use is the 512-byte line limit: a client whose buffer
reaches 512 bytes without a '\\n' gets disconnected. That lets us prove that
a client was accepted, polled and read, and that buffering works across
several recv() calls. When commands start answering (PING -> PONG, 001...),
add tests that check the replies.

Run:
    make test       # runs every test twice: normally, then with the server
                    # under valgrind (leaks, memory errors, open sockets),
                    # and prints valgrind's full report for each server run

    python3 tests/test_server.py              # quick run, no valgrind
    VALGRIND=1 python3 tests/test_server.py   # valgrind run only
"""

import os
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(ROOT, "ircserv")
PASSWORD = "testpass"
MAX_LINE = 512                      # RFC 2812 2.3, "\r\n" included

USE_VALGRIND = os.environ.get("VALGRIND") == "1"
# valgrind makes the server ~10-50x slower, so give it more time.
TIMEOUT = 15.0 if USE_VALGRIND else 3.0
# How long we wait to decide "the server did NOT close this connection".
QUIET = 1.0 if USE_VALGRIND else 0.3

VALGRIND_CMD = [
    "valgrind",
    "--error-exitcode=42",          # any memory error or leak -> exit 42
    "--leak-check=full",
    "--show-leak-kinds=all",        # full report, still-reachable included
    "--errors-for-leak-kinds=definite,indirect",   # these fail the test
    "--track-fds=yes",              # report fds still open at exit
]


def free_port():
    """Ask the kernel for a port nobody is using."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Server:
    """Starts ./ircserv in the background and stops it with SIGINT."""

    def __init__(self, label):
        self.label = label          # shown in the valgrind report header
        self.port = free_port()
        self.tmp = tempfile.mkdtemp(prefix="ircserv-test-")
        self.log_path = os.path.join(self.tmp, "server.log")
        self.vg_path = os.path.join(self.tmp, "valgrind.log")
        self.proc = None

    def start(self):
        cmd = [BINARY, str(self.port), PASSWORD]
        if USE_VALGRIND:
            cmd = VALGRIND_CMD + ["--log-file=" + self.vg_path] + cmd
        self.log = open(self.log_path, "w")
        self.proc = subprocess.Popen(cmd, stdout=self.log, stderr=subprocess.STDOUT)

        # Wait until the port accepts connections instead of a fixed sleep.
        deadline = time.time() + TIMEOUT
        while time.time() < deadline:
            if self.proc.poll() is not None:
                raise AssertionError("server exited at startup:\n" + self.output())
            try:
                socket.create_connection(("127.0.0.1", self.port), timeout=1).close()
                return
            except OSError:
                time.sleep(0.05)
        raise AssertionError("server did not start listening:\n" + self.output())

    def stop(self):
        """SIGINT the server and check it shut down cleanly.

        Returns a list of problems (empty = all good)."""
        problems = []
        if self.proc.poll() is not None:
            problems.append("server died during the tests (exit %d)" % self.proc.returncode)
        else:
            self.proc.send_signal(signal.SIGINT)
            try:
                self.proc.wait(timeout=TIMEOUT)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
                problems.append("server ignored SIGINT and had to be killed")
            if self.proc.returncode == 42 and USE_VALGRIND:
                problems.append("valgrind found memory errors or leaks")
            elif self.proc.returncode != 0:
                problems.append("server exited with %d after SIGINT" % self.proc.returncode)

        if USE_VALGRIND and os.path.exists(self.vg_path):
            with open(self.vg_path) as f:
                vg = f.read()
            # Only sockets: std fds and fds inherited from the CI runner are
            # not ours. A leaked client or listening socket shows up here.
            if re.search(r"Open AF_INET6? socket", vg):
                problems.append("sockets still open at exit")
            # Always show the full report, so leaks can be read even when
            # nothing failed (e.g. "still reachable" memory).
            bar = "=" * 70
            sys.stderr.write("\n%s\nvalgrind report: %s\n%s\n%s%s\n"
                             % (bar, self.label, bar, vg, bar))
            sys.stderr.flush()
        self.log.close()
        if problems:
            problems.append("--- server output ---\n" + self.output())
        return problems

    def output(self):
        self.log.flush()
        with open(self.log_path) as f:
            return f.read()


class IrcTestCase(unittest.TestCase):
    """One server shared by every test in the class (faster, and it also
    proves the server survives everything the previous tests did)."""

    @classmethod
    def setUpClass(cls):
        cls.server = Server(cls.__name__)
        cls.server.start()

    @classmethod
    def tearDownClass(cls):
        problems = cls.server.stop()
        if problems:
            raise AssertionError("\n".join(problems))

    def setUp(self):
        self.sockets = []

    def tearDown(self):
        for s in self.sockets:
            s.close()

    # --- helpers -----------------------------------------------------------

    def connect(self):
        s = socket.create_connection(("127.0.0.1", self.server.port), timeout=TIMEOUT)
        self.sockets.append(s)
        return s

    def send_in_pieces(self, sock, *pieces):
        """Send each piece separately, with a pause, so they arrive as
        different TCP packets (the subject's `com^Dman^Dd` test)."""
        for piece in pieces:
            sock.sendall(piece)
            time.sleep(0.05)

    def assert_closed_by_server(self, sock):
        """The server must close this connection (recv() returns b'')."""
        sock.settimeout(TIMEOUT)
        try:
            while True:
                if sock.recv(4096) == b"":
                    return
        except ConnectionResetError:
            return              # closed with unread data -> RST, also closed
        except socket.timeout:
            self.fail("server did not close the connection")

    def assert_still_open(self, sock):
        """The server must NOT close this connection. Replies are fine."""
        sock.settimeout(QUIET)
        try:
            while True:
                if sock.recv(4096) == b"":
                    self.fail("server closed the connection")
        except socket.timeout:
            return
        except ConnectionResetError:
            self.fail("server reset the connection")

    def assert_server_alive(self):
        self.assertIsNone(self.server.proc.poll(), "server process died")
        # A new client must still be served: prove it with an overlong line.
        s = self.connect()
        s.sendall(b"A" * MAX_LINE)
        self.assert_closed_by_server(s)


class TestConnections(IrcTestCase):
    """Issue #8 / #9: poll loop, accept, client tracking."""

    def test_accepts_many_clients(self):
        # Each client is only closed if the server accepted it, polled it
        # and read from it -- a connect() alone would just sit in the backlog.
        clients = [self.connect() for _ in range(30)]
        for c in clients:
            c.sendall(b"A" * MAX_LINE)
        for c in clients:
            self.assert_closed_by_server(c)

    def test_disconnect_does_not_affect_others(self):
        # Removing an entry from the middle of the poll set must not skip
        # or break the clients after it.
        a, b, c = self.connect(), self.connect(), self.connect()
        self.assert_still_open(b)
        b.close()
        time.sleep(0.1)
        for s in (a, c):
            s.sendall(b"A" * MAX_LINE)
            self.assert_closed_by_server(s)

    def test_client_closes_mid_line(self):
        s = self.connect()
        s.sendall(b"NICK ab")
        s.close()
        self.assert_server_alive()

    def test_client_resets_connection(self):
        # SO_LINGER 0 makes close() send RST instead of a normal FIN.
        s = self.connect()
        s.sendall(b"NICK ab\r\n")
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, b"\x01\x00\x00\x00\x00\x00\x00\x00")
        s.close()
        self.assert_server_alive()

    def test_many_connect_disconnect_cycles(self):
        # Freed fds get reused; stale map/poll entries would break this.
        for _ in range(100):
            s = socket.create_connection(("127.0.0.1", self.server.port), timeout=TIMEOUT)
            s.close()
        self.assert_server_alive()


class TestInputBuffering(IrcTestCase):
    """Input buffering: TCP may split or glue messages."""

    def test_subject_partial_command(self):
        # Exactly the subject's test: 'com', then 'man', then 'd\n'.
        s = self.connect()
        self.send_in_pieces(s, b"com", b"man", b"d\n")
        self.assert_still_open(s)

    def test_buffer_kept_between_recv(self):
        # 300 + 300 bytes with no '\n': only overlong if the server kept
        # the first 300 bytes while waiting for the rest.
        s = self.connect()
        self.send_in_pieces(s, b"A" * 300, b"A" * 300)
        self.assert_closed_by_server(s)

    def test_buffer_cleared_after_line(self):
        # Same 600 bytes, but with line ends: each line is short, so the
        # server must remove processed lines from the buffer.
        s = self.connect()
        self.send_in_pieces(s, b"A" * 299 + b"\n", b"A" * 299 + b"\n")
        self.assert_still_open(s)

    def test_several_commands_in_one_send(self):
        s = self.connect()
        s.sendall(b"PASS testpass\r\nNICK ana\r\nUSER ana 0 * :Ana P\r\n")
        self.assert_still_open(s)

    def test_bare_lf_accepted(self):
        s = self.connect()
        s.sendall(b"PING x\n")
        self.assert_still_open(s)

    def test_garbage_lines_ignored(self):
        s = self.connect()
        s.sendall(b"\r\n\r\n:\r\n: NICK\r\n123456\r\nNI CK\x00x\r\n\xff\xfe\r\n")
        self.assert_still_open(s)


class TestLineLimit(IrcTestCase):
    """512-byte limit (RFC 2812 2.3), "\\r\\n" included."""

    def test_max_length_line_accepted(self):
        s = self.connect()
        s.sendall(b"A" * (MAX_LINE - 2) + b"\r\n")
        self.assert_still_open(s)

    def test_511_bytes_without_newline_kept(self):
        s = self.connect()
        s.sendall(b"A" * (MAX_LINE - 1))
        self.assert_still_open(s)

    def test_512_bytes_without_newline_disconnects(self):
        s = self.connect()
        s.sendall(b"A" * MAX_LINE)
        self.assert_closed_by_server(s)


class TestShutdown(unittest.TestCase):
    """SIGINT with clients still connected: exit 0, nothing leaked."""

    def test_sigint_with_connected_clients(self):
        server = Server(self.id().split('.')[-1])
        server.start()
        clients = []
        try:
            for i in range(5):
                c = socket.create_connection(("127.0.0.1", server.port), timeout=TIMEOUT)
                c.sendall(b"NICK half" + str(i).encode())    # unfinished line
                clients.append(c)
            time.sleep(QUIET)
            problems = server.stop()
            self.assertEqual(problems, [], "\n".join(problems))
        finally:
            for c in clients:
                c.close()


class TestArguments(unittest.TestCase):
    """./ircserv <port> <password> must reject bad input with exit 1."""

    def run_server(self, *args):
        return subprocess.run([BINARY] + list(args), capture_output=True, timeout=TIMEOUT)

    def assert_rejected(self, *args):
        result = self.run_server(*args)
        self.assertEqual(result.returncode, 1,
                         "args %r: expected exit 1, got %d" % (args, result.returncode))
        self.assertNotEqual(result.stderr, b"", "args %r: no error message" % (args,))

    def test_missing_arguments(self):
        self.assert_rejected()
        self.assert_rejected("6667")
        self.assert_rejected("6667", "pw", "extra")

    def test_invalid_ports(self):
        for port in ("abc", "66a7", "-1", "0", "65536", "", "99999999999999999999"):
            with self.subTest(port=port):
                self.assert_rejected(port, PASSWORD)

    def test_empty_password(self):
        self.assert_rejected("6667", "")

    def test_port_already_in_use(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as busy:
            busy.bind(("0.0.0.0", 0))
            busy.listen(1)
            self.assert_rejected(str(busy.getsockname()[1]), PASSWORD)


if __name__ == "__main__":
    if not os.path.exists(BINARY):
        raise SystemExit("ircserv not found: run `make` first")
    unittest.main(verbosity=2)
