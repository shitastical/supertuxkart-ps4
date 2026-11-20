#!/usr/bin/env python3
"""Replace console files without the package installer.

An installed fake package mounts /user/app/<TITLE_ID>/app.pkg. Replacing that
single file over FTP does not work: the console rejects the swapped image and
the app reports "data is corrupted". The iteration loop is uninstall + install
of an eboot-only package (~60 MB instead of ~1.16 GB); game data is already
file-level under /data/stk/data.

    python3 ps4/push.py                  # newest out/*.pkg -> /data/pkg/<name>.pkg
    python3 ps4/push.py --installed      # disabled: the console rejects a swapped app.pkg
    python3 ps4/push.py --restore        # put out/app.pkg.backup back over FTP

Close the game first: a running app keeps the old image mounted until relaunch.
Exit status is nonzero on a failed transfer or a size mismatch; nothing loops.
"""

import argparse
import ftplib
import hashlib
import os
import sys
import time

TITLE_ID = "STKR00001"
APP_DIR = f"/user/app/{TITLE_ID}"
APP_PKG = f"{APP_DIR}/app.pkg"
PKG_DIR = "/data/pkg"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "out")
BACKUP = os.path.join(OUT, "app.pkg.backup")
CHUNK = 1 << 20


def log(msg):
    print(msg, flush=True)


def connect(host, timeout):
    ftp = ftplib.FTP(timeout=timeout)
    ftp.connect(host, 2121)
    ftp.login("anonymous", "anonymous")
    ftp.voidcmd("TYPE I")
    return ftp


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for block in iter(lambda: fh.read(CHUNK), b""):
            h.update(block)
    return h.hexdigest()


def upload(ftp, local, remote):
    size = os.path.getsize(local)
    sent = 0
    started = time.time()

    def progress(block):
        nonlocal sent
        sent += len(block)
        if sent % (32 * CHUNK) < len(block) or sent == size:
            rate = sent / 1048576 / max(time.time() - started, 1e-6)
            log(f"  {sent / 1048576:7.1f} / {size / 1048576:.1f} MiB  {rate:.1f} MiB/s")

    with open(local, "rb") as fh:
        ftp.storbinary(f"STOR {remote}", fh, blocksize=CHUNK, callback=progress)
    remote_size = ftp.size(remote)
    if remote_size != size:
        raise RuntimeError(f"{remote}: remote size {remote_size:,} != local {size:,}")
    return remote_size


def download(ftp, remote, local):
    size = ftp.size(remote)
    got = 0
    started = time.time()
    tmp = local + ".part"

    def progress(block):
        nonlocal got
        got += len(block)
        if got % (64 * CHUNK) < len(block) or got == size:
            rate = got / 1048576 / max(time.time() - started, 1e-6)
            log(f"  {got / 1048576:7.1f} / {size / 1048576:.1f} MiB  {rate:.1f} MiB/s")

    with open(tmp, "wb") as fh:
        ftp.retrbinary(f"RETR {remote}", fh.write, blocksize=CHUNK)
    if os.path.getsize(tmp) != size:
        os.unlink(tmp)
        raise RuntimeError(f"{remote}: downloaded size mismatch")
    os.replace(tmp, local)
    return size


def newest_pkg():
    if not os.path.isdir(OUT):
        return None
    pkgs = [os.path.join(OUT, n) for n in os.listdir(OUT) if n.endswith(".pkg")]
    return max(pkgs, key=os.path.getmtime) if pkgs else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default=os.environ.get("PS4_HOST", "192.168.0.3"))
    ap.add_argument("--pkg", help="package to push (default: newest out/*.pkg)")
    ap.add_argument("--installed", action="store_true",
                    help="disabled: replacing app.pkg trips the console's image validation")
    ap.add_argument("--restore", action="store_true",
                    help="upload out/app.pkg.backup back to the console")
    ap.add_argument("--timeout", type=int, default=30, help="FTP socket timeout (seconds)")
    args = ap.parse_args()

    if args.installed:
        sys.exit("refusing --installed: the console validates the installed image and the app "
                 "fails with 'data is corrupted'. Use uninstall + install, or a new TITLE_ID.")

    if args.restore:
        if not os.path.isfile(BACKUP):
            sys.exit(f"no backup at {BACKUP}")
        log(f"restoring {BACKUP} ({os.path.getsize(BACKUP):,} bytes) -> {APP_PKG}")
        ftp = connect(args.host, args.timeout)
        try:
            upload(ftp, BACKUP, APP_PKG)
        finally:
            ftp.quit()
        log("restored; launch the app to confirm the original build runs again")
        return 0

    pkg = args.pkg or newest_pkg()
    if not pkg or not os.path.isfile(pkg):
        sys.exit("no package: build one with ps4/deploy.sh or pass --pkg")
    size = os.path.getsize(pkg)
    log(f"package: {pkg} ({size:,} bytes)")
    log(f"sha256:  {sha256(pkg)}")

    ftp = connect(args.host, args.timeout)
    try:
        remote_piece = f"{PKG_DIR}/{os.path.basename(pkg)}"
        log(f"-> {remote_piece}")
        upload(ftp, pkg, remote_piece)

        if args.installed:
            if not os.path.isfile(BACKUP):
                log(f"no backup yet: saving {APP_PKG} to {BACKUP} (one time)")
                os.makedirs(OUT, exist_ok=True)
                download(ftp, APP_PKG, BACKUP)
                log(f"  backup: {os.path.getsize(BACKUP):,} bytes, sha256 {sha256(BACKUP)}")
            log(f"-> {APP_PKG}")
            upload(ftp, pkg, APP_PKG)
    finally:
        ftp.quit()

    if args.installed:
        log("")
        log("replaced the installed image; launch the app from the XMB.")
        log("if it does not start: python3 ps4/push.py --restore")
    return 0


if __name__ == "__main__":
    sys.exit(main())
