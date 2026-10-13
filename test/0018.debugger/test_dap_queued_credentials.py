#!/usr/bin/env python3
"""Exercise authentication in the accept-to-PASSCRED scheduling window."""

import argparse
import importlib.util
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("queued_credentials_server", ROOT / "tools/debug/secure_server.py")
server = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(server)


@unittest.skipUnless(sys.platform == "linux" and hasattr(os, "pidfd_open"), "requires Linux credentials and pidfds")
class QueuedCredentialsTest(unittest.TestCase):
    def test_capability_queued_before_per_client_passcred_is_authenticated(self):
        ready = threading.Event()
        accepted = threading.Event()
        release = threading.Event()
        original_socket = socket.socket
        original_popen = subprocess.Popen
        children = []
        clients = []
        outcomes = []
        errors = []

        class CredentialWindowSocket(original_socket):
            def listen(self, backlog):
                super().listen(backlog)
                ready.set()

            def accept(self):
                result = super().accept()
                accepted.set()
                if not release.wait(10):
                    raise RuntimeError("queued-client test was not released")
                return result

        def launch(argv, **kwargs):
            descriptor = argv[argv.index("--debug-jit-control-fd") + 1]
            code = ("import socket,sys; s=socket.socket(fileno=int(sys.argv[1])); "
                    "assert s.recv(8448)==b'status'; assert s.send(b'running\\n')==8; s.close()")
            child = original_popen([sys.executable, "-c", code, descriptor], **kwargs)
            children.append(child)
            return child

        with tempfile.TemporaryDirectory(prefix="uwvm-queued-", dir="/tmp") as raw:
            directory = Path(raw)
            arguments = argparse.Namespace(socket_dir=directory, uwvm=Path(sys.executable), vm_args=["transport-test"])

            def run_server():
                try:
                    outcomes.append(server.serve(arguments))
                except BaseException as error:
                    errors.append(error)

            with patch.object(server.socket, "socket", CredentialWindowSocket), patch.object(server.subprocess, "Popen", launch):
                worker = threading.Thread(target=run_server, name="queued-credentials-server")
                worker.start()
                try:
                    self.assertTrue(ready.wait(10))
                    capability = (directory / server.CAPABILITY_NAME).read_bytes()
                    self.assertEqual(len(capability), server.CAPABILITY_BYTES)
                    client_code = ("import pathlib,socket,sys; p=pathlib.Path(sys.argv[1]); "
                                   "s=socket.socket(socket.AF_UNIX,socket.SOCK_SEQPACKET); s.settimeout(10); "
                                   "s.connect(str(p/'control.sock')); print('connected',flush=True); "
                                   "assert sys.stdin.readline()=='send\\n'; token=(p/'capability').read_bytes(); "
                                   "assert s.send(token)==32; print('queued',flush=True); "
                                   "assert s.recv(65536)==b'ready\\n'; assert s.send(b'status')==6; "
                                   "assert s.recv(65536)==b'running\\n'; s.close()")
                    client = original_popen([sys.executable, "-c", client_code, str(directory)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    clients.append(client)
                    self.assertEqual(client.stdout.readline(), b"connected\n")
                    self.assertTrue(accepted.wait(10))
                    self.assertEqual(client.stdin.write(b"send\n"), 5)
                    client.stdin.flush()
                    self.assertEqual(client.stdout.readline(), b"queued\n")
                    self.assertFalse(release.is_set())
                    release.set()
                    stdout, stderr = client.communicate(timeout=15)
                    self.assertEqual((client.returncode, stdout, stderr), (0, b"", b""))
                finally:
                    release.set()
                    for child in clients + children:
                        try:
                            child.wait(timeout=2)
                        except subprocess.TimeoutExpired:
                            child.terminate()
                            child.wait(timeout=5)
                    worker.join(15)
                self.assertFalse(worker.is_alive())
                self.assertEqual(errors, [])
                self.assertEqual(outcomes, [0])
                self.assertEqual(len(children), 1)
                self.assertEqual(children[0].wait(timeout=1), 0)
                self.assertFalse((directory / server.CAPABILITY_NAME).exists())
                self.assertFalse((directory / server.SOCKET_NAME).exists())


if __name__ == "__main__":
    unittest.main()
