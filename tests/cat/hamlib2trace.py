#!/usr/bin/env python3
"""Convert a Hamlib debug log (rigctl -Z -vvvvv) into a CAT trace.

Hamlib's TX dumps become host data (H1), its RX dumps radio data (R). Time
stamps are made relative to the first converted line. Bytes Hamlib never read
(flushed or still in the buffer when it timed out) are not in the log.

  hamlib2trace.py [--lines FIRST:LAST] [--time FROM:TO] [--addr 94] rigctl.log > out.trace

--lines restricts the conversion to a range of log lines (1-based, inclusive).
--time keeps only bytes between FROM and TO seconds (relative to the first
converted line; time stamps keep that origin).
--addr writes a '@civ addr=..' header line.
"""
import argparse
import os
import re
import sys

TS = re.compile(r'^\d{4}-\d{2}-\d{2}T(\d{2}):(\d{2}):(\d{2}\.\d+)[+-]\d{4}: ')
HDR = re.compile(r'(write_block\(\): TX|read_string_generic\(\): RX) (\d+) (bytes|characters)')
HEX = re.compile(r'^[0-9a-f]{4}\s+((?:[0-9a-f]{2} )+)')


def seconds(m):
    return int(m.group(1)) * 3600 + int(m.group(2)) * 60 + float(m.group(3))


def convert(path, first, last, t_from, t_to, out):
    t0 = None
    t_prev = 0.0
    replay = False
    cur = None          # [tag, t, expected, bytes]

    def flush():
        nonlocal t0, t_prev
        tag, t, _, data = cur
        if t0 is None:
            t0 = t
        # Hamlib -Z stamps can be up to 1 s early (seconds from time(), fraction
        # from gettimeofday()), keep the trace monotonic.
        rel = max(t - t0, t_prev)
        t_prev = rel
        if rel < t_from or (t_to and rel > t_to):
            return
        out.write(f'{rel:.3f} {tag} {" ".join(f"{b:02x}" for b in data)}\n')

    with open(path, errors='replace') as f:
        for n, line in enumerate(f, 1):
            if n < first:
                continue
            if last and n > last:
                break
            m = TS.match(line)
            if not m:
                continue
            t = seconds(m)
            line = line[m.end():]

            # rigerror() replays the debug history after a failure: skip it.
            if ': failed: ' in line:
                replay = True
                continue
            if replay:
                if 'icom_transaction returning(-' in line:
                    replay = False
                continue

            if cur:
                h = HEX.match(line)
                if h:
                    cur[3] += bytes.fromhex(h.group(1).replace(' ', ''))
                    if len(cur[3]) >= cur[2]:
                        flush()
                        cur = None
                    continue
                flush()     # dump shorter than announced
                cur = None

            h = HDR.search(line)
            if h:
                cur = ['H1' if 'TX' in h.group(1) else 'R', t, int(h.group(2)), b'']
            elif 'Timed out' in line and 'read_string_generic' in line and t0 is not None:
                if t_prev >= t_from and (not t_to or t_prev <= t_to):
                    out.write('# Hamlib: read timed out\n')
    if cur and cur[3]:
        flush()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--lines', default='1:0', help='FIRST:LAST log lines (LAST 0 = end)')
    ap.add_argument('--time', default='0:0', help='FROM:TO seconds (TO 0 = end)')
    ap.add_argument('--addr', help='CI-V address of the radio (hex)')
    ap.add_argument('log')
    args = ap.parse_args()
    first, last = (int(x or 0) for x in args.lines.split(':'))
    t_from, t_to = (float(x or 0) for x in args.time.split(':'))

    out = sys.stdout
    out.write(f'# converted from {os.path.basename(args.log)}, log lines {args.lines}, '
              f'seconds {args.time}\n')
    if args.addr:
        out.write(f'@civ addr={args.addr}\n')
    convert(args.log, max(first, 1), last, t_from, t_to, out)


if __name__ == '__main__':
    main()
