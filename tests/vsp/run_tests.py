#!/usr/bin/env python3
#
#  mhuxd - mircoHam device mutliplexer/demultiplexer
#  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
#
#  This program can be distributed under the terms of the GNU GPLv2.
#  See the file COPYING
#
"""VSP test suite.

Drives tests/vsp/vsp_harness (which owns the router end of the socketpair) while
acting as a normal client application on /dev/mhuxd/<name>, so the tty behaviour
of con_vsp.c is exercised through real read/write/poll/ioctl syscalls.

Needs root and the cuse module:

    modprobe cuse
    make -C tests/vsp
    sudo ./tests/vsp/run_tests.py [-v] [--only NAME] [--allow-known]

Tests tagged with a finding number from docs/review_con_csp.md are expected to
fail until that finding is fixed; --allow-known exits 0 when only those fail.
"""

import argparse
import os
import fcntl
import select
import signal
import subprocess
import struct
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HARNESS = os.path.join(HERE, "vsp_harness")
DEVDIR = "/dev/mhuxd"
CUSE = "/dev/cuse"

TIMEOUT = 2.0
WATCHDOG = 30      # per test, guards against a test looping forever

TIOCGICOUNT = 0x545D
# struct serial_icounter_struct: cts dsr rng dcd rx tx frame overrun parity brk
# buf_overrun, then reserved[9]
ICOUNT_FIELDS = ["cts", "dsr", "rng", "dcd", "rx", "tx", "frame", "overrun",
                 "parity", "brk", "buf_overrun"]


def icount(fd):
    """TIOCGICOUNT as a dict."""
    raw = fcntl.ioctl(fd, TIOCGICOUNT, bytes(20 * 4), True)
    vals = struct.unpack("20i", raw)
    return dict(zip(ICOUNT_FIELDS, vals))

TESTS = []


def test(name, known=None):
    """Register a test. known="#12" marks a known-failing finding."""
    def deco(fn):
        TESTS.append((name, fn, known))
        return fn
    return deco


class Failure(Exception):
    pass


def require(cond, msg):
    if not cond:
        raise Failure(msg)


class Harness:
    """One vsp_harness process and the VSP device it creates."""

    _seq = 0

    def __init__(self, maxcon=4, verbose=False):
        Harness._seq += 1
        self.name = "vsptest%d_%d" % (os.getpid(), Harness._seq)
        self.path = os.path.join(DEVDIR, self.name)
        self.verbose = verbose
        self.logfile = "/tmp/vsp_harness_%s.log" % self.name
        env = dict(os.environ, VSP_HARNESS_LOG=self.logfile)
        self.proc = subprocess.Popen(
            [HARNESS, self.name, str(maxcon)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1, env=env)
        line = self.proc.stdout.readline().strip()
        if line != "ready":
            raise Failure("harness did not start: %r (log: %s)" % (line, self.logfile))
        self._wait_for_device()

    def _wait_for_device(self):
        """Wait for the node to exist *and* be openable.

        The kernel creates it as 0600 root:root and udev only then applies
        59-mhuxd.rules (SUBSYSTEM=="cuse", KERNEL=="mhuxd*", GROUP="mhuxd"),
        which takes a few milliseconds. Waiting for existence alone loses that
        race and the following open fails with EACCES.
        """
        deadline = time.time() + TIMEOUT
        existed = False
        while time.time() < deadline:
            if os.path.exists(self.path):
                existed = True
                if os.access(self.path, os.R_OK | os.W_OK):
                    return
            time.sleep(0.002)
        if existed:
            raise Failure("%s exists but is not openable - are you in the 'mhuxd' "
                          "group, or root? (see 59-mhuxd.rules)" % self.path)
        raise Failure("device %s never appeared" % self.path)

    def cmd(self, line):
        if self.verbose:
            print("    > %s" % line)
        self.proc.stdin.write(line + "\n")
        self.proc.stdin.flush()
        resp = self.proc.stdout.readline().strip()
        if self.verbose:
            print("    < %s" % resp)
        if resp.startswith("err"):
            raise Failure("harness: %s (for %r)" % (resp, line))
        return resp

    def send(self, data: bytes):
        self.cmd("send " + data.hex())

    def flood(self, n):
        self.cmd("flood %d" % n)

    def recv(self) -> bytes:
        resp = self.cmd("recv")
        return bytes.fromhex(resp[5:]) if len(resp) > 5 else b""

    def recv_until(self, n, timeout=TIMEOUT) -> bytes:
        """Collect at least n bytes from the router end, or return what arrived."""
        got = b""
        deadline = time.time() + timeout
        while len(got) < n and time.time() < deadline:
            got += self.recv()
            if len(got) < n:
                time.sleep(0.01)
        return got

    def stop_reading(self):
        self.cmd("stopreading")

    def resume(self):
        self.cmd("resume")

    def open_client(self, nonblock=False):
        flags = os.O_RDWR | os.O_NOCTTY
        if nonblock:
            flags |= os.O_NONBLOCK
        # os.access() above is advisory, so still retry briefly on EACCES in case
        # udev has not finished with the node yet.
        deadline = time.time() + TIMEOUT
        while True:
            try:
                return os.open(self.path, flags)
            except PermissionError:
                if time.time() >= deadline:
                    raise
                time.sleep(0.002)

    def close(self):
        try:
            self.proc.stdin.write("quit\n")
            self.proc.stdin.flush()
        except (BrokenPipeError, ValueError):
            pass
        try:
            self.proc.wait(timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
        try:
            os.unlink(self.logfile)
        except OSError:
            pass


def wait_poll(fd, events, timeout=TIMEOUT):
    """Wait for any of `events`, return the reported mask (0 on timeout)."""
    p = select.poll()
    p.register(fd, events)
    res = p.poll(timeout * 1000)
    return res[0][1] if res else 0


def drain(fd, limit=64):
    """Read whatever is pending. Tolerates both EAGAIN (correct) and a 0-byte
    reply (finding #13) so tests that only need to empty the buffer do not
    depend on that finding being fixed."""
    got = b""
    for _ in range(limit):
        try:
            chunk = os.read(fd, 4096)
        except (BlockingIOError, OSError):
            break
        if not chunk:
            break
        got += chunk
    return got


def read_deadline(fd, size, timeout=TIMEOUT):
    """Blocking read in a thread. Returns (data, error, timed_out)."""
    box = {}

    def worker():
        try:
            box["data"] = os.read(fd, size)
        except OSError as e:
            box["err"] = e

    t = threading.Thread(target=worker, daemon=True)
    t.start()
    t.join(timeout)
    if t.is_alive():
        return None, None, True
    return box.get("data"), box.get("err"), False


# ---------------------------------------------------------------- tests

@test("smoke_roundtrip")
def t_smoke(h):
    """The rig itself works: bytes flow both ways."""
    fd = h.open_client(nonblock=True)
    try:
        h.send(b"\x01\x02\x03\x04\x05")
        require(wait_poll(fd, select.POLLIN) & select.POLLIN, "no POLLIN after send")
        require(os.read(fd, 512) == b"\x01\x02\x03\x04\x05", "wrong data from device")

        os.write(fd, b"hello")
        require(h.recv_until(5) == b"hello", "router did not see the write")
    finally:
        os.close(fd)


@test("poll_pollin", known=None)
def t_poll_in(h):
    """poll() reports POLLIN only once data is actually there."""
    fd = h.open_client(nonblock=True)
    try:
        require(not (wait_poll(fd, select.POLLIN, 0.2) & select.POLLIN),
                "POLLIN reported on an empty device")
        h.send(b"\xaa")
        require(wait_poll(fd, select.POLLIN) & select.POLLIN, "POLLIN missing after send")
    finally:
        os.close(fd)


@test("blocking_read_returns_available", known="#12")
def t_blocking_read(h):
    """A blocking read must return as soon as any data is there (VMIN=1),
    not wait for the full requested count."""
    fd = h.open_client(nonblock=False)
    try:
        h.send(b"\x01\x02\x03\x04\x05")
        require(wait_poll(fd, select.POLLIN) & select.POLLIN, "no POLLIN after send")

        data, err, timed_out = read_deadline(fd, 512, 1.0)
        require(not timed_out,
                "read(fd, 512) still blocked after 1s although 5 bytes were available")
        require(err is None, "read failed: %s" % err)
        require(data == b"\x01\x02\x03\x04\x05", "wrong data: %r" % data)
    finally:
        os.close(fd)


@test("nonblocking_read_empty_is_eagain", known="#13")
def t_nonblocking_read(h):
    """A non-blocking read with no data must fail with EAGAIN. Returning 0 bytes
    means EOF to the application, which makes it close the port."""
    fd = h.open_client(nonblock=True)
    try:
        try:
            data = os.read(fd, 512)
        except BlockingIOError:
            return  # correct
        raise Failure("read returned %d bytes (EOF) instead of raising EAGAIN" % len(data))
    finally:
        os.close(fd)


@test("nonblocking_write_full_is_eagain", known="#13")
def t_nonblocking_write(h):
    """A non-blocking write with no buffer space must fail with EAGAIN.
    Returning 0 makes applications spin."""
    h.stop_reading()
    fd = h.open_client(nonblock=True)
    try:
        chunk = b"\x5a" * 256
        for i in range(200):
            try:
                n = os.write(fd, chunk)
            except BlockingIOError:
                return  # correct
            require(n != 0,
                    "write returned 0 after %d chunks instead of raising EAGAIN" % i)
            if n < len(chunk):
                # short write is fine, keep going until it refuses entirely
                continue
        raise Failure("buffer never filled up after 200 chunks")
    finally:
        os.close(fd)
        h.resume()


@test("poll_pollout_after_drain")
def t_pollout(h):
    """Once the router drains, a client waiting for POLLOUT must be woken."""
    h.stop_reading()
    fd = h.open_client(nonblock=True)
    try:
        for _ in range(200):
            try:
                if os.write(fd, b"\x5a" * 256) == 0:
                    break
            except BlockingIOError:
                break
        require(not (wait_poll(fd, select.POLLOUT, 0.2) & select.POLLOUT),
                "POLLOUT still reported although the buffer is full")
        h.resume()
        require(wait_poll(fd, select.POLLOUT, TIMEOUT) & select.POLLOUT,
                "POLLOUT never reported after the router drained")
    finally:
        os.close(fd)


@test("two_clients_one_idle")
def t_two_clients(h):
    """A client that stops reading must not starve the other clients."""
    idle = h.open_client(nonblock=True)      # opened first, scanned first
    active = h.open_client(nonblock=True)
    try:
        # overflow the idle client's 512 byte buffer
        h.send(b"\x11" * 512)
        time.sleep(0.2)
        drain(active)

        h.send(b"MARKER")
        deadline = time.time() + TIMEOUT
        got = b""
        while time.time() < deadline and b"MARKER" not in got:
            if wait_poll(active, select.POLLIN, 0.1) & select.POLLIN:
                got += drain(active)
        require(b"MARKER" in got,
                "the reading client got %d bytes, marker never arrived" % len(got))
    finally:
        os.close(idle)
        os.close(active)


@test("overrun_is_counted")
def t_overrun(h):
    """Bytes dropped because a client is not reading must show up in
    TIOCGICOUNT's buf_overrun, the way a tty driver counts flip buffer drops."""
    fd = h.open_client(nonblock=True)
    try:
        before = icount(fd)
        require(before["buf_overrun"] == 0,
                "buf_overrun started at %d" % before["buf_overrun"])

        # 512 bytes fills buf_out exactly, the rest has nowhere to go
        h.send(b"\x22" * 512)
        time.sleep(0.2)
        h.send(b"\x22" * 256)
        time.sleep(0.2)

        after = icount(fd)
        require(after["buf_overrun"] > 0,
                "buf_overrun still 0 after overflowing the buffer")
        require(after["rx"] > 0, "rx counter not advancing (%d)" % after["rx"])
    finally:
        os.close(fd)


@test("throughput_smoke")
def t_throughput(h):
    """Push data through and report the rate. The connector drops on overflow
    rather than stalling the router, so this measures, it does not assert."""
    total = 64 * 1024
    fd = h.open_client(nonblock=True)
    try:
        start = time.time()
        h.flood(total)
        got = bytearray()
        deadline = time.time() + 10.0
        while len(got) < total and time.time() < deadline:
            if wait_poll(fd, select.POLLIN, 0.2) & select.POLLIN:
                chunk = drain(fd)
                if not chunk:
                    break
                got += chunk
            elif len(got):
                break  # nothing more coming
        elapsed = time.time() - start

        gaps = 0
        for i in range(1, len(got)):
            if got[i] != (got[i - 1] + 1) & 0xFF:
                gaps += 1
        require(len(got) > 0, "received nothing at all")
        h.note = ("%d/%d bytes in %.2fs (%.0f kB/s), %d discontinuities - the "
                  "connector drops on overflow instead of stalling the router"
                  % (len(got), total, elapsed, len(got) / elapsed / 1024, gaps))
    finally:
        os.close(fd)


# ---------------------------------------------------------------- runner

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="echo the harness protocol")
    ap.add_argument("--only", metavar="NAME", help="run only tests matching NAME")
    ap.add_argument("--allow-known", action="store_true",
                    help="exit 0 if only known-issue tests fail")
    args = ap.parse_args()

    if not os.path.exists(HARNESS):
        print("SKIP: %s not built, run 'make -C tests/vsp' first" % HARNESS)
        return 77
    if not os.path.exists(CUSE):
        print("SKIP: %s missing, try 'modprobe cuse'" % CUSE)
        return 77
    if not os.access(CUSE, os.R_OK | os.W_OK):
        print("SKIP: no access to %s, run as root" % CUSE)
        return 77

    def watchdog(signum, frame):
        raise Failure("test timed out after %ds (watchdog)" % WATCHDOG)

    signal.signal(signal.SIGALRM, watchdog)

    failed, known_failed, passed = [], [], 0

    for name, fn, known in TESTS:
        if args.only and args.only not in name:
            continue
        tag = " [%s]" % known if known else ""
        print("  %-36s%s" % (name, tag), end=" ", flush=True)
        h = None
        try:
            signal.alarm(WATCHDOG)
            h = Harness(verbose=args.verbose)
            fn(h)
            signal.alarm(0)
            print("PASS" + (" (known issue is fixed)" if known else ""))
            if getattr(h, "note", None):
                print("      %s" % h.note)
            passed += 1
        except Failure as e:
            print("FAIL\n      %s" % e)
            (known_failed if known else failed).append((name, known, str(e)))
        except Exception as e:  # noqa: BLE001 - report anything a test throws
            print("ERROR\n      %s: %s" % (type(e).__name__, e))
            failed.append((name, known, "%s: %s" % (type(e).__name__, e)))
        finally:
            signal.alarm(0)
            if h:
                h.close()

    print("\n%d passed, %d failed, %d known-issue failures"
          % (passed, len(failed), len(known_failed)))
    for name, known, err in known_failed:
        print("  known %s  %s" % (known, name))
    for name, known, err in failed:
        print("  FAIL     %s" % name)

    if failed:
        return 1
    if known_failed and not args.allow_known:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
