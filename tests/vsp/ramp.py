#!/usr/bin/env python3
#
#  mhuxd - mircoHam device mutliplexer/demultiplexer
#  Copyright (C) 2012-2026  Matthias Moeller, DJ5QV
#
#  This program can be distributed under the terms of the GNU GPLv2.
#  See the file COPYING
#
"""VSP throughput ramp.

Steps through the standard baud rates in both directions and reports where data
starts to be lost, so a bottleneck on a slow machine can be pinned down.

    ./ramp.py [--seconds 0.5] [--max-baud 230400] [--direction in|out|both]

Inbound (rig -> application) is the interesting direction: when the connector's
buf_out overflows, bytes are dropped silently. The exact count comes from
TIOCGICOUNT's buf_overrun, and the harness reports how much it managed to push,
which separates the two possible bottlenecks:

    overrun > 0                 the reader is not draining buf_out fast enough
    sent < offered              the connector is not draining the router socket

Outbound (application -> rig) cannot lose data: a full buffer shows up as a short
write or EAGAIN, so what is reported there is the ceiling an application can
sustain, not loss.

Note the client here is Python, so its own overhead is part of the inbound
measurement. A C application will get further on the same machine.
"""

import argparse
import os
import select
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from run_tests import Harness, Failure, icount, preflight  # noqa: E402

BAUDS = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600]


def baud_to_bps(baud):
    """8N1: one byte costs 10 bits on the wire."""
    return baud / 10.0


def measure_inbound(h, baud, seconds, reader_delay=0.0):
    """Harness -> client. Returns a result dict."""
    bps = baud_to_bps(baud)
    fd = h.open_client(nonblock=True)
    try:
        h.cmd("reset")
        before = icount(fd)
        offered = int(bps * seconds)

        start = time.time()
        h.cmd("paced %f %d" % (bps, int(seconds * 1000)))

        got = 0
        pacing_done = start + seconds
        deadline = pacing_done + 0.5          # hard cap, only hit if something sticks
        while time.time() < deadline:
            if wait_readable(fd, 0.05):
                try:
                    chunk = os.read(fd, 65536)
                except BlockingIOError:
                    continue
                got += len(chunk)
                if reader_delay:
                    # model an application that does work between reads
                    time.sleep(reader_delay)
            elif time.time() >= pacing_done:
                # generator finished and nothing left to read: stop waiting out
                # the drain allowance, it costs a step's worth of time each time
                break
        elapsed = time.time() - start

        after = icount(fd)
        sent = int(h.cmd("stats").split()[1])
        dropped = after["buf_overrun"] - before["buf_overrun"]
        return {
            "offered": offered,
            "sent": sent,
            "got": got,
            "dropped": dropped,
            "elapsed": elapsed,
        }
    finally:
        os.close(fd)


def measure_outbound(h, baud, seconds):
    """Client -> harness. Returns a result dict."""
    bps = baud_to_bps(baud)
    fd = h.open_client(nonblock=True)
    try:
        h.cmd("reset")
        h.cmd("sink on")
        offered = int(bps * seconds)

        chunk_size = max(1, int(bps / 100))       # aim for ~100 writes/s
        written = 0
        short = 0
        again = 0
        start = time.time()
        pos = 0
        while written < offered:
            now = time.time()
            if now - start > seconds + 0.5:
                break
            target = int((now - start) * bps)
            if written >= target:
                time.sleep(0.002)
                continue
            want = min(chunk_size, offered - written, target - written)
            buf = bytes((pos + i) & 0xFF for i in range(want))
            try:
                n = os.write(fd, buf)
            except BlockingIOError:
                again += 1
                time.sleep(0.002)
                continue
            if n < want:
                short += 1
            written += n
            pos += n
        elapsed = time.time() - start

        # let the tail drain, but no longer than it actually takes
        tail_deadline = time.time() + 0.3
        while time.time() < tail_deadline:
            if int(h.cmd("stats").split()[2]) >= written:
                break
            time.sleep(0.01)
        stats = h.cmd("stats").split()
        received, seq_errors = int(stats[2]), int(stats[3])
        h.cmd("sink off")
        return {
            "offered": offered,
            "written": written,
            "received": received,
            "seq_errors": seq_errors,
            "short": short,
            "again": again,
            "elapsed": elapsed,
        }
    finally:
        os.close(fd)


def wait_readable(fd, timeout):
    p = select.poll()
    p.register(fd, select.POLLIN)
    return bool(p.poll(timeout * 1000))


def as_baud(byte_rate):
    return byte_rate * 10


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seconds", type=float, default=0.5,
                    help="measurement time per step (default 0.5)")
    ap.add_argument("--max-baud", type=int, default=230400)
    ap.add_argument("--direction", choices=("in", "out", "both"), default="both")
    ap.add_argument("--reader-delay", type=float, default=0.0, metavar="MS",
                    help="stall the inbound reader this long between reads, to "
                         "model a busy application (default 0)")
    args = ap.parse_args()

    rc = preflight()
    if rc is not None:
        return rc

    rates = [b for b in BAUDS if b <= args.max_baud]

    if args.direction in ("in", "both"):
        print("\ninbound  (rig -> application), %.1fs per step%s" %
              (args.seconds,
               ", reader stalls %.0f ms between reads" % args.reader_delay
               if args.reader_delay else ""))
        print("  %9s %10s %10s %10s  %s" % ("baud", "offered", "delivered", "dropped", "verdict"))
        clean = None
        first_fail = None
        h = Harness()
        try:
            for baud in rates:
                r = measure_inbound(h, baud, args.seconds, args.reader_delay / 1000.0)
                if r["dropped"] == 0 and r["sent"] >= r["offered"] * 0.98:
                    verdict = "ok"
                    clean = baud
                elif r["dropped"] > 0:
                    if first_fail is None:
                        first_fail = baud
                    verdict = "reader too slow (buf_out overflow)"
                else:
                    verdict = "connector too slow (router socket backed up)"
                print("  %9d %10d %10d %10d  %s"
                      % (baud, r["offered"], r["got"], r["dropped"], verdict))
        finally:
            h.close()
        print("  -> highest clean inbound rate: %s"
              % ("%d baud" % clean if clean else "none of the tested rates"))
        if args.reader_delay and first_fail:
            # A stall of D seconds at B bytes/s needs D*B bytes of buf_out to ride
            # out. Printing the requirement at the first failing rate says how big
            # the buffer would have to be to pass that step.
            need = int(baud_to_bps(first_fail) * args.reader_delay / 1000.0)
            print("     %.0f ms of stall needs %d bytes of buf_out at %d baud "
                  "(BUFFER_CAPACITY is %d)" % (args.reader_delay, need, first_fail, 512))

    if args.direction in ("out", "both"):
        print("\noutbound (application -> rig), %.1fs per step" % args.seconds)
        print("  %9s %10s %10s %8s %8s  %s"
              % ("baud", "offered", "received", "short", "eagain", "verdict"))
        clean = None
        h = Harness()
        try:
            for baud in rates:
                r = measure_outbound(h, baud, args.seconds)
                achieved = r["written"] / r["elapsed"] if r["elapsed"] else 0
                if r["seq_errors"]:
                    verdict = "CORRUPTION: %d sequence errors" % r["seq_errors"]
                elif r["written"] >= r["offered"] * 0.98:
                    verdict = "ok"
                    clean = baud
                else:
                    verdict = "ceiling ~%d baud" % as_baud(int(achieved))
                print("  %9d %10d %10d %8d %8d  %s"
                      % (baud, r["offered"], r["received"], r["short"], r["again"], verdict))
        finally:
            h.close()
        print("  -> highest clean outbound rate: %s"
              % ("%d baud" % clean if clean else "none of the tested rates"))

    print("\nNote: the client is Python, its overhead is included in these numbers.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Failure as e:
        print("error: %s" % e)
        sys.exit(1)
