#!/usr/bin/env python3
"""Offline production-verifier attack tests, with real OpenSSL PKI and COSE signatures.

Execute on the designated Linux cgroup. No network, system clock, trust store,
or cgroup setting is changed. All keys are disposable local test fixtures.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def cbor(value):
    def head(major, number):
        if number < 24:
            return bytes([(major << 5) | number])
        for width, marker in [(1, 24), (2, 25), (4, 26), (8, 27)]:
            if number < 1 << (width * 8):
                return bytes([(major << 5) | marker]) + number.to_bytes(width, 'big')
        raise ValueError('CBOR integer overflow')
    if isinstance(value, int):
        return head(0 if value >= 0 else 1, value if value >= 0 else -1 - value)
    if isinstance(value, bytes):
        return head(2, len(value)) + value
    if isinstance(value, str):
        data = value.encode('utf-8')
        return head(3, len(data)) + data
    if isinstance(value, list):
        return head(4, len(value)) + b''.join(map(cbor, value))
    if isinstance(value, dict):
        encoded = sorted((cbor(k), cbor(v)) for k, v in value.items())
        return head(5, len(encoded)) + b''.join(k + v for k, v in encoded)
    raise TypeError(type(value))


def raw_ecdsa(der):
    offset = 0
    def item(tag):
        nonlocal offset
        if der[offset] != tag:
            raise ValueError('invalid OpenSSL ECDSA encoding')
        offset += 1
        size = der[offset]
        offset += 1
        if size & 128:
            width = size & 127
            size = int.from_bytes(der[offset:offset + width], 'big')
            offset += width
        data = der[offset:offset + size]
        offset += size
        return data
    sequence = item(0x30)
    der = sequence
    offset = 0
    r, s = [int.from_bytes(item(2), 'big') for _ in range(2)]
    order = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
    s = min(s, order - s)
    return r.to_bytes(32, 'big') + s.to_bytes(32, 'big')


class Suite:
    def __init__(self, args):
        self.args = args
        self.out = args.out.resolve()
        self.out.mkdir(mode=0o700, parents=True, exist_ok=False)
        self.rows = []
        self.commands = []
        self.now = int(time.time())

    def command(self, args, *, data=None, allowed=(0,)):
        process = subprocess.run([str(v) for v in args], input=data, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, timeout=60)
        self.commands.append({'argv': [str(v) for v in args], 'returncode': process.returncode,
                              'stdout': process.stdout.decode(errors='replace'),
                              'stderr': process.stderr.decode(errors='replace')})
        if process.returncode not in allowed:
            raise RuntimeError(f'command failed ({process.returncode}): {args}: {process.stderr.decode(errors="replace")}')
        return process.stdout

    def openssl(self, *args, data=None):
        return self.command(['openssl', *args], data=data)

    def pki(self, name, *, eku='codeSigning', curve='prime256v1', key_usage='digitalSignature', ca_constraint='false'):
        directory = self.out / name
        directory.mkdir(mode=0o700)
        (directory / 'newcerts').mkdir()
        (directory / 'index').write_text('')
        (directory / 'serial').write_text('1000\n')
        (directory / 'crlnumber').write_text('1000\n')
        config = directory / 'ca.cnf'
        config.write_text(f'''[ca]
default_ca=local
[local]
dir={directory}
database=$dir/index
new_certs_dir=$dir/newcerts
certificate=$dir/root.pem
private_key=$dir/root.key
serial=$dir/serial
crlnumber=$dir/crlnumber
default_md=sha256
default_days=2
default_crl_days=1
policy=names
unique_subject=no
x509_extensions=signer
[names]
commonName=supplied
[signer]
basicConstraints=critical,CA:{ca_constraint}
keyUsage=critical,{key_usage}
extendedKeyUsage={eku}
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
''')
        self.openssl('genpkey', '-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:prime256v1', '-out', directory / 'root.key')
        self.openssl('req', '-new', '-x509', '-key', directory / 'root.key', '-out', directory / 'root.pem',
                     '-days', '3', '-subj', f'/CN={name} Root', '-addext', 'basicConstraints=critical,CA:TRUE',
                     '-addext', 'keyUsage=critical,keyCertSign,cRLSign')
        self.openssl('genpkey', '-algorithm', 'EC', '-pkeyopt', f'ec_paramgen_curve:{curve}', '-out', directory / 'signer.key')
        self.openssl('req', '-new', '-key', directory / 'signer.key', '-out', directory / 'signer.csr', '-subj', f'/CN={name} Release')
        self.openssl('ca', '-batch', '-notext', '-config', config, '-in', directory / 'signer.csr', '-out', directory / 'signer.pem')
        self.openssl('ca', '-batch', '-config', config, '-gencrl', '-out', directory / 'fresh.crl.pem')
        pubkey = self.openssl('x509', '-in', directory / 'signer.pem', '-pubkey', '-noout')
        spki = self.openssl('pkey', '-pubin', '-outform', 'DER', data=pubkey)
        return {'directory': directory, 'spki': hashlib.sha256(spki).hexdigest()}

    def policy(self, name, ca, *, signer=None, version=1, minimum=1, epoch=0, caps=0, max_age=86400):
        directory = self.out / name
        directory.mkdir(mode=0o700)
        state = directory / 'state'
        state.mkdir(mode=0o700)
        state_file = state / 'state'
        state_file.write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
        state_file.chmod(0o600)
        policy = directory / 'policy'
        policy.write_text(f'''UWVM-WSEA-POLICY-1
policy_version={version}
trust_root={ca['directory'] / 'root.pem'}
state_dir={state}
max_crl_age_seconds={max_age}
max_module_bytes=1048576
module=example.app|uwvm2|{signer or ca['spki']}|{minimum}|{epoch}|{caps}
''')
        policy.chmod(0o600)
        return policy

    def module(self, name, ca, *, module_id='example.app', version=1, epoch=0, caps=0, algorithm=-7,
               not_before=None, not_after=None, payload=None, signature_mutation=False, content=None):
        path = self.out / f'{name}.wasm'
        # Minimal valid module exports a no-op _start function.
        path.write_bytes(bytes.fromhex('0061736d0100000001040160000003020100070a01065f737461727400000a040102000b')
                         if content is None else content)
        content = path.read_bytes()
        if payload is None:
            payload = cbor(['uwvm-wsea-v1', module_id, version, epoch, len(content), hashlib.sha256(content).digest(),
                            self.now - 60 if not_before is None else not_before,
                            self.now + 3600 if not_after is None else not_after, 'uwvm2', caps])
        protected = cbor({1: algorithm})
        to_sign = cbor(['Signature1', protected, b'', payload])
        signature = raw_ecdsa(self.openssl('dgst', '-sha256', '-sign', ca['directory'] / 'signer.key', data=to_sign))
        if signature_mutation:
            signature = bytes([signature[0] ^ 1]) + signature[1:]
        Path(str(path) + '.sig.cose').write_bytes(b'\xd2' + cbor([protected, {}, payload, signature]))
        shutil.copyfile(ca['directory'] / 'signer.pem', str(path) + '.signer.pem')
        shutil.copyfile(ca['directory'] / 'fresh.crl.pem', str(path) + '.crl.pem')
        return path

    def case(self, name, policy, module, expect, *, mutate=False, replace=False, mode=None, codes=None, verifier=None):
        argv = [verifier or self.args.verifier, policy, module]
        if mutate:
            argv.append('mutate')
        if replace:
            argv.append('replace')
        if mode:
            argv.append(mode)
        process = subprocess.run([str(v) for v in argv], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
        output = process.stdout.decode(errors='replace') + process.stderr.decode(errors='replace')
        accepted = process.returncode == 0 and output.startswith('ALLOW')
        denied = process.returncode == 2 and output.startswith('DENY ')
        passed = accepted if expect else denied
        expected_codes = {
            'unsigned module': ['SIG_001'],
            'whole-module digest includes every byte': ['SIG_004'],
            'ECDSA signature forgery': ['SIG_003'],
            'downloaded self-signed root is untrusted': ['CERT_002', 'CERT_001'],
            'authorized CA does not authorize another publisher': ['CERT_006'],
            'publisher module identity binding': ['CERT_006'],
            'missing revocation evidence': ['CERT_005'],
            'expired CRL': ['CERT_005'],
            'fresh CRL cannot hide another expired issuer CRL': ['CERT_005'],
            'CRL freshness policy': ['CERT_005'],
            'algorithm downgrade/confusion': ['SIG_002', 'SIG_005'],
            'malleable high-S ECDSA encoding': ['SIG_003'],
            'untrusted CRL issuer': ['CERT_005'],
            'trailing COSE bytes': ['SIG_002'],
            'future signed manifest': ['POL_004'],
            'expired signed manifest': ['POL_004'],
            'local minimum module version': ['POL_001'],
            'local minimum security epoch': ['POL_001'],
            'signed capability exceeds local policy': ['POL_003'],
            'persistent module rollback rejection': ['POL_001'],
            'persistent policy rollback rejection': ['POL_002'],
            'policy contents cannot change at same version': ['POL_002'],
            'clock rollback against protected last time': ['POL_004'],
            'world writable trust policy': ['POL_002'],
            'symlink trusted policy': ['POL_002'],
            'missing protected state fails closed': ['POL_001'],
            'leaf code signing EKU required': ['CERT_006'],
            'leaf digitalSignature key usage required': ['CERT_006'],
            'CA true leaf cannot act as release signer': ['SIG_005'],
            'revoked signer': ['CERT_004'],
        }
        codes = codes if codes is not None else expected_codes.get(name)
        if codes and not expect:
            passed = passed and any(code in output for code in codes)
        row = {'name': name, 'status': 'PASS' if passed else 'FAIL', 'expected': 'ALLOW' if expect else 'DENY',
               'argv': [str(v) for v in argv], 'returncode': process.returncode, 'output': output}
        self.rows.append(row)
        print(row['status'], name, output.strip(), flush=True)

    def vm_case(self, name, arguments, expect, *, contains=None, forbidden=None, syntax_error=False):
        argv = [str(self.args.uwvm), *map(str, arguments)]
        process = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        output = process.stdout.decode(errors='replace') + process.stderr.decode(errors='replace')
        passed = process.returncode == 0 if expect else process.returncode != 0 and ('SIG_' in output or 'POL_' in output or 'CERT_' in output)
        if syntax_error:
            passed = process.returncode != 0 and '--secure' in output
        if contains is not None:
            passed = passed and contains in output
        if forbidden is not None:
            passed = passed and forbidden not in output
        row = {'name': name, 'status': 'PASS' if passed else 'FAIL', 'expected': 'ALLOW' if expect else 'DENY',
               'argv': argv, 'returncode': process.returncode, 'output': output}
        self.rows.append(row)
        print(row['status'], name, output.strip(), flush=True)

    def guest_trap_case(self, policy, module):
        argv = [str(self.args.uwvm), '--secure', 'module', str(module), str(policy), '--run', str(module)]
        process = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        output = process.stdout.decode(errors='replace') + process.stderr.decode(errors='replace')
        passed = process.returncode != 0 and 'unreachable' in output.lower() and not any(code in output for code in ['SIG_', 'CERT_', 'POL_'])
        row = {'name': 'full VM executes the authenticated guest unreachable instruction', 'status': 'PASS' if passed else 'FAIL',
               'expected': 'guest unreachable trap after successful authorization', 'argv': argv,
               'returncode': process.returncode, 'output': output}
        self.rows.append(row)
        print(row['status'], row['name'], output.strip(), flush=True)

    def execute(self):
        ca = self.pki('trusted')
        other = self.pki('attacker')
        fresh = lambda name, **kw: self.policy(name, ca, **kw)
        good = self.module('valid', ca)
        self.case('valid ES256 X.509 with fresh CRL', fresh('valid-policy'), good, True)
        if self.args.unsupported_verifier:
            self.case('unsupported cryptography backend fails closed', fresh('unsupported-policy'), good, False,
                      codes=['SIG_005'], verifier=self.args.unsupported_verifier)
        self.case('authorization seal detects changed in-memory bytes', fresh('seal-policy'), good, True, mutate=True)
        self.case('policy change invalidates an already issued authorization', fresh('policy-change-seal'), good, True, mode='change-policy')
        receipt_module = self.module('receipt-original', ca)
        changed_manifest = self.module('receipt-changed-manifest', ca, version=2)
        self.case('another valid manifest cannot replace an issued authorization', fresh('manifest-change-seal'), receipt_module,
                  True, mode='replace-signature=' + str(changed_manifest) + '.sig.cose')
        replacement = self.module('disk-replacement', ca)
        self.case('disk replacement cannot change verified snapshot', fresh('replacement-policy'), replacement, True, replace=True)
        unsigned = self.module('unsigned', ca)
        Path(str(unsigned) + '.sig.cose').unlink()
        self.case('unsigned module', fresh('unsigned-policy'), unsigned, False)
        changed = self.module('mutated', ca)
        changed.write_bytes(changed.read_bytes() + b'\x00')
        self.case('whole-module digest includes every byte', fresh('mutated-policy'), changed, False)
        badsig = self.module('invalid-signature', ca, signature_mutation=True)
        self.case('ECDSA signature forgery', fresh('badsig-policy'), badsig, False)
        foreign = self.module('foreign', other)
        self.case('downloaded self-signed root is untrusted', fresh('foreign-policy', signer=other['spki']), foreign, False)
        self.case('authorized CA does not authorize another publisher', fresh('publisher-policy', signer=other['spki']), good, False)
        wrong_id = self.module('wrong-id', ca, module_id='example.other')
        self.case('publisher module identity binding', fresh('identity-policy'), wrong_id, False)
        missing = self.module('missing-crl', ca)
        Path(str(missing) + '.crl.pem').unlink()
        self.case('missing revocation evidence', fresh('missing-crl-policy'), missing, False)
        stale = self.module('expired-crl', ca)
        old = dt.datetime.fromtimestamp(self.now - 7200, dt.timezone.utc).strftime('%Y%m%d%H%M%SZ')
        expired = dt.datetime.fromtimestamp(self.now - 3600, dt.timezone.utc).strftime('%Y%m%d%H%M%SZ')
        self.openssl('ca', '-batch', '-config', ca['directory'] / 'ca.cnf', '-gencrl', '-crl_lastupdate', old,
                     '-crl_nextupdate', expired, '-out', str(stale) + '.crl.pem')
        self.case('expired CRL', fresh('expired-crl-policy'), stale, False)
        mixed = self.module('mixed-expired-crl', ca)
        Path(str(mixed) + '.crl.pem').write_bytes(Path(str(stale) + '.crl.pem').read_bytes() + (ca['directory'] / 'fresh.crl.pem').read_bytes())
        self.case('fresh CRL cannot hide another expired issuer CRL', fresh('mixed-expired-crl-policy'), mixed, False)
        aged = self.module('aged-crl', ca)
        future = dt.datetime.fromtimestamp(self.now + 3600, dt.timezone.utc).strftime('%Y%m%d%H%M%SZ')
        self.openssl('ca', '-batch', '-config', ca['directory'] / 'ca.cnf', '-gencrl', '-crl_lastupdate', old,
                     '-crl_nextupdate', future, '-out', str(aged) + '.crl.pem')
        self.case('CRL freshness policy', fresh('aged-crl-policy', max_age=60), aged, False)
        unknown = self.module('wrong-algorithm', ca, algorithm=-8)
        self.case('algorithm downgrade/confusion', fresh('algorithm-policy'), unknown, False)
        high_s = self.module('noncanonical-high-s', ca)
        high_s_file = Path(str(high_s) + '.sig.cose')
        encoded = high_s_file.read_bytes()
        order = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
        high_s_file.write_bytes(encoded[:-32] + (order - int.from_bytes(encoded[-32:], 'big')).to_bytes(32, 'big'))
        self.case('malleable high-S ECDSA encoding', fresh('high-s-policy'), high_s, False)
        bad_revocation = self.module('wrong-crl-issuer', ca)
        shutil.copyfile(other['directory'] / 'fresh.crl.pem', str(bad_revocation) + '.crl.pem')
        self.case('untrusted CRL issuer', fresh('crl-issuer-policy'), bad_revocation, False)
        malformed = self.module('trailing-cbor', ca)
        with Path(str(malformed) + '.sig.cose').open('ab') as stream:
            stream.write(b'\x00')
        self.case('trailing COSE bytes', fresh('trailing-policy'), malformed, False)
        future_module = self.module('not-yet-valid', ca, not_before=self.now + 3600, not_after=self.now + 7200)
        self.case('future signed manifest', fresh('future-policy'), future_module, False)
        expired_module = self.module('expired', ca, not_before=self.now - 7200, not_after=self.now - 3600)
        self.case('expired signed manifest', fresh('expired-policy'), expired_module, False)
        self.case('local minimum module version', fresh('minimum-policy', minimum=2), good, False)
        self.case('local minimum security epoch', fresh('epoch-policy', epoch=2), good, False)
        excessive = self.module('excessive-caps', ca, caps=1)
        self.case('signed capability exceeds local policy', fresh('caps-policy'), excessive, False)
        rollback_policy = fresh('rollback-policy')
        new = self.module('version-two', ca, version=2)
        self.case('record newer authorized version', rollback_policy, new, True)
        self.case('persistent module rollback rejection', rollback_policy, good, False)
        policy_rollback = fresh('policy-rollback', version=2)
        self.case('record newer policy version', policy_rollback, good, True)
        policy_rollback.write_text(policy_rollback.read_text().replace('policy_version=2', 'policy_version=1'))
        self.case('persistent policy rollback rejection', policy_rollback, good, False)
        policy_tamper = fresh('policy-same-version')
        self.case('record authenticated local policy digest', policy_tamper, good, True)
        policy_tamper.write_text(policy_tamper.read_text().replace('max_module_bytes=1048576', 'max_module_bytes=1048577'))
        self.case('policy contents cannot change at same version', policy_tamper, good, False)
        clock_policy = fresh('clock-rollback')
        self.case('record trusted local verification time', clock_policy, good, True)
        clock_state = clock_policy.parent / 'state/state'
        lines = clock_state.read_text().splitlines()
        header = lines[1].split()
        header[1] = str(self.now + 3600)
        lines[1] = ' '.join(header)
        clock_state.write_text('\n'.join(lines) + '\n')
        self.case('clock rollback against protected last time', clock_policy, good, False)
        unsafe = fresh('writable-policy')
        unsafe.chmod(0o666)
        self.case('world writable trust policy', unsafe, good, False)
        symbolic = fresh('symlink-policy')
        link = symbolic.parent / 'policy-link'
        link.symlink_to(symbolic)
        self.case('symlink trusted policy', link, good, False)
        absent_state = fresh('absent-state')
        (absent_state.parent / 'state/state').unlink()
        self.case('missing protected state fails closed', absent_state, good, False)
        if self.args.uwvm:
            vm_policy = fresh('vm-policy')
            self.vm_case('CLI secure category help', ['--help', 'secure'], True, contains='--secure module')
            self.vm_case('CLI default help includes secure category', ['--help'], True, contains='<secure>')
            self.vm_case('CLI accepts selected signed trusted main', ['--secure', 'module', good, vm_policy, '--run', good], True)
            self.vm_case('CLI retains ordinary unsigned behavior when unselected', ['--run', unsigned], True)
            self.vm_case('CLI denies selected unsigned main', ['--secure', 'module', unsigned, vm_policy, '--run', unsigned], False)
            self.vm_case('CLI denies selected modified main', ['--secure', 'module', changed, vm_policy, '--run', changed], False)
            self.vm_case('CLI denies unsigned preload before policy argument',
                         ['--wasm-preload-library', unsigned, 'dep', '--secure', 'module', unsigned, vm_policy, '--run', good], False)
            self.vm_case('CLI denies unsigned preload after policy argument',
                         ['--secure', 'module', unsigned, vm_policy, '--wasm-preload-library', unsigned, 'dep', '--run', good], False)
            dependency = self.module('trusted-dependency', ca)
            self.vm_case('CLI verifies signed preload before policy argument',
                         ['--wasm-preload-library', dependency, 'dep', '--secure', 'module', dependency, vm_policy, '--run', unsigned], True)
            second_policy = fresh('vm-policy-two')
            self.vm_case('CLI supports independent policy selection for main and preload',
                         ['--secure', 'module', good, vm_policy, '--wasm-preload-library', dependency, 'dep',
                          '--secure', 'module', dependency, second_policy, '--run', good], True)
            multi_policy = fresh('vm-multi-policy')
            with multi_policy.open('a') as stream:
                stream.write(f"module=example.other|uwvm2|{ca['spki']}|1|0|0\n")
            self.vm_case('CLI requires explicit identity for a multi-rule policy',
                         ['--secure', 'module', good, multi_policy, '--run', good], False)
            self.vm_case('CLI binds named-module to the authorized signed identity',
                         ['--secure', 'named-module', good, multi_policy, 'example.app', '--run', good], True)
            self.vm_case('CLI rejects a signed module with another requested identity',
                         ['--secure', 'named-module', good, multi_policy, 'example.other', '--run', good], False)
            self.vm_case('CLI rejects named-module with missing requested identity',
                         ['--secure', 'named-module', good, multi_policy], False, syntax_error=True)
            def wasm_string(text):
                encoded = text.encode()
                return bytes([len(encoded)]) + encoded
            def section(number, payload):
                return bytes([number, len(payload)]) + payload
            wasi_import = (b'\0asm\x01\0\0\0'
                           + section(1, bytes.fromhex('0260017f00600000'))
                           + section(2, b'\x01' + wasm_string('wasi_snapshot_preview1') + wasm_string('proc_exit') + b'\0\0')
                           + section(3, b'\x01\x01')
                           + section(7, b'\x01' + wasm_string('_start') + b'\0\x01')
                           + section(10, b'\x01\x02\0\x0b'))
            undeclared = self.module('undeclared-wasi', ca, content=wasi_import)
            self.vm_case('initializer denies host import missing signed capability',
                         ['--secure', 'module', undeclared, vm_policy, '--run', undeclared], False)
            self.vm_case('CLI denies native library ingress',
                         ['--secure', 'module', good, vm_policy, '--wasm-register-dl', '/does-not-exist/libuntrusted.so', '--run', good], False)
            if not self.args.loader_only:
                trap_bytes = good.read_bytes()[:-6] + bytes.fromhex('0a05010300000b')
                trap_module = self.module('guest-unreachable', ca, content=trap_bytes)
                self.guest_trap_case(vm_policy, trap_module)
                signature = Path(str(trap_module) + '.sig.cose')
                data = bytearray(signature.read_bytes())
                data[-1] ^= 1
                signature.write_bytes(data)
                self.vm_case('invalid signature prevents guest entry before the trap instruction',
                             ['--secure', 'module', trap_module, vm_policy, '--run', trap_module], False,
                             contains='SIG_003', forbidden='catch unreachable')
        no_eku_ca = self.pki('wrong-eku', eku='serverAuth')
        no_eku = self.module('wrong-eku-module', no_eku_ca)
        self.case('leaf code signing EKU required', self.policy('eku-policy', no_eku_ca), no_eku, False)
        no_usage_ca = self.pki('wrong-key-usage', key_usage='keyEncipherment')
        no_usage = self.module('wrong-key-usage-module', no_usage_ca)
        self.case('leaf digitalSignature key usage required', self.policy('key-usage-policy', no_usage_ca), no_usage, False)
        ca_signer = self.pki('ca-leaf', ca_constraint='true')
        ca_module = self.module('ca-leaf-module', ca_signer)
        self.case('CA true leaf cannot act as release signer', self.policy('ca-leaf-policy', ca_signer), ca_module, False)
        self.openssl('ca', '-batch', '-config', ca['directory'] / 'ca.cnf', '-revoke', ca['directory'] / 'signer.pem')
        revoked = self.module('revoked', ca)
        self.openssl('ca', '-batch', '-config', ca['directory'] / 'ca.cnf', '-gencrl', '-out', str(revoked) + '.crl.pem')
        self.case('revoked signer', fresh('revoked-policy'), revoked, False)
        revoke_memory = fresh('revocation-memory-policy')
        self.case('record authenticated signer revocation', revoke_memory, revoked, False, codes=['CERT_004'])
        old_good = self.module('revoked-old-fresh-good', ca)
        self.case('older still-fresh good CRL cannot erase learned revocation', revoke_memory, old_good, False, codes=['CERT_004'])
        late_revocation = self.module('late-revocation', ca)
        late_policy = fresh('late-revocation-policy')
        self.case('revocation invalidates an already issued authorization', late_policy, late_revocation,
                  True, mode='replace-crl=' + str(revoked) + '.crl.pem')
        shutil.copyfile(ca['directory'] / 'fresh.crl.pem', str(late_revocation) + '.crl.pem')
        self.case('receipt revocation remains denied after old good evidence replay', late_policy, late_revocation,
                  False, codes=['CERT_004'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verifier', type=Path, required=True)
    parser.add_argument('--unsupported-verifier', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--require-cgroup', action='store_true')
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--loader-only', action='store_true', help='identify a parser/loader/initializer probe without guest execution')
    args = parser.parse_args()
    before = None
    if args.require_cgroup:
        spec = importlib.util.spec_from_file_location('wsea_cgroup_probe', ROOT / 'tests/ros_cgroup/probe.py')
        probe = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(probe)
        before = probe.probe()
        if before['status'] != 'PASS':
            print(json.dumps(before, indent=2))
            return 77
        allowed = set([0, 2, 4, 6, *range(16, 32)])
        if not set(os.sched_getaffinity(0)) <= allowed:
            print('BLOCKED: run taskset -c 0,2,4,6,16-31')
            return 77
    suite = Suite(args)
    report = {'status': 'FAIL', 'cgroup_before': before, 'started_unix': int(time.time()),
              'verifier_sha256': sha(args.verifier), 'driver_sha256': sha(__file__),
              'production_header_sha256': sha(ROOT / 'src/uwvm2/utils/security/wsea.h')}
    if args.uwvm:
        report['uwvm_sha256'] = sha(args.uwvm)
        report['uwvm_scope'] = 'production parser/loader/initializer only; no guest execution' if args.loader_only else 'full VM executable'
    try:
        suite.execute()
        report['status'] = 'PASS' if all(row['status'] == 'PASS' for row in suite.rows) else 'FAIL'
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        report.update(cases=suite.rows, commands=suite.commands, finished_unix=int(time.time()))
        if before is not None:
            after = probe.probe()
            report['cgroup_after'] = after
            if after['status'] != 'PASS' or before['cgroup_incarnation'] != after['cgroup_incarnation']:
                report['status'] = 'FAIL'
            for old, new in zip(before['cgroup_ancestors'], after['cgroup_ancestors']):
                if old.get('memory.events') is None:
                    continue
                previous = dict(line.split() for line in old['memory.events'].splitlines())
                current = dict(line.split() for line in new['memory.events'].splitlines())
                if any(current.get(key) != previous.get(key) for key in ['oom', 'oom_kill']):
                    report['status'] = 'FAIL'
        (suite.out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
