#!/usr/bin/env python3
"""Pull SuperTuxKart PS4 logs over the console's FTP and klog sockets.

Usage: python3 ps4/pull-logs.py [host] [ftp_port] [klog_port]

Saves everything under /tmp/stklogs-<utc-stamp>/ and prints the tail of
stdout.log plus the last "STK VK" diagnostic line. No dependencies.
"""

import ftplib
import os
import socket
import sys
import time
from datetime import datetime, timezone

HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.0.3"
FTP_PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 2121
KLOG_PORT = int(sys.argv[3]) if len(sys.argv) > 3 else 3232

REMOTE_FILES = [
    "/data/stk/config-0.10/stdout.log",
    "/data/stk/config-0.10/stdout.log.1",
    "/data/stk/config-0.10/stdout.log.2",
    "/data/stk/config-0.10/stdout.log.3",
    "/data/ps4-kit/crash.log",
    "/data/ps4-kit/hang.log",
    "/data/ps4-kit/boot-trace.log",
    # RADV's own GPU hang report (ac_ps4_drm.c writes it when /data/Crema exists)
    "/data/Crema/gpu/gpu-hang.txt",
    "/data/Crema/gpu/radv-ps4.log",
]

MARKERS = ["/data/Crema", "/data/stk/vk-trace", "/data/stk/profile-on"]


def pull(out_dir):
    ftp = ftplib.FTP()
    ftp.connect(HOST, FTP_PORT, timeout=10)
    ftp.login()
    print("connected to ftp://%s:%d" % (HOST, FTP_PORT))

    for remote in REMOTE_FILES:
        local = os.path.join(out_dir, os.path.basename(remote))
        try:
            with open(local, "wb") as fh:
                ftp.retrbinary("RETR " + remote, fh.write)
            print("  ok   %-48s %8d bytes" % (remote, os.path.getsize(local)))
        except ftplib.error_perm as err:
            print("  skip %-48s %s" % (remote, err))
        except OSError as err:
            print("  FAIL %-48s %s" % (remote, err))

    for marker in MARKERS:
        try:
            size = ftp.size(marker)
            print("  marker %-46s present (%s)" % (marker, size))
        except ftplib.error_perm:
            pass
        except OSError:
            pass

    try:
        ftp.quit()
    except OSError:
        pass


def klog(out_dir, seconds=4.0):
    dest = os.path.join(out_dir, "klog.txt")
    try:
        sock = socket.create_connection((HOST, KLOG_PORT), timeout=5)
    except OSError as err:
        print("klog unavailable: %s" % err)
        return
    sock.settimeout(0.5)
    deadline = time.time() + seconds
    chunks = []
    while time.time() < deadline:
        try:
            data = sock.recv(65536)
        except socket.timeout:
            continue
        except OSError:
            break
        if not data:
            break
        chunks.append(data)
    sock.close()
    with open(dest, "wb") as fh:
        fh.write(b"".join(chunks))
    print("  klog %d bytes" % sum(len(c) for c in chunks))


def summarize(out_dir):
    stdout = os.path.join(out_dir, "stdout.log")
    if not os.path.exists(stdout):
        print("no stdout.log retrieved")
        return
    with open(stdout, "r", errors="replace") as fh:
        lines = fh.readlines()
    print("\n--- last 40 stdout.log lines ---")
    for line in lines[-40:]:
        print(line.rstrip())
    trace = os.path.join(out_dir, "boot-trace.log")
    if os.path.exists(trace):
        with open(trace, "r", errors="replace") as fh:
            tlines = fh.readlines()
        print("\n--- last 8 boot-trace.log lines (flips carry timestamps) ---")
        for line in tlines[-8:]:
            print(line.rstrip())
    hits = [line.rstrip() for line in lines if "STK VK" in line]
    print("\n--- last 15 STK VK lines ---")
    for line in hits[-15:]:
        print(line)


def main():
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    out_dir = "/tmp/stklogs-" + stamp
    os.makedirs(out_dir, exist_ok=True)
    print("saving to " + out_dir)
    pull(out_dir)
    klog(out_dir)
    summarize(out_dir)
    print("\nlogs in " + out_dir)


if __name__ == "__main__":
    main()
