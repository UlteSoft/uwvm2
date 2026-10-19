#!/usr/bin/env python3
"""Retain pidfds for an explicitly spawned, new-session subprocess and its helpers.

Call remember_process(child) immediately after Popen(start_new_session=True),
before poll/wait can reap a short-lived leader, then during every wait. Discovery
is throttled to 0.5 s; simultaneous owners share a 0.25 s /proc snapshot cache.
Membership requires a live authenticated session/group anchor, a live retained
parent, or the still-unreaped direct leader of that same new session/group.
Every candidate is rechecked after pidfd_open; cleanup signals only these
retained descriptors, including when Popen has already reaped the leader.

This is sampled ownership tracking, not a process-creation monitor. A helper
which forks, detaches and loses every observable owned ancestor/group anchor
before first discovery or between scans can be missed. Never compensate by
signalling an unanchored numeric PID/PGID.
Keep polling through collector stop/finalization/report generation. Weak finalizers
close forgotten descriptors but deliberately never signal processes.
Failed retirement retains its authenticated descriptors for an explicit retry
by the same live caller. Neither a cleanup exception nor descriptor closure is
proof of physical retirement; callers must retain failure evidence and stop.
"""
import os
from pathlib import Path
import select
import signal
import subprocess
import time
import weakref


_SCAN_INTERVAL = .5
_CACHE_INTERVAL = .25
_snapshot_at = float('-inf')
_snapshot = {}


def _stat(pid):
    try:
        text = Path(f'/proc/{pid}/stat').read_text(errors='surrogateescape')
    except (FileNotFoundError, ProcessLookupError, PermissionError):
        return None
    # comm is parenthesized and may contain spaces or closing parentheses.
    fields = text.rpartition(')')[2].split()
    return {'pid': pid, 'state': fields[0], 'ppid': int(fields[1]),
            'pgrp': int(fields[2]), 'session': int(fields[3]), 'start': int(fields[19])}


def _same(first, second):
    return second is not None and (first['pid'], first['start']) == (second['pid'], second['start'])


def _alive(fd):
    poller = select.poll()
    poller.register(fd, select.POLLIN | select.POLLHUP | select.POLLERR)
    events = poller.poll(0)
    if any(mask & select.POLLNVAL for _, mask in events):
        raise RuntimeError('retained ownership pidfd was closed unexpectedly')
    return not events


def _processes(force=False):
    global _snapshot_at, _snapshot
    now = time.monotonic()
    if force or now - _snapshot_at >= _CACHE_INTERVAL:
        result = {}
        for path in Path('/proc').iterdir():
            if path.name.isdigit():
                value = _stat(int(path.name))
                if value is not None:
                    result[value['pid']] = value
        _snapshot, _snapshot_at = result, now
    return _snapshot


class _Owner:
    def __init__(self, child):
        assert os.uname().sysname == 'Linux' and hasattr(os, 'pidfd_open')
        assert hasattr(signal, 'pidfd_send_signal'), 'pidfd signalling is required'
        assert child.returncode is None, 'register ownership before Popen poll/wait reaps the leader'
        self.child = weakref.ref(child)
        self.pid = child.pid
        self.members = {}
        self.closed = False
        self.last_scan = float('-inf')
        self.scans = 0
        self.signals = []
        self.retirement_errors = []
        first = _stat(self.pid)
        assert first and first['ppid'] == os.getpid(), 'leader is not our direct child'
        assert first['pgrp'] == self.pid and first['session'] == self.pid, 'new session/group required'
        member = self._capture(first, 'leader', lambda current: current['ppid'] == os.getpid()
                               and current['pgrp'] == self.pid and current['session'] == self.pid,
                               initial_leader=True)
        assert member is not None, 'leader exited/changed identity before initial pidfd authentication'
        self.leader_start = member['stat']['start']
        self.initial_leader_already_ended = member['registered_ended']

    def _leader_zombie(self, first, fd):
        """An unreaped direct child pins this exact PID/session identity.

        An ended pidfd is never a signal target or a live ancestry anchor.
        Its original new-session leader may authenticate only that same group
        while /proc still proves Z, our parentage and its unreused start time.
        A Popen poll/wait, changed identity or missing /proc revokes the anchor.
        """
        child = self.child()
        if child is None or child.returncode is not None or first['pid'] != self.pid:
            return None
        current = _stat(self.pid)
        if (not _same(first, current) or current['state'] != 'Z'
                or current['ppid'] != os.getpid()
                or current['pgrp'] != self.pid or current['session'] != self.pid or _alive(fd)):
            return None
        final = _stat(self.pid)
        if (not _same(current, final) or final['state'] != 'Z'
                or final['ppid'] != os.getpid()
                or final['pgrp'] != self.pid or final['session'] != self.pid
                or child.returncode is not None or _alive(fd)):
            return None
        return final

    def _unreaped_leader_anchor(self):
        member = self.members.get((self.pid, self.leader_start))
        return (member is not None and member['fd'] is not None
                and self._leader_zombie(member['stat'], member['fd']) is not None)

    def _capture(self, first, origin, permitted, initial_leader=False):
        fd = None
        try:
            fd = os.pidfd_open(first['pid'])
            current = _stat(first['pid'])
            if not _same(first, current) or not permitted(current):
                return None
            registered_ended = not _alive(fd)
            if registered_ended:
                if not initial_leader or origin != 'leader':
                    return None
                current = self._leader_zombie(first, fd)
                if current is None or not permitted(current):
                    return None
            member = {'fd': fd, 'stat': current, 'origin': origin,
                      'registered_ended': registered_ended}
            self.members[(current['pid'], current['start'])] = member
            fd = None
            return member
        except (FileNotFoundError, ProcessLookupError):
            return None
        finally:
            # Includes the race where the second /proc lookup disappeared.
            if fd is not None:
                os.close(fd)

    def _current(self, member):
        if not _alive(member['fd']):
            return None
        current = _stat(member['stat']['pid'])
        if not _same(member['stat'], current) or not _alive(member['fd']):
            return None
        return current

    def _group_anchor(self):
        for member in self.members.values():
            current = self._current(member)
            if current and current['pgrp'] == self.pid and current['session'] == self.pid:
                return True
        return False

    def observe(self, force=False):
        if self.closed or (not force and time.monotonic() - self.last_scan < _SCAN_INTERVAL):
            return
        self.last_scan = time.monotonic()
        self.scans += 1
        trusted = {member['stat']['pid']: member for member in self.members.values()
                   if self._current(member) is not None}
        if not trusted and not self._unreaped_leader_anchor():
            return
        pending = dict(_processes(force))
        changed = True
        while changed:
            changed = False
            for pid, first in list(pending.items()):
                if (pid, first['start']) in self.members:
                    pending.pop(pid)
                    continue
                parent = trusted.get(first['ppid'])
                grouped = first['pgrp'] == self.pid and first['session'] == self.pid
                if parent is not None:
                    def permitted(current, parent=parent):
                        return current['ppid'] == parent['stat']['pid'] and self._current(parent) is not None
                    origin = 'descendant'
                elif grouped and (self._group_anchor() or self._unreaped_leader_anchor()):
                    def permitted(current):
                        return (current['pgrp'] == self.pid and current['session'] == self.pid
                                and (self._group_anchor() or self._unreaped_leader_anchor()))
                    origin = 'session-group' if self._group_anchor() else 'unreaped-leader-session-group'
                else:
                    continue
                member = self._capture(first, origin, permitted)
                pending.pop(pid)
                if member is not None:
                    trusted[pid] = member
                    changed = True

    def send(self, number, sent):
        for member in self.members.values():
            fd = member['fd']
            if fd in sent or not _alive(fd):
                continue
            try:
                signal.pidfd_send_signal(fd, number)
                self.signals.append({'pid': member['stat']['pid'], 'start': member['stat']['start'],
                                     'signal': int(number)})
                sent.add(fd)
            except ProcessLookupError:
                pass

    def living(self):
        return [member for member in self.members.values() if _alive(member['fd'])]

    def record(self):
        return {'leader_pid': self.pid, 'leader_start': self.leader_start, 'closed': self.closed,
                'initial_leader_already_ended': self.initial_leader_already_ended,
                'discovery_scans': self.scans,
                'members': [{'identity': dict(member['stat']), 'origin': member['origin'],
                             'pidfd_retained': member['fd'] is not None}
                            for member in self.members.values()], 'signals': [dict(event) for event in self.signals],
                'retirement_errors': [dict(error) for error in self.retirement_errors],
                'limitation': 'helpers detaching and losing every observable owned ancestry/group anchor before first discovery or between scans may be missed'}

    def close(self):
        if not self.closed:
            self.closed = True
            for member in self.members.values():
                os.close(member['fd'])
                member['fd'] = None


def remember_process(child, force=False):
    """Authenticate at spawn, then retain sampled helper pidfds without reaping."""
    if child is None:
        return None
    owner = getattr(child, '_uwvm_owned_process', None)
    if owner is None:
        owner = _Owner(child)
        child._uwvm_owned_process = owner
        weakref.finalize(child, owner.close)
        force = True
    owner.observe(force)
    return owner.record()


def owned_process_record(child):
    owner = getattr(child, '_uwvm_owned_process', None)
    return owner.record() if owner is not None else None


def wait_process(child, timeout, observe=None):
    """Bounded polling wait; observe may also remember other active owners."""
    deadline = time.monotonic() + timeout
    while True:
        remember_process(child)
        if observe is not None:
            observe()
        code = child.poll()
        if code is not None:
            return code
        if time.monotonic() >= deadline:
            raise subprocess.TimeoutExpired(child.args, timeout)
        time.sleep(.05)


def terminate_group(child, term_timeout=10, kill_timeout=5):
    """Drain authenticated pidfds even after leader reap; never kill a raw PGID."""
    if child is None:
        return
    remember_process(child, force=True)
    owner = child._uwvm_owned_process
    if owner.closed:
        return
    try:
        for number, timeout in [(signal.SIGTERM, term_timeout), (signal.SIGKILL, kill_timeout)]:
            sent = set()
            deadline = time.monotonic() + timeout
            while True:
                owner.observe()
                owner.send(number, sent)
                if not owner.living():
                    break
                if time.monotonic() >= deadline:
                    break
                time.sleep(.05)
            # Discover remaining group/descendant members while a retained live
            # anchor still exists, never after trusting only a recycled number.
            owner.observe(force=True)
            owner.send(number, sent)
            if not owner.living():
                break
        if owner.living():
            raise RuntimeError('owned pidfd processes remain after bounded SIGKILL drain')
        child.wait(timeout=1)
    except BaseException as error:
        # Keep these exact pidfds usable by a same-caller cleanup retry. Never
        # manufacture successful retirement by closing still-live ownership.
        owner.retirement_errors.append({'type': type(error).__name__, 'message': str(error)})
        raise
    else:
        owner.close()
