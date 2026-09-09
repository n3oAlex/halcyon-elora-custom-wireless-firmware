#!/usr/bin/env python3
"""Capture ZMK USB debug logs from a Halcyon half or the dongle into logs/<part>-<time>.log.

Needs a debug_* firmware on the part. No dependencies beyond Python 3 (stdlib only), macOS/Linux.

    python3 tools/capture-log.py left          # guided: unplug, press Enter, plug in, log
    python3 tools/capture-log.py right --device /dev/cu.usbmodem101
    python3 tools/capture-log.py --list

Stop with Ctrl-C. The file is kept, and a summary of interesting lines is printed.
"""
import argparse
import datetime
import glob
import os
import re
import select
import signal
import sys
import termios
import time

PATTERNS = ["/dev/cu.usbmodem*", "/dev/ttyACM*"]
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
INTERESTING = {
    "errors/warnings": re.compile(r"<err>|<wrn>|ERR|WRN|Unable|fail", re.I),
    "encoder (EC11)": re.compile(r"ec11", re.I),
    "touchpad (pinnacle)": re.compile(r"pinnacle", re.I),
    "split link": re.compile(r"split", re.I),
    "sensor events": re.compile(r"sensor", re.I),
    "input events": re.compile(r"input", re.I),
    "keys (kscan/position)": re.compile(r"kscan|position", re.I),
}


def candidates():
    found = []
    for pat in PATTERNS:
        found += glob.glob(pat)
    return sorted(found)


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0                                        # iflag: raw
    attrs[1] = 0                                        # oflag: raw
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0                                        # lflag: no echo/canonical
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    try:  # ZMK only emits log lines once the host asserts DTR
        import fcntl
        import struct
        fcntl.ioctl(fd, termios.TIOCMBIS, struct.pack("I", termios.TIOCM_DTR))
    except (AttributeError, OSError):
        pass
    return fd


def wait_for_new_device(before, timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        new = [c for c in candidates() if c not in before]
        if new:
            time.sleep(1.0)  # let the CDC port settle
            return new[0]
        time.sleep(0.3)
    return None


def pick_device(part):
    print(f"Unplug the {part} from USB if it is plugged in, then press Enter.")
    input()
    before = candidates()
    print(f"Now plug the {part} in. Waiting for a new serial device (60 s max)...")
    dev = wait_for_new_device(before)
    if not dev:
        sys.exit("No new serial device appeared. Is a debug_* firmware flashed? Try --list.")
    print(f"Found {dev}")
    return dev


def main():
    sys.stdout.reconfigure(line_buffering=True)
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("part", nargs="?", choices=["left", "right", "dongle"], help="which part is on USB")
    ap.add_argument("--device", help="serial device path, skips the unplug/plug detection")
    ap.add_argument("--list", action="store_true", help="list serial devices and exit")
    ap.add_argument("--seconds", type=int, default=0, help="stop automatically after N seconds")
    args = ap.parse_args()

    if args.list:
        devs = candidates()
        print("\n".join(devs) if devs else "no serial devices found")
        return
    if not args.part:
        ap.error("part is required (left, right or dongle)")

    dev = args.device or pick_device(args.part)
    os.makedirs("logs", exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    out_path = os.path.join("logs", f"{args.part}-{stamp}.log")
    out = open(out_path, "w", buffering=1)

    print(f"\nLogging {dev} -> {out_path}")
    print("Suggested sequence, pausing a few seconds between steps:")
    print("  1. press the reset button on the part once (captures the boot lines)")
    print("  2. wait ~10 s for it to reconnect to the dongle")
    print("  3. type a few keys on this half")
    print("  4. left: turn the encoder 5 clicks each way, then press it")
    print("     right: tap the touchpad, then swipe around, then tap again")
    print("  5. Ctrl-C to stop\n")

    # Ctrl-C and SIGTERM both end the capture cleanly and print the summary.
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt))

    counts = {k: 0 for k in INTERESTING}
    samples = {k: [] for k in INTERESTING}
    fd = None
    buf = b""
    lines = 0
    known_ports = [c for c in candidates() if c != dev]
    deadline = time.time() + args.seconds if args.seconds else None
    try:
        while True:
            if deadline and time.time() > deadline:
                raise KeyboardInterrupt
            if fd is None:
                try:
                    fd = open_port(dev)
                    note = f"--- connected to {dev} ---"
                    print(note)
                    out.write(note + "\n")
                except OSError:
                    # After a reset the part may come back under a new name. Only adopt a
                    # device that was NOT present before it went away; never grab another
                    # port that was already there (that would be the dongle).
                    time.sleep(0.5)
                    if dev not in candidates():
                        new = [c for c in candidates() if c not in known_ports]
                        if len(new) == 1:
                            dev = new[0]
                    if args.seconds and time.time() > deadline:
                        raise KeyboardInterrupt
                    continue
            try:
                ready, _, _ = select.select([fd], [], [], 0.5)
                if not ready:
                    continue
                chunk = os.read(fd, 4096)
                if not chunk:
                    raise OSError("EOF")
            except OSError:
                note = "--- device went away (reset?), waiting for it to come back ---"
                print(note)
                out.write(note + "\n")
                try:
                    os.close(fd)
                except OSError:
                    pass
                fd = None
                buf = b""
                time.sleep(1.0)
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                text = ANSI.sub("", raw.decode("utf-8", "replace")).rstrip("\r")
                if not text.strip():
                    continue
                stamped = f"{datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]} {text}"
                print(stamped)
                out.write(stamped + "\n")
                lines += 1
                for key, rx in INTERESTING.items():
                    if rx.search(text):
                        counts[key] += 1
                        if len(samples[key]) < 5:
                            samples[key].append(text)
    except KeyboardInterrupt:
        pass
    finally:
        if fd is not None:
            try:
                os.close(fd)
            except OSError:
                pass
        out.close()

    print(f"\nSaved {lines} lines to {out_path}")
    print("Summary:")
    for key in INTERESTING:
        print(f"  {key:24s} {counts[key]}")
        for s in samples[key][:3]:
            print(f"      {s[:110]}")
    if lines == 0:
        print("\nNothing arrived. Checks: is this the debug_* image? did you press reset after "
              "flashing? is the part actually on this port (--list)?")


if __name__ == "__main__":
    main()
