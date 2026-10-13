#!/usr/bin/env python3
"""Run a macOS test with a 4 GiB aggregate resident-memory watchdog."""

import argparse
import os
import signal
import subprocess
import sys
import time


def process_tree_rss(root):
    rows = subprocess.run(
        ["ps", "-A", "-o", "pid=,ppid=,rss="],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    children = {}
    sizes = {}
    for line in rows.splitlines():
        fields = line.split()
        if len(fields) != 3:
            continue
        pid, ppid, rss_kib = map(int, fields)
        children.setdefault(ppid, []).append(pid)
        sizes[pid] = rss_kib * 1024
    stack = [root]
    pids = []
    total = 0
    while stack:
        pid = stack.pop()
        pids.append(pid)
        total += sizes.get(pid, 0)
        stack.extend(children.get(pid, ()))
    return total, pids


def kill_process_tree(root, pids):
    # A signed macOS child may make killpg return EPERM even though the
    # ordinary compiler processes in the same group remain killable.
    try:
        os.killpg(root, signal.SIGKILL)
    except (PermissionError, ProcessLookupError):
        pass
    denied = []
    for pid in reversed(pids):
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        except PermissionError:
            denied.append(pid)
    return denied


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--limit-bytes", type=int, default=4 * 1024 ** 3)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("missing command")
    proc = subprocess.Popen(command, start_new_session=True)
    peak = 0
    exceeded = False
    kill_denied = []
    usage = None
    status = None
    while status is None:
        current, pids = process_tree_rss(proc.pid)
        peak = max(peak, current)
        if peak > args.limit_bytes and not exceeded:
            exceeded = True
            print(f"RSS_LIMIT_EXCEEDED_AT_BYTES={peak}", file=sys.stderr, flush=True)
            kill_denied = kill_process_tree(proc.pid, pids)
        waited, raw_status, observed_usage = os.wait4(proc.pid, os.WNOHANG)
        if waited:
            status = os.waitstatus_to_exitcode(raw_status)
            usage = observed_usage
            break
        time.sleep(0.02)
    proc.returncode = status
    # Darwin reports ru_maxrss in bytes. Sampling also covers compiler/linker
    # descendants; wait4 captures short-lived direct test programs accurately.
    peak = max(peak, usage.ru_maxrss)
    print(f"MACOS_RSS_LIMIT_BYTES={args.limit_bytes}")
    print(f"PEAK_PROCESS_TREE_RSS_BYTES={peak}")
    print(f"COMMAND_EXIT={status}")
    if kill_denied:
        print(f"RSS_KILL_DENIED_PIDS={','.join(map(str, kill_denied))}", file=sys.stderr)
    if exceeded:
        print("RSS_LIMIT_EXCEEDED", file=sys.stderr)
        return 1
    return status


if __name__ == "__main__":
    sys.exit(main())
