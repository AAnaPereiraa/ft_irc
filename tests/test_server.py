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

Everything runs twice: once normally, then with the server under valgrind
(memory leaks, memory errors, sockets left open). Full logs of every server
run are saved in tests/.logs/.

Run:
    make test                              # both passes
    make test FULL=1                       # + print every valgrind report
    make test FAST=1                       # normal pass only, no valgrind

    python3 tests/test_server.py --help    # all options
    python3 tests/test_server.py LineLimit # only tests whose name matches
"""

import argparse
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import time
import traceback
import unittest
from unittest.suite import _ErrorHolder

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(ROOT, "ircserv")
LOG_DIR = os.path.join(ROOT, "tests", ".logs")
PASSWORD = "testpass"
MAX_LINE = 512                      # RFC 2812 2.3, "\r\n" included

VALGRIND_CMD = [
    "valgrind",
    "--error-exitcode=42",          # any memory error or leak -> exit 42
    "--leak-check=full",
    "--show-leak-kinds=all",        # full report, still-reachable included
    "--errors-for-leak-kinds=definite,indirect",   # these fail the test
    "--track-fds=yes",              # report fds still open at exit
]

# Current pass. set_mode() changes these between the two passes; every
# function reads them at call time.
PASS_NAME = "normal"
USE_VALGRIND = False
TIMEOUT = 3.0       # max wait for something that must happen
QUIET = 0.3         # how long silence means "the server did NOT close it"


def set_mode(valgrind):
    global PASS_NAME, USE_VALGRIND, TIMEOUT, QUIET
    PASS_NAME = "valgrind" if valgrind else "normal"
    USE_VALGRIND = valgrind
    # valgrind makes the server ~10-50x slower, so give it more time.
    TIMEOUT = 15.0 if valgrind else 3.0
    QUIET = 1.0 if valgrind else 0.3


# ===========================================================================
# Terminal output
# ===========================================================================

class UI:
    """Colors and symbols. Colors are off when the output is not a terminal
    (except on GitHub Actions, which shows them) or when NO_COLOR is set."""

    def __init__(self):
        env = os.environ
        self.color = not env.get("NO_COLOR") and (
            sys.stdout.isatty() or env.get("GITHUB_ACTIONS") == "true"
            or bool(env.get("FORCE_COLOR")))
        utf8 = (sys.stdout.encoding or "").lower().replace("-", "") == "utf8"
        self.OK, self.FAIL, self.DOT, self.RULE, self.ARROW, self.BAR = (
            ("✓", "✗", "·", "─", "▶", "│") if utf8 else ("+", "x", "-", "-", ">", "|"))

    def _c(self, code, text):
        return "\033[%sm%s\033[0m" % (code, text) if self.color else text

    def bold(self, t):   return self._c("1", t)
    def dim(self, t):    return self._c("2", t)
    def red(self, t):    return self._c("31", t)
    def green(self, t):  return self._c("32", t)
    def yellow(self, t): return self._c("33", t)
    def cyan(self, t):   return self._c("36", t)

    def write(self, text=""):
        sys.stdout.write(text + "\n")
        sys.stdout.flush()

    def rule(self):
        self.write(self.dim(self.RULE * 64))


ui = UI()
NAME_WIDTH = 46


def test_title(test):
    """First docstring line, or the method name made readable."""
    doc = test._testMethodDoc
    if doc:
        return doc.strip().splitlines()[0]
    return test._testMethodName[len("test_"):].replace("_", " ")


def group_title(cls):
    return getattr(cls, "title", cls.__name__)


def short_reason(err):
    """One readable line for an exception."""
    exc_type, value, _ = err
    text = str(value).strip()
    if exc_type is AssertionError:
        # assertX(..., msg) gives "<auto text> : <msg>": keep our msg.
        if " : " in text.splitlines()[0]:
            text = text.split(" : ", 1)[1]
        return text.splitlines()[0] if text else "assertion failed"
    return "%s: %s" % (exc_type.__name__, text.splitlines()[0] if text else "")


def full_reason(err):
    exc_type, value, tb = err
    if exc_type is AssertionError:
        return str(value)
    return "".join(traceback.format_exception(exc_type, value, tb)).rstrip()


# ===========================================================================
# valgrind reports
# ===========================================================================

def _num(pattern, text):
    m = re.search(pattern, text)
    return int(m.group(1).replace(",", "")) if m else 0


def parse_valgrind(text):
    return {
        "allocs":    _num(r"total heap usage: ([\d,]+) allocs", text),
        "frees":     _num(r"([\d,]+) frees", text),
        "definite":  _num(r"definitely lost: ([\d,]+) bytes", text),
        "indirect":  _num(r"indirectly lost: ([\d,]+) bytes", text),
        "possible":  _num(r"possibly lost: ([\d,]+) bytes", text),
        "reachable": _num(r"still reachable: ([\d,]+) bytes", text),
        "errors":    _num(r"ERROR SUMMARY: ([\d,]+) errors", text),
        # Only sockets: std fds, valgrind's own log file and fds inherited
        # from the CI runner are not ours.
        "sockets":   len(re.findall(r"Open AF_INET6? socket", text)),
    }


def clean_valgrind_log(text):
    """Make the report readable: drop the '==PID==' prefixes, the banner,
    fds that are not ours, and shorten long library names."""
    skip = re.compile(r"(Memcheck,|Copyright|Using Valgrind|Command:|Parent PID:"
                      r"|For lists of detected|For counts of detected|Rerun with"
                      r"|FILE DESCRIPTORS:)")
    lines = []
    skip_fd_details = False
    for line in text.splitlines():
        line = re.sub(r"^==\d+== ?", "", line)
        # fd list: keep sockets (ours), hide std fds / valgrind's own log.
        if re.match(r"Open (file descriptor|AF_)", line):
            skip_fd_details = not line.startswith("Open AF_")
            if skip_fd_details:
                continue
        elif skip_fd_details and line.startswith("   "):
            continue
        else:
            skip_fd_details = False
        if skip.match(line) or (line == "" and (not lines or lines[-1] == "")):
            continue
        line = line.replace("std::__cxx11::basic_string<char, std::char_traits<char>, "
                            "std::allocator<char> >", "std::string")
        line = re.sub(r"\(in (/[^)]*/)?([^/)]+)\)", r"(\2)", line)   # path -> file name
        lines.append(line)
    return "\n".join(lines).strip("\n")


class MemoryCheck:
    """Result of one server run under valgrind."""

    def __init__(self, label, log_text, log_path):
        self.label = label
        self.info = parse_valgrind(log_text)
        self.report = clean_valgrind_log(log_text)
        self.log_path = log_path
        i = self.info
        self.problems = []
        if i["definite"] or i["indirect"]:
            self.problems.append("{:,} bytes leaked ({:,} definitely, {:,} indirectly lost)"
                                 .format(i["definite"] + i["indirect"], i["definite"], i["indirect"]))
        # Leaks are counted as errors too, so only report errors on their
        # own (invalid read/write, use after free, ...).
        if i["errors"] and not self.problems:
            self.problems.append("{:,} memory error(s)".format(i["errors"]))
        if i["sockets"]:
            self.problems.append("%d socket(s) still open at exit" % i["sockets"])

    @property
    def ok(self):
        return not self.problems

    def print(self, full):
        i = self.info
        if self.ok:
            text = "no leaks %s %s allocs / %s frees %s 0 errors %s no open sockets" % (
                ui.DOT, format(i["allocs"], ","), format(i["frees"], ","), ui.DOT, ui.DOT)
            if i["reachable"]:
                text += " " + ui.yellow("%s %s bytes still reachable" % (ui.DOT, format(i["reachable"], ",")))
            ui.write("    %s %s %s" % (ui.green(ui.OK), ui.cyan("valgrind"), ui.dim(text)))
        else:
            ui.write("    %s %s %s" % (ui.red(ui.FAIL), ui.cyan("valgrind"),
                                       ui.red(" %s " % ui.DOT).join(ui.red(p) for p in self.problems)))
        if full or not self.ok:
            for line in self.report.splitlines():
                ui.write("        %s %s" % (ui.dim(ui.BAR), line))
            ui.write("        %s %s" % (ui.dim(ui.BAR), ui.dim("log: " + os.path.relpath(self.log_path, ROOT))))


# Filled by Server.stop(), printed by the result at the right place.
PENDING_MEMORY = []


# ===========================================================================
# The server under test
# ===========================================================================

def free_port():
    """Ask the kernel for a port nobody is using."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Server:
    """Starts ./ircserv in the background and stops it with SIGINT."""

    def __init__(self, label):
        self.label = label
        self.port = free_port()
        log_dir = os.path.join(LOG_DIR, PASS_NAME)
        os.makedirs(log_dir, exist_ok=True)
        self.log_path = os.path.join(log_dir, label + ".server.log")
        self.vg_path = os.path.join(log_dir, label + ".valgrind.log")
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
                raise AssertionError("server exited at startup\n" + self.output_tail())
            try:
                socket.create_connection(("127.0.0.1", self.port), timeout=1).close()
                return
            except OSError:
                time.sleep(0.05)
        raise AssertionError("server did not start listening\n" + self.output_tail())

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
            if self.proc.returncode not in (0, 42):
                problems.append("server exited with %d after SIGINT" % self.proc.returncode)
        self.log.close()

        if USE_VALGRIND and os.path.exists(self.vg_path):
            with open(self.vg_path) as f:
                check = MemoryCheck(self.label, f.read(), self.vg_path)
            if self.proc.returncode == 42 and check.ok:
                check.problems.append("valgrind reported errors (see log)")
            PENDING_MEMORY.append(check)
            problems += ["valgrind: " + p for p in check.problems]

        if problems:
            problems.append(self.output_tail())
        return problems

    def output_tail(self, max_lines=12, width=100):
        """Last lines of the server output, long lines cut."""
        lines = self.output().splitlines() or ["(empty)"]
        shown = [l if len(l) <= width else l[:width] + "..." for l in lines[-max_lines:]]
        head = "last %d lines of server output" % len(shown) if len(lines) > max_lines \
            else "server output"
        return "%s (full: %s):\n  %s" % (head, os.path.relpath(self.log_path, ROOT),
                                         "\n  ".join(shown))

    def output(self):
        if not self.log.closed:
            self.log.flush()
        with open(self.log_path) as f:
            return f.read().rstrip()


# ===========================================================================
# Pretty test result
# ===========================================================================

class PrettyResult(unittest.TestResult):
    """Prints one line per test, grouped by class, as the tests run."""

    def __init__(self, full_report):
        super().__init__()
        self.full_report = full_report
        self.current_class = None
        self.started = 0.0
        self.passed = 0
        self.failed_tests = set()
        self.failed_checks = 0      # server start/shutdown checks
        self.details = []           # (title, text) for the failure section
        self.memory = []            # MemoryCheck for every server run

    # -- helpers --
    def flush_memory(self):
        while PENDING_MEMORY:
            check = PENDING_MEMORY.pop(0)
            self.memory.append(check)
            check.print(self.full_report)

    def line(self, ok, title, reason=None):
        mark = ui.green(ui.OK) if ok else ui.red(ui.FAIL)
        name = title.ljust(NAME_WIDTH)
        ui.write("    %s %s %s" % (mark, name if ok else ui.red(name),
                                   ui.dim("%5.2fs" % (time.time() - self.started))))
        if reason:
            ui.write("      %s %s" % (ui.dim("└"), ui.red(reason)))

    def record_failure(self, test, title, err):
        self.failed_tests.add(test.id())
        self.details.append(("%s %s %s" % (group_title(type(test)), ui.ARROW, title),
                             full_reason(err)))

    # -- unittest hooks --
    def startTest(self, test):
        super().startTest(test)
        cls = type(test)
        if cls is not self.current_class:
            self.flush_memory()     # from the previous class's tearDownClass
            self.current_class = cls
            doc = (cls.__doc__ or "").strip().splitlines()
            ui.write()
            ui.write("  %s  %s" % (ui.bold(group_title(cls)), ui.dim(doc[0] if doc else "")))
        self.started = time.time()

    def addSuccess(self, test):
        super().addSuccess(test)
        self.passed += 1
        self.line(True, test_title(test))
        self.flush_memory()

    def addFailure(self, test, err):
        super().addFailure(test, err)
        self.line(False, test_title(test), short_reason(err))
        self.record_failure(test, test_title(test), err)
        self.flush_memory()

    def addError(self, test, err):
        super().addError(test, err)
        if isinstance(test, _ErrorHolder):
            # setUpClass / tearDownClass: the shared server failed to start,
            # or something went wrong when it was stopped.
            self.flush_memory()
            what = "server start" if "setUp" in test.description else "server shutdown"
            self.started = time.time()
            ui.write("    %s %s" % (ui.red(ui.FAIL), ui.red(what + " check failed")))
            ui.write("      %s %s" % (ui.dim("└"), ui.red(short_reason(err))))
            self.failed_checks += 1
            self.details.append(("%s %s %s" % (
                group_title(self.current_class) if self.current_class else test.description,
                ui.ARROW, what), full_reason(err)))
            return
        self.line(False, test_title(test), short_reason(err))
        self.record_failure(test, test_title(test), err)
        self.flush_memory()

    def addSubTest(self, test, subtest, err):
        super().addSubTest(test, subtest, err)
        if err is not None:
            params = ", ".join("%s=%r" % kv for kv in subtest.params.items())
            title = "%s [%s]" % (test_title(test), params)
            self.line(False, title, short_reason(err))
            self.record_failure(test, title, err)

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        ui.write("    %s %s %s" % (ui.yellow("-"), test_title(test).ljust(NAME_WIDTH),
                                   ui.yellow("skipped: " + reason)))


# ===========================================================================
# Tests
# ===========================================================================

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


class TestArguments(unittest.TestCase):
    """./ircserv <port> <password> rejects bad input with exit 1"""
    title = "Arguments"

    def run_server(self, *args):
        return subprocess.run([BINARY] + list(args), capture_output=True, timeout=TIMEOUT)

    def assert_rejected(self, *args):
        result = self.run_server(*args)
        self.assertEqual(result.returncode, 1,
                         "args %r: expected exit 1, got %d" % (args, result.returncode))
        self.assertNotEqual(result.stderr, b"", "args %r: no error message" % (args,))

    def test_missing_arguments(self):
        """missing or extra arguments"""
        self.assert_rejected()
        self.assert_rejected("6667")
        self.assert_rejected("6667", "pw", "extra")

    def test_invalid_ports(self):
        """invalid ports (abc, 0, 65536, ...)"""
        for port in ("abc", "66a7", "-1", "0", "65536", "", "99999999999999999999"):
            with self.subTest(port=port):
                self.assert_rejected(port, PASSWORD)

    def test_empty_password(self):
        """empty password"""
        self.assert_rejected("6667", "")

    def test_port_already_in_use(self):
        """port already in use"""
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as busy:
            busy.bind(("0.0.0.0", 0))
            busy.listen(1)
            self.assert_rejected(str(busy.getsockname()[1]), PASSWORD)


class TestConnections(IrcTestCase):
    """poll loop, accept, client tracking (issues #8, #9)"""
    title = "Connections"

    def test_accepts_many_clients(self):
        """30 clients at once are all served"""
        # Each client is only closed if the server accepted it, polled it
        # and read from it -- a connect() alone would just sit in the backlog.
        clients = [self.connect() for _ in range(30)]
        for c in clients:
            c.sendall(b"A" * MAX_LINE)
        for c in clients:
            self.assert_closed_by_server(c)

    def test_disconnect_does_not_affect_others(self):
        """a disconnect doesn't affect other clients"""
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
        """client closes in the middle of a line"""
        s = self.connect()
        s.sendall(b"NICK ab")
        s.close()
        self.assert_server_alive()

    def test_client_resets_connection(self):
        """client resets the connection (RST)"""
        # SO_LINGER 0 makes close() send RST instead of a normal FIN.
        s = self.connect()
        s.sendall(b"NICK ab\r\n")
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, b"\x01\x00\x00\x00\x00\x00\x00\x00")
        s.close()
        self.assert_server_alive()

    def test_many_connect_disconnect_cycles(self):
        """100 connect/close cycles (fd reuse)"""
        # Freed fds get reused; stale map/poll entries would break this.
        for _ in range(100):
            s = socket.create_connection(("127.0.0.1", self.server.port), timeout=TIMEOUT)
            s.close()
        self.assert_server_alive()


class TestInputBuffering(IrcTestCase):
    """TCP may split or glue messages"""
    title = "Input buffering"

    def test_subject_partial_command(self):
        """subject test: 'com' + 'man' + 'd\\n'"""
        s = self.connect()
        self.send_in_pieces(s, b"com", b"man", b"d\n")
        self.assert_still_open(s)

    def test_buffer_kept_between_recv(self):
        """data kept between recv() calls"""
        # 300 + 300 bytes with no '\n': only overlong if the server kept
        # the first 300 bytes while waiting for the rest.
        s = self.connect()
        self.send_in_pieces(s, b"A" * 300, b"A" * 300)
        self.assert_closed_by_server(s)

    def test_buffer_cleared_after_line(self):
        """processed lines removed from buffer"""
        # Same 600 bytes, but with line ends: each line is short, so the
        # server must remove processed lines from the buffer.
        s = self.connect()
        self.send_in_pieces(s, b"A" * 299 + b"\n", b"A" * 299 + b"\n")
        self.assert_still_open(s)

    def test_several_commands_in_one_send(self):
        """several commands in one send()"""
        s = self.connect()
        s.sendall(b"PASS testpass\r\nNICK ana\r\nUSER ana 0 * :Ana P\r\n")
        self.assert_still_open(s)

    def test_bare_lf_accepted(self):
        """bare '\\n' accepted as line end"""
        s = self.connect()
        s.sendall(b"PING x\n")
        self.assert_still_open(s)

    def test_garbage_lines_ignored(self):
        """garbage lines ignored, no disconnect"""
        s = self.connect()
        s.sendall(b"\r\n\r\n:\r\n: NICK\r\n123456\r\nNI CK\x00x\r\n\xff\xfe\r\n")
        self.assert_still_open(s)


class TestLineLimit(IrcTestCase):
    """512 bytes including \\r\\n (RFC 2812 2.3)"""
    title = "Line limit"

    def test_max_length_line_accepted(self):
        """510 chars + '\\r\\n' accepted"""
        s = self.connect()
        s.sendall(b"A" * (MAX_LINE - 2) + b"\r\n")
        self.assert_still_open(s)

    def test_511_bytes_without_newline_kept(self):
        """511 bytes without '\\n' kept"""
        s = self.connect()
        s.sendall(b"A" * (MAX_LINE - 1))
        self.assert_still_open(s)

    def test_512_bytes_without_newline_disconnects(self):
        """512 bytes without '\\n' disconnects"""
        s = self.connect()
        s.sendall(b"A" * MAX_LINE)
        self.assert_closed_by_server(s)


class TestShutdown(unittest.TestCase):
    """SIGINT exits 0 and frees everything"""
    title = "Shutdown"

    def test_sigint_with_connected_clients(self):
        """Ctrl+C with 5 clients still connected"""
        server = Server("TestShutdown")
        server.start()
        clients = []
        try:
            for i in range(5):
                c = socket.create_connection(("127.0.0.1", server.port), timeout=TIMEOUT)
                c.sendall(b"NICK half" + str(i).encode())    # unfinished line
                clients.append(c)
            time.sleep(QUIET)
            problems = server.stop()
            if problems:
                self.fail("\n".join(problems))
        finally:
            for c in clients:
                c.close()


# Display order.
TEST_CLASSES = [TestArguments, TestConnections, TestInputBuffering, TestLineLimit, TestShutdown]


# ===========================================================================
# Runner
# ===========================================================================

def build_suite(filters):
    suite = unittest.TestSuite()
    for cls in TEST_CLASSES:
        # Methods in the order they are written in this file (unittest
        # would sort them alphabetically).
        for name in [n for n in vars(cls) if n.startswith("test_")]:
            test = cls(name)
            if not filters or any(f.lower() in test.id().lower() for f in filters):
                suite.addTest(test)
    return suite


def run_pass(number, total, valgrind, args):
    set_mode(valgrind)
    suite = build_suite(args.filters)
    label = "valgrind" if valgrind else "normal"
    note = "server under valgrind: leaks, memory errors, open sockets" if valgrind \
        else "server run directly"
    ui.write()
    ui.write(ui.bold(ui.cyan("%s Pass %d/%d %s %s" % (ui.ARROW, number, total, ui.DOT, label)))
             + "  " + ui.dim(note))
    result = PrettyResult(args.full_report)
    started = time.time()
    suite.run(result)
    result.flush_memory()
    return label, result, time.time() - started, suite.countTestCases()


def print_summary(passes):
    failures = [d for _, r, _, _ in passes for d in r.details]
    if failures:
        ui.write()
        ui.rule()
        ui.write(ui.bold(ui.red("Failures")))
        for (label, result, _, _) in passes:
            for title, text in result.details:
                ui.write()
                ui.write("  %s %s  %s" % (ui.red(ui.FAIL), ui.bold(title), ui.dim("(%s pass)" % label)))
                for line in text.splitlines():
                    ui.write("      " + line)

    ui.write()
    ui.rule()
    ui.write(ui.bold("Summary"))
    all_ok = True
    for label, result, seconds, count in passes:
        failed = len(result.failed_tests)
        bad = failed or result.failed_checks
        all_ok = all_ok and not bad
        # Pad before coloring: escape codes would break ljust().
        status = "%d/%d passed" % (count - failed, count)
        if result.failed_checks:
            status += ", %d server check(s) failed" % result.failed_checks
        status = status.ljust(30)
        status = ui.red(status) if bad else ui.green(status)
        ui.write("  %s %s %s" % (label.ljust(10), status, ui.dim("%6.1fs" % seconds)))
        if result.memory:
            clean = sum(1 for m in result.memory if m.ok)
            all_ok = all_ok and clean == len(result.memory)
            text = "%d/%d server runs clean" % (clean, len(result.memory))
            ui.write("  %s %s" % ("memory".ljust(10), ui.green(text) if clean == len(result.memory)
                                  else ui.red(text)))
    ui.write("  %s %s" % ("logs".ljust(10), ui.dim(os.path.relpath(LOG_DIR, ROOT) + "/")))
    ui.write()
    if all_ok:
        ui.write(ui.bold(ui.green("%s All tests passed" % ui.OK)))
    else:
        ui.write(ui.bold(ui.red("%s Some tests failed" % ui.FAIL)))
    return all_ok


def main():
    parser = argparse.ArgumentParser(description="ircserv integration tests")
    parser.add_argument("filters", nargs="*",
                        help="only run tests whose name contains one of these (e.g. LineLimit)")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--no-valgrind", action="store_true", help="normal pass only")
    group.add_argument("--valgrind-only", action="store_true", help="valgrind pass only")
    parser.add_argument("--full-report", action="store_true",
                        help="print the full valgrind report of every server run")
    args = parser.parse_args()

    if not os.path.exists(BINARY):
        ui.write(ui.red("ircserv not found: run `make` first"))
        return 1
    modes = [False, True]
    if args.no_valgrind:
        modes = [False]
    elif args.valgrind_only:
        modes = [True]
    if True in modes and not shutil.which("valgrind"):
        ui.write(ui.red("valgrind is not installed. Install it, or skip the valgrind pass "
                        "with `make test FAST=1`."))
        return 1

    shutil.rmtree(LOG_DIR, ignore_errors=True)
    ui.write(ui.bold("ircserv integration tests"))
    passes = [run_pass(n, len(modes), vg, args) for n, vg in enumerate(modes, 1)]
    return 0 if print_summary(passes) else 1


if __name__ == "__main__":
    sys.exit(main())
