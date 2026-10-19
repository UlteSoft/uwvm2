#!/usr/bin/env python3
"""Real-OpenSSL WSEA positive/adversarial fixtures and truthful QEMU execution.

No candidates are built by this driver. Generate fixtures once on the authorized
Linux host, then run them under each explicitly supplied target runner/sysroot.
Every run gets fresh private rollback storage; existing security state is never
reset. A missing producer/candidate or infrastructure error is FAIL/BLOCKED,
never evidence that an adversarial signature was rejected.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = 'uwvm-wsea-algorithm-matrix-v2'
WASM = bytes.fromhex('0061736d0100000001040160000003020100070a01065f737461727400000a040102000b')
GUEST_PREFIX = 'WSEA-GUEST-'
ALGORITHMS = {
    'ES256': (-7, 'EC', 'sha256', 'prime256v1', 32),
    'ESP256': (-9, 'EC', 'sha256', 'prime256v1', 32),
    'ES384': (-35, 'EC', 'sha384', 'secp384r1', 48),
    'ESP384': (-51, 'EC', 'sha384', 'secp384r1', 48),
    'ES512': (-36, 'EC', 'sha512', 'secp521r1', 66),
    'ESP512': (-52, 'EC', 'sha512', 'secp521r1', 66),
    'Ed25519': (-19, 'ED25519', None, None, 64),
    'PS256': (-37, 'RSA-PSS', 'sha256', None, 384),
    'PS384': (-38, 'RSA-PSS', 'sha384', None, 384),
    'PS512': (-39, 'RSA-PSS', 'sha512', None, 384),
    'RS256': (-257, 'RSA', 'sha256', None, 384),
    'RS384': (-258, 'RSA', 'sha384', None, 384),
    'RS512': (-259, 'RSA', 'sha512', None, 384),
}
ORDERS = {
    32: int('ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551', 16),
    48: int('ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973', 16),
    66: int('01fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409', 16),
}
TARGETS = {
    'aarch64': (183, 64, 'little', 'aarch64', False),
    'x86_64': (62, 64, 'little', 'x86_64', False),
    'i686': (3, 32, 'little', 'i686', False),
    'arm': (40, 32, 'little', 'arm', False),
    'armhf': (40, 32, 'little', 'arm', True),
    'armeb': (40, 32, 'big', 'arm', True),
    'aarch64_be': (183, 64, 'big', 'aarch64', False),
    'ppc64': (21, 64, 'big', 'ppc64', False),
    'mips64': (8, 64, 'big', 'mips64', False),
    'loongarch64': (258, 64, 'little', 'loongarch64', False),
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def uleb(value):
    result = bytearray()
    while True:
        byte, value = value & 127, value >> 7
        result.append(byte | (128 if value else 0))
        if not value:
            return bytes(result)


def marker_guest(name):
    """WASI _start writes one fixed marker through fd_write(1,iov,1,8)."""
    marker = GUEST_PREFIX + name + '\n'
    message = marker.encode('ascii')
    text = lambda value: uleb(len(value)) + value
    section = lambda number, data: bytes([number]) + uleb(len(data)) + data
    types = b'\x02\x60\x04\x7f\x7f\x7f\x7f\x01\x7f\x60\x00\x00'
    imports = b'\x01' + text(b'wasi_snapshot_preview1') + text(b'fd_write') + b'\x00\x00'
    exports = b'\x02' + text(b'memory') + b'\x02\x00' + text(b'_start') + b'\x00\x01'
    instructions = b'\x00\x41\x01\x41\x00\x41\x01\x41\x08\x10\x00\x1a\x0b'
    data = struct.pack('<II', 16, len(message)) + bytes(8) + message
    body = (b'\x00asm\x01\x00\x00\x00' + section(1, types) + section(2, imports) +
            section(3, b'\x01\x01') + section(5, b'\x01\x00\x01') + section(7, exports) +
            section(10, b'\x01' + text(instructions)) + section(11, b'\x01\x00\x41\x00\x0b' + text(data)))
    return body, marker


def head(major, n):
    if n < 24:
        return bytes([(major << 5) | n])
    for count, extra in ((1, 24), (2, 25), (4, 26), (8, 27)):
        if n < 1 << (8 * count):
            return bytes([(major << 5) | extra]) + n.to_bytes(count, 'big')
    raise ValueError('CBOR integer overflow')


def cbor(value):
    if isinstance(value, int):
        return head(0 if value >= 0 else 1, value if value >= 0 else -1 - value)
    if isinstance(value, bytes):
        return head(2, len(value)) + value
    if isinstance(value, str):
        data = value.encode('utf-8')
        return head(3, len(data)) + data
    if isinstance(value, list):
        return head(4, len(value)) + b''.join(cbor(x) for x in value)
    if isinstance(value, dict):
        return head(5, len(value)) + b''.join(k + v for k, v in sorted((cbor(k), cbor(v)) for k, v in value.items()))
    raise TypeError(type(value))


SECTION_NAME = b'uwvm.wsea.v2'

def pack_embedded(core, signature, certificates, crls):
    payload = cbor([1, signature, certificates, crls])
    content = uleb(len(SECTION_NAME)) + SECTION_NAME + payload
    return core + b'\0' + uleb(len(content)) + content

def embedded_parts(image):
    def number(offset):
        value = 0
        for shift in range(0, 35, 7):
            byte = image[offset]; offset += 1; value |= (byte & 127) << shift
            if not byte & 128: return value, offset
        raise ValueError('overflowing WASM ULEB')
    offset = 8
    while offset < len(image):
        start = offset; tag = image[offset]; length, content = number(offset + 1); end = content + length
        if end > len(image): raise ValueError('truncated embedded WASM')
        if tag == 0:
            size, name = number(content)
            if image[name:name+size] == SECTION_NAME:
                if end != len(image): raise ValueError('WSEA is not final')
                data = image[name+size:end]
                if data[:2] != b'\x84\x01': raise ValueError('embedded envelope version')
                values, p = [], 2
                for unused in range(3):
                    byte = data[p]; p += 1
                    if byte >> 5 != 2: raise ValueError('embedded evidence type')
                    size = byte & 31
                    if size >= 24:
                        width = {24:1,25:2,26:4,27:8}[size]
                        size = int.from_bytes(data[p:p+width], 'big');p += width
                    values.append(data[p:p+size]);p += size
                if p != len(data): raise ValueError('embedded evidence tail')
                return image[:start], *values
        offset = end
    raise ValueError('missing WSEA section')


def der_length(n):
    data = n.to_bytes(max(1, (n.bit_length() + 7) // 8), 'big')
    return bytes([n]) if n < 128 else bytes([128 | len(data)]) + data


def der_item(tag, content):
    return bytes([tag]) + der_length(len(content)) + content


def der_integer(n):
    data = n.to_bytes(max(1, (n.bit_length() + 7) // 8), 'big')
    if data[0] & 128:
        data = b'\0' + data
    return der_item(2, data)


def pem_der(path, label):
    label = label.encode('ascii')
    raw = Path(path).read_bytes()
    match = re.fullmatch(rb'\s*-----BEGIN ' + label + rb'-----\s*(.*?)\s*-----END ' + label + rb'-----\s*', raw, re.S)
    if match is None:
        raise RuntimeError('unexpected producer PEM envelope: ' + str(path))
    return base64.b64decode(re.sub(rb'\s+', b'', match[1]), validate=True)


def write_pem(path, label, der):
    label = label.encode('ascii')
    encoded = base64.b64encode(der)
    lines = [encoded[i:i + 64] for i in range(0, len(encoded), 64)]
    Path(path).write_bytes(b'-----BEGIN ' + label + b'-----\n' + b'\n'.join(lines) + b'\n-----END ' + label + b'-----\n')


def append_inside_pem(path, label, extra_object=False):
    original = pem_der(path, label)
    write_pem(path, label, original + (original if extra_object else b'\x00'))


def der_take(data, offset=0):
    start = offset
    if offset + 2 > len(data) or (data[offset] & 31) == 31:
        raise ValueError('unsupported producer DER tag')
    tag, size = data[offset], data[offset + 1]
    offset += 2
    if size & 128:
        count = size & 127
        if not count or count > 4 or offset + count > len(data):
            raise ValueError('invalid producer DER length')
        size = int.from_bytes(data[offset:offset + count], 'big')
        offset += count
    if offset + size > len(data):
        raise ValueError('truncated producer DER value')
    return tag, data[offset:offset + size], data[start:offset + size], offset + size


def der_payload(encoded, expected):
    tag, content, _, end = der_take(encoded)
    if tag != expected or end != len(encoded):
        raise ValueError('unexpected outer producer DER value')
    return content


def der_parts(encoded, expected=0x30):
    """Split a known producer's definite DER TLV, preserving complete children."""
    content = der_payload(encoded, expected)
    values, offset = [], 0
    while offset < len(content):
        _, _, value, offset = der_take(content, offset)
        values.append(value)
    return values


def ec_raw(der, width):
    def take(data, offset, tag):
        if offset >= len(data) or data[offset] != tag:
            raise ValueError('unexpected DER tag')
        offset += 1
        n = data[offset]
        offset += 1
        if n & 128:
            count = n & 127
            if not count or offset + count > len(data):
                raise ValueError('invalid DER length')
            n = int.from_bytes(data[offset:offset + count], 'big')
            offset += count
        if offset + n > len(data):
            raise ValueError('truncated DER')
        return data[offset:offset + n], offset + n
    sequence, end = take(der, 0, 0x30)
    if end != len(der):
        raise ValueError('trailing DER')
    r, offset = take(sequence, 0, 2)
    s, offset = take(sequence, offset, 2)
    if offset != len(sequence):
        raise ValueError('trailing ECDSA integer')
    r, s = int.from_bytes(r, 'big'), int.from_bytes(s, 'big')
    s = min(s, ORDERS[width] - s)
    return r.to_bytes(width, 'big') + s.to_bytes(width, 'big')


def cgroup_snapshot(required):
    if not required:
        return None
    spec = importlib.util.spec_from_file_location('wsea_algorithm_cgroup', ROOT / 'tests/ros_cgroup/probe.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    result = module.probe()
    if result.get('status') != 'PASS':
        raise RuntimeError('BLOCKED: approved Linux cgroup unavailable: ' + json.dumps(result))
    return result


class Fixtures:
    def __init__(self, output, *, openssl='openssl', ed_variant_signer=None, rsa_keygen_timeout=600):
        self.out = Path(output).absolute()
        self.out.mkdir(mode=0o700, parents=True, exist_ok=False)
        os.chmod(self.out, 0o700)
        self.openssl_path = openssl
        self.variant_signer = ed_variant_signer
        self.rsa_keygen_timeout = rsa_keygen_timeout
        self.commands, self.cases, self.producers, self.algorithms = [], [], [], {}
        self.pki_cache, self.guest_algorithms = {}, {}
        self.now = int(time.time())
        self.counter = 0

    def command(self, argv, *, data=None, allowed=(0,), timeout=120):
        started = time.monotonic()
        try:
            process = subprocess.run(list(map(str, argv)), input=data, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, timeout=timeout)
        except subprocess.TimeoutExpired:
            self.commands.append({'argv': list(map(str, argv)), 'status': 'TIMEOUT', 'timeout_seconds': timeout,
                                  'elapsed_seconds': time.monotonic() - started})
            raise
        self.commands.append({'argv': list(map(str, argv)), 'returncode': process.returncode,
                              'elapsed_seconds': time.monotonic() - started,
                              'stdout': process.stdout.decode(errors='replace'),
                              'stderr': process.stderr.decode(errors='replace')})
        if process.returncode not in allowed:
            raise RuntimeError('fixture producer failed: ' + json.dumps(self.commands[-1]))
        return process

    def openssl(self, *argv, data=None):
        return self.command([self.openssl_path, *argv], data=data).stdout

    def pki(self, name, family='EC', curve='prime256v1', *, bits=3072, exponent=65537,
            eku='codeSigning', issuer_family='EC', issuer_pss=False, issuer_eku=None):
        if name in self.pki_cache:
            return self.pki_cache[name]
        directory = self.out / 'pki' / name
        directory.mkdir(mode=0o700, parents=True)
        (directory / 'newcerts').mkdir()
        for file, text in [('index', ''), ('serial', '1000\n'), ('crlnumber', '1000\n')]:
            (directory / file).write_text(text)
        def generate(path, key_family, key_curve=None, key_bits=3072, key_exponent=65537):
            opts = ['genpkey', '-algorithm', 'RSA-PSS' if key_family == 'RSA-PSS-RESTRICTED' else 'RSA' if key_family.startswith('RSA') else key_family]
            if key_family == 'EC':
                opts += ['-pkeyopt', 'ec_paramgen_curve:' + key_curve]
            elif key_family.startswith('RSA'):
                opts += ['-pkeyopt', f'rsa_keygen_bits:{key_bits}', '-pkeyopt', f'rsa_keygen_pubexp:{key_exponent}']
                if key_family == 'RSA-PSS-RESTRICTED':
                    opts += ['-pkeyopt', 'rsa_pss_keygen_md:sha256', '-pkeyopt', 'rsa_pss_keygen_mgf1_md:sha256',
                             '-pkeyopt', 'rsa_pss_keygen_saltlen:32']
            self.command([self.openssl_path, *opts, '-out', path], timeout=self.rsa_keygen_timeout if key_bits >= 4096 else 120)
        generate(directory / 'root.key', issuer_family, 'prime256v1')
        root_opts = ['req', '-new', '-x509', '-key', directory / 'root.key', '-out', directory / 'root.pem',
                     '-days', '3', '-subj', f'/CN=WSEA Matrix {name} Root', '-addext', 'basicConstraints=critical,CA:TRUE',
                     '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-addext', 'subjectKeyIdentifier=hash']
        if issuer_family != 'ED25519':
            root_opts += ['-sha256']
        if issuer_eku:
            root_opts += ['-addext', 'extendedKeyUsage=' + issuer_eku]
        if issuer_pss:
            root_opts += ['-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:32', '-sigopt', 'rsa_mgf1_md:sha256']
        self.openssl(*root_opts)
        generate(directory / 'signer.key', family, curve, bits, exponent)
        self.openssl('req', '-new', '-key', directory / 'signer.key', '-out', directory / 'signer.csr',
                     '-subj', f'/CN=WSEA Matrix {name} Release')
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
{'' if issuer_family == 'ED25519' else 'default_md=sha256'}
default_days=2
default_crl_days=1
policy=names
unique_subject=no
x509_extensions=signer
[names]
commonName=supplied
[signer]
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature
extendedKeyUsage={eku}
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
''')
        sign_opts = ['-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:32', '-sigopt', 'rsa_mgf1_md:sha256'] if issuer_pss else []
        self.openssl('ca', '-batch', '-notext', '-config', config, '-in', directory / 'signer.csr', '-out', directory / 'signer.pem', *sign_opts)
        self.openssl('ca', '-batch', '-config', config, '-gencrl', '-out', directory / 'fresh.crl.pem', *sign_opts)
        pubkey = self.openssl('x509', '-in', directory / 'signer.pem', '-pubkey', '-noout')
        spki = self.openssl('pkey', '-pubin', '-outform', 'DER', data=pubkey)
        result = {'directory': str(directory), 'key': str(directory / 'signer.key'), 'certificate': str(directory / 'signer.pem'),
                  'root': str(directory / 'root.pem'), 'crl': str(directory / 'fresh.crl.pem'),
                  'pin': hashlib.sha256(spki).hexdigest(), 'family': family, 'curve': curve,
                  'bits': bits, 'exponent': exponent, 'issuer_family': issuer_family, 'issuer_pss': issuer_pss}
        self.pki_cache[name] = result
        return result

    def policy(self, ca, algorithms=None, *, version=1, minimum=1, module_id='matrix.app', raw_algorithms=None, capabilities=0):
        fields = f'{module_id}|uwvm2|{ca["pin"]}|{minimum}|0|{capabilities}'
        if algorithms is not None or raw_algorithms is not None:
            fields += '|' + (raw_algorithms if raw_algorithms is not None else ','.join(algorithms))
        return (f'UWVM-WSEA-POLICY-1\npolicy_version={version}\ntrust_root={ca["root"]}\n'
                f'state_dir=@STATE_DIR@\nmax_crl_age_seconds=86400\nmax_module_bytes=1048576\nmodule={fields}\n')

    def signature(self, algorithm, ca, structure, *, padding=None, mgf=None, salt=None, digest=None, raw_der=False, variant=None):
        _, family, default_digest, _, width = ALGORITHMS[algorithm]
        digest = digest or default_digest
        self.counter += 1
        prefix = self.out / f'producer-{self.counter:04d}'
        source = prefix.with_suffix('.input')
        output = prefix.with_suffix('.signature')
        source.write_bytes(structure)
        if family == 'ED25519':
            if variant:
                if not self.variant_signer:
                    self.producers.append({'producer': variant, 'status': 'BLOCKED', 'reason': '--ed-variant-signer was not supplied'})
                    return None
                proc = self.command([self.variant_signer, ca['key'], source, output, variant,
                                     'WSEA adversarial context' if variant == 'Ed25519ctx' else ''], allowed=(0, 77))
                if proc.returncode == 77:
                    self.producers.append({'producer': variant, 'status': 'BLOCKED', 'reason': 'OpenSSL provider lacks requested EdDSA instance'})
                    return None
            else:
                self.openssl('pkeyutl', '-sign', '-rawin', '-inkey', ca['key'], '-in', source, '-out', output)
        else:
            opts = ['dgst', '-' + digest, '-sign', ca['key'], '-out', output]
            if family.startswith('RSA'):
                selected_padding = padding or ('pss' if family == 'RSA-PSS' else 'pkcs1')
                opts += ['-sigopt', 'rsa_padding_mode:' + selected_padding]
                if selected_padding == 'pss':
                    opts += ['-sigopt', 'rsa_mgf1_md:' + (mgf or digest), '-sigopt',
                             'rsa_pss_saltlen:' + str(salt if salt is not None else hashlib.new(digest).digest_size)]
            self.openssl(*opts, source)
        signature = output.read_bytes()
        if family == 'EC' and not raw_der:
            signature = ec_raw(signature, width)
        return signature

    def module(self, name, algorithm, ca, *, policy=None, protected=None, payload=None, signature=None,
               sign_algorithm=None, sign_options=None, body=WASM, wire=None, version=1, capabilities=0, guest_marker=None):
        directory = self.out / 'modules'
        directory.mkdir(mode=0o700, exist_ok=True)
        path = directory / (name + '.wasm')
        path.write_bytes(body)
        protected = cbor({1: ALGORITHMS[algorithm][0]}) if protected is None else protected
        payload = payload if payload is not None else cbor(['uwvm-wsea-v2', 'matrix.app', version, 0, len(body),
                    hashlib.sha256(body).digest(), self.now - 60, self.now + 43200, 'uwvm2', capabilities])
        structure = cbor(['Signature1', protected, b'', payload])
        signature = signature if signature is not None else self.signature(sign_algorithm or algorithm, ca, structure, **(sign_options or {}))
        if signature is None:
            return None
        envelope = b'\xd2' + cbor([protected, {}, payload, signature]) if wire is None else wire(protected, payload, signature)
        Path(str(path) + '.sig.cose').write_bytes(envelope)
        shutil.copyfile(ca['certificate'], str(path) + '.signer.pem')
        shutil.copyfile(ca['crl'], str(path) + '.crl.pem')
        return {'module': str(path), 'algorithm': algorithm, 'policy': policy or self.policy(ca, [algorithm], capabilities=capabilities),
                'protected': protected.hex(), 'payload': payload.hex(), 'signature': signature.hex(), 'pki': ca,
                'capabilities': capabilities, 'guest_marker': guest_marker}

    def add(self, name, fixture, allow, *, codes=None, group=None, vm=True):
        if fixture is None:
            return
        self.cases.append({'name': name, **fixture, 'expected': 'ALLOW' if allow else 'DENY',
                           'codes': list(codes or []), 'group': group or name, 'vm': vm})

    def generate(self):
        self.openssl('version', '-a')
        shared = {}
        for algorithm, (_, family, _, curve, _) in ALGORITHMS.items():
            key_name = curve if family == 'EC' else 'ed25519' if family == 'ED25519' else 'rsa3072'
            ca = shared.setdefault(key_name, self.pki(key_name, 'RSA' if family.startswith('RSA') else family, curve))
            good = self.module('valid-' + algorithm, algorithm, ca)
            self.algorithms[algorithm] = good
            self.add(algorithm + ' valid exact profile', good, True)
            body, marker = marker_guest(algorithm)
            witness = self.module('guest-marker-' + algorithm, algorithm, ca, body=body, capabilities=3, guest_marker=marker)
            self.guest_algorithms[algorithm] = witness
            self.add(algorithm + ' authenticated fd_write guest entry witness', witness, True)
            raw_signature = bytes.fromhex(witness['signature'])
            forged = bytes([raw_signature[0] ^ 1]) + raw_signature[1:]
            self.add(algorithm + ' forbidden signature never reaches fd_write guest',
                self.module('guest-marker-forged-' + algorithm, algorithm, ca, body=body, capabilities=3,
                    guest_marker=marker, signature=forged, payload=bytes.fromhex(witness['payload'])), False, codes=['SIG_003'])
            legacy = dict(good, policy=self.policy(ca))
            self.add(algorithm + ' six-field policy remains ES256 only', legacy, algorithm == 'ES256', codes=['SIG_005'])
            # Every signature family must reject a header change even with a
            # policy that allows both algorithms: protected bytes enter TBS.
            alternate = {'ES256':'ESP256','ESP256':'ES256','ES384':'ESP384','ESP384':'ES384',
                         'ES512':'ESP512','ESP512':'ES512','Ed25519':'ES256',
                         'PS256':'PS384','PS384':'PS512','PS512':'PS256',
                         'RS256':'RS384','RS384':'RS512','RS512':'RS256'}[algorithm]
            changed_header = cbor({1: ALGORITHMS[alternate][0]})
            changed = self.module('header-substitution-' + algorithm, alternate, ca,
                                  signature=bytes.fromhex(good['signature']), protected=changed_header,
                                  policy=self.policy(ca, [algorithm, alternate]))
            self.add(algorithm + ' protected algorithm substitution', changed, False, codes=['SIG_003', 'SIG_005'])
            mutated = self.module('signature-bitflip-' + algorithm, algorithm, ca,
                                  signature=bytes([int(good['signature'][:2], 16) ^ 1]) + bytes.fromhex(good['signature'])[1:])
            self.add(algorithm + ' forged signature', mutated, False, codes=['SIG_003'])
            custom = self.module('custom-section-' + algorithm, algorithm, ca,
                                 body=WASM + b'\x00\x04\x01x\x00\xff')
            self.add(algorithm + ' whole custom section is signed', custom, True)
            appended = self.module('appended-after-sign-' + algorithm, algorithm, ca)
            Path(appended['module']).write_bytes(WASM + b'\0')
            self.add(algorithm + ' appended byte invalidates full-file digest', appended, False, codes=['SIG_002', 'SIG_004'])
            if family == 'EC':
                width = ALGORITHMS[algorithm][4]
                raw = bytes.fromhex(good['signature'])
                high = raw[:width] + (ORDERS[width] - int.from_bytes(raw[width:], 'big')).to_bytes(width, 'big')
                self.add(algorithm + ' high S rejected', self.module('high-s-' + algorithm, algorithm, ca, signature=high), False, codes=['SIG_003'])
                self.add(algorithm + ' zero r and s rejected', self.module('zero-' + algorithm, algorithm, ca, signature=bytes(width * 2)), False, codes=['SIG_003'])
                self.add(algorithm + ' raw signature length is exact', self.module('oversize-' + algorithm, algorithm, ca, signature=raw + b'\0'), False, codes=['SIG_002', 'SIG_003'])
                self.add(algorithm + ' DER cannot replace COSE raw encoding', self.module('der-' + algorithm, algorithm, ca, sign_options={'raw_der': True}), False, codes=['SIG_002', 'SIG_003'])
                other = shared.setdefault('curve-mismatch-' + key_name,
                    self.pki('curve-mismatch-' + key_name, 'EC', 'secp384r1' if width != 48 else 'prime256v1'))
                self.add(algorithm + ' curve must match algorithm', self.module('wrong-curve-' + algorithm, algorithm, other, signature=raw), False, codes=['SIG_005'])
                if width == 66:
                    nonzero_padding = bytes([raw[0] | 128]) + raw[1:]
                    self.add(algorithm + ' P521 high padding bits rejected', self.module('p521-padding-' + algorithm, algorithm, ca, signature=nonzero_padding), False, codes=['SIG_003'])
            elif family.startswith('RSA'):
                wrong_padding = 'pkcs1' if family == 'RSA-PSS' else 'pss'
                self.add(algorithm + ' no RSA padding fallback', self.module('padding-' + algorithm, algorithm, ca, sign_options={'padding': wrong_padding}), False, codes=['SIG_003'])
                if family == 'RSA-PSS':
                    self.add(algorithm + ' MGF digest must equal message digest', self.module('mgf-' + algorithm, algorithm, ca,
                             sign_options={'mgf': 'sha384' if ALGORITHMS[algorithm][2] != 'sha384' else 'sha256'}), False, codes=['SIG_003'])
                    for salt in (0, hashlib.new(ALGORITHMS[algorithm][2]).digest_size - 1, -2):
                        self.add(algorithm + ' exact PSS salt length ' + str(salt), self.module('salt-' + algorithm + '-' + str(salt), algorithm, ca,
                                 sign_options={'salt': salt}), False, codes=['SIG_003'])
        ec = shared['prime256v1']
        good = self.algorithms['ES256']
        # These envelopes are signed over the malformed protected bytes. A
        # rejection therefore tests strict parsing rather than a stale signature.
        protected_cases = {
            'non-minimal negative algorithm integer': b'\xa1\x01\x38\x06',
            'algorithm is a text string': cbor({1: 'ES256'}),
            'algorithm is unsigned positive integer': cbor({1: 7}),
            'duplicate algorithm header': b'\xa2\x01\x26\x01\x26',
            'extra protected field': cbor({1: -7, 2: 0}),
            'indefinite protected map': b'\xbf\x01\x26\xff',
            'legacy ambiguous EdDSA identifier': cbor({1: -8}),
            'unknown COSE algorithm': cbor({1: -12345}),
            'weak SHA1 algorithm identifier': cbor({1: -65535}),
        }
        for index, (name, protected) in enumerate(protected_cases.items()):
            self.add(name, self.module('protected-' + str(index), 'ES256', ec, protected=protected,
                     policy=self.policy(ec, list(ALGORITHMS))), False, codes=['SIG_002', 'SIG_005'])
        for name, wire in {
            'missing mandatory tag18': lambda p, b, s: cbor([p, {}, b, s]),
            'unprotected algorithm is forbidden': lambda p, b, s: b'\xd2' + cbor([p, {1: -7}, b, s]),
            'trailing COSE bytes cannot be ignored': lambda p, b, s: b'\xd2' + cbor([p, {}, b, s]) + b'\0',
            'extra COSE element cannot be ignored': lambda p, b, s: b'\xd2' + cbor([p, {}, b, s, b'']),
            'non-minimal outer array': lambda p, b, s: b'\xd2\x98\x04' + cbor(p) + cbor({}) + cbor(b) + cbor(s),
        }.items():
            self.add(name, self.module(name.replace(' ', '-'), 'ES256', ec, wire=wire), False, codes=['SIG_002'])
        for index, value in enumerate(['', 'ES256,ES256', 'ES256,', ',ES256', 'ES256,,Ed25519', 'ES256,unknown', 'es256', 'ES256, Ed25519', 'EdDSA', 'Ed448', 'DSA', 'SHA1', 'MD5']):
            self.add('invalid algorithm policy CSV ' + repr(value), dict(good, policy=self.policy(ec, raw_algorithms=value)), False, codes=['POL_002'])
        self.add('algorithm policy CSV order is not semantically significant', dict(good, policy=self.policy(ec, list(reversed(ALGORITHMS)))), True)
        for extra_object in (False, True):
            label = 'extra-object' if extra_object else 'garbage-byte'
            for extension, pem_label, error in [('signer.pem', 'CERTIFICATE', 'CERT_002'), ('crl.pem', 'X509 CRL', 'CERT_005')]:
                fixture = self.module('pem-' + extension + '-' + label, 'ES256', ec)
                append_inside_pem(fixture['module'] + '.' + extension, pem_label, extra_object)
                self.add('PEM ' + extension + ' must consume all armored DER ' + label, fixture, False, codes=[error])
            root = self.out / ('root-inner-der-' + label + '.pem')
            shutil.copyfile(ec['root'], root)
            append_inside_pem(root, 'CERTIFICATE', extra_object)
            fixture = self.module('pem-root-' + label, 'ES256', ec, policy=self.policy(dict(ec, root=str(root)), ['ES256']))
            self.add('PEM trust root must consume all armored DER ' + label, fixture, False, codes=['CERT_001'])
        for name, bits, exponent in [('rsa2048', 2048, 65537), ('rsa-e3', 3072, 3)]:
            ca = self.pki(name, 'RSA', bits=bits, exponent=exponent)
            # Keep the envelope inside its permitted length bounds so this
            # specifically reaches key admission, rather than CBOR rejection.
            fixture = self.module(name, 'RS256', ca, signature=bytes(384)) if bits == 2048 else self.module(name, 'RS256', ca)
            self.add(name + ' public key fails minimum profile', fixture, False, codes=['SIG_005', 'CERT_002'])
        self.oversized_rsa(ec)
        self.dsa_key_case(ec)
        for digest in ('sha1', 'md5'):
            self.add('RS256 never falls back to ' + digest,
                self.module('rsa-weak-digest-' + digest, 'RS256', shared['rsa3072'], sign_options={'digest': digest}),
                False, codes=['SIG_003'])
        ed = shared['ed25519']
        for variant in ('Ed25519ph', 'Ed25519ctx'):
            self.add(variant + ' cannot replace pure Ed25519', self.module(variant, 'Ed25519', ed, sign_options={'variant': variant}), False, codes=['SIG_003'])
        raw = bytes.fromhex(self.algorithms['Ed25519']['signature'])
        order = 2**252 + 27742317777372353535851937790883648493
        high_scalar = int.from_bytes(raw[32:], 'little') + order
        self.add('Ed25519 noncanonical S scalar', self.module('ed-high-s', 'Ed25519', ed,
                 signature=raw[:32] + high_scalar.to_bytes(32, 'little')), False, codes=['SIG_003'])
        for length in (0, 63, 65, 114):
            self.add('Ed25519 exact signature length ' + str(length), self.module('ed-length-' + str(length), 'Ed25519', ed,
                     signature=bytes(length)), False, codes=['SIG_002', 'SIG_003'])
        foreign_ed = self.pki('foreign-ed25519', 'ED25519')
        foreign_signed = self.module('ed-foreign-signature', 'Ed25519', foreign_ed)
        wrong_key = self.module('ed-wrong-key', 'Ed25519', ed, signature=bytes.fromhex(foreign_signed['signature']))
        self.add('Ed25519 signature from another certified key', wrong_key, False, codes=['SIG_003'])
        ed448 = self.pki('forbidden-ed448', 'ED448')
        self.add('Ed448 key cannot impersonate Ed25519', self.module('ed448-key', 'Ed25519', ed448, signature=bytes(64)), False, codes=['SIG_005', 'CERT_002'])
        self.edwards_encoding_cases(ec, ed)
        self.rsa_boundary_cases()
        self.spki_encoding_cases(ec, shared['rsa3072'], ed)
        self.crl_structure_cases()
        self.leading_pem_cases(ec)
        self.issuer_and_revocation_cases()
        self.policy_upgrade_cases(ec, ed)
        legacy_payload = cbor(['uwvm-wsea-v1', 'matrix.app', 1, 0, len(WASM), hashlib.sha256(WASM).digest(),
                               self.now-60, self.now+43200, 'uwvm2', 0])
        self.add('detached v1 signature cannot authorize an embedded v2 module',
                 self.module('schema-downgrade-v1', 'ES256', ec, payload=legacy_payload), False, codes=['SIG_002'])
        # Freeze the final embedded artifact only after all evidence mutations.
        # Disposable sidecars remain producer inputs; neither VM nor Go reads them.
        for filename in dict.fromkeys(case['module'] for case in self.cases):
            path = Path(filename)
            path.write_bytes(pack_embedded(path.read_bytes(),
                Path(filename + '.sig.cose').read_bytes(), Path(filename + '.signer.pem').read_bytes(),
                Path(filename + '.crl.pem').read_bytes()))
        base = self.algorithms['ES256']; sealed = Path(base['module']).read_bytes()
        core, signature, certificates, crls = embedded_parts(sealed)
        content = uleb(len(SECTION_NAME)) + SECTION_NAME + cbor([1, signature, certificates, crls])
        encoded_size = bytearray(uleb(len(content))); encoded_size[-1] |= 128; encoded_size.append(0)
        padded_name = b'\x8c\x00' + SECTION_NAME + cbor([1, signature, certificates, crls])
        extra_content = uleb(len(SECTION_NAME)) + SECTION_NAME + cbor([1, signature, certificates, crls, b'extra'])
        framing = {
            'embedded signature cannot be followed by padding': sealed + b'\x00',
            'embedded signature cannot be followed by another section': sealed + b'\x00\x01\x00',
            'duplicate embedded signature section': sealed + sealed[len(core):],
            'truncated embedded section': sealed[:-1],
            'noncanonical signature section length': core + b'\0' + encoded_size + content,
            'noncanonical signature name length': core + b'\0' + uleb(len(padded_name)) + padded_name,
            'extra unsigned embedded bundle field': core + b'\0' + uleb(len(extra_content)) + extra_content,
        }
        for index, (name, content) in enumerate(framing.items()):
            path = self.out / 'modules' / ('embedded-framing-' + str(index) + '.wasm'); path.write_bytes(content)
            self.add(name, dict(base, module=str(path)), False, codes=['SIG_002'])
        path = self.out / 'modules' / 'embedded-missing.wasm'; path.write_bytes(core)
        self.add('selected unsigned module has no sidecar fallback', dict(base, module=str(path)), False, codes=['SIG_001'])
        files = {str(path.relative_to(self.out)): sha(path) for path in self.out.rglob('*') if path.is_file()}
        manifest = {'schema': SCHEMA, 'source_revision': 'embedded-v2', 'created_unix': self.now, 'openssl_command': self.openssl_path,
                    'algorithms': self.algorithms, 'guest_algorithms': self.guest_algorithms, 'cases': self.cases, 'producer_constraints': self.producers,
                    'commands': self.commands, 'files_sha256': files,
                    'source_sha256': sha(__file__), 'runner_source_sha256': sha(Path(__file__).with_name('algorithm_matrix_runner.cpp'))}
        (self.out / 'fixtures.json').write_text(json.dumps(manifest, indent=2) + '\n')
        return manifest

    def oversized_rsa(self, ca):
        # A real CA authenticates an oversized RSA SPKI. No slow >8192-bit key
        # generation is needed: the size-policy check must precede signature math.
        # OpenSSL performs the SPKI import and certificate production.
        directory = self.out / 'oversized-rsa'
        directory.mkdir(mode=0o700)
        rsa = der_item(0x30, der_integer((1 << 8192) + 1) + der_integer(65537))
        algid = bytes.fromhex('300d06092a864886f70d0101010500')
        spki = der_item(0x30, algid + der_item(3, b'\0' + rsa))
        (directory / 'public.der').write_bytes(spki)
        self.openssl('pkey', '-pubin', '-inform', 'DER', '-in', directory / 'public.der', '-out', directory / 'public.pem')
        self.openssl('x509', '-req', '-in', Path(ca['directory']) / 'signer.csr', '-force_pubkey', directory / 'public.pem',
                     '-CA', ca['root'], '-CAkey', Path(ca['directory']) / 'root.key', '-set_serial', '7001', '-days', '2', '-sha256',
                     '-extfile', Path(ca['directory']) / 'ca.cnf', '-extensions', 'signer', '-out', directory / 'signer.pem')
        custom = dict(ca, certificate=str(directory / 'signer.pem'), pin=hashlib.sha256(spki).hexdigest())
        self.add('RSA modulus above 8192 bits rejected before signature math', self.module('rsa8193', 'RS256', custom,
                 signature=bytes(1024)), False, codes=['SIG_005', 'CERT_002'])

    def dsa_key_case(self, ca):
        directory = self.out / 'dsa-key'
        directory.mkdir(mode=0o700)
        self.openssl('genpkey', '-genparam', '-algorithm', 'DSA', '-pkeyopt', 'dsa_paramgen_bits:2048', '-out', directory / 'params.pem')
        self.openssl('genpkey', '-paramfile', directory / 'params.pem', '-out', directory / 'signer.key')
        self.openssl('req', '-new', '-key', directory / 'signer.key', '-out', directory / 'signer.csr', '-subj', '/CN=WSEA Forbidden DSA')
        self.openssl('ca', '-batch', '-notext', '-config', Path(ca['directory']) / 'ca.cnf',
                     '-in', directory / 'signer.csr', '-out', directory / 'signer.pem')
        public = self.openssl('x509', '-in', directory / 'signer.pem', '-pubkey', '-noout')
        spki = self.openssl('pkey', '-pubin', '-outform', 'DER', data=public)
        custom = dict(ca, key=str(directory / 'signer.key'), certificate=str(directory / 'signer.pem'),
                      pin=hashlib.sha256(spki).hexdigest())
        self.add('DSA key cannot impersonate ECDSA', self.module('dsa-key', 'ES256', custom, signature=bytes(64)),
                 False, codes=['SIG_005', 'CERT_002'])

    def edwards_encoding_cases(self, ca, ed):
        directory = self.out / 'edwards-encoding'
        directory.mkdir(mode=0o700)
        p = 2**255 - 19
        values = [0, 1, p - 1,
            int.from_bytes(bytes.fromhex('26e8958fc2b227b045c3f489f2ef98f0d5dfac05d3c63339b13802886d53fc05'), 'little'),
            int.from_bytes(bytes.fromhex('c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac037a'), 'little'),
            p, p + 1]
        for number, y in enumerate(values):
            for sign in (0, 1):
                point = (y | (sign << 255)).to_bytes(32, 'little')
                label = f'ed-point-{number}-sign-{sign}'
                # Force the public SPKI only: a real authorized strong CA signs
                # this adversarial leaf, without an unavailable private key.
                spki = bytes.fromhex('302a300506032b6570032100') + point
                public = directory / (label + '.der')
                pem = directory / (label + '.pub.pem')
                certificate = directory / (label + '.pem')
                public.write_bytes(spki)
                self.openssl('pkey', '-pubin', '-inform', 'DER', '-in', public, '-out', pem)
                self.openssl('x509', '-req', '-in', Path(ca['directory']) / 'signer.csr', '-force_pubkey', pem,
                    '-CA', ca['root'], '-CAkey', Path(ca['directory']) / 'root.key', '-set_serial', str(8000 + number * 2 + sign),
                    '-days', '2', '-sha256', '-extfile', Path(ca['directory']) / 'ca.cnf', '-extensions', 'signer', '-out', certificate)
                custom = dict(ca, certificate=str(certificate), pin=hashlib.sha256(spki).hexdigest())
                identity_signature = b'\x01' + bytes(63)  # R=identity, S=0
                self.add('Ed25519 rejects certified low-order/noncanonical public key ' + label,
                    self.module(label + '-key', 'Ed25519', custom, signature=identity_signature), False, codes=['SIG_005', 'SIG_003'])
                self.add('Ed25519 rejects low-order/noncanonical R ' + label,
                    self.module(label + '-R', 'Ed25519', ed, signature=point + bytes(32)), False, codes=['SIG_003'])

    def sign_ca_der(self, name, tbs, algorithm, ca):
        # Preserve adversarial ASN.1 bytes exactly, rather than routing through
        # EVP/X509 serialization that could normalize the SPKI or algorithms.
        # Both production and independent verification are native OpenSSL.
        if algorithm != bytes.fromhex('300a06082a8648ce3d040302') or ca['issuer_family'] != 'EC':
            raise ValueError('raw DER fixture signer expects its P256/SHA256 CA')
        directory = self.out / 'signed-der'
        directory.mkdir(mode=0o700, exist_ok=True)
        source, signature, public = [directory / (name + suffix) for suffix in ('.tbs', '.signature', '.root.pub.pem')]
        source.write_bytes(tbs)
        key = Path(ca['directory']) / 'root.key'
        self.openssl('dgst', '-sha256', '-sign', key, '-out', signature, source)
        self.openssl('pkey', '-in', key, '-pubout', '-out', public)
        self.openssl('dgst', '-sha256', '-verify', public, '-signature', signature, source)
        return der_item(0x30, tbs + algorithm + der_item(3, b'\0' + signature.read_bytes()))

    def rsa_boundary_cases(self):
        for bits in (4096, 8192):
            started = time.monotonic()
            try:
                ca = self.pki('rsa' + str(bits), 'RSA', bits=bits)
            except subprocess.TimeoutExpired:
                self.producers.append({'producer': 'RSA' + str(bits) + ' genuine key generation', 'status': 'BLOCKED',
                    'reason': 'OpenSSL producer exceeded its configured deadline', 'timeout_seconds': self.rsa_keygen_timeout,
                    'elapsed_seconds': time.monotonic() - started})
                continue
            for algorithm in ('RS256', 'PS512'):
                body, marker = marker_guest('RSA' + str(bits) + '-' + algorithm)
                fixture = self.module('rsa' + str(bits) + '-' + algorithm, algorithm, ca, body=body,
                                      capabilities=3, guest_marker=marker)
                self.add('RSA' + str(bits) + ' genuine ' + algorithm + ' boundary and guest witness', fixture, True)
        restricted = self.pki('restricted-rsa-pss', 'RSA-PSS-RESTRICTED')
        self.add('Restricted RSA-PSS SPKI is outside generic RSA signing-key profile',
                 self.module('restricted-rsa-pss', 'PS256', restricted), False, codes=['SIG_005', 'CERT_002'])

    def spki_encoding_cases(self, ec, rsa, ed):
        cases = [('rsa-absent-null', rsa, 'RS256'), ('ec-compressed', ec, 'ES256'),
                 ('ec-explicit-curve', ec, 'ES256'), ('ed25519-null-params', ed, 'Ed25519')]
        for attempt in range(32):
            candidate = ed if attempt == 0 else self.pki('ed25519-aligned-key-' + str(attempt), 'ED25519')
            certificate = der_parts(pem_der(candidate['certificate'], 'CERTIFICATE'))
            fields = der_parts(certificate[0])
            public = der_parts(fields[6 if fields[0][0] == 0xa0 else 5])
            if not (der_payload(public[1], 3)[-1] & 1):
                cases.append(('ed25519-nonbytealigned', candidate, 'Ed25519'))
                break
        else:
            self.producers.append({'producer': '255-bit Ed25519 SPKI encoding', 'status': 'BLOCKED',
                                   'reason': 'no zero-padding final public-key bit in 32 genuine generated keys'})
        for name, ca, algorithm in cases:
            certificate = der_parts(pem_der(ca['certificate'], 'CERTIFICATE'))
            fields = der_parts(certificate[0])
            index = 6 if fields[0][0] == 0xa0 else 5
            spki = der_parts(fields[index])
            identifiers = der_parts(spki[0])
            if name == 'rsa-absent-null':
                if len(identifiers) != 2 or identifiers[1] != b'\x05\x00':
                    raise ValueError('expected rsaEncryption explicit NULL from OpenSSL')
                spki[0] = der_item(0x30, identifiers[0])
            elif name == 'ed25519-null-params':
                if len(identifiers) != 1:
                    raise ValueError('expected Ed25519 absent parameters from OpenSSL')
                spki[0] = der_item(0x30, identifiers[0] + b'\x05\x00')
            elif name == 'ec-compressed':
                point = der_payload(spki[1], 3)
                if len(point) != 66 or point[:2] != b'\x00\x04':
                    raise ValueError('expected uncompressed P256 public point')
                compressed = bytes([2 | (point[-1] & 1)]) + point[2:34]
                spki[1] = der_item(3, b'\x00' + compressed)
            elif name == 'ed25519-nonbytealigned':
                point = bytearray(der_payload(spki[1], 3))
                if point[0] != 0 or point[-1] & 1:
                    raise ValueError('expected zero final padding bit from selected real Ed25519 key')
                point[0] = 1  # Legal ASN.1 padding value: the unused bit itself is zero.
                spki[1] = der_item(3, bytes(point))
            else:
                explicit = self.openssl('ecparam', '-name', 'prime256v1', '-param_enc', 'explicit', '-outform', 'DER')
                der_parts(explicit)
                spki[0] = der_item(0x30, identifiers[0] + explicit)
            public_der = der_item(0x30, b''.join(spki))
            fields[index] = public_der
            path = self.out / (name + '.signer.pem')
            write_pem(path, 'CERTIFICATE', self.sign_ca_der(name, der_item(0x30, b''.join(fields)), certificate[1], ca))
            custom = dict(ca, certificate=str(path), pin=hashlib.sha256(public_der).hexdigest())
            fixture = self.module(name, algorithm, custom)
            self.add('CA-authenticated SPKI encoding must be rejected: ' + name, fixture, False, codes=['SIG_005', 'CERT_002'])

    def crl_structure_cases(self):
        ca = self.pki('crl-structure')
        original = der_parts(pem_der(ca['crl'], 'X509 CRL'))
        fields = der_parts(original[0])
        index = 1 if fields[0][0] == 2 else 0
        changed = list(fields)
        changed[index] = bytes.fromhex('300a06082a8648ce3d040303')  # inner ECDSA-SHA384
        fixture = self.module('crl-inner-outer-mismatch', 'ES256', ca)
        write_pem(fixture['module'] + '.crl.pem', 'X509 CRL', self.sign_ca_der('crl-inner-outer-mismatch',
            der_item(0x30, b''.join(changed)), original[1], ca))  # outer genuine ECDSA-SHA256
        self.add('CA-authenticated CRL inner and outer algorithms must agree', fixture, False, codes=['CERT_005', 'SIG_005'])
        # A genuine, signed v1 complete CRL is outside the strict v2 profile.
        v1 = [value for i, value in enumerate(fields) if not (i == 0 and value[0] == 2) and value[0] != 0xa0]
        fixture = self.module('crl-v1', 'ES256', ca)
        write_pem(fixture['module'] + '.crl.pem', 'X509 CRL', self.sign_ca_der('crl-v1',
            der_item(0x30, b''.join(v1)), original[1], ca))
        self.add('CA-authenticated v1 CRL is outside the strict complete-v2 profile', fixture, False, codes=['CERT_005'])
        remove = self.pki('crl-remove-from-crl')
        directory = Path(remove['directory'])
        self.openssl('ca', '-batch', '-config', directory / 'ca.cnf', '-revoke', remove['certificate'], '-crl_reason', 'removeFromCRL')
        self.openssl('ca', '-batch', '-config', directory / 'ca.cnf', '-gencrl', '-out', directory / 'reason8.crl.pem')
        encoded = pem_der(directory / 'reason8.crl.pem', 'X509 CRL')
        if bytes.fromhex('0603551d1504030a0108') not in encoded:
            raise ValueError('OpenSSL did not encode requested removeFromCRL reason 8')
        fixture = self.module('crl-remove-from-crl', 'ES256', remove)
        shutil.copyfile(directory / 'reason8.crl.pem', fixture['module'] + '.crl.pem')
        self.add('Full CRL reason removeFromCRL does not reinstate a listed signer', fixture, False, codes=['CERT_004'])

    def leading_pem_cases(self, ca):
        for nested in ('same-label', 'different-label'):
            for suffix, label, code in [('signer.pem', 'CERTIFICATE', 'CERT_002'), ('crl.pem', 'X509 CRL', 'CERT_005'),
                                        ('root.pem', 'CERTIFICATE', 'CERT_001')]:
                fixture = self.module('pem-leading-' + suffix + '-' + nested, 'ES256', ca)
                if suffix == 'root.pem':
                    path = self.out / ('pem-leading-root-' + nested + '.pem')
                    shutil.copyfile(ca['root'], path)
                    fixture['policy'] = self.policy(dict(ca, root=str(path)), ['ES256'])
                else:
                    path = Path(fixture['module'] + '.' + suffix)
                valid = path.read_bytes()
                prefix = b'-----BEGIN ' + label.encode() + b'-----\nAAAA\n'
                if nested == 'different-label':
                    prefix += b'-----BEGIN UNRELATED OBJECT-----\nQQ==\n-----END UNRELATED OBJECT-----\n'
                path.write_bytes(prefix + valid)
                self.add('PEM missing END cannot skip to nested valid block ' + suffix + ' ' + nested,
                         fixture, False, codes=[code])

    def issuer_and_revocation_cases(self):
        for name, family, algorithm, issuer, pss in [('ed-chain', 'ED25519', 'Ed25519', 'ED25519', False),
                    ('pss-chain', 'RSA', 'PS256', 'RSA', True)]:
            ca = self.pki(name, family, issuer_family=issuer, issuer_pss=pss)
            self.add(name + ' certificate and CRL signatures accepted', self.module(name, algorithm, ca), True)
            issuer_bad = self.pki(name + '-issuer-eku', family, issuer_family=issuer, issuer_pss=pss, issuer_eku='serverAuth')
            self.add(name + ' issuer EKU constrains code signing', self.module(name + '-issuer-eku', algorithm, issuer_bad),
                     False, codes=['CERT_006', 'CERT_002'])
            if pss:
                for label, mgf, salt in [('mgf', 'sha384', 32), ('salt', 'sha256', 31)]:
                    mismatch = self.module(name + '-crl-' + label, algorithm, ca)
                    self.openssl('ca', '-batch', '-config', Path(ca['directory']) / 'ca.cnf', '-gencrl',
                        '-out', mismatch['module'] + '.crl.pem', '-sigopt', 'rsa_padding_mode:pss',
                        '-sigopt', 'rsa_pss_saltlen:' + str(salt), '-sigopt', 'rsa_mgf1_md:' + mgf)
                    self.add(name + ' CRL PSS parameters must match strict profile ' + label, mismatch,
                             False, codes=['SIG_005', 'CERT_005'])
                    wrong_cert = self.module(name + '-certificate-' + label, algorithm, ca)
                    self.openssl('ca', '-batch', '-notext', '-config', Path(ca['directory']) / 'ca.cnf',
                        '-in', Path(ca['directory']) / 'signer.csr', '-out', wrong_cert['module'] + '.signer.pem',
                        '-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:' + str(salt),
                        '-sigopt', 'rsa_mgf1_md:' + mgf)
                    self.add(name + ' certificate PSS parameters must match strict profile ' + label,
                             wrong_cert, False, codes=['SIG_005', 'CERT_002'])
            bad = self.pki(name + '-eku', family, issuer_family=issuer, issuer_pss=pss, eku='serverAuth')
            self.add(name + ' codeSigning EKU remains mandatory', self.module(name + '-eku', algorithm, bad), False, codes=['CERT_006', 'CERT_002'])
            module = self.module(name + '-revoked', algorithm, ca)
            directory = Path(ca['directory'])
            self.openssl('ca', '-batch', '-config', directory / 'ca.cnf', '-revoke', ca['certificate'])
            opts = ['-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:32', '-sigopt', 'rsa_mgf1_md:sha256'] if pss else []
            self.openssl('ca', '-batch', '-config', directory / 'ca.cnf', '-gencrl', '-out', directory / 'revoked.crl.pem', *opts)
            shutil.copyfile(directory / 'revoked.crl.pem', module['module'] + '.crl.pem')
            self.add(name + ' authenticated revocation', module, False, codes=['CERT_004'], group=name + '-revocation')
            replay = self.module(name + '-old-good', algorithm, ca)
            self.add(name + ' old good CRL cannot undo tombstone', replay, False, codes=['CERT_004'], group=name + '-revocation')

    def policy_upgrade_cases(self, ec, ed):
        # One key admits two EC algorithm IDs; bytes are identical, but the
        # trusted policy must explicitly broaden at a newer policy version.
        old = self.module('policy-upgrade-old', 'ES256', ec, policy=self.policy(ec, version=1))
        new = self.module('policy-upgrade-new', 'ESP256', ec, policy=self.policy(ec, ['ES256', 'ESP256'], version=2))
        self.add('legacy policy initial authorization', old, True, group='policy-upgrade')
        self.add('new algorithm is not enabled by signature metadata', dict(new, policy=self.policy(ec, version=1)), False, codes=['SIG_005'], group='policy-upgrade')
        self.add('explicit newer policy broadens algorithms', new, True, group='policy-upgrade')
        self.add('old six-field policy cannot restore older trust state', old, False, codes=['POL_002'], group='policy-upgrade')


def elf_identity(path):
    data = Path(path).read_bytes()
    if len(data) < 52 or data[:4] != b'\x7fELF' or data[4] not in (1, 2) or data[5] not in (1, 2):
        raise RuntimeError('candidate must be an actual ELF binary')
    endian = '<' if data[5] == 1 else '>'
    bits = 32 if data[4] == 1 else 64
    result = {'bits': bits, 'byte_order': 'little' if data[5] == 1 else 'big',
            'machine': struct.unpack_from(endian + 'H', data, 18)[0],
            'flags': struct.unpack_from(endian + 'I', data, 36 if bits == 32 else 48)[0]}
    phoff = struct.unpack_from(endian + ('I' if bits == 32 else 'Q'), data, 28 if bits == 32 else 32)[0]
    phsize, phcount = struct.unpack_from(endian + 'HH', data, 42 if bits == 32 else 54)
    if phcount and phsize < (32 if bits == 32 else 56):
        raise RuntimeError('invalid ELF program-header width')
    for index in range(phcount):
        offset = phoff + index * phsize
        if offset + phsize > len(data):
            raise RuntimeError('truncated ELF program headers')
        if struct.unpack_from(endian + 'I', data, offset)[0] != 3:  # PT_INTERP
            continue
        start = struct.unpack_from(endian + ('I' if bits == 32 else 'Q'), data, offset + (4 if bits == 32 else 8))[0]
        length = struct.unpack_from(endian + ('I' if bits == 32 else 'Q'), data, offset + (16 if bits == 32 else 32))[0]
        value = data[start:start + length]
        name, separator, padding = value.partition(b'\0')
        # Some authenticated MIPS SDK libc images align .interp with extra
        # zero bytes. PT_INTERP describes a NUL-terminated pathname, rather
        # than requiring that the first terminator be the segment's last byte.
        if (not separator or not name or len(value) != length or any(padding) or
                'interpreter' in result):
            raise RuntimeError('invalid ELF interpreter record')
        result['interpreter'] = name.decode('ascii')
        if padding:
            result['interpreter_zero_padding'] = len(padding)
    return result


def sysroot_file(root, guest_path):
    """Resolve target absolute symlinks inside the supplied root, not host /."""
    if not guest_path.startswith('/'):
        raise RuntimeError('ELF interpreter must have an absolute target path')
    pending, parts, links = guest_path.split('/'), [], 0
    while pending:
        component = pending.pop(0)
        if component in ('', '.'):
            continue
        if component == '..':
            if parts:
                parts.pop()
            continue
        path = root.joinpath(*parts, component)
        if path.is_symlink():
            links += 1
            if links > 40:
                raise RuntimeError('target loader symlink loop')
            value = os.readlink(path)
            if value.startswith('/'):
                parts = []
            pending = value.split('/') + pending
        else:
            parts.append(component)
    result = root.joinpath(*parts)
    if not result.is_file():
        raise RuntimeError('ELF interpreter is absent from the actual target sysroot: ' + str(result))
    return result


def target_elf(actual, target):
    machine, bits, endian, _, hard_float = TARGETS[target]
    if (actual['machine'], actual['bits'], actual['byte_order']) != (machine, bits, endian):
        raise RuntimeError('ELF identity does not match claimed target; native relabeling is forbidden')
    if target in ('arm', 'armhf', 'armeb') and bool(actual['flags'] & 0x400) != hard_float:
        raise RuntimeError('ARM ELF floating-point ABI does not match target')
    if target in ('arm', 'armhf', 'armeb') and (actual['flags'] & 0xff000000) != 0x05000000:
        raise RuntimeError('ARM target must use the intended EABI5 profile')
    if target == 'ppc64' and (actual['flags'] & 3) != 1:
        raise RuntimeError('big-endian ppc64 must use the intended ELFv1 ABI')
    if target == 'mips64' and actual['flags'] & (0x20 | 0xf000):
        raise RuntimeError('big-endian mips64 must use n64, never n32/o32 ABI')
    if target == 'loongarch64' and actual['flags'] & 7 != 3:
        raise RuntimeError('LoongArch requires LP64D')


def selected_cases(manifest, profile):
    if profile == 'full':
        return manifest['cases']
    if profile != 'product':
        raise ValueError('unknown qualification profile')
    # The byte-identical verifier is exhaustively qualified once per target.
    # Every other actual product executes all algorithms and their forged guest,
    # RSA boundaries, state transitions and complete embedded-container checks.
    return [case for case in manifest['cases'] if (
        'authenticated fd_write guest entry witness' in case['name'] or
        'forbidden signature never reaches fd_write guest' in case['name'] or
        'genuine ' in case['name'] or
        case['group'] in ('policy-upgrade', 'ed-chain-revocation', 'pss-chain-revocation') or
        case['name'].startswith(('embedded ', 'duplicate embedded ', 'truncated embedded ',
            'noncanonical signature ', 'extra unsigned embedded ', 'selected unsigned ', 'detached v1 ')))]


class Execution:
    def __init__(self, args):
        self.args = args
        self.out = args.output.absolute()
        self.out.mkdir(mode=0o700, parents=True, exist_ok=False)
        os.chmod(self.out, 0o700)
        self.prefix = json.loads(args.exec_prefix_json)
        if not isinstance(self.prefix, list) or not all(isinstance(x, str) for x in self.prefix):
            raise RuntimeError('--exec-prefix-json must be an argv string array')
        self.runner = args.runner.resolve(strict=True)
        self.records, self.contexts = [], {}
        self.report = {'schema': SCHEMA, 'target': args.target, 'execution': args.execution,
                       'execution_oracle_revision': 'r4',
                       'guest_execution': 'actual_full_vm_requested' if args.vm else 'NOT_RUN',
                       'runner': str(self.runner), 'runner_sha256': sha(self.runner),
                       'exec_prefix': self.prefix, 'sysroot': str(args.target_sysroot.resolve(strict=True)),
                       'runtime_root': str(args.runtime_root.resolve(strict=True)), 'elf': elf_identity(self.runner),
                       'driver_sha256': sha(__file__), 'started_unix': int(time.time()), 'records': self.records}
        receipt = args.runtime_receipt.resolve(strict=True)
        runtime = json.loads(receipt.read_text())
        if any(runtime.get(key) != self.report[value] for key, value in
               (('target', 'target'), ('sysroot', 'sysroot'), ('runtime_root', 'runtime_root'))):
            raise RuntimeError('runtime receipt is for a different target/sysroot/runtime root')
        if not runtime.get('runtime_files'):
            raise RuntimeError('runtime receipt must list actual target runtime files')
        files = []
        for row in runtime['runtime_files']:
            path = Path(row['path'])
            if not path.is_absolute() or sha(path) != row['sha256']:
                raise RuntimeError('runtime receipt file is absent or changed: ' + str(path))
            record = {'path': str(path), 'sha256': row['sha256']}
            if path.read_bytes()[:4] == b'\x7fELF':
                record['elf'] = elf_identity(path)
                target_elf(record['elf'], args.target)
            files.append(record)
        self.report['runtime_receipt'] = {'path': str(receipt), 'sha256': sha(receipt), 'metadata': runtime, 'verified_files': files}
        self.check_identity()

    def invoke(self, arguments, *, vm=False):
        executable = self.args.vm.resolve(strict=True) if vm else self.runner
        return subprocess.run([*self.prefix, str(executable), *map(str, arguments)], capture_output=True,
                              text=True, timeout=self.args.timeout)

    def check_identity(self):
        machine, bits, endian, arch, hard_float = TARGETS[self.args.target]
        actual = self.report['elf']
        target_elf(actual, self.args.target)
        if self.args.execution == 'qemu':
            qemu_index = 0
            host_loader = None
            if len(self.prefix) >= 4 and self.prefix[1] == '--library-path':
                host_loader = Path(self.prefix[0]).resolve(strict=True)
                host_identity = elf_identity(host_loader)
                if (host_identity['machine'], host_identity['bits'], host_identity['byte_order']) != (62, 64, 'little'):
                    raise RuntimeError('QEMU host loader must be the recorded x86_64 ELF loader')
                qemu_index = 3
            if len(self.prefix) <= qemu_index or not Path(self.prefix[qemu_index]).is_absolute() or not Path(self.prefix[qemu_index]).name.startswith('qemu-'):
                raise RuntimeError('QEMU execution requires the actual qemu executable in the prefix')
            if '-L' not in self.prefix:
                raise RuntimeError('QEMU filesystem prefix must be explicit')
            filesystem_prefix = self.prefix[self.prefix.index('-L') + 1]
            explicit_loader = None
            if filesystem_prefix != self.report['sysroot']:
                # A library-only QEMU sysroot rewrites open("/") and breaks
                # the VM's intentional root-fd/O_NOFOLLOW policy walk. Execute
                # the frozen target loader explicitly with the real filesystem.
                expected = sysroot_file(Path(self.report['sysroot']), actual['interpreter'])
                if (filesystem_prefix != '/' or len(self.prefix) < 3 or self.prefix[-2] != '--library-path' or
                        Path(self.prefix[-3]).resolve(strict=True) != expected.resolve(strict=True)):
                    raise RuntimeError('explicit target loader must match the frozen PT_INTERP provider')
                explicit_loader = expected
            self.report['qemu'] = {'path': str(Path(self.prefix[qemu_index]).resolve(strict=True)), 'sha256': sha(self.prefix[qemu_index])}
            if host_loader:
                self.report['qemu']['host_loader'] = {'path': str(host_loader), 'sha256': sha(host_loader),
                                                    'library_path': self.prefix[2]}
            self.report['qemu']['filesystem_prefix'] = filesystem_prefix
            if explicit_loader:
                self.report['qemu']['explicit_target_loader'] = {'path': str(explicit_loader), 'sha256': sha(explicit_loader)}
        elif self.prefix:
            raise RuntimeError('native execution cannot hide a nonempty execution prefix')
        if actual.get('interpreter'):
            loader = sysroot_file(Path(self.report['sysroot']), actual['interpreter'])
            loader_elf = elf_identity(loader)
            target_elf(loader_elf, self.args.target)
            if not any(row['sha256'] == sha(loader) for row in self.report['runtime_receipt']['verified_files']):
                raise RuntimeError('actual PT_INTERP loader is not frozen in the runtime receipt')
            self.report['loader'] = {'path': str(loader), 'sha256': sha(loader), 'elf': loader_elf}
        process = self.invoke(['--identity'])
        if process.returncode:
            raise RuntimeError('candidate did not execute its identity under the selected wrapper: ' + process.stderr)
        identity = json.loads(process.stdout)
        if (identity['architecture'], identity['pointer_bits'], identity['byte_order']) != (arch, bits, endian):
            raise RuntimeError('executed target identity differs from ELF/declared target')
        if self.args.target.startswith('arm') and identity['hard_float'] != hard_float:
            raise RuntimeError('executed target float ABI differs from the declared target')
        if identity['openssl_number'] < 0x30000000:
            raise RuntimeError('candidate did not execute the supported OpenSSL backend')
        self.report['executed_identity'] = identity
        if self.args.vm:
            vm_elf = elf_identity(self.args.vm)
            target_elf(vm_elf, self.args.target)
            if vm_elf.get('interpreter') != actual.get('interpreter'):
                raise RuntimeError('VM and runner must use the same frozen target loader')
            self.report['vm'] = {'path': str(self.args.vm.resolve(strict=True)), 'sha256': sha(self.args.vm), 'elf': vm_elf}

    def policy(self, case):
        group = case['group']
        if group not in self.contexts:
            directory = self.out / 'state' / ('group-' + hashlib.sha256(group.encode()).hexdigest()[:20])
            directory.mkdir(mode=0o700, parents=True)
            (directory / 'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
            (directory / 'state').chmod(0o600)
            self.contexts[group] = (directory, directory.parent / (directory.name + '.policy'))
        directory, policy = self.contexts[group]
        policy.write_text(case['policy'].replace('@STATE_DIR@', str(directory)))
        policy.chmod(0o600)
        return policy

    def run_case(self, case):
        policy = self.policy(case)
        arguments = [policy, case['module']]
        process = self.invoke(arguments)
        text = process.stdout + process.stderr
        expect = case['expected']
        passed = (process.returncode == 0 and process.stdout.strip() == 'ALLOW') if expect == 'ALLOW' else (
            process.returncode == 2 and process.stdout.startswith('DENY ') and
            (not case['codes'] or any(code in text for code in case['codes'])))
        row = {'name': case['name'], 'algorithm': case['algorithm'], 'expected': expect, 'codes': case['codes'],
               'expected_exit_codes': [0] if expect == 'ALLOW' else [2],
               'returncode': process.returncode, 'stdout': process.stdout, 'stderr': process.stderr,
               'argv': [*self.prefix, str(self.runner), *map(str, arguments)], 'passed': passed}
        self.records.append(row)
        print('PASS' if passed else 'FAIL', case['name'], flush=True)
        if self.args.vm and case['vm']:
            vm_args = ['--secure', 'module', case['module'], policy, '--run', case['module']]
            vm = self.invoke(vm_args, vm=True)
            output = vm.stdout + vm.stderr
            marker = case.get('guest_marker')
            vm_ok = (vm.returncode == 0 and (not marker or marker in vm.stdout)) if expect == 'ALLOW' else (vm.returncode in (126, 127) and
                bool(re.search(r'\b(?:SIG|CERT|POL|VM)_\d{3}\b', output)) and
                GUEST_PREFIX not in output and (not case['codes'] or any(code in output for code in case['codes'])))
            self.records.append({'name': case['name'] + ' full VM', 'algorithm': case['algorithm'],
                'expected': expect, 'returncode': vm.returncode, 'stdout': vm.stdout, 'stderr': vm.stderr,
                'expected_exit_codes': [0] if expect == 'ALLOW' else [126, 127],
                'required_guest_marker': marker,
                'argv': [*self.prefix, str(self.args.vm), *map(str, vm_args)], 'passed': vm_ok})

    def run(self, fixtures):
        manifest = json.loads((fixtures / 'fixtures.json').read_text())
        if manifest['schema'] != SCHEMA:
            raise RuntimeError('unsupported fixture manifest')
        if (manifest.get('source_revision') != 'embedded-v2' or set(manifest['algorithms']) != set(ALGORITHMS) or
                set(manifest.get('guest_algorithms', {})) != set(ALGORITHMS) or not manifest['cases']):
            raise RuntimeError('fixture manifest is missing required algorithm coverage')
        for rel, expected in manifest['files_sha256'].items():
            if sha(fixtures / rel) != expected:
                raise RuntimeError('frozen fixture changed: ' + rel)
        self.report['fixtures_sha256'] = sha(fixtures / 'fixtures.json')
        self.report['fixture_generator_sha256'] = manifest['source_sha256']
        self.report['producer_constraints'] = manifest['producer_constraints']
        profile = getattr(self.args, 'case_profile', 'full')
        chosen = selected_cases(manifest, profile)
        self.report['case_profile'] = profile
        self.report['available_fixture_cases'] = len(manifest['cases'])
        self.report['executed_fixture_cases'] = len(chosen)
        for case in chosen:
            self.run_case(case)
        incomplete = bool(manifest['producer_constraints'])
        self.report['status'] = 'FAIL' if not all(x['passed'] for x in self.records) else 'BLOCKED' if incomplete else 'PASS'
        self.report['finished_unix'] = int(time.time())
        (self.out / 'report.json').write_text(json.dumps(self.report, indent=2) + '\n')
        return 0 if self.report['status'] == 'PASS' else 77 if incomplete and all(x['passed'] for x in self.records) else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--prepare', action='store_true')
    parser.add_argument('--openssl', default='openssl')
    parser.add_argument('--ed-variant-signer', type=Path)
    parser.add_argument('--rsa-keygen-timeout', type=int, default=600, help='per genuine RSA4096/8192 key-generation deadline; timeout is BLOCKED')
    parser.add_argument('--fixtures', type=Path)
    parser.add_argument('--runner', type=Path)
    parser.add_argument('--vm', type=Path, help='actual full guest-executing VM, never a loader-only probe')
    parser.add_argument('--target', choices=TARGETS)
    parser.add_argument('--execution', choices=['qemu', 'native'], default='qemu')
    parser.add_argument('--exec-prefix-json', default='[]')
    parser.add_argument('--target-sysroot', type=Path)
    parser.add_argument('--runtime-root', type=Path)
    parser.add_argument('--runtime-receipt', type=Path)
    parser.add_argument('--case-profile', choices=['full', 'product'], default='full')
    parser.add_argument('--timeout', type=int, default=90)
    parser.add_argument('--require-cgroup', action='store_true')
    args = parser.parse_args()
    os.umask(0o077)
    if sys.platform != 'linux':
        print('BLOCKED: execute only on the authorized SSH Linux host', file=sys.stderr)
        return 77
    before = cgroup_snapshot(args.require_cgroup)
    if args.prepare:
        if args.rsa_keygen_timeout <= 0:
            parser.error('--rsa-keygen-timeout must be positive')
        generated = Fixtures(args.output, openssl=args.openssl, ed_variant_signer=args.ed_variant_signer,
                             rsa_keygen_timeout=args.rsa_keygen_timeout).generate()
        print(json.dumps({'status': 'GENERATED_NOT_EXECUTED', 'cases': len(generated['cases']),
                          'producer_constraints': generated['producer_constraints'], 'manifest': str(args.output / 'fixtures.json')}))
        return 77 if generated['producer_constraints'] else 0
    for key in ('fixtures', 'runner', 'target', 'target_sysroot', 'runtime_root', 'runtime_receipt'):
        if getattr(args, key) is None:
            parser.error('--' + key.replace('_', '-') + ' is required for an execution report')
    execution = None
    try:
        execution = Execution(args)
        result = execution.run(args.fixtures.resolve(strict=True))
        after = cgroup_snapshot(args.require_cgroup)
        if before is not None:
            execution.report.update(cgroup_before=before, cgroup_after=after)
            if before['cgroup_incarnation'] != after['cgroup_incarnation']:
                execution.report['status'] = 'FAIL'
                result = 1
            for old, new in zip(before['cgroup_ancestors'], after['cgroup_ancestors']):
                if old.get('memory.events') is None:
                    continue
                old_events = dict(row.split() for row in old['memory.events'].splitlines())
                new_events = dict(row.split() for row in new['memory.events'].splitlines())
                if any(old_events.get(key) != new_events.get(key) for key in ('oom', 'oom_kill')):
                    execution.report['status'] = 'FAIL'
                    result = 1
            (execution.out / 'report.json').write_text(json.dumps(execution.report, indent=2) + '\n')
        print(json.dumps({'status': execution.report['status'], 'checks': len(execution.records), 'report': str(execution.out / 'report.json')}))
        return result
    except Exception as error:
        if execution is not None:
            execution.report.update(status='FAIL', error=str(error), finished_unix=int(time.time()))
            (execution.out / 'report.json').write_text(json.dumps(execution.report, indent=2) + '\n')
        print('HARNESS_ERROR', str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
