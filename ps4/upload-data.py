#!/usr/bin/env python3
"""Mirror SuperTuxKart's data trees to /data/stk/data on the console (resumable).

The port reads its assets from /data/stk/data so that a code iteration only installs a small
eboot-only package. This walks both local trees, compares every file with the remote size, and
uploads only what is missing or different.

The FTP connection has a socket timeout, retries with reconnect, and a wall-clock cap, so a stalled
data connection ends the run instead of hanging forever. Re-run it to continue.

    python3 ps4/upload-data.py                 # resume/complete the mirror
    python3 ps4/upload-data.py --dry-run       # list what would be sent
    python3 ps4/upload-data.py --host 192.168.0.3 --port 2121
"""

import argparse
import ftplib
import os
import sys
import time

STK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = [
    os.path.join(STK, "data"),
    os.path.join(os.path.dirname(STK), "stk-assets"),
]
REMOTE = "/data/stk/data"


def connect(args):
    ftp = ftplib.FTP(timeout=args.timeout)
    ftp.encoding = "utf-8"
    ftp.connect(args.host, args.port)
    ftp.login(args.user, args.password)
    ftp.voidcmd("TYPE I")
    return ftp


def ensure_dir(ftp, path, made):
    parts = [p for p in path.split("/") if p]
    current = ""
    for part in parts:
        current += "/" + part
        if current in made:
            continue
        try:
            ftp.mkd(current)
        except ftplib.error_perm:
            pass  # already exists; a real failure surfaces at STOR
        made.add(current)


def remote_sizes(ftp, base):
    """Map remote path -> size using MLSD, so resume needs one pass, not one SIZE per file."""
    sizes = {}
    stack = [base]
    while stack:
        current = stack.pop()
        try:
            for name, facts in ftp.mlsd(current):
                if name in (".", ".."):
                    continue
                full = current + "/" + name
                kind = facts.get("type")
                if kind in ("dir", "cdir", "pdir"):
                    stack.append(full)
                elif kind == "file":
                    try:
                        sizes[full] = int(facts.get("size", "-1"))
                    except (TypeError, ValueError):
                        sizes[full] = -1
        except ftplib.error_perm as exc:
            print(f"  cannot list {current}: {exc}", file=sys.stderr)
    return sizes


def local_files():
    """Map remote path -> local file, later sources winning.

    Some relative paths exist in both trees with different content (stk_config.xml is one). The
    package stages data/ first and merges stk-assets/ over it, so the assets tree is the copy the
    game gets; mirror that precedence here, and only upload each remote path once. Without this the
    two copies overwrite each other and the mirror never converges.
    """
    files = {}
    for root in SOURCES:
        if not os.path.isdir(root):
            print(f"skipping missing {root}", file=sys.stderr)
            continue
        for dirpath, _dirnames, names in os.walk(root):
            rel = os.path.relpath(dirpath, root)
            remote_dir = REMOTE if rel == "." else REMOTE + "/" + rel.replace(os.sep, "/")
            for name in names:
                local = os.path.join(dirpath, name)
                files[remote_dir + "/" + name] = (local, os.path.getsize(local))
    return [(local, remote, size) for remote, (local, size) in files.items()]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="192.168.0.3")
    parser.add_argument("--port", type=int, default=2121)
    parser.add_argument("--user", default="anonymous")
    parser.add_argument("--password", default="anonymous")
    parser.add_argument("--timeout", type=float, default=20.0, help="socket timeout, seconds")
    parser.add_argument("--max-seconds", type=float, default=900.0, help="stop cleanly after this")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    local = local_files()
    total = sum(size for _local, _remote, size in local)
    print(f"local: {len(local)} files, {total / 1048576:.0f} MiB")

    ftp = connect(args)
    made = set()
    ensure_dir(ftp, REMOTE, made)
    print("scanning remote ...", flush=True)
    remote = remote_sizes(ftp, REMOTE)
    print(f"remote: {len(remote)} files")

    todo = [(local_path, remote_path, size)
            for local_path, remote_path, size in local
            if remote.get(remote_path) != size]
    print(f"to upload: {len(todo)} files, "
          f"{sum(size for _l, _r, size in todo) / 1048576:.0f} MiB", flush=True)

    if args.dry_run:
        for local_path, remote_path, _size in todo[:25]:
            print(f"  {local_path} -> {remote_path}")
        return 0
    if not todo:
        print("already up to date")
        return 0

    started = time.time()
    sent = 0
    for index, (local_path, remote_path, size) in enumerate(todo, 1):
        if time.time() - started > args.max_seconds:
            print(f"stopped after {args.max_seconds:.0f}s ({index - 1}/{len(todo)} sent); "
                  f"run again to continue", file=sys.stderr)
            return 3
        ensure_dir(ftp, remote_path.rsplit("/", 1)[0], made)
        for attempt in range(3):
            try:
                with open(local_path, "rb") as handle:
                    ftp.storbinary("STOR " + remote_path, handle, blocksize=1 << 20)
                break
            except (ftplib.all_errors, OSError) as exc:
                if attempt == 2:
                    raise
                print(f"  retry {remote_path}: {exc}", file=sys.stderr)
                try:
                    ftp.close()
                except OSError:
                    pass
                ftp = connect(args)
                ensure_dir(ftp, remote_path.rsplit("/", 1)[0], made)
        sent += size
        if index % 200 == 0 or index == len(todo):
            elapsed = max(time.time() - started, 0.001)
            print(f"  {index}/{len(todo)}  {sent / 1048576:.0f} MiB  "
                  f"{sent / 1048576 / elapsed:.1f} MiB/s", flush=True)

    ftp.quit()
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
